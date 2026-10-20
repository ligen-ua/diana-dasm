#include "orthia_diana_image_format.h"
#include "orthia_elf.h"

#include "diana_executable.h"

namespace orthia
{
    namespace
    {
        class CElfImageFormat : public CDianaImageFormat
        {
        public:
            CElfImageFormat()
                : CDianaImageFormat(DIANA_EXECUTABLE_TYPE_ELF)
            {
            }
            const char* GetId() const override { return "elf"; }
            const char* GetDisplayName() const override { return "ELF"; }
            int GetBuiltInFlag() const override { return ModuleInfo::builtInFlags_moduleTypeElf; }

        protected:
            std::shared_ptr<ISimpleFile> MapDianaFile(const std::vector<char>& data,
                const MapFileParameters& params) const override
            {
                auto file = std::make_shared<CSimpleElfFile>();
                file->MapFile(data, params);
                return file;
            }
        };
    }

    const IImageFormat& GetElfImageFormat()
    {
        static const CElfImageFormat format;
        return format;
    }
}
