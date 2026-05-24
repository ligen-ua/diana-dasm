#pragma once

#include "oui_label.h"
#include "oui_win_styles.h"

namespace oui
{

    class CButton:public MouseFocusable<CWindow>
    {
        using Parent_type = MouseFocusable<CWindow>;

        std::shared_ptr<ButtonColorProfile> m_colorProfile;
        std::function<String()> m_getText;
        std::function<void()> m_onClick;
        static String m_chunk, m_chunkText;

    protected:

    public:
        CButton(std::shared_ptr<ButtonColorProfile> colorProfile, std::function<String()> getText);
        ~CButton();

        void SetClickHandler(std::function<void()> handler);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        String GetText() const;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext) override;
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override;
    };

}
