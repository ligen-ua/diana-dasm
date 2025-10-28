#pragma once

#include "oui_color.h"

namespace oui
{

    struct TextMarkup
    {
        static const std::uint16_t flag_ManualHighlight = 1;
        struct Range
        {
            std::uint32_t id = 0;
            std::uint16_t sizeInTChars = 0;
            std::uint16_t flags = 0;
            LabelColorProfile colorProfile;
        };
        std::vector<Range> ranges;
    };

    const std::uint32_t g_id_user_range = 0x1000;

    class CTextMarkupBuilder
    {
        TextMarkup m_markup;
        std::uint32_t m_id = 0;
    public:
        void AddNextRange(size_t sizeInTChars, const LabelColorProfile& colorProfile, std::uint32_t id = 0, std::uint16_t flags = 0);
        TextMarkup Build();
    };

}