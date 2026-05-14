#include "ui_main_window.h"

CMainWindow::CMainWindow(std::shared_ptr<orthia::CProgramModel> model)
    :
        m_model(model)
{
}
void CMainWindow::SetFocusImpl()
{
    if (m_disasmWindow)
    {
        m_disasmWindow->SetFocus();
        return;
    }
    oui::SimpleBrush<oui::Fullscreen<oui::CWindow>>::SetFocusImpl();
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
void CMainWindow::OnPreWorkspaceItemChange(int itemId)
{
    m_stateManager.SaveState(itemId);
}
void CMainWindow::OnWorkspaceItemRemoved(int itemId)
{
    m_stateManager.RemoveItem(itemId);
}
void CMainWindow::CloseCurrentItem()
{
    auto activeId = m_model->GetActiveItemId();
    if (!activeId)
        return;
    m_model->RemoveItem(activeId);
}
void CMainWindow::SwitchToSectionsPanel()
{
    if (m_defaultGroup && m_sectionsWindow)
        m_defaultGroup->SwitchPanel(m_sectionsWindow);
}
void CMainWindow::OnWorkspaceItemChanged(int itemId)
{
    if (m_stateManager.ReloadState(itemId))
    {
        if (m_workspaceInitializer)
        {
            m_workspaceInitializer();
            m_workspaceInitializer = 0;
            m_stateManager.ReloadState(itemId);
        }
    }
    m_stateManager.SetActiveItem(itemId);

    orthia::WorkplaceItem item;
    if (!m_model->QueryActiveWorkspaceItem(item))
    {
        SetDefaultTitle();
    }
    else
    {
        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
        console->SetTitle(oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("caption-file")), item.name));
    }
}
void CMainWindow::OnWorkspaceDataRefreshed(int itemId)
{
    if (m_disasmWindow)
        m_disasmWindow->ReloadVisibleItems();
    if (m_modulesWindow)
        m_modulesWindow->OnWorkspaceItemChanged();
}
void CMainWindow::OnWorkspaceItemChanged(const oui::fsui::OpenResult& result)
{

    auto wit = result.extraInfo.find(orthia::model_OpenResult_extraInfo_WorkspaceId);

    orthia::WorkplaceItem item;
    auto console = GetConsole();
    if (console == nullptr || wit == result.extraInfo.end() || !m_model->QueryWorkspaceItem(std::any_cast<int>(wit->second), item))
    {
        return;
    }

    m_workspaceInitializer = [=]() {
        // load initial data
        orthia::Address_type addressHint = 0;
        auto it = result.extraInfo.find(orthia::model_OpenResult_extraInfo_InitalAddress);
        if (it != result.extraInfo.end())
        {
            addressHint = std::any_cast<orthia::Address_type>(it->second);
        }
        m_stateManager.SaveState(item.uid);
        auto state = m_stateManager.GetUIState(item.uid, m_disasmWindow);
        if (state)
        {
            m_disasmWindow->PrepareParameters(*state, item.uid, addressHint);
        }
    };
    m_model->SetActiveItem(item.uid);
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
    m_defaultGroup = m_panelContainerWindow->CreateDefaultGroup();
    auto defaultGroup = m_defaultGroup;
    auto topGroup = defaultGroup;
    {
        // disasm panel
        auto disasmNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.disasm"));
        m_disasmWindow = std::make_shared<CDisasmWindow>([=]() {  return disasmNode->QueryValue(ORTHIA_TCSTR("caption"));  },
            m_model);
        defaultGroup->AddPanel(m_disasmWindow);

        m_stateManager.Register(m_disasmWindow);

        m_hotkeys.Register(oui::Hotkey(oui::KeyState(oui::KeyState::AnyAlt),
            oui::VirtualKey::kD), [=]() { 
                defaultGroup->SwitchPanel(m_disasmWindow);
            });
    }
    {
        // modules window
        auto workspaceNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.modules"));
        m_modulesWindow = std::make_shared<CModulesWindow>([=]() {  return workspaceNode->QueryValue(ORTHIA_TCSTR("caption"));  },
            m_model,
            defaultGroup,
            [this, defaultGroup](auto address) {
                defaultGroup->SwitchPanel(m_disasmWindow);
                m_disasmWindow->DoGotoRequest(address);
            },
            [this](auto address, const auto& name) {
                SwitchToSectionsPanel();
                if (m_sectionsWindow)
                    m_sectionsWindow->NavigateTo(address, name);
            });
        defaultGroup->AddPanel(m_modulesWindow);
        m_stateManager.Register(m_modulesWindow);

        m_hotkeys.Register(oui::Hotkey(oui::KeyState(oui::KeyState::AnyAlt),
            oui::VirtualKey::kM), [=]() { 
                defaultGroup->SwitchPanel(m_modulesWindow);
            });
    }
    {
        // commands window
        auto workspaceNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.commands"));
        m_commandWindow = std::make_shared<CCommandWindow>([=]() {  return workspaceNode->QueryValue(ORTHIA_TCSTR("caption"));  },
            m_model,
            defaultGroup);
        defaultGroup->AddPanel(m_commandWindow);
        m_stateManager.Register(m_commandWindow);

        m_hotkeys.Register(oui::Hotkey(oui::KeyState(oui::KeyState::AnyAlt),
            oui::VirtualKey::kC), [=]() { 
                defaultGroup->SwitchPanel(m_commandWindow);
            });
    }
    {
        // sections window
        auto sectionsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.sections"));
        m_sectionsWindow = std::make_shared<CSectionsWindow>(
            [=]() { return sectionsNode->QueryValue(ORTHIA_TCSTR("caption")); },
            m_model,
            defaultGroup);
        defaultGroup->AddPanel(m_sectionsWindow);
        m_stateManager.Register(m_sectionsWindow);

        m_hotkeys.Register(oui::Hotkey(oui::KeyState(oui::KeyState::AnyAlt),
            oui::VirtualKey::kS), [=]() {
                defaultGroup->SwitchPanel(m_sectionsWindow);
            });
    }
    {
        // output window
        auto bottomPanel = m_panelContainerWindow->AttachNewGroup(defaultGroup, oui::GroupLocation::Bottom, oui::GroupAttachMode::Sibling);

        auto outputNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.output"));
        m_outputWindow = std::make_shared<COutputWindow>([=]() {  return outputNode->QueryValue(ORTHIA_TCSTR("caption"));  });
        // m_outputWindow->SetBackgroundColor(oui::ColorBlue());

        bottomPanel->AddPanel(m_outputWindow);

        m_hotkeys.Register(oui::Hotkey(oui::KeyState(oui::KeyState::AnyAlt),
            oui::VirtualKey::kO), [=]() { 
                bottomPanel->SwitchPanel(m_outputWindow);
            });
    }
    {
        // workspace window
        auto workspacePanel = m_panelContainerWindow->AttachNewGroup(topGroup, oui::GroupLocation::Left, oui::GroupAttachMode::Child);
        oui::Size size;
        size.width = 30;
        workspacePanel->SetPreferredSize(size);
        auto workspaceNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.workspace"));
        m_workspaceWindow = std::make_shared<CWorkspaceWindow>([=]() {  return workspaceNode->QueryValue(ORTHIA_TCSTR("caption"));  },
            m_model);
        workspacePanel->AddPanel(m_workspaceWindow);
        workspacePanel->SetVisible(false);
        m_stateManager.Register(m_workspaceWindow);

        m_hotkeys.Register(oui::Hotkey(oui::KeyState(oui::KeyState::AnyAlt),
            oui::VirtualKey::kW), [=]() { 
                if (workspacePanel->IsVisible())
                {
                    workspacePanel->SwitchPanel(m_workspaceWindow);
                }
            });
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

    RegisterAsLog(m_outputWindow);
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
        if (info.process)
        {
            if (!AsyncOpenProcess(info.process))
            {
                m_outputWindow->AddLine(oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("file-error-name")), info.process->GetFullFileNameForUI()));
            }
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
            evt.keyState.HasJustAlt())
        {
            ToggleMenu(false);
        }

        if (evt.keyEvent.virtualKey == oui::VirtualKey::Escape)
        {
            if (m_menu->IsActive())
            {
                ToggleMenu(false);
            }
        }

        // check linux style terminal switch
#ifndef DIANA_HAS_POSIX
        if (evt.keyState.HasJustAlt())
        {
            if (evt.keyEvent.virtualKey == oui::VirtualKey::k0)
            {
                m_outputWindow->SetFocus();
            }
            else
            if (evt.keyEvent.virtualKey >= oui::VirtualKey::k1 && evt.keyEvent.virtualKey <= oui::VirtualKey::k9)
            {
                auto tabId = (int)evt.keyEvent.virtualKey - (int)oui::VirtualKey::k1;
                auto defGroup = m_panelContainerWindow->GetDefaultGroup();
                defGroup->SwitchPanel(tabId);
            }
        }
#endif
    }
    return true;
}
