#include "ui_main_window.h"

CMainWindow::CMainWindow(std::shared_ptr<orthia::CProgramModel> model)
    :
        m_model(model)
{
}
void CMainWindow::SetDefaultTitle()
{
    auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));

    auto console = GetConsole();
    if (!console)
    {
        return;
    }
    console->SetTitle(mainNode->QueryValue(ORTHIA_TCSTR("caption")));
}
void CMainWindow::OnWorkspaceItemChanged(const oui::fsui::OpenResult& result)
{
    orthia::WorkplaceItem item;
    if (!m_model->QueryActiveWorkspaceItem(item))
    {
        SetDefaultTitle();
        m_disasmWindow->SetActiveItem(-1);
        return;
    }
    auto console = GetConsole();
    if (!console)
    {
        return;
    }

    auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
    console->SetTitle(oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("caption-file")), item.name));

    orthia::Address_type addressHint = 0;
    auto it = result.extraInfo.find(orthia::model_OpenResult_extraInfo_InitalAddress);
    if (it != result.extraInfo.end())
    {
        addressHint = std::any_cast<orthia::Address_type>(it->second);
    }
    m_disasmWindow->SetActiveItem(item.uid, addressHint);
}

void CMainWindow::AddInitialArgument(const InitialOpenFileInfo& info)
{
    m_fileToOpen.push_back(info);
}
void CMainWindow::AddInitialTextOutputInfo(const oui::String& text)
{
    m_initialText.push_back(text);
}
void CMainWindow::ConstructChilds()
{
    CMainWindow::ConstuctMenu();

    // construct panels
    m_panelContainerWindow = AddChild_t(std::make_shared<oui::CPanelContainerWindow>());
    auto defaultGroup = m_panelContainerWindow->CreateDefaultGroup();
    auto topGroup = defaultGroup;
    {
        // disasm panel
        auto disasmNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.disasm"));
        m_disasmWindow = std::make_shared<CDisasmWindow>([=]() {  return disasmNode->QueryValue(ORTHIA_TCSTR("caption"));  },
            m_model);
        defaultGroup->AddPanel(m_disasmWindow);
    }

    {
        // output window
        auto bottomPanel = m_panelContainerWindow->AttachNewGroup(defaultGroup, oui::GroupLocation::Bottom, oui::GroupAttachMode::Sibling);

        auto outputNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.output"));
        m_outputWindow = std::make_shared<COutputWindow>([=]() {  return outputNode->QueryValue(ORTHIA_TCSTR("caption"));  });
        // m_outputWindow->SetBackgroundColor(oui::ColorBlue());

        bottomPanel->AddPanel(m_outputWindow);
    }
    {
        // workspace window
        auto workspacePanel = m_panelContainerWindow->AttachNewGroup(topGroup, oui::GroupLocation::Left, oui::GroupAttachMode::Child);
        oui::Size size;
        size.width = 30;
        workspacePanel->SetPreferredSize(size);
        auto workspaceNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.workspace"));
        m_workspaceWindow = std::make_shared<CWorkspaceWindow>([=]() {  return workspaceNode->QueryValue(ORTHIA_TCSTR("caption"));  });
        workspacePanel->AddPanel(m_workspaceWindow);

       // m_workspaceWindow->SetBackgroundColor(oui::ColorBlue());
    }
    m_model->SetUILog(m_outputWindow);

    // we need to set focus somewhere
    SetOnResize([&]() {
        
        m_menu->Dock();
        const auto menuSize = m_menu->GetSize();

        oui::Rect clientRect = GetClientRect();
        
        oui::Size panelSize = clientRect.size;
        panelSize.height -= menuSize.height;
        
        m_panelContainerWindow->MoveTo({0, menuSize.height });
        m_panelContainerWindow->Resize(panelSize);
    });
}

void CMainWindow::OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool)
{
    SetDefaultTitle();
    m_disasmWindow->Activate();

    for (const auto& line: m_initialText)
    {
        m_outputWindow->AddLine(line);
    }
    auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
    for (const auto& info : m_fileToOpen)
    {
        if (info.errorCode)
        {
            m_outputWindow->AddLine(oui::PassParameter2(mainNode->QueryValue(ORTHIA_TCSTR("file-error-name-code")),
                info.name,
                oui::GetErrorText(info.errorCode)));
            continue;
        }
        if (!AsyncOpenFile(info.file))
        {
            m_outputWindow->AddLine(oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("file-error-name")), info.file->GetFullFileNameForUI()));
        }
    }
}
bool CMainWindow::ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext)
{
    auto pool = GetPool();
    if (!pool)
    {
        return false;
    }
    if (!pool->GetFocus())
    {
        if (auto modalWindow = pool->GetModalWindow())
        {
            if (!evt.resizeEvent.valid)
            {
                return modalWindow->ProcessEvent(evt, evtContext);
            }
        }
    }
    if (oui::Fullscreen<oui::CWindow>::ProcessEvent(evt, evtContext))
    {
        return true;
    }
    if (evt.keyEvent.valid)
    {       
        // check focused
        if (auto ptr = pool->GetFocus())
        {
            for (; ptr;)
            {
                if (ptr.get() != this)
                {
                    if (ptr->ProcessEvent(evt, evtContext))
                    {
                        return true;
                    }
                }
                ptr = ptr->GetParent();
            }
        }

        // check hotkeys
        if (m_hotkeys.ProcessEvent(evt))
        {
            return true;
        }

        // no focused or focused couldn't process this, check alt menu
        if (evt.keyEvent.virtualKey == oui::VirtualKey::None &&
            (evt.keyState.state & evt.keyState.AnyAlt) &&
            !(evt.keyState.state & evt.keyState.AnyCtrl) &&
            !(evt.keyState.state & evt.keyState.AnyShift))
        {
            ToggleMenu(false);
        }
    }
    return true;
}
