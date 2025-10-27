#include "oui_text_markup.h"

namespace oui
{

    void CTextMarkupBuilder::AddNextRange(size_t sizeInTChars, const LabelColorProfile& colorProfile, std::uint32_t id)
    {
        TextMarkup::Range range;
        range.id = id;
        range.sizeInTChars = (std::uint32_t)sizeInTChars;
        range.colorProfile = colorProfile;
        if (!range.id)
        {
            range.id = ++m_id;
        }
        m_markup.ranges.push_back(range);
    }

    TextMarkup CTextMarkupBuilder::Build()
    {
        TextMarkup result = m_markup;
        m_markup.ranges.clear();
        m_id = 0;
        return result;
    }
};
