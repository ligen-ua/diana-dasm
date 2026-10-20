#pragma once

#include "orthia_image_format.h"
#include "orthia_memory_cache.h"
#include "orthia_streams.h"
#include "diana_executable.h"

namespace orthia
{
    class CDianaImageFormat;

    // A module as Diana sees it: DianaExecutable in module mode over the mapped image.
    // Writes the parser makes (import linking) stay in its own cache.
    class CDianaModuleImage : public CModuleImageBase, oui::Noncopyable
    {
        const CDianaImageFormat& m_format;
        CMemoryStorageOfModifiedData m_writeCache;
        CMemoryCache m_module;
        DianaMemoryStream m_stream;
        DianaExecutable m_exe;
        bool m_initialized = false;
    protected:
        Address_type m_base;
        Address_type m_size;

        DianaExecutable& GetExecutable() { return m_exe; }
        const DianaExecutable& GetExecutable() const { return m_exe; }
        DianaMemoryStream& GetStream() { return m_stream; }
        // an export's value as an address
        virtual Address_type ToAbsolute(OPERAND_SIZE value) const { return m_base + value; }
        virtual Address_type QueryEntryPoint() const = 0;

        void QueryExports(std::vector<NameInfo>& out) override;
    public:
        CDianaModuleImage(const CDianaImageFormat& format, IMemoryReader& reader, Address_type base, Address_type size);
        ~CDianaModuleImage();
        // DI_SUCCESS when the headers parsed as this image's format
        int Init();

        const IImageFormat& GetFormat() const override;
        WorkAddressRangeInfo GetRange() const override;
        void QueryImports(std::vector<NameInfo>& out) override;
    };

    // What the formats Diana parses (PE, ELF) share: detection by DianaExecutable_DetectType,
    // the parser's error codes turned into messages, and a main module described by its headers
    class CDianaImageFormat : public IImageFormat
    {
        int m_executableType;
    public:
        explicit CDianaImageFormat(int executableType);
        int GetExecutableType() const { return m_executableType; }

        bool Detect(const char* header, size_t size) const override;
        std::unique_ptr<IModuleImage> OpenModuleImage(IMemoryReader& reader,
            Address_type base,
            Address_type size) const final;
        std::shared_ptr<ISimpleFile> MapFile(const std::vector<char>& data,
            const MapFileParameters& params) const final;
        bool RegisterMainModule(CModuleManager& manager,
            ISimpleFile& file,
            IMemoryReader& reader,
            const PlatformString_type& shortName,
            bool deepAnalysis) const override;
    protected:
        virtual std::unique_ptr<CDianaModuleImage> CreateModuleImage(IMemoryReader& reader,
            Address_type base,
            Address_type size) const = 0;
        // may throw diana::CException
        virtual std::shared_ptr<ISimpleFile> MapDianaFile(const std::vector<char>& data,
            const MapFileParameters& params) const = 0;
    };

    const IImageFormat& GetPeImageFormat();
    const IImageFormat& GetElfImageFormat();
}
