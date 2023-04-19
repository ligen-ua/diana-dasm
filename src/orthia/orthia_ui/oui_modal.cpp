#include "oui_modal.h"

namespace oui
{

    CBaseModalWindow::CBaseModalWindow()
    {
    }

    void CBaseModalWindow::OnInit(std::shared_ptr<CWindowsPool> pool)
    {
        m_prevFocus = pool->GetFocus();
        Activate();
    }
    void CBaseModalWindow::FinishDialog()
    {
        OnFinishDialog();
        Deactivate();
        auto pool = GetPool();
        if (pool)
        {
            pool->SetFocus(m_prevFocus.lock());
            m_prevFocus.reset();
        }
        Destroy();
    }
    bool CBaseModalWindow::ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext)
    {
        if (!CWindow::ProcessEvent(evt, evtContext))
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
        // non-popup modal dialogs showdn't allow global hotkeys to flee
        return !IsPopup();
    }


    // CModalWindow
    void CModalWindow::OnInit(std::shared_ptr<CWindowsPool> pool)
    {
        m_lastModalWindow = pool->GetModalWindow();
        pool->SetModalWindow(GetPtr());
        Parent_type::OnInit(pool);
    }
    void CModalWindow::OnFinishDialog() 
    {
        auto pool = GetPool();
        if (!pool)
        {
            return;
        }
        pool->SetModalWindow(m_lastModalWindow);
        m_lastModalWindow = nullptr;
    }
    void CModalWindow::Dock()
    {
        auto parent = GetParent();
        if (!parent)
        {
            return;
        }
        const auto parentRect = parent->GetClientRect();
        int xBorder = parentRect.size.width / 6;
        int yBorder = parentRect.size.height / 6;
        Rect rect = parentRect;
        rect.position.x += xBorder;
        rect.position.y += yBorder;
        rect.size.width -= xBorder*2;
        rect.size.height -= yBorder*2;
        this->MoveTo(rect.position);
        this->Resize(rect.size);
    }


}