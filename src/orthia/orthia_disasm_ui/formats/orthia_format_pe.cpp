#include "orthia_diana_image_format.h"
#include "orthia_pe.h"

#include "diana_executable.h"

namespace orthia
{
    namespace
    {
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

        protected:
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
