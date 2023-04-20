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
        using Parent_type = WithBorder<CBaseModalWindow>;
        
        std::shared_ptr<CWindow> m_lastModalWindow;
        static String m_chunk;
        int m_lastTabY = 0;
        Range m_captionRange, m_closeRange;

        String m_caption;
        Point m_lastMouseMovePoint;
        std::shared_ptr<PanelColorProfile> m_panelColorProfile;

        void OnInit(std::shared_ptr<CWindowsPool> pool) override;
        void OnFinishDialog() override;
        void PaintTitle(const Rect& rect, DrawParameters& parameters);
        bool Drag_MoveHandler(DragEvent event,
            const Point& initialPoint,
            const Point& currentPoint,
            std::shared_ptr<CWindow> wnd,
            const Rect& initialRect);
    public:
        CModalWindow();

        void Dock(); 
        bool IsPopup() const override { return false; }
        void SetCaption(const String& caption);
        String GetCaption() const;
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
    };
}