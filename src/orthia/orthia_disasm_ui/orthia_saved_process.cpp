#include "orthia_saved_process.h"
#include "orthia_image_source.h"
#include "orthia_floating_code.h"
#include "orthia_process_adapter.h"
#include "formats/orthia_image_format.h"
#include "orthia_tiny_xml.h"
#include "orthia_log.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace orthia
{
    const ORTHIA_TCHAR* const g_savedModulesFileName = ORTHIA_TCSTR("modules.xml");

    namespace
    {
        namespace fs = std::filesystem;

        const int c_savedModulesVersion = 1;

        std::string ToHex(unsigned long long value)
        {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%llx", value);
            return buffer;
        }

        bool ParseHex(const char* text, unsigned long long& value)
        {
            if (!text || !*text)
            {
                return false;
            }
            char* end = nullptr;
            value = strtoull(text, &end, 16);
            return end && *end == 0;
        }

        unsigned long long HexAttribute(const tinyxml2::XMLElement* element, const char* name)
        {
            unsigned long long value = 0;
            ParseHex(element->Attribute(name), value);
            return value;
        }

        std::string TextAttribute(const tinyxml2::XMLElement* element, const char* name)
        {
            const char* value = element->Attribute(name);
            return value ? value : "";
        }

        bool ReadWholeFile(const fs::path& path, std::vector<char>& data, size_t maxSize)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return false;
            }
            data.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            return data.size() <= maxSize;
        }

        // the identity attributes, as module metainfo has them (orthia_image_identity.cpp)
        void WriteIdentity(tinyxml2::XMLElement* element, const ImageIdentity& identity)
        {
            if (!identity.IsValid())
            {
                return;
            }
            element->SetAttribute("id_format", identity.format);
            if (identity.format == ImageIdentity::format_Pe)
            {
                element->SetAttribute("id_timestamp", ToHex(identity.timeDateStamp).c_str());
                element->SetAttribute("id_sizeofimage", ToHex(identity.sizeOfImage).c_str());
                if (identity.hasGuid)
                {
                    element->SetAttribute("id_guid", BytesToHex(&identity.guid, sizeof(identity.guid)).c_str());
                    element->SetAttribute("id_age", ToHex(identity.age).c_str());
                }
            }
            else if (identity.format == ImageIdentity::format_Elf)
            {
                element->SetAttribute("id_buildid", identity.buildIdHex.c_str());
            }
        }

        void ReadIdentity(const tinyxml2::XMLElement* element, ImageIdentity& identity)
        {
            identity = ImageIdentity();
            const int format = element->IntAttribute("id_format", ImageIdentity::format_Unknown);
            if (format == ImageIdentity::format_Pe)
            {
                identity.format = format;
                identity.timeDateStamp = (DI_UINT32)HexAttribute(element, "id_timestamp");
                identity.sizeOfImage = (DI_UINT32)HexAttribute(element, "id_sizeofimage");
                std::vector<unsigned char> guid;
                if (HexToBytes(TextAttribute(element, "id_guid"), guid) && guid.size() == sizeof(identity.guid))
                {
                    memcpy(&identity.guid, guid.data(), sizeof(identity.guid));
                    identity.hasGuid = true;
                    identity.age = (DI_UINT32)HexAttribute(element, "id_age");
                }
            }
            else if (format == ImageIdentity::format_Elf)
            {
                identity.buildIdHex = TextAttribute(element, "id_buildid");
                // no build-id: nothing tells this file from another build
                if (!identity.buildIdHex.empty())
                {
                    identity.format = format;
                }
            }
        }
    }

    std::string FormatSavedModules(const SavedModuleList& list)
    {
        tinyxml2::XMLDocument doc;
        auto* root = doc.NewElement("modules");
        doc.InsertEndChild(root);
        root->SetAttribute("format_version", c_savedModulesVersion);
        root->SetAttribute("main", ToHex(list.mainModule).c_str());
        for (const auto& saved : list.modules)
        {
            const auto& mod = saved.module;
            auto* element = doc.NewElement("module");
            element->SetAttribute("base", ToHex(mod.address).c_str());
            element->SetAttribute("size", ToHex(mod.size).c_str());
            element->SetAttribute("name", PlatformStringToUtf8(mod.name).c_str());
            element->SetAttribute("path", PlatformStringToUtf8(mod.fullName).c_str());
            element->SetAttribute("mode", mod.dianaMode);
            element->SetAttribute("arch", mod.dianaArch);
            element->SetAttribute("entry", ToHex(mod.entryPoint).c_str());
            element->SetAttribute("builtin", mod.builtInFlags);
            WriteIdentity(element, saved.identity);
            root->InsertEndChild(element);
        }
        tinyxml2::XMLPrinter printer;
        doc.Print(&printer);
        return printer.CStr();
    }

    bool ParseSavedModules(const std::string& text, SavedModuleList& list)
    {
        tinyxml2::XMLDocument doc;
        if (doc.Parse(text.c_str(), text.size()) != tinyxml2::XML_SUCCESS)
        {
            return false;
        }
        const auto* root = doc.FirstChildElement("modules");
        if (!root || root->IntAttribute("format_version", 0) <= 0)
        {
            return false;
        }
        SavedModuleList result;
        result.mainModule = (Address_type)HexAttribute(root, "main");
        for (const auto* element = root->FirstChildElement("module"); element; element = element->NextSiblingElement("module"))
        {
            SavedModuleInfo saved;
            auto& mod = saved.module;
            mod.address = (Address_type)HexAttribute(element, "base");
            mod.size = (Address_type)HexAttribute(element, "size");
            if (!mod.size)
            {
                return false;
            }
            mod.lastValidAddress = mod.address + mod.size - 1;
            mod.name = Utf8ToPlatformString(TextAttribute(element, "name"));
            mod.fullName = Utf8ToPlatformString(TextAttribute(element, "path"));
            mod.dianaMode = element->IntAttribute("mode", 0);
            mod.dianaArch = element->IntAttribute("arch", 0);
            mod.entryPoint = (Address_type)HexAttribute(element, "entry");
            mod.builtInFlags = element->IntAttribute("builtin", 0);
            ReadIdentity(element, saved.identity);
            result.modules.push_back(std::move(saved));
        }
        std::sort(result.modules.begin(), result.modules.end(),
            [](const SavedModuleInfo& a, const SavedModuleInfo& b) { return a.module.address < b.module.address; });
        list = std::move(result);
        return true;
    }

    void WriteSavedModules(const PlatformString_type& folder, const SavedModuleList& list)
    {
        const std::string text = FormatSavedModules(list);
        const fs::path target = fs::path(folder) / g_savedModulesFileName;
        std::vector<char> current;
        if (ReadWholeFile(target, current, 64 * 1024 * 1024) && std::string(current.begin(), current.end()) == text)
        {
            return;
        }
        // a reader never sees half a file
        fs::path temp = target;
        temp += ".tmp";
        {
            std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
            stream.write(text.data(), (std::streamsize)text.size());
            if (!stream)
            {
                throw std::runtime_error("Can't write " + PlatformStringToUtf8(temp.native()));
            }
        }
        std::error_code ec;
        fs::rename(temp, target, ec);
        if (ec)
        {
            fs::remove(temp, ec);
            throw std::runtime_error("Can't write " + PlatformStringToUtf8(target.native()));
        }
    }

    bool ReadSavedModules(const PlatformString_type& folder, SavedModuleList& list)
    {
        std::vector<char> data;
        if (!ReadWholeFile(fs::path(folder) / g_savedModulesFileName, data, 64 * 1024 * 1024))
        {
            return false;
        }
        return ParseSavedModules(std::string(data.begin(), data.end()), list);
    }

    // CSavedProcess
    CSavedProcess::CSavedProcess(const PlatformString_type& folder, const FolderMeta& meta, const SavedModuleList& list)
        :
        m_folder(folder),
        m_meta(meta),
        m_list(list)
    {
        ReadCaptureIndex(folder, m_capture);
        for (const auto& region : m_capture.regions)
        {
            auto& end = m_allocationEnds[region.allocationBase ? region.allocationBase : region.base];
            end = std::max<Address_type>(end, region.base + region.size);
        }
        for (const auto& saved : m_list.modules)
        {
            const auto& mod = saved.module;
            const auto* format = FindFormatByModule(mod);
            std::shared_ptr<IImageSource> source;
            if (!format)
            {
                source = std::make_shared<CNullImageSource>(mod.address, mod.size, PlatformString_type(ORTHIA_TCSTR("unknown format")));
            }
            else if (!saved.identity.IsValid())
            {
                source = std::make_shared<CNullImageSource>(mod.address, mod.size,
                    PlatformString_type(ORTHIA_TCSTR("nothing tells the build of its file")));
            }
            else
            {
                auto linked = std::make_shared<CLinkedFileImageSource>(mod.address, mod.size, mod.fullName, saved.identity,
                    *format, mod.dianaMode, mod.dianaArch);
                linked->PreCheckHeader();
                source = linked;
            }
            m_sources.push_back(source);
        }
    }

    IImageSource* CSavedProcess::FindSource(Address_type address) const
    {
        auto it = std::upper_bound(m_sources.begin(), m_sources.end(), address,
            [](Address_type value, const std::shared_ptr<IImageSource>& source) { return value < source->GetBase(); });
        if (it == m_sources.begin())
        {
            return nullptr;
        }
        --it;
        return (*it)->Contains(address) ? it->get() : nullptr;
    }

    const CapturedRegion* CSavedProcess::FindCaptured(Address_type address) const
    {
        const auto& regions = m_capture.regions;
        auto it = std::upper_bound(regions.begin(), regions.end(), address,
            [](Address_type value, const CapturedRegion& region) { return value < region.base; });
        if (it == regions.begin())
        {
            return nullptr;
        }
        --it;
        return address - it->base < it->size ? &*it : nullptr;
    }

    bool CSavedProcess::IsCaptured(Address_type address) const
    {
        const auto* region = FindCaptured(address);
        return region && !region->file.empty();
    }

    void CSavedProcess::CopyCaptured(unsigned long long offset, void* pBuffer, size_t size, bool& found) const
    {
        const unsigned long long end = offset + size < offset ? ~0ull : offset + size;
        const auto& regions = m_capture.regions;
        auto it = std::upper_bound(regions.begin(), regions.end(), offset,
            [](unsigned long long value, const CapturedRegion& region) { return value < region.base; });
        if (it != regions.begin())
        {
            --it;
        }
        for (; it != regions.end() && it->base < end; ++it)
        {
            if (it->file.empty())
            {
                continue;
            }
            const unsigned long long start = std::max<unsigned long long>(offset, it->base);
            const unsigned long long stop = std::min<unsigned long long>(end, it->base + it->size);
            if (start >= stop)
            {
                continue;
            }
            std::ifstream stream(fs::path(m_folder) / g_captureFolder / Utf8ToPlatformString(it->file), std::ios::binary);
            if (!stream)
            {
                continue;
            }
            stream.seekg((std::streamoff)(start - it->base));
            stream.read((char*)pBuffer + (start - offset), (std::streamsize)(stop - start));
            if (stream.gcount() == (std::streamsize)(stop - start))
            {
                found = true;
            }
        }
    }

    bool CSavedProcess::AddSnapshot(const FloatingRegionInfo& info)
    {
        if (info.snapshotFile.empty())
        {
            return false;
        }
        Snapshot snapshot;
        snapshot.base = info.allocationBase;
        snapshot.allocationSize = info.allocationSize;
        snapshot.protect = info.protect;
        snapshot.type = info.type;
        snapshot.mappedName = info.mappedName;
        const auto path = fs::path(m_folder) / g_floatingSnapshotFolder / Utf8ToPlatformString(info.snapshotFile);
        if (!ReadWholeFile(path, snapshot.data, (size_t)info.allocationSize) || snapshot.data.empty())
        {
            ORTHIA_LOG(orthia::LogSeverity::Warning, "Can't read the snapshot ", PlatformStringToUtf8(path.native()));
            return false;
        }
        std::lock_guard<std::mutex> guard(m_lock);
        m_snapshots.erase(std::remove_if(m_snapshots.begin(), m_snapshots.end(),
            [&](const Snapshot& s) { return s.base == snapshot.base; }), m_snapshots.end());
        m_snapshots.push_back(std::move(snapshot));
        return true;
    }

    std::tuple<int, unsigned long long> CSavedProcess::GetSizeInBytes() const
    {
        return std::make_tuple(0, m_meta.dianaMode == DIANA_MODE32 ? 0xFFFFFFFFull : ~0ull);
    }

    int CSavedProcess::ReadExact(std::shared_ptr<oui::BaseOperation>, unsigned long long offset, size_t size, std::vector<char>& data)
    {
        data.resize(size);
        return ReadExactEx2(offset, data.data(), size);
    }

    oui::String CSavedProcess::GetFullFileName() const
    {
        return oui::String(m_meta.name);
    }

    oui::String CSavedProcess::GetFullFileNameForUI() const
    {
        return oui::String(m_meta.name);
    }

    int CSavedProcess::ReadExactEx2(unsigned long long offset, void* pBuffer, size_t size)
    {
        memset(pBuffer, 0, size);
        if (!size)
        {
            return 1;
        }
        bool found = false;
        const unsigned long long end = offset + size < offset ? ~0ull : offset + size;
        auto copy = [&](Address_type base, const std::vector<char>& data) {
            const unsigned long long dataEnd = base + data.size();
            const unsigned long long start = std::max<unsigned long long>(offset, base);
            const unsigned long long stop = std::min<unsigned long long>(end, dataEnd);
            if (start < stop)
            {
                memcpy((char*)pBuffer + (start - offset), data.data() + (start - base), (size_t)(stop - start));
                found = true;
            }
        };
        for (const auto& source : m_sources)
        {
            if (source->GetBase() >= end || source->GetLastValidAddress() < offset)
            {
                continue;
            }
            // a captured module needs no file
            if (IsCaptured(source->GetBase()))
            {
                continue;
            }
            if (auto image = source->GetImage())
            {
                copy(source->GetBase(), image->GetMappedFile());
            }
        }
        {
            std::lock_guard<std::mutex> guard(m_lock);
            for (const auto& snapshot : m_snapshots)
            {
                copy(snapshot.base, snapshot.data);
            }
        }
        // the memory as it was: over the files and the snapshots
        CopyCaptured(offset, pBuffer, size, found);
        return found ? 0 : 1;
    }

    WorkAddressData CSavedProcess::ReadExactEx(unsigned long long offset, size_t size)
    {
        std::vector<char> buffer(size);
        if (ReadExactEx2(offset, buffer.data(), size))
        {
            return WorkAddressData();
        }
        auto* pBufferStart = buffer.data();
        return WorkAddressData(pBufferStart, size, nullptr, WorkAddressData::flags_FullValid,
            [buffer = std::move(buffer)](WorkAddressData*) mutable {});
    }

    int CSavedProcess::QueryModules(std::vector<ModuleInfo>& modules, int& processModuleOffset,
        std::function<void(const ModuleInfo& info, oui::ModuleDisasmContext&)> contextCallback)
    {
        modules.clear();
        processModuleOffset = -1;
        ProcessReaderAdapter reader(this);
        for (size_t i = 0; i < m_list.modules.size(); ++i)
        {
            ModuleInfo info = m_list.modules[i].module;
            auto& source = m_sources[i];
            // its exports, which also loads the file
            std::unique_ptr<IModuleImage> image;
            const bool captured = IsCaptured(info.address);
            if (captured || (source->GetKind() != ImageSourceKind::None && source->GetImage()))
            {
                if (const auto* format = FindFormatByModule(info))
                {
                    image = format->OpenModuleImage(reader, info.address, info.size);
                }
            }
            info.imageSourceKnown = true;
            info.imageSourceKind = (int)(captured ? ImageSourceKind::MemoryImage : source->GetKind());
            info.imageState = (int)(captured ? ImageState::Ready : source->GetState());
            info.imageStateReason = captured ? PlatformString_type() : source->GetStateReason();
            if (image)
            {
                oui::ModuleDisasmContext context;
                context.image = image.get();
                contextCallback(info, context);
            }
            if (info.address == m_list.mainModule)
            {
                processModuleOffset = (int)modules.size();
            }
            modules.push_back(info);
        }
        return 0;
    }

    namespace
    {
        class CSavedThread : public oui::IThread
        {
            CapturedThread m_thread;
        public:
            explicit CSavedThread(const CapturedThread& thread) : m_thread(thread) {}
            int GetInfo(oui::ThreadInfo& info) const override
            {
                info.tid = m_thread.tid;
                info.startAddress = m_thread.startAddress;
                if (!m_thread.startTime.empty())
                {
                    info.startTime.InitFromSQL(m_thread.startTime);
                }
                return 0;
            }
            int CaptureStack(int framesCount, std::vector<Address_type>& stack) const override
            {
                stack.assign(m_thread.stack.begin(), m_thread.stack.begin() +
                    std::min<size_t>(m_thread.stack.size(), (size_t)std::max(framesCount, 0)));
                return 0;
            }
        };
        class CSavedThreadEnumerator : public oui::IThreadEnumerator
        {
            std::vector<CapturedThread> m_threads;
            size_t m_next = 0;
        public:
            explicit CSavedThreadEnumerator(const std::vector<CapturedThread>& threads) : m_threads(threads) {}
            std::tuple<int, std::shared_ptr<oui::IThread>> GetNextThread() override
            {
                if (m_next >= m_threads.size())
                {
                    return std::make_tuple(0, std::shared_ptr<oui::IThread>());
                }
                return std::make_tuple(0, std::shared_ptr<oui::IThread>(std::make_shared<CSavedThread>(m_threads[m_next++])));
            }
        };
    }

    std::tuple<int, std::shared_ptr<oui::IThreadEnumerator>> CSavedProcess::CreateThreadEnumerator()
    {
        // the threads of a .capture; none otherwise
        if (!m_capture.hasThreads)
        {
            return std::make_tuple(1, std::shared_ptr<oui::IThreadEnumerator>());
        }
        return std::make_tuple(0, std::shared_ptr<oui::IThreadEnumerator>(std::make_shared<CSavedThreadEnumerator>(m_capture.threads)));
    }

    oui::ProcessStartInfo CSavedProcess::GetStartInfo() const
    {
        oui::ProcessStartInfo info;
        info.unixSeconds = m_meta.startUnix;
        info.key = m_meta.startKey;
        return info;
    }

    int CSavedProcess::QueryMemoryRegion(unsigned long long address, oui::MemoryRegionInfo& info)
    {
        info = oui::MemoryRegionInfo();
        if (const auto* region = FindCaptured(address))
        {
            info.allocationBase = region->allocationBase ? region->allocationBase : region->base;
            auto end = m_allocationEnds.find(info.allocationBase);
            info.allocationSize = (end != m_allocationEnds.end() ? end->second : region->base + region->size) - info.allocationBase;
            info.regionBase = region->base;
            info.regionSize = region->size;
            info.protect = region->protect;
            info.committed = region->committed;
            info.type = region->type;
            info.mappedName = region->name;
            return 0;
        }
        {
            std::lock_guard<std::mutex> guard(m_lock);
            for (const auto& snapshot : m_snapshots)
            {
                if (address - snapshot.base < snapshot.allocationSize)
                {
                    info.allocationBase = info.regionBase = snapshot.base;
                    info.allocationSize = info.regionSize = snapshot.allocationSize;
                    info.protect = snapshot.protect;
                    info.committed = true;
                    info.type = snapshot.type;
                    info.mappedName = snapshot.mappedName;
                    return 0;
                }
            }
        }
        if (auto* source = FindSource(address))
        {
            info.allocationBase = info.regionBase = source->GetBase();
            info.allocationSize = info.regionSize = source->GetSize();
            info.protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_exec;
            info.committed = source->GetState() != ImageState::Unavailable;
            info.type = oui::MemoryRegionInfo::type_image;
            return 0;
        }
        return 1;
    }

    PlatformString_type ResolveSavedProcessFolder(const std::vector<DatabaseInfo>& all, const PlatformString_type& argument)
    {
        std::error_code ec;
        fs::path path(argument);
        if (fs::is_regular_file(path, ec))
        {
            // its data.db, or another file in it
            path = path.parent_path();
        }
        if (fs::is_directory(path, ec))
        {
            auto folder = fs::absolute(path, ec).native();
            EraseLastSlash(folder);
            return folder;
        }
        // a pid or a folder name of the data folder
        auto selected = ResolveDatabaseSelectors(all, { argument }, [&](const PlatformString_type&) -> PlatformString_type {
            throw std::runtime_error("No process folder: " + PlatformStringToUtf8(argument));
        });
        if (selected.size() != 1 || selected[0].kind != DatabaseInfo::Kind::Process)
        {
            throw std::runtime_error("Not a process folder: " + PlatformStringToUtf8(argument));
        }
        return selected[0].folder;
    }

    std::shared_ptr<CSavedProcess> OpenSavedProcess(const PlatformString_type& folder)
    {
        const std::string folderText = PlatformStringToUtf8(folder);
        FolderMeta meta;
        if (!ReadFolderMeta((fs::path(folder) / ORTHIA_TCSTR("meta.xml")).native(), meta))
        {
            throw std::runtime_error("Not an orthia folder (no meta.xml): " + folderText);
        }
        if (meta.kind != g_folderKindProcess)
        {
            throw std::runtime_error("Not a process folder: " + folderText);
        }
        if (!IsFolderReadable(meta))
        {
            throw std::runtime_error("The folder was made by a newer orthia: " + folderText);
        }
        std::error_code ec;
        if (!fs::is_regular_file(fs::path(folder) / ORTHIA_TCSTR("data.db"), ec))
        {
            throw std::runtime_error("The folder has no data.db: " + folderText);
        }
        SavedModuleList list;
        // a folder made before modules.xml: the database and the snapshots only
        ReadSavedModules(folder, list);
        return std::make_shared<CSavedProcess>(folder, meta, list);
    }
}
