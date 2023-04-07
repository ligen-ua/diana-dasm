#pragma once

#include "oui_containers.h"

class CDisasmWindow:public oui::CPanelWindow
{
public:
    CDisasmWindow(std::function<oui::String()> getCaption);
};