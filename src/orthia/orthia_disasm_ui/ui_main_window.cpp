#include "ui_main_window.h"

void CMainWindow::ConstuctChilds()
{
    CMainWindow::ConstuctMenu();

    SetOnResize([&]() {

        m_menu->Dock();
    });
}

bool CMainWindow::ProcessEvent(oui::InputEvent& evt)
{
    if (oui::Fullscreen<oui::CWindow>::ProcessEvent(evt))
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
        if (auto focused = pool->GetFocus())
        {
            if (focused.get() != this)
            {
                if (focused->ProcessEvent(evt))
                {
                    return true;
                }
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
