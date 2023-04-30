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
        int m_windowRightIterator = 0;

        static String m_chunk;
        void InsertText(const String& text);


        int m_windowSymStart = 0;
        int m_windowSymSize = 0;

        int GetCursorPosition() const;
        void SetTextImpl(const String& text);

        void ProcessEnter();
        void ProcessDelete();
        void ProcessBackpace();
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