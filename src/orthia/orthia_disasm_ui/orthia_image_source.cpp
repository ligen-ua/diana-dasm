#include "orthia_image_source.h"
#include "orthia_model_modules.h"
#include "orthia_files.h"
#include "orthia_log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

extern "C"
{
#include "diana_pe.h"
#include "diana_executable.h"
}

namespace orthia
{
    // CMainImageSource
    CMainImageSource::CMainImageSource(std::shared_ptr<ISimpleFile> file)
        : m_file(std::move(file))
    {
    }
    Address_type CMainImageSource::GetBase() const
    {
        return m_file->GetImageBase();
    }
    Address_type CMainImageSource::GetSize() const
    {
        return m_file->GetMappedFile().size();
    }

    // CNullImageSource
    CNullImageSource::CNullImageSource(Address_type base, Address_type size, const PlatformString_type& reason)
        : m_base(base), m_size(size), m_reason(reason)
    {
    }

    // CLinkedFileImageSource
    CLinkedFileImageSource::CLinkedFileImageSource(Address_type base,
        Address_type size,
        const PlatformString_type& path,
        const ImageIdentity& identity,
        int executableType,
        int dianaMode)
        :
        m_base(base),
        m_size(size),
        m_path(path),
        m_identity(identity),
        m_executableType(executableType),
        m_dianaMode(dianaMode),
        m_state((int)ImageState::NotLoaded)
    {
    }
    void CLinkedFileImageSource::SetState(ImageState state, const std::string& reason)
    {
        // called under m_lock
        m_reason = Utf8ToPlatformString(reason);
        m_state.store((int)state);
        if (state == ImageState::Stale || state == ImageState::Failed)
        {
            ORTHIA_LOG(orthia::LogSeverity::Warning, "Dependency image not available: ", PlatformStringToUtf8(m_path), ": ", reason);
        }
    }
    PlatformString_type CLinkedFileImageSource::GetStateReason() const
    {
        // the UI refreshes the module list often: never wait for a load in progress
        // (the reason is only set once the state leaves NotLoaded)
        std::unique_lock<std::mutex> guard(m_lock, std::try_to_lock);
        if (!guard.owns_lock())
        {
            return PlatformString_type();
        }
        return m_reason;
    }
    std::shared_ptr<const ISimpleFile> CLinkedFileImageSource::GetImage()
    {
        std::lock_guard<std::mutex> guard(m_lock);
        if (m_image)
        {
            return m_image;
        }
        if (m_state.load() != (int)ImageState::NotLoaded)
        {
            // stale or failed: decided once, not retried on every read
            return nullptr;
        }
        m_image = LoadImpl();
        return m_image;
    }
    std::shared_ptr<const ISimpleFile> CLinkedFileImageSource::LoadImpl()
    {
        std::vector<char> raw;
        if (LoadFileToVector_Silent(m_path, raw) != 0 || raw.empty())
        {
            SetState(ImageState::Stale, "file not found");
            return nullptr;
        }

        std::shared_ptr<ISimpleFile> image;
        try
        {
            MapFileParameters params;
            // both formats use 1 for "keep the preferred base": the relocation below moves it
            params.mapFlags = DIANA_PE_MAP_DO_NOT_RELOCATE;
            image = MakeSimpleFile(m_executableType, raw, params);
        }
        catch (const std::exception& e)
        {
            SetState(ImageState::Failed, std::string("cannot map: ") + e.what());
            return nullptr;
        }
        if (!image || image->GetMappedFile().empty())
        {
            SetState(ImageState::Failed, "cannot map");
            return nullptr;
        }
        if (image->GetDianaMode() != m_dianaMode)
        {
            SetState(ImageState::Stale, "bitness mismatch");
            return nullptr;
        }
        ImageIdentity actual;
        if (!QueryImageIdentity(*image, actual))
        {
            SetState(ImageState::Failed, "unknown format");
            return nullptr;
        }
        if (!SameIdentity(m_identity, actual))
        {
            SetState(ImageState::Stale, DescribeIdentityMismatch(m_identity, actual));
            return nullptr;
        }
        if (image->GetMappedFile().size() != m_size)
        {
            SetState(ImageState::Stale, "mapped size mismatch");
            return nullptr;
        }
        try
        {
            image->Relocate(m_base);
        }
        catch (const std::exception& e)
        {
            SetState(ImageState::Failed, std::string("cannot relocate: ") + e.what());
            return nullptr;
        }
        SetState(ImageState::Ready, std::string());
        return image;
    }
    void CLinkedFileImageSource::PreCheckHeader()
    {
        if (m_identity.format != ImageIdentity::format_Pe)
        {
            return;
        }
        std::lock_guard<std::mutex> guard(m_lock);
        if (m_state.load() != (int)ImageState::NotLoaded)
        {
            return;
        }
        std::vector<char> header(0x1000);
        {
            std::ifstream stream(std::filesystem::path(m_path), std::ios::binary);
            if (!stream)
            {
                SetState(ImageState::Stale, "file not found");
                return;
            }
            stream.read(header.data(), header.size());
            header.resize((size_t)stream.gcount());
        }
        ImageIdentity actual;
        if (!QueryPeHeaderIdentity(header.data(), header.size(), actual))
        {
            SetState(ImageState::Stale, "not a PE file");
            return;
        }
        // the header has no GUID, so only the timestamp and size are compared here
        if (!SameIdentity(m_identity, actual))
        {
            SetState(ImageState::Stale, DescribeIdentityMismatch(m_identity, actual));
        }
    }

