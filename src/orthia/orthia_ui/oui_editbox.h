#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"
#include "oui_text_markup.h"

namespace oui
{
    class CEditBox;
    struct EditBoxLowLevelHandlers
    {
        std::function<bool(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext)> mouseHandler;
        std::function<bool(InputEvent& evt)> ctrlCHandler;
        std::function<bool(InputEvent& evt)> ctrlAHandler;
        std::function<void(std::shared_ptr<CEditBox> editBox)> onPaintStart;
        std::function<void()> onPaintDone;
    };
    
    struct EditBoxSelectionRange;

    class CEditBox:public SimpleBrush<MouseFocusable<CWindow>>
    {
        using Parent_type = SimpleBrush<MouseFocusable<CWindow>>;

        std::shared_ptr<DialogColorProfile> m_colorProfile;
        String m_text;
        TextMarkup m_markup;
        Rect m_lastRect;
        std::vector<SymbolInfo> m_symbols;
        int m_cursorIterator = 0;
        int m_windowRightIterator = 0;

        // selection, in symbols
        int m_selPosStart = 0;
        int m_selPosEnd = 0;

        static String m_chunk, m_chunk2;
        void InsertText(const String& text);


        int m_windowSymStart = 0;
        int m_windowSymSize = 0;

        bool m_readOnly = false;
        bool m_selectAllOnFocus = false;
        std::function<void(const String&text)> m_enterHandler;

        EditBoxLowLevelHandlers m_llHandlers;
        Point m_lastMouseMovePoint;
        TextMarkup::Range m_lastSelectedRange;

        std::uint16_t m_manualHighlight = 0;
        void SetTextImpl(const String& text);
        void MoveToNextWordRight();
        void MoveToNextWordLeft();
        void ProcessDelete();
        void ProcessBackpace();
        int GetSymOffset(int symbol) const;
        void DoPaintMarkupText(DrawParameters& parameters, 
            std::vector<TextMarkup::Range>::const_iterator& it,
            int& rangePos,
            Point& target, String* stringToRender, 
            int startPos, int endPos,
            int chunkStartOffset);
        void DoPaintImpl(const Rect& rect, DrawParameters& parameters);

    public:
        CEditBox(std::shared_ptr<DialogColorProfile> colorProfile);
        void SetLastMousePoint(Point lastMouseMovePoint);
        void ResetSelection();
        void SetSelectAllOnFocus(bool selectAllOnFocus);
        void SetLowLevelHandlers(EditBoxLowLevelHandlers&& handlers);
        void SetReadOnly(bool readOnly);
        bool IsReadOnly() const;
        void SetEnterHandler(std::function<void(const String& text)> enterHandler);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext) override;
        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
        void Clear();
        String GetText() const;
        void SetMarkup(const TextMarkup& markup);
        TextMarkup::Range GetCursorRange() const;
        TextMarkup::Range GetLastSelectedRange() const;

        void SetText(const String& text);
        void ScrollRight();
        void OnFocusLost() override;
        void OnFocusEnter() override;
        bool SelectionIsActive() const;
        String ExtractSelected(bool cut);

        void SelectAll(bool moveCursor = true);
        void SelectCurrentWord();
        void Select(int startX, int endX);

        void HighlightRegion(std::uint16_t id);
        int AdjustVirtualCursorPosition(int startX);
        void SetCursorPosition(int newScreenX, bool changeSelecton, bool shiftMode);
        int GetVirtualCursorPosition() const;
        void SetVirtualCursorPosition(int newIterator, bool changeSelecton, bool shiftMode);
    };

}