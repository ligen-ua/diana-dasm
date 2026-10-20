#pragma once

#include "orthia_image_format.h"

namespace orthia
{
    // What the formats Diana parses (PE, ELF) share: detection by DianaExecutable_DetectType,
    // the parser's error codes turned into messages, and a main module described by its headers
    class CDianaImageFormat : public IImageFormat
    {
        int m_executableType;
    public:
        explicit CDianaImageFormat(int executableType);
        int GetExecutableType() const { return m_executableType; }

        bool Detect(const char* header, size_t size) const override;
        std::shared_ptr<ISimpleFile> MapFile(const std::vector<char>& data,
            const MapFileParameters& params) const final;
        void RegisterMainModule(CModuleManager& manager,
            ISimpleFile& file,
            IMemoryReader& reader,
            const PlatformString_type& shortName,
            bool deepAnalysis) const override;
    protected:
        // may throw diana::CException
        virtual std::shared_ptr<ISimpleFile> MapDianaFile(const std::vector<char>& data,
            const MapFileParameters& params) const = 0;
    };

    const IImageFormat& GetPeImageFormat();
    const IImageFormat& GetElfImageFormat();
}
