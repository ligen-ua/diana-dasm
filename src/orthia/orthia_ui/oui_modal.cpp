#include "oui_modal.h"

namespace oui
{

    CModalWindow::CModalWindow()
    {
    }

    void CModalWindow::OnInit(std::shared_ptr<CWindowsPool> pool)
    {
        m_prevFocus = pool->GetFocus();
        Activate();
    }
    void CModalWindow::FinishDialog()
    {
        Deactivate();
        auto pool = GetPool();
        if (pool)
        {
            pool->SetFocus(m_prevFocus.lock());
            m_prevFocus.reset();
        }
        Destroy();
    }
    bool CModalWindow::ProcessEvent(oui::InputEvent& evt)
    {
        if (!CWindow::ProcessEvent(evt))
        {
            if (evt.keyEvent.valid)
            {
                switch (evt.keyEvent.virtualKey)
                {
                case oui::VirtualKey::Escape:
                    FinishDialog();
                    return true;
                }
            }
        }
        return false;
    }

}