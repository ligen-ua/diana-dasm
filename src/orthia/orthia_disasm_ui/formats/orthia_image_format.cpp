#include "orthia_image_format.h"
#include "orthia_diana_image_format.h"
#include "../orthia_helpers.h"
#include "../orthia_external_symbols.h"

namespace orthia
{
    const IImageFormat& GetRawImageFormat();

    std::unique_ptr<IExternalSymbolsLoader> IImageFormat::CreateSymbolsLoader(const SymbolsLoaderParams&) const
    {
        return nullptr;
    }

    void CModuleImageBase::AddName(std::vector<NameInfo>& out, int flags, Address_type address, const std::string& utf8Name)
    {
        NameInfo info;
        info.flags = flags;
        info.address = address;
        info.name = Utf8ToPlatformString(utf8Name);
        out.push_back(std::move(info));
    }

    void CModuleImageBase::QueryNames(std::vector<NameInfo>& out)
    {
        // an entry point at the base is the "no entry point" of a DLL or a shared library
        const auto range = GetRange();
        if (range.entryPoint && range.entryPoint != range.address)
        {
            AddName(out, NameInfo::flags_Export, range.entryPoint, "$entrypoint");
        }
        // before the exports: a global IFUNC's resolver is its export too ($ifunc_memcpy and
        // memcpy), and a caller that keeps one name per address keeps the "$" one
        QuerySpecialNames(out);
        QueryExports(out);
    }

    const std::vector<const IImageFormat*>& GetImageFormats()
    {
        // a new format is one more line here
        static const std::vector<const IImageFormat*> formats = {
            &GetPeImageFormat(),
            &GetElfImageFormat(),
            &GetRawImageFormat(),
        };
        return formats;
    }

    const IImageFormat* FindFormatByData(const char* header, size_t size)
    {
        for (auto* format : GetImageFormats())
        {
            if (format->Detect(header, size))
            {
                return format;
            }
        }
        return nullptr;
    }

    const IImageFormat* FindFormatById(const std::string& id)
    {
        for (auto* format : GetImageFormats())
        {
            if (id == format->GetId())
            {
                return format;
            }
        }
        return nullptr;
    }

    bool QueryMappedFileNames(const IImageFormat& format, const ISimpleFile& file, std::vector<NameInfo>& out)
    {
        const auto& mapped = file.GetMappedFile();
        CMemoryReaderOnLoadedData reader(file.GetImageBase(), mapped.data(), mapped.size());
        auto image = format.OpenModuleImage(reader, file.GetImageBase(), mapped.size());
        if (!image)
        {
            return false;
        }
        image->QueryNames(out);
        return true;
    }

    const IImageFormat* FindFormatByModule(const ModuleInfo& mod)
    {
        for (auto* format : GetImageFormats())
        {
            if (mod.builtInFlags & format->GetBuiltInFlag())
            {
                return format;
            }
        }
        PlatformString_type ext;
        GetExtensionOfFile(mod.fullName, &ext);
        if (ext == ORTHIA_TCSTR("dll") || ext == ORTHIA_TCSTR("exe") || ext == ORTHIA_TCSTR("sys"))
        {
            return FindFormatById("pe");
        }
        return nullptr;
    }

    void QueryModuleSections(const ModuleAccess& access, ImageSections& sections)
    {
        sections = ImageSections();
        if (auto* format = FindFormatByModule(*access.mod))
        {
            format->QuerySections(access, sections);
            return;
        }
        QueryImageSections(access.reader, access.mod->address, access.imageFile, access.openFile, sections);
    }

}
