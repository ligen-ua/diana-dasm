#pragma once

#include "oui_base.h"
#include "oui_color.h"
#include "oui_string.h"

namespace oui
{
    struct PanelBorderSymbols
    {
        oui::String::char_type vertical;
        oui::String::char_type horizontal;
        oui::String::char_type left_top;
        oui::String::char_type right_top;
        oui::String::char_type left_bottom;
        oui::String::char_type right_bottom;
    };

    PanelBorderSymbols GetPanelBorderSymbols();
}

#include "oui_console_win32.h"
#include "oui_console_posix.h"
