#include "oui_text_markup.h"

namespace oui
{

    void CTextMarkupBuilder::AddNextRange(std::uint32_t id, std::uint32_t sizeInBytes, const LabelColorProfile& colorProfile)
    {
        TextMarkup::Range range;
        range.id = id;
        range.sizeInBytes = sizeInBytes;
        range.colorProfile = colorProfile;
        m_markup.ranges.push_back(range);
    }

    TextMarkup CTextMarkupBuilder::Build()
    {
        return m_markup;
    }
};
