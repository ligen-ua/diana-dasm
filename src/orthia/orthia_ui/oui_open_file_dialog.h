#pragma once

#include "oui_modal.h"

namespace oui
{
    class COpenFileDialog:public oui::SimpleBrush<CModalWindow>
    {
        using Parent_type = oui::SimpleBrush<CModalWindow>;
    public:
        COpenFileDialog();
    };

}