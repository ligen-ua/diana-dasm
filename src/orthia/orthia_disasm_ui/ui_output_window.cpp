#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_WARNINGS

#include "ui_output_window.h"
#include "orthia_log.h"
#include "oui_menu.h"
#include "orthia_model_interfaces.h"

class OutputLogProxy :public orthia::RefCountedBase_t<orthia::ILowLevelLog>
{
    std::weak_ptr<COutputWindow> m_outputWindow;
public:
    OutputLogProxy(std::shared_ptr<COutputWindow> outputWindow)
        :
            m_outputWindow(outputWindow)
    {

    }
    ~OutputLogProxy()
    {

    }
    void LogData(const ORTHIA_TCHAR* pData, int size)
    {
        if (auto out = m_outputWindow.lock())
        {
            out->AddLine(orthia::PlatformString_type(pData, size));
        }
    }
    orthia::log_meta::LogEncoding_type QueryEncoding() const
    {
        return orthia::log_meta::leUtf16LE;
    }
};

void RegisterAsLog(std::shared_ptr<COutputWindow> outputWindow)
{
    orthia::intrusive_ptr<orthia::ILowLevelLog> lowLevelLog(new OutputLogProxy(outputWindow));
    orthia::DefLog_Init(new orthia::CProgramLog(lowLevelLog, false));
}

COutputWindow::COutputWindow(std::function<oui::String()> getCaption)
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
void COutputWindow::AddLine(const oui::String& line_in)
{
    oui::String line = line_in;
    orthia::TrimRightIf(line.native, orthia::IsWhiteSpace);
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
    if (m_view->IsCursorOutOfText())
    {
        m_view->GoToLastLine();
    }
}

void COutputWindow::ConstructChilds()
{
    AddChild(m_view);
}
void COutputWindow::OnResize()
{
    const oui::Rect clientRect = GetClientRect();
    m_view->Resize(clientRect.size);
}
void COutputWindow::SetFocusImpl()
{
    m_view->SetFocus();
}
void COutputWindow::OnContextMenu(const oui::Point& point)
{
    if (!m_view->SelectionIsActive())
        return;

    auto contextMenuNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.output.contextmenu"));
    std::vector<oui::PopupItem> items;
    items.push_back({ contextMenuNode->QueryValue(ORTHIA_TCSTR("copy")),
        [this]() { m_view->CopySelected(); } });

    oui::Point pointToUse{ point.x + 1, point.y + 1 };
    auto parent = GetPool()->GetRootWindow();
    auto popup = parent->AddChild_t(std::make_shared<oui::CMenuPopup>(std::move(items)));
    popup->Init(parent->GetPtr());
    popup->Dock(pointToUse);
    popup->SetFocus();
}

std::shared_ptr<oui::CMultiLineView> COutputWindow::SF_GetView()
{
    return m_view;
}
oui::CConsole* COutputWindow::SF_GetConsole()
{
    return GetConsole();
}


// orthia::IUILogInterface
void COutputWindow::WriteLog(const oui::String& text)
{
    auto console = GetConsole();
    if (!console)
    {
        return;
    }

    std::vector<oui::String::string_type> lines;
    orthia::SplitStringWithoutWhitespace(text.native, ORTHIA_TCSTR("\x0A"), &lines);

    for (auto& line : lines)
    {
        oui::String line2Add = std::move(line);
        console->FilterOrReplaceUnreadableSymbols(line2Add);
        AddLine(line2Add);
    }
}
