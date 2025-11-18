#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"
#include "oui_column_param.h"

namespace oui
{
    struct ListBoxItem
    {
        std::vector<String> text;
        std::function<void()> openHandler;
        std::function<LabelColorState()> colorsHandler;
        int fsFlags = 0;
    };

    class CListBox;
    struct IListBoxOwner
    {
        virtual ~IListBoxOwner() {}
        virtual int GetTotalCount() const = 0;
        virtual void CancelAllQueries() = 0;
        virtual void ShiftViewWindow(int newOffset) = 0;
        virtual void OnVisibleItemChanged() = 0;
        virtual bool ShiftViewWindowToSymbol(const String& symbol) = 0;
    };
    class ListBoxOwnerProxy :public IListBoxOwner
    {
    public:
        std::function<int()> getTotalCount;
        std::function<void()> cancelAllQueries;
        std::function<void(int newOffset)> shiftViewWindow;
        std::function<void()> onVisibleItemChanged;
        std::function<bool(const String& symbol)> shiftViewWindowToSymbol;

        int GetTotalCount() const
        {
            if (!getTotalCount)
            {
                return 0;
            }
            return getTotalCount();
        }
        void CancelAllQueries()
        {
            if (!cancelAllQueries)
            {
                return;
            }
            return cancelAllQueries();
        }
        void ShiftViewWindow(int newOffset)
        {
            if (!shiftViewWindow)
            {
                return;
            }
            return shiftViewWindow(newOffset);
        }
        void OnVisibleItemChanged()
        {
            if (!onVisibleItemChanged)
            {
                return;
            }
            return onVisibleItemChanged();
        }
        bool ShiftViewWindowToSymbol(const String& symbol) 
        {
            if (!shiftViewWindowToSymbol)
            {
                return false;
            }
            return shiftViewWindowToSymbol(symbol);
        }
    };

    class CListBox:public MouseFocusable<WithBorder<CWindow>>
    {
        struct ResizeState
        {
            int columnPos = 0;
            int columnSize = 0;
        };

        using Parent_type = MouseFocusable<WithBorder<CWindow>>;

        std::shared_ptr<DialogColorProfile> m_colorProfile;
        Rect m_lastRect;
        IListBoxOwner* m_owner;
        
        // columnts
        std::vector<ColumnParam> m_columns;
        int m_columnsCount = 0;

        // page state
        int m_selectedPosition = 0;
        int m_offset = 0;
        int m_visibleSize = 0;
        std::vector<ListBoxItem> m_pageItems;
        bool m_paintInProgress = false;
        ListBoxItem m_headerListBoxItem;
        int m_headerSize = 0;
        bool m_dockable = false;

        // temporary data for painting
        static String m_chunk;

        void InitSize();
        void UIShiftWindow(int newOffset, int newPosition);
        void OpenSelectedItem();
        void RebuildHeaderListBoxItem();
        void RenderColumnText(bool needPrintHeader, int columnPos, const String & inText, String& outText);

    protected:
        void OnResize() override;
        void DoPaintImpl(const Rect& rect, DrawParameters& parameters);
        bool StartColumnDrag(const Point& lastMousePoint, int columnPos);
        void OpenContextMenuItem(const Point& point, int targetColumn);

    public:
        CListBox(std::shared_ptr<DialogColorProfile> colorProfile, IListBoxOwner* owner);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext) override;
        void Destroy() override;

        int GetColumnsCount() const;

        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
    
        bool HasReportMode() const { return !m_columns.empty(); }
        // list mode
        void InitColumns(int columnsCount);

