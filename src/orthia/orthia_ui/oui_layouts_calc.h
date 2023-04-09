#pragma once

#include "oui_layouts.h"


namespace oui
{
    void RepositionLayout(std::shared_ptr<PanelLayout> layout, const Rect& wndRect, bool clearCache, bool moveGroups);

    void ResizeLayoutY(std::shared_ptr<PanelLayout> layout, int newHeight, bool top);
}