#pragma once

#include "oui_color.h"

namespace oui
{

    struct TextMarkup
    {
        struct Range
        {
            std::uint32_t id = 0;
            std::uint32_t sizeInBytes = 0;
            LabelColorProfile colorProfile;
        };
        std::vector<Range> ranges;
    };

    class CTextMarkupBuilder
    {
        TextMarkup m_markup;
    public:
        void AddNextRange(std::uint32_t id, std::uint32_t sizeInBytes, const LabelColorProfile& colorProfile);
        TextMarkup Build();
    };

}