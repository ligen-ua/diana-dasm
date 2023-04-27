#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{
    class CEditBox:public SimpleBrush<MouseFocusable<CWindow>>
    {
        using Parent_type = SimpleBrush<MouseFocusable<CWindow>>;

        std::shared_ptr<DialogColorProfile> m_colorProfile;
        String m_text;
        Rect m_lastRect;
        std::vector<SymbolInfo> m_symbols;
        int m_cursorIterator = 0;

        int GetCursorPosition() const;

    public:
        CEditBox(std::shared_ptr<DialogColorProfile> colorProfile);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
        void Clear();
        String GetText() const;
        void SetText(const String& text);
        void ScrollRight();
        void OnFocusLost() override;
        void OnFocusEnter() override;
    };

}