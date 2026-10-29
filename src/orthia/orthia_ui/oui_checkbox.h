#pragma once

#include "oui_label.h"
#include "oui_win_styles.h"

namespace oui
{
    // "[x] text" / "[ ] text" in one row. Space or a click toggles it; Enter goes to the
    // parent, so in a dialog it confirms the dialog
    class CCheckBox:public MouseFocusable<CWindow>
    {
        using Parent_type = MouseFocusable<CWindow>;

        std::shared_ptr<ButtonColorProfile> m_colorProfile;
        std::function<String()> m_getText;
        std::function<void(bool)> m_onChange;
        bool m_checked = false;
        static String m_chunk, m_chunkText;

    public:
        CCheckBox(std::shared_ptr<ButtonColorProfile> colorProfile, std::function<String()> getText, bool checked = false);
        ~CCheckBox();

        bool IsChecked() const { return m_checked; }
        // no change handler call
        void SetChecked(bool checked);
        // as the user does it: the change handler is called
        void Toggle();
        void SetChangeHandler(std::function<void(bool)> handler);

        String GetText() const;
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext) override;
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override;

        // Space without Ctrl or Alt: Space has no VirtualKey, it comes as text
        static bool IsToggleKey(const InputEvent& evt);
    };
}
