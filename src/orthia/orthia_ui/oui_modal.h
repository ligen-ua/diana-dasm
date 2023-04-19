#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{
    class CBaseModalWindow:public CWindow
    {
    protected:
        std::weak_ptr<CWindow> m_prevFocus;
        void OnInit(std::shared_ptr<CWindowsPool> pool);
        virtual void OnFinishDialog() {}
    public:
        CBaseModalWindow();

        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
        void FinishDialog();
    };


    class CModalWindow:public WithBorder<CBaseModalWindow>
    {
        std::shared_ptr<CWindow> m_lastModalWindow;

        using Parent_type = WithBorder<CBaseModalWindow>;
        void OnInit(std::shared_ptr<CWindowsPool> pool) override;
        void OnFinishDialog() override;
    public:
        void Dock(); 
        bool IsPopup() const override { return false; }
    };
}