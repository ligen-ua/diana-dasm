#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"
#include "oui_editbox.h"

namespace oui
{
    struct MultiLineEditSelectionContext
    {
        int lineNumber = 0;
        int symbolPos = 0;
    };
    inline int Compare(const MultiLineEditSelectionContext& ctx1, const MultiLineEditSelectionContext& ctx2)
    {
        if (ctx1.lineNumber < ctx2.lineNumber)
            return - 1;  
        if (ctx1.lineNumber > ctx2.lineNumber)
            return 1;
        if (ctx1.symbolPos < ctx2.symbolPos)
            return -1;
        if (ctx1.symbolPos > ctx2.symbolPos)
            return 1;
        return 0;
    }
    inline bool operator == (const MultiLineEditSelectionContext& ctx1, const MultiLineEditSelectionContext& ctx2)
    {
        return Compare(ctx1, ctx2) == 0;
    }
    inline bool operator != (const MultiLineEditSelectionContext& ctx1, const MultiLineEditSelectionContext& ctx2)
    {
        return Compare(ctx1, ctx2) != 0;
    }
    inline bool operator > (const MultiLineEditSelectionContext& ctx1, const MultiLineEditSelectionContext& ctx2)
    {
        return Compare(ctx1, ctx2) > 0;
    }
    inline bool operator < (const MultiLineEditSelectionContext & ctx1, const MultiLineEditSelectionContext & ctx2)
    {
        return Compare(ctx1, ctx2) < 0;
    }


    struct IMultilineViewTag
    {
        virtual ~IMultilineViewTag() {}
    };
    struct MultiLineViewItem
    {
        String text;
        int intTag = 0;
        std::shared_ptr<IMultilineViewTag> interfaceTag;
        oui::TextMarkup markup;
    };
    
    class LineIndex
    {
        std::uint64_t m_index = 0;
        int m_subIndex = 0;
    public:
        LineIndex()
        {
        }
        LineIndex(std::uint64_t index, int subIndex)
            : 
                m_index(index), m_subIndex(subIndex)
        {
        }
        std::uint64_t GetIndex() const
        {
            return m_index;
        }

        int CompareWith(const LineIndex& other) const
        {
            if (m_index < other.m_index) {
                return -2;
            }
            if (m_index > other.m_index) {
                return 2;
            }
            if (m_subIndex < other.m_subIndex) {
                return  -1;
            }
            if (m_subIndex > other.m_subIndex) {
                return 1;
            }
            return 0;
        }
    };

    inline bool operator < (const LineIndex& l1, const LineIndex& l2)
    {
        return l1.CompareWith(l2) < 0;
    }
    inline bool operator == (const LineIndex& l1, const LineIndex& l2)
    {
        return l1.CompareWith(l2) == 0;
    }
    inline bool operator != (const LineIndex& l1, const LineIndex& l2)
    {
        return l1.CompareWith(l2) != 0;
    }

    struct MultiLineSelPoint
    {
        // symbol coordinates
        int x = 0;
        LineIndex y;
    };
    inline bool operator < (const MultiLineSelPoint& p1, const MultiLineSelPoint& p2)
    {
        if (p1.y < p2.y) {
            return true;
        }
        if (p2.y < p1.y) {
            return false;
        }
        if (p1.x < p2.x) {
            return true;
        }
        if (p1.x > p2.x) {
            return false;
        }
        return false;
    }
    struct IMultiLineViewOwner
    {
        virtual ~IMultiLineViewOwner() {}
        virtual void CancelAllQueries() = 0;
        virtual bool ScrollUp(MultiLineViewItem* item, int count) = 0;
        virtual bool ScrollDown(MultiLineViewItem* item, int count) = 0;
        virtual LineIndex GetLineIndex(int offsetInPage) const = 0;
        virtual void CopySelected(const MultiLineSelPoint& selPosStart, const MultiLineSelPoint& selPosEnd) = 0;
        virtual bool SelectAll() = 0;
        virtual void OnEnter() = 0;
        virtual void OnPaintStart(std::shared_ptr<CEditBox> ) {}
    };


    class CMultiLineView:public SimpleBrush<MouseFocusable<CWindow>>
    {
    public:
        struct SelectedRangeInfo
        {
            LineIndex index;
            TextMarkup::Range range;
            int offsetInPage = 0;
        };
    private:
        using Parent_type = SimpleBrush<MouseFocusable<CWindow>>;
        using SelectionContext = MultiLineEditSelectionContext;

        std::shared_ptr<DialogColorProfile> m_colorProfile;

        int m_firstVisibleLineIndex = 0;

        // screen coordinates
        int m_yCursopPos = 0;

        bool m_cursorOutOfText = true;
        bool m_dynamicLogMode = true;

        std::vector<MultiLineViewItem> m_lines;

        std::shared_ptr<CEditBox> m_editBox;
        std::shared_ptr<CEditBox> m_paintBox;

        // selection
        MultiLineSelPoint m_selPosStart;
        MultiLineSelPoint m_selPosEnd;

        bool m_selectionIsActive = false;
        KeyState m_lastKeyState;
        // end

        Point m_lastMouseMovePoint;

        IMultiLineViewOwner* m_owner = 0;
        std::vector<SelectedRangeInfo> m_lastSelectedRanges, m_prevSelectedRanges;

        bool m_paintIsProgress = false;
        oui::LineIndex m_currentPaintedLineIndex;

        void ConstructChilds() override;
        void OnResize() override;
        void SetFocusImpl() override;

        void ScrollUp(int count);
        void ScrollDown(int count);

        void SetupHandlers();
        void SetNewCursor(const Point& pt);
        void SetNewYCursorPosImpl(int newCursor);

        bool HandleMouseEventImpl(const Rect& rect, InputEvent& evt, bool fromEditBox, MouseEventContext& mouseEventContext);
        bool KeyStateHasSelection() const;
        void ActivateSelection();
        void CancelSelection();
        void CancelSelectionIfNecessary();
        void OnEditBoxPaintDone();
        void DoPaintImpl(const Rect& rect, DrawParameters& parameters);

    public:
        CMultiLineView(std::shared_ptr<DialogColorProfile> colorProfile, IMultiLineViewOwner* owner, bool dynamicLogMode);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        void OnFocusLost() override;
        void OnFocusEnter() override;
        void Destroy() override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext) override;
        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
        const std::vector<SelectedRangeInfo>& GetPrevSelectedRanges();

        LineIndex GetCurrentLineIndex() const;
        oui::LineIndex GetCurrentPaintedLineIndex() const;

        bool SelectionIsActive() const;
        void FixupTopSelectionRange();
        bool CopySelected();
        void SelectAllCached();
        bool PaintInProgress() const;

        void Clear();
        void Init(std::vector<MultiLineViewItem>&& lines);
        void AddLine(MultiLineViewItem && item);
        std::pair<MultiLineViewItem, bool> GetItem(int offsetInPage);
        MultiLineViewItem GetCurrentItem() const;
        TextMarkup::Range GetCurrentItemRange() const;

        std::vector<MultiLineViewItem>::iterator VisibleItemsBegin();
        std::vector<MultiLineViewItem>::iterator VisibleItemsEnd();

        int GetCursorYPos() const;
        void SetCursorYPos(int newPos);
        bool SetCursorYPos(const oui::LineIndex& index);
        String ExtractSelected();
    };

}