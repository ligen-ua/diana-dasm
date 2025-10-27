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
    };
    void QueryDefaultColorProfile(DisasmColorsProfile& profile);

}