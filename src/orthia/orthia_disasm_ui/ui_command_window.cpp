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


        auto itemId = model->GetActiveItemId();

        auto item = model->GetActiveItem();
        if (!item)
        {
            AddLine(oui::String(OUI_TCSTR("No active workspace")));
            return;
        }
       
        if (m_currentOperation)
        {
            return;
        }
        auto operation = std::make_shared<oui::Operation<orthia::CCommandProcessor::ExecuteProgressHandler_type>>(GetThread(), 
            [=](std::shared_ptr<oui::BaseOperation> operation,
                const oui::String& text,
                bool finalText)
                {
                    WriteLog(text);
                    if (finalText)
                    {
                        switch (m_lastCmd)
                        {
                        case orthia::CCommandProcessor::SpecialUICommands::ClearScreen:
                            m_view->Clear();
                            m_view->Invalidate();
                            if (!m_cmdHistory.empty())
                            {
                                WriteCmdHeaderText(itemId, m_cmdHistory.back());
                            }
                            break;
                        }
                        AddLine(oui::String());
                        m_currentOperation = nullptr;
                    }
                }
            );

        auto cmdCallbackOperation = std::make_shared<oui::Operation<orthia::CCommandProcessor::SpecialUICommandHandler_type>>(GetThread(),
            [=](std::shared_ptr<oui::BaseOperation> operation,
                orthia::CCommandProcessor::SpecialUICommands cmd)
        {
            m_lastCmd = cmd;
        }
        );

        WriteCmdHeaderText(itemId, text);
        m_commandEdit->Clear();
        m_currentOperation = operation;
        m_lastCmd = orthia::CCommandProcessor::SpecialUICommands::None;
        PushHistory(text);
        model->GetCommandProcessor()->AsyncExecute(GetThread(), operation, cmdCallbackOperation, text.native, item);
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

void CCommandWindow::WriteCmdHeaderText(int itemId, const oui::String& text)
{
    orthia::PlatformString_type itemStr;
    orthia::ObjectToString_t(itemId, itemStr);

    WriteLog(itemStr + OUI_TCSTR("> ") + text.native);
}
void CCommandWindow::PushHistory(const oui::String& cmd)
{
    m_cmdHistory.push_back(cmd);
    m_cmdHistoryPointer = (int)m_cmdHistory.size();
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
    if (evt.keyEvent.valid)
    {
        bool handled = false;
        switch (evt.keyEvent.virtualKey)
        {
        case oui::VirtualKey::kC:
            if (m_currentOperation && (evt.keyState.state & evt.keyState.AnyCtrl))
            {
                m_currentOperation->Cancel();
            }
            break;
        case oui::VirtualKey::Up:
            --m_cmdHistoryPointer;
            if (m_cmdHistoryPointer < 0)
            {
                m_cmdHistoryPointer = 0;
            }
            if (m_cmdHistoryPointer >= (int)m_cmdHistory.size())
            {
                m_cmdHistoryPointer = (int)m_cmdHistory.size();
                m_commandEdit->SetText(oui::String());
            }
            else
            {
                m_commandEdit->SetText(m_cmdHistory[m_cmdHistoryPointer]);

                oui::InputEvent evt;
                evt.keyEvent.valid = true;
                evt.keyEvent.virtualKey = oui::VirtualKey::End;
                m_commandEdit->ProcessEvent(evt, evtContext);
            }
            handled = true;
            break;

        case oui::VirtualKey::Down:
            if (!m_cmdHistory.empty())
            {
                int oldPtr = m_cmdHistoryPointer;
                ++m_cmdHistoryPointer;
                if (m_cmdHistoryPointer >= (int)m_cmdHistory.size())
                {
                    m_cmdHistoryPointer = (int)m_cmdHistory.size();
                    if (oldPtr != m_cmdHistoryPointer)
                    {
                        m_commandEdit->SetText(oui::String());
                    }
                }
                else
                {
                    m_commandEdit->SetText(m_cmdHistory[m_cmdHistoryPointer]);
                }
            }
            handled = true;
            break;

        case oui::VirtualKey::Escape:
            m_commandEdit->SetFocus();
            handled = true;
            break;
        }

        if (handled)
        {
            Invalidate();
            return true;
        }
        else
        {
            oui::CConsole* console = GetConsole();
            if (console)
            {
                oui::String text = evt.keyEvent.rawText.native;
                console->FilterOrReplaceUnreadableSymbols(text);
                if (!text.native.empty() && !m_commandEdit->IsFocused())
                {
                    m_commandEdit->SetFocus();
                    m_commandEdit->ProcessEvent(evt, evtContext);
                }
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
    m_view->GoToLastLine();
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

    std::vector<orthia::StringInfo> lines;
    orthia::SplitString(text.native, ORTHIA_TCSTR("\x0A"), &lines);

    for (auto& line : lines)
    {
        oui::String line2Add = line.ToString();
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