        // report mode
        void InitColumns(const ColumnParam& param1 = ColumnParam(),
            const ColumnParam& param2 = ColumnParam(),
            const ColumnParam& param3 = ColumnParam(),
            const ColumnParam& param4 = ColumnParam(),
            const ColumnParam& param5 = ColumnParam(),
            const ColumnParam& param6 = ColumnParam(),
            const ColumnParam& param7 = ColumnParam(),
            const ColumnParam& param8 = ColumnParam(),
            const ColumnParam& param9 = ColumnParam(),
            const ColumnParam& param10 = ColumnParam(),
            const ColumnParam& param11 = ColumnParam(),
            const ColumnParam& param12 = ColumnParam(),
            const ColumnParam& param13 = ColumnParam(),
            const ColumnParam& param14 = ColumnParam());

        void Clear();
        int GetVisibleSize() const;
        int GetOffset() const;
        void SetOffset(int offset);
        int GetSelectedPosition() const;
        void SetSelectedPosition(int selectedPosition);
        std::vector<ListBoxItem>& GetItems();

        void SelectRow();
        bool GetSelectedItem(ListBoxItem& item) const;

        void Dock();
    };

    void DefaultHighlightItem(std::shared_ptr<CListBox> filesBox, int highlightItemOffset, size_t containerSize);
    void DefaultShiftViewWindow(std::shared_ptr<CListBox> filesBox, int newOffset, size_t totalFilesAvailable_in);

    template<class Type>
    class DefaultPredicate
    {
        std::function<oui::String(const Type& obj)> m_getStringHandler;
    public:
        template<class T>
        DefaultPredicate(T&& value)
            :
            m_getStringHandler(std::forward<T>(value))
        {
        }
        bool operator ()(const Type& obj, const String& symbol)
        {
            return StartsWith(m_getStringHandler(obj).native, symbol.native);
        }
    };
    template<class OwnerType, class ContainerType, class Predicate>
    bool DefaultShiftViewWindowToSymbol(OwnerType * owner, 
        std::shared_ptr<CListBox> listBox,
        const String& symbol, 
        ContainerType& container, 
        Predicate predicate)
    {
        const int totalProcessAvailable = (int)container.size();
        const int selectionOffset = listBox->GetOffset() + listBox->GetSelectedPosition();

        // scan forward till end
        for (int i = selectionOffset + 1; i < totalProcessAvailable; ++i)
        {
            if (predicate(container[i], symbol.native))
            {
                owner->HighlightItem(i);
                owner->UpdateVisibleItems();
                return true;
            }
        }

        // scan from start
        for (int i = 0; i <= std::min((int)container.size() - 1, selectionOffset); ++i)
        {
            if (predicate(container[i], symbol.native))
            {
                owner->HighlightItem(i);
                owner->UpdateVisibleItems();
                return true;
            }
        }
        return false;
    }

    template<class OwnerType, class ContainerType, class ItemHandler>
    void DefaultUpdateVisibleItems(OwnerType owner,
        IListBoxOwner * ifaceOwner,
        std::shared_ptr<CListBox> filesBox, 
        ContainerType& container,
        ItemHandler && itemHandler)
    {
        if (!owner->IsVisible())
        {
            return;
        }
        auto console = owner->GetConsole();
        if (!console)
        {
            return;
        }
        // update visible items
        const auto visibleSize = filesBox->GetVisibleSize();
        auto& visibleItems = filesBox->GetItems();
        const int maxSize = (int)container.size();

        auto offset = filesBox->GetOffset();
        if ((offset + visibleSize) >= maxSize)
        {
            // set to the last file here
            visibleItems.clear();
            if (container.empty())
            {
                filesBox->Invalidate();
                return;
            }
            // we have some files, show last page
            offset = (int)container.size() - visibleSize;
            if (offset < 0)
            {
                offset = 0;
            }
            filesBox->SetOffset(offset);
        }

        auto sizeToProceed = std::min(maxSize - offset, visibleSize);
        visibleItems.resize(sizeToProceed);

        auto it = container.begin() + offset;
        auto it_end = it + sizeToProceed;
        auto vit = visibleItems.begin();
        for (; it != it_end; ++it, ++vit)
        {
            itemHandler(it, vit);
        }
        ifaceOwner->OnVisibleItemChanged();
        filesBox->Invalidate();
     }
}
