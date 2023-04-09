#include "ui_main_window.h"

void CMainWindow::ConstuctChilds()
{
    CMainWindow::ConstuctMenu();

    // construct panels
    m_panelContainerWindow = AddChild_t(std::make_shared<oui::CPanelContainerWindow>());
    auto defaultGroup = m_panelContainerWindow->CreateDefaultGroup();
    {
        auto disasmNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.disasm"));
        m_disasmWindow = std::make_shared<CDisasmWindow>([=]() {  return disasmNode->QueryValue(ORTHIA_TCSTR("caption"));  });
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
    m_disasmWindow->Activate();
}
bool CMainWindow::ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext)
{
    if (oui::Fullscreen<oui::CWindow>::ProcessEvent(evt, evtContext))
    {
        return true;
    }
    if (evt.keyEvent.valid)
    {
        // check hotkeys
        if (m_hotkeys.ProcessEvent(evt))
        {
            return true;
        }
        
        // check focused
        auto pool = GetPool();
        if (!pool)
        {
            return false;
        }
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
