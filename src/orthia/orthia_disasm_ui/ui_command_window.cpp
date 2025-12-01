#include "ui_command_window.h"

CCommandWindow::CCommandWindow(std::function<oui::String()> getCaption,
    std::shared_ptr<orthia::CProgramModel> model,
    std::shared_ptr<oui::IPanelChildSwitcher> parentTabSwitcher)
    : 
        Parent_type(getCaption)
{
    // FOR WIN LOGIC DEBUG
    //    SetBackgroundColor(oui::ColorBlue());

    m_colorProfile = std::make_shared<oui::DialogColorProfile>();
    QueryDefaultColorProfile(*m_colorProfile);

    // view
    oui::IMultiLineViewOwner* param = this;
    m_view = std::make_shared<oui::CMultiLineView>(m_colorProfile, param, true);

    // edit
    m_commandEdit = std::make_shared<oui::CEditBox>(m_colorProfile);
    m_commandEdit->SetEnterHandler([=](const oui::String& text) { 
    });

    // label
    auto labelProfile = std::shared_ptr<oui::LabelColorProfile>(m_colorProfile, &m_colorProfile->label);
    labelProfile->normal.text = oui::ColorBrightYellow();
    labelProfile->mouseHighlight.text = oui::ColorBrightYellow();
    m_textLabel = std::make_shared<oui::CLabel>(labelProfile, []() { return OUI_TCSTR(">"); });

    RegisterSwitch(m_commandEdit);
    RegisterSwitch(m_view);
    RegisterSwitchParent(parentTabSwitcher);
}

bool CCommandWindow::ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext)
{
    if (evt.keyState.state & evt.keyState.AnyCtrl)
    {
        if (evt.keyEvent.valid && evt.keyEvent.virtualKey == oui::VirtualKey::Tab)
        {
            if (auto parent = ChildSwitcher_GetParent())
            {
                return parent->SwitchNextPanel();
            }
        }
    }
    return Parent_type::ProcessEvent(evt, evtContext);
}

void CCommandWindow::AddLine(const oui::String& line_in)
{
    oui::String line = line_in;
    orthia::TrimRightIf(line.native, orthia::IsWhiteSpace);

    oui::MultiLineViewItem item;
    item.text = line.native;
    m_view->AddLine(std::move(item));
}

void CCommandWindow::ConstructChilds()
{
    AddChild(m_commandEdit);
    AddChild(m_view);
    AddChild(m_textLabel);
}

void CCommandWindow::OnResize()
{
    const oui::Rect clientRect = GetClientRect();
    
    // view
    oui::Rect viewRect = clientRect;
    --viewRect.size.height;
    m_view->Resize(viewRect.size);

    // edit
    oui::Rect editRect = clientRect;
    editRect.position.x += 2;
    editRect.position.y += viewRect.size.height;
    editRect.size.width -= 2;
    editRect.size.height = 1;

    m_commandEdit->MoveTo(editRect.position);
    m_commandEdit->Resize(editRect.size);

    // label
    oui::Rect labelRect = clientRect;
    labelRect.position.y += viewRect.size.height;
    labelRect.size.width = 2;
    labelRect.size.height = 1;

    m_textLabel->MoveTo(labelRect.position);
    m_textLabel->Resize(labelRect.size);
}
void CCommandWindow::OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool)
{
    m_commandEdit->SetFocus();
}
void CCommandWindow::SetFocusImpl()
{
    m_commandEdit->SetFocus();
}

std::shared_ptr<oui::CMultiLineView> CCommandWindow::SF_GetView()
{
    return m_view;
}
oui::CConsole* CCommandWindow::SF_GetConsole()
{
    return GetConsole();
}


// orthia::IUILogInterface
void CCommandWindow::WriteLog(const oui::String& text)
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
void CCommandWindow::OnWorkspaceItemChanged()
{
}
void CCommandWindow::SetActiveWorkspaceItem(int itemId)
{
    OnWorkspaceItemChanged();
}

void CCommandWindow::ReloadState(const UIState& state)
{
    Invalidate();
}

void CCommandWindow::SaveState(UIState& state)
{
}