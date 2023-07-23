#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_WARNINGS

#include "ui_workspace_window.h"
#include <ctime>

CWorkspaceWindow::CWorkspaceWindow(std::function<oui::String()> getCaption)
    : 
     oui::SimpleBrush<oui::CPanelWindow>(getCaption)
{
    // FOR WIN LOGIC DEBUG
    //    SetBackgroundColor(oui::ColorBlue());

    m_colorProfile = std::make_shared<oui::DialogColorProfile>();
    QueryDefaultColorProfile(*m_colorProfile);

    oui::IMultiLineViewOwner* param = this;
    m_view = std::make_shared<oui::CMultiLineView>(m_colorProfile, param, true);

}
void CWorkspaceWindow::AddLine(const oui::String& line)
{
    auto timeval = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(timeval);
    tm tm = { 0 };

#ifdef OUI_SYS_WINDOWS    
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::chrono::system_clock::time_point time_without_ms = std::chrono::system_clock::from_time_t(time);
    int milliseconds = (int)std::chrono::duration_cast<std::chrono::milliseconds>(timeval - time_without_ms).count();

    oui::String::char_type buffer[64];
    buffer[0] = 0;
    OUI_SPRINTF(buffer,
        OUI_TCSTR("%02i:%02i:%02i:%03i  "),
        (int)tm.tm_hour,
        (int)tm.tm_min,
        (int)tm.tm_sec,
        (int)milliseconds);
   

    oui::MultiLineViewItem item;
    item.text = oui::String::string_type(buffer) + line.native;
    m_view->AddLine(std::move(item));
}
void CWorkspaceWindow::CancelAllQueries()
{
}
bool CWorkspaceWindow::ScrollUp(oui::MultiLineViewItem* item, int count)
{
    return false;
}
bool CWorkspaceWindow::ScrollDown(oui::MultiLineViewItem* item, int count)
{
    return false;
}
void CWorkspaceWindow::ConstructChilds()
{
    AddChild(m_view);
}
void CWorkspaceWindow::OnResize()
{
    const oui::Rect clientRect = GetClientRect();
    m_view->Resize(clientRect.size);
}
void CWorkspaceWindow::SetFocusImpl()
{
    m_view->SetFocus();
}

// orthia::IUILogInterface
void CWorkspaceWindow::WriteLog(const oui::String& text)
{
    auto console = GetConsole();
    if (!console)
    {
        return;
    }

    std::vector<oui::String::string_type> lines;
    orthia::SplitStringWithoutWhitespace(text.native, L"\x0A", &lines);

    for (auto& line : lines)
    {
        oui::String line2Add = std::move(line);
        console->FilterOrReplaceUnreadableSymbols(line2Add);
        AddLine(line2Add);
    }
}