    // ImageSourceTable
    void ImageSourceTable::Add(std::shared_ptr<IImageSource> source)
    {
        m_sources.push_back(std::move(source));
    }
    void ImageSourceTable::SetMain(std::shared_ptr<IImageSource> main)
    {
        m_main = main;
        m_sources.push_back(std::move(main));
    }
    void ImageSourceTable::Sort()
    {
        std::sort(m_sources.begin(), m_sources.end(),
            [](const std::shared_ptr<IImageSource>& a, const std::shared_ptr<IImageSource>& b)
        {
            return a->GetBase() < b->GetBase();
        });
    }
    IImageSource* ImageSourceTable::Find(Address_type address) const
    {
        // first source with base > address, then step back
        auto it = std::upper_bound(m_sources.begin(), m_sources.end(), address,
            [](Address_type value, const std::shared_ptr<IImageSource>& source)
        {
            return value < source->GetBase();
        });
        if (it == m_sources.begin())
        {
            return nullptr;
        }
        --it;
        return (*it)->Contains(address) ? it->get() : nullptr;
    }
    IImageSource* ImageSourceTable::FindNext(Address_type address) const
    {
        auto it = std::upper_bound(m_sources.begin(), m_sources.end(), address,
            [](Address_type value, const std::shared_ptr<IImageSource>& source)
        {
            return value < source->GetBase();
        });
        return it == m_sources.end() ? nullptr : it->get();
    }

    // CCompositeMemoryReader
    CCompositeMemoryReader::CCompositeMemoryReader(std::shared_ptr<const ImageSourceTable> table)
        : m_table(std::move(table))
    {
    }
    void CCompositeMemoryReader::Read(Address_type offset,
        Address_type bytesToRead,
        void* pBuffer,
        Address_type* pBytesRead,
        int flags,
        Address_type selectorValue,
        DianaUnifiedRegister selectorHint)
    {
        *pBytesRead = 0;
        auto* source = m_table ? m_table->Find(offset) : nullptr;
        if (!source)
        {
            return;
        }
        auto image = source->GetImage();
        if (!image)
        {
            return;
        }
        const auto& data = image->GetMappedFile();
        const Address_type base = image->GetImageBase();
        if (offset < base)
        {
            return;
        }
        Address_type relativeOffset = offset - base;
        if (relativeOffset >= data.size())
        {
            return;
        }
        Address_type relativeEnd = relativeOffset;
        DI_CHECK_CPP(Diana_SafeAdd(&relativeEnd, bytesToRead));
        if (relativeEnd > data.size())
        {
            relativeEnd = data.size();
        }
        Address_type sizeToCopy = relativeEnd - relativeOffset;
        memcpy(pBuffer, data.data() + relativeOffset, (size_t)sizeToCopy);
        *pBytesRead = sizeToCopy;
    }

    // status texts
    PlatformString_type ModuleStatusText(const ModuleInfo& mod)
    {
        PlatformString_type text;
        if (mod.builtInFlags & ModuleInfo::builtInFlags_unresolved)
        {
            text = ORTHIA_TCSTR("unresolved");
        }
        else
        {
            switch ((ImageSourceKind)mod.imageSourceKind)
            {
            case ImageSourceKind::LinkedFile:
                text = ((ImageState)mod.imageState == ImageState::Stale || (ImageState)mod.imageState == ImageState::Failed)
                    ? ORTHIA_TCSTR("stale") : ORTHIA_TCSTR("linked");
                break;
            case ImageSourceKind::None:
                if (mod.imageState == (int)ImageState::Unavailable && mod.imageSourceKnown)
                {
                    text = ORTHIA_TCSTR("unlinked");
                }
                break;
            default:
                break;
            }
        }
        auto append = [&](const PlatformString_type::value_type* part)
        {
            if (!text.empty())
            {
                text += ORTHIA_TCSTR(", ");
            }
            text += part;
        };
        if (mod.flags & ModuleInfo::flags_analyzeDone)
        {
            append(ORTHIA_TCSTR("analysis"));
        }
        if (mod.flags & ModuleInfo::flags_symbolsLoaded)
        {
            append(ORTHIA_TCSTR("symbols"));
        }
        return text;
    }
    PlatformString_type ModuleImageSourceText(const ModuleInfo& mod)
    {
        switch ((ImageSourceKind)mod.imageSourceKind)
        {
        case ImageSourceKind::Main:
            return ORTHIA_TCSTR("main");
        case ImageSourceKind::LinkedFile:
        {
            auto state = (ImageState)mod.imageState;
            if (state == ImageState::Stale || state == ImageState::Failed)
            {
                return ORTHIA_TCSTR("stale (") + mod.imageStateReason + ORTHIA_TCSTR(")");
            }
            return state == ImageState::Ready ? ORTHIA_TCSTR("linked, loaded") : ORTHIA_TCSTR("linked");
        }
        case ImageSourceKind::None:
            if (mod.builtInFlags & ModuleInfo::builtInFlags_unresolved)
            {
                return ORTHIA_TCSTR("none (unresolved dependency)");
            }
            return mod.imageStateReason.empty() ? ORTHIA_TCSTR("none") : ORTHIA_TCSTR("none (") + mod.imageStateReason + ORTHIA_TCSTR(")");
        default:
            return PlatformString_type();
        }
    }
}
