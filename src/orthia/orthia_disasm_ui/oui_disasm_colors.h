#pragma once

#include "oui_color.h"

namespace oui
{

    struct DisasmColorsProfile
    {
        LabelColorProfile address;
        LabelColorProfile bytes;
        LabelColorProfile command;
        LabelColorProfile spaces;
        LabelColorProfile operand;
        LabelColorProfile generalMeta;
    };
    void QueryDefaultColorProfile(DisasmColorsProfile& profile);

}