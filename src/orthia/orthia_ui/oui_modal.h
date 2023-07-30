#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"
#include "oui_label.h"

namespace oui
{
    class CBaseModalWindow:public CWindow
    {
    protected:
        bool m_dialogFinished = false;
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
        
    protected:
        std::shared_ptr<CWindow> m_lastModalWindow;
        static String m_chunk;
        int m_lastTabY = 0;
        Range m_captionRange, m_closeRange;

        String m_caption;
        Point m_lastMouseMovePoint;
        std::shared_ptr<PanelColorProfile> m_panelColorProfile;
        std::shared_ptr<DialogColorProfile> m_colorProfile;

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

        std::shared_ptr<DialogColorProfile> GetColorProfile();

        void Dock();

        bool IsPopup() const override { return false; }
        void SetCaption(const String& caption);
        String GetCaption() const;
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
    };


    class CMessageBoxWindow :public oui::SimpleBrush<CModalWindow>
    {
        using Parent_type = oui::SimpleBrush<CModalWindow>;

    protected:
        std::shared_ptr<CLabel> m_fileLabel;
        std::function<void()> m_onDestroy;

        void OnFinishDialog() override;
    public:
        CMessageBoxWindow(std::function<String()> getText, std::function<void()> onDestroy);
        void ConstructChilds() override;
        void OnResize() override;
        bool Resize(const Size& newSize) override;

    };

}