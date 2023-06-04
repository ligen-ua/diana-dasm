#pragma once

#include "oui_containers.h"

class COutputWindow:public oui::SimpleBrush<oui::CPanelWindow>
{
    std::vector<oui::String> m_lines;
public:
    COutputWindow(std::function<oui::String()> getCaption);
    void AddLine(const oui::String& line);
};