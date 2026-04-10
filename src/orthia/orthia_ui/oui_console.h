#pragma once

#include "oui_base.h"
#include "oui_color.h"
#include "oui_string.h"

namespace oui
{
    struct PanelBorderSymbols
    {
        oui::String vertical;
        oui::String horizontal;
        oui::String left_top;
        oui::String right_top;
        oui::String left_bottom;
        oui::String right_bottom;
    };

    PanelBorderSymbols GetPanelBorderSymbols();
}

#include "oui_console_win32.h"
#include "oui_console_posix.h"
