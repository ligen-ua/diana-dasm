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

        // selection
        int m_selPosStart = 0;
        int m_selPosEnd = 0;

        static String m_chunk, m_chunk2;
        void InsertText(const String& text);


        int m_windowSymStart = 0;
        int m_windowSymSize = 0;

        std::function<void(const String&text)> m_enterHandler;
        int GetCursorPosition() const;
        void SetTextImpl(const String& text);

        void ProcessDelete();
        void ProcessBackpace();
        void ResetSelection();
        int GetSymOffset(int symbol) const;

    public:
        CEditBox(std::shared_ptr<DialogColorProfile> colorProfile);
        void SetEnterHandler(std::function<void(const String& text)> enterHandler);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
        void Clear();
        String GetText() const;
        void SetText(const String& text);
        void ScrollRight();
        void OnFocusLost() override;
        void OnFocusEnter() override;
        bool SelectionIsActive() const;
        String ExtractSelected(bool cut);
        void SelectAll();
    };

}