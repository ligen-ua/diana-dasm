#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{
    class CModalWindow:public CWindow
    {
        std::weak_ptr<CWindow> m_prevFocus;

    protected:
        void OnInit(std::shared_ptr<CWindowsPool> pool);

    public:
        CModalWindow();

        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
        void FinishDialog();
    };
}