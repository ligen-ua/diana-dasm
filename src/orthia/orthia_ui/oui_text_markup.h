#pragma once

#include "oui_color.h"

namespace oui
{

    struct TextMarkup
    {
        struct Range
        {
            std::uint32_t id = 0;
            std::uint32_t sizeInTChars= 0;
            LabelColorProfile colorProfile;
        };
        std::vector<Range> ranges;
    };

    class CTextMarkupBuilder
    {
        TextMarkup m_markup;
        std::uint32_t m_id = 0;
    public:
        void AddNextRange(size_t sizeInTChars, const LabelColorProfile& colorProfile, std::uint32_t id = 0);
        TextMarkup Build();
    };

}