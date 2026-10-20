#include "orthia_diana_image_format.h"
#include "orthia_pe.h"
#include "../orthia_model_modules.h"

namespace orthia
{
    namespace
    {
        class CPeModuleImage : public CDianaModuleImage
        {
        public:
            using CDianaModuleImage::CDianaModuleImage;

        protected:
            Address_type QueryEntryPoint() const override
            {
                const OPERAND_SIZE rva = GetExecutable().u.peFile.pImpl->addressOfEntryPoint;
                Address_type entryPoint = m_base;
                if (!rva || Diana_SafeAdd(&entryPoint, rva))
                {
                    return 0;
                }
                return entryPoint;
            }
            // TLS callbacks run before the entry point
            void QuerySpecialNames(std::vector<NameInfo>& out) override
            {
                void* pTlsCallbacks = 0;
                int tlsCallbacksCount = 0;
                OPERAND_SIZE addressOfTLSIndex = 0;
                if (DianaExecutable_QueryTLSCallbacks(&GetExecutable(),
                    m_base,
                    &GetStream(),
                    &pTlsCallbacks,
                    &tlsCallbacksCount,
                    &addressOfTLSIndex,
                    DIANA_ANALYZE_RANDOM_READ_ABSOLUTE))
                {
                    return;
                }
                const int dianaMode = GetExecutable().dianaMode;
                const char* pTls = (const char*)pTlsCallbacks;
                for (int i = 0; i < tlsCallbacksCount; ++i)
                {
                    // pointers in the image: already addresses
                    const OPERAND_SIZE callback = Diana_ReadValue(pTls, dianaMode);
                    if (callback)
                    {
                        AddName(out, NameInfo::flags_Export, callback, "$tls_" + ToAnsiStringAsHex((unsigned short)i));
                    }
                    pTls += dianaMode;
                }
                DIANA_FREE(pTlsCallbacks);
            }
        };

        class CPeImageFormat : public CDianaImageFormat
        {
        public:
            CPeImageFormat()
                : CDianaImageFormat(DIANA_EXECUTABLE_TYPE_PE)
            {
            }
            const char* GetId() const override { return "pe"; }
            const char* GetDisplayName() const override { return "PE"; }
            int GetBuiltInFlag() const override { return ModuleInfo::builtInFlags_moduleTypePe; }
            // TimeDateStamp and SizeOfImage are in the NT headers; the RSDS GUID is not
            size_t GetHeaderIdentitySize() const override { return 0x1000; }
            bool QueryHeaderIdentity(const char* header, size_t size, ImageIdentity& identity) const override
            {
                return QueryPeHeaderIdentity(header, size, identity);
            }
            std::unique_ptr<IDependencyLoader> CreateDependencyLoader(
                std::shared_ptr<oui::BaseOperation> operation) const override
            {
                return std::make_unique<CPEImportsLoader>(operation);
            }

        protected:
            std::unique_ptr<CDianaModuleImage> CreateModuleImage(IMemoryReader& reader,
                Address_type base,
                Address_type size) const override
            {
                return std::make_unique<CPeModuleImage>(*this, reader, base, size);
            }
            std::shared_ptr<ISimpleFile> MapDianaFile(const std::vector<char>& data,
                const MapFileParameters& params) const override
            {
                auto file = std::make_shared<CSimplePeFile>();
                file->MapFile(data, params);
                return file;
            }
        };
    }

    const IImageFormat& GetPeImageFormat()
    {
        static const CPeImageFormat format;
        return format;
    }
}
