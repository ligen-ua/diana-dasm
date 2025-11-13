#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{

    class CScrollable : public oui::SimpleBrush<CWindow>
    {
        using Parent_type = oui::SimpleBrush<CWindow>;
        std::shared_ptr<CWindow> m_pTarget;
        int m_xPosPercentage = 0;
        Point m_leftScroll, m_rightScroll;
        bool m_highlightLeftScroll = false, m_highlightRightScroll = false;

        void ConstructChilds() override;
        void DrawChilds(const Rect& rect, DrawParameters& parameters, bool& force) override;

        void ScrollHorizontally(int step);

    public:
        CScrollable(std::shared_ptr<CWindow> pTarget);
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override;
        bool ProcessMouseEvent(const Rect& rect, InputEvent& evt, WindowEventContext& evtContext) override;

        void SetFocus() override;
        bool IsFocused() const override;
        void OnFocusLost() override;
        void OnFocusEnter() override;
        std::shared_ptr<CWindow> GetPopupPrevFocusTarget() override;

        void Activate() override;
        void Deactivate() override;
        bool IsActive() const override;
        bool IsActiveOrFocused() const override;
        
        void OnResize() override;
        void DrawTo(const Rect& rect, DrawParameters& parameters, bool& force) override;

    };

    class CVerticalScrollBar : public WithBorder<CWindow>
    {
        Point m_lastMouseMovePoint;
        std::function<void(const Point& point)> m_onStartDrag;
    public:
        void SetDragHandler(std::function<void (const Point &point)> onStartDrag);
        bool Resize(const Size& newSize) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext) override;

    };
}
