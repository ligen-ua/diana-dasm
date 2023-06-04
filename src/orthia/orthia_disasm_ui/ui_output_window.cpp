#include "ui_output_window.h"

COutputWindow::COutputWindow(std::function<oui::String()> getCaption)
    : 
     oui::SimpleBrush<oui::CPanelWindow>(getCaption)
{
    // FOR WIN LOGIC DEBUG
    //    SetBackgroundColor(oui::ColorBlue());
}
void COutputWindow::AddLine(const oui::String& line)
{
    m_lines.push_back(line);
}
