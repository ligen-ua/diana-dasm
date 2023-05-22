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
void CMainWindow::OnWorkspaceItemChanged()
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
    m_disasmWindow->SetActiveItem(item.uid);
}
void CMainWindow::ConstructChilds()
{
    CMainWindow::ConstuctMenu();

    // construct panels
    m_panelContainerWindow = AddChild_t(std::make_shared<oui::CPanelContainerWindow>());
    auto defaultGroup = m_panelContainerWindow->CreateDefaultGroup();
    {
        auto disasmNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.disasm"));
        m_disasmWindow = std::make_shared<CDisasmWindow>([=]() {  return disasmNode->QueryValue(ORTHIA_TCSTR("caption"));  },
            m_model);
        defaultGroup->AddPanel(m_disasmWindow);
    }

    {
        auto bottomPanel = m_panelContainerWindow->AttachNewGroup(defaultGroup, oui::GroupLocation::Bottom, oui::GroupAttachMode::Sibling);

        auto outputNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.output"));
        m_outputWindow = std::make_shared<COutputWindow>([=]() {  return outputNode->QueryValue(ORTHIA_TCSTR("caption"));  });
        // m_outputWindow->SetBackgroundColor(oui::ColorBlue());

        bottomPanel->AddPanel(m_outputWindow);
    }
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
