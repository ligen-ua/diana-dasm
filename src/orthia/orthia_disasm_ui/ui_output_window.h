#pragma once

#include "oui_containers.h"

class COutputWindow:public oui::SimpleBrush<oui::CPanelWindow>
{
public:
    COutputWindow(std::function<oui::String()> getCaption);
};