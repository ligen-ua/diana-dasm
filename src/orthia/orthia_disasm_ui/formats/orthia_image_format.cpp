#include "orthia_image_format.h"
#include "orthia_diana_image_format.h"

namespace orthia
{
    const std::vector<const IImageFormat*>& GetImageFormats()
    {
        // a new format is one more line here
        static const std::vector<const IImageFormat*> formats = {
            &GetPeImageFormat(),
            &GetElfImageFormat(),
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

    const IImageFormat* FindFormatByExecutableType(int executableType)
    {
        for (auto* format : GetImageFormats())
        {
            auto* dianaFormat = dynamic_cast<const CDianaImageFormat*>(format);
            if (dianaFormat && dianaFormat->GetExecutableType() == executableType)
            {
                return format;
            }
        }
        return nullptr;
    }
}
