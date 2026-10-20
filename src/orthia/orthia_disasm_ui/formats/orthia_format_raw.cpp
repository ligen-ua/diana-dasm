#include "orthia_image_format.h"
#include "orthia_shellcode.h"
#include "orthia_module_manager.h"

namespace orthia
{
    namespace
    {
        // bytes without headers: no entry point, no names, no imports
        class CRawModuleImage : public CModuleImageBase
        {
            const IImageFormat& m_format;
            Address_type m_base;
            Address_type m_size;
        public:
            CRawModuleImage(const IImageFormat& format, Address_type base, Address_type size)
                : m_format(format), m_base(base), m_size(size)
            {
            }
            const IImageFormat& GetFormat() const override { return m_format; }
            WorkAddressRangeInfo GetRange() const override
            {
                WorkAddressRangeInfo range;
                range.address = m_base;
                range.size = m_size;
                range.lastValidAddress = m_base;
                Diana_SafeAdd(&range.lastValidAddress, m_size ? m_size - 1 : 0);
                return range;
            }
        };

        // raw code: placed where the user says and disassembled in the mode the user says
        class CRawImageFormat : public IImageFormat
        {
        public:
            const char* GetId() const override { return "raw"; }
            const char* GetDisplayName() const override { return "raw code"; }
            int GetBuiltInFlag() const override { return ModuleInfo::builtInFlags_moduleTypeRaw; }

            // nothing to recognize: the user chooses it for a file no other format takes
            bool Detect(const char*, size_t) const override { return false; }
            std::unique_ptr<IModuleImage> OpenModuleImage(IMemoryReader&,
                Address_type base,
                Address_type size) const override
            {
                return std::make_unique<CRawModuleImage>(*this, base, size);
            }

            bool NeedsUserParameters() const override { return true; }
            std::shared_ptr<ISimpleFile> MapFile(const std::vector<char>& data,
                const MapFileParameters& params) const override
            {
                if (params.imageBase == (DI_UINT64)(-1) || !params.dianaMode)
                {
                    throw std::runtime_error("Raw code needs a base address and a mode");
                }
                return std::make_shared<CSimpleShellcodeFile>(data, params.imageBase, params.dianaMode);
            }
            // no headers to find the code from: the whole range is analyzed, always
            bool RegisterMainModule(CModuleManager& manager,
                ISimpleFile& file,
                IMemoryReader& reader,
                const PlatformString_type&,
                bool) const override
            {
                manager.ReloadRange(file.GetImageBase(), file.GetMappedFile().size(), &reader, file.GetDianaMode(), 0);
                return true;
            }
            Address_type GetInitialAddress(ISimpleFile& file) const override
            {
                return file.GetImageBase();
            }
        };
    }

    const IImageFormat& GetRawImageFormat()
    {
        static const CRawImageFormat format;
        return format;
    }
}
