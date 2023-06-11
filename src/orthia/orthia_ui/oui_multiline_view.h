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


    struct IMultiLineViewOwner
    {
        virtual ~IMultiLineViewOwner() {}
        virtual void CancelAllQueries() = 0;
    };

    struct MultiLineViewItem
    {
        String text;
        std::function<void(int number)> populateNextHandler;        
    };

    class CMultiLineView:public SimpleBrush<MouseFocusable<CWindow>>
    {
        using Parent_type = SimpleBrush<MouseFocusable<CWindow>>;
        using SelectionContext = MultiLineEditSelectionContext;

        std::shared_ptr<DialogColorProfile> m_colorProfile;

        int m_firstVisibleLineIndex = 0;
        int m_yCursopPos = 0;
        bool m_cursorOutOfText = true;
        std::vector<MultiLineViewItem> m_lines;

        std::shared_ptr<CEditBox> m_editBox;
        std::shared_ptr<CEditBox> m_paintBox;
        IMultiLineViewOwner* m_owner = 0;
        void ConstructChilds() override;
        void OnResize() override;
        void SetFocusImpl() override;

    public:
        CMultiLineView(std::shared_ptr<DialogColorProfile> colorProfile, IMultiLineViewOwner* owner);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        void OnFocusLost() override;
        void OnFocusEnter() override;
        void Destroy() override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;

        void Init(std::vector<MultiLineViewItem>&& lines);
        void AddLine(MultiLineViewItem && item);
    };

}