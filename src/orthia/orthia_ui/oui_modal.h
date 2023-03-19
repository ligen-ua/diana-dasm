#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{
    class CModalWindow:public WithBorder<CWindow>
    {
        std::weak_ptr<CWindow> m_prevFocus;

    protected:
        void OnInit(std::shared_ptr<CWindowsPool> pool);

    public:
        CModalWindow(const Point& position, const Size& size);

        bool ProcessEvent(oui::InputEvent& evt) override;
        
        void ConstuctChilds() override;
        void FinishDialog();
    };
}