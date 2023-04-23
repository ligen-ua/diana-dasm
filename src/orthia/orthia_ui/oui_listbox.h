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
    };

    class CListBox;
    struct IListBoxOwner
    {
        virtual ~IListBoxOwner() {}
        virtual void CancelAllQueries() = 0;
    };

    class CListBox:public WithBorder<CWindow>
    {
        using Parent_type = WithBorder<CWindow>;

        std::shared_ptr<DialogColorProfile> m_colorProfile;
        Rect m_lastRect;
        IListBoxOwner* m_owner;
        
        // columnts
        std::vector<ColumnParam> m_columns;
        int m_columnsCount = 0;

        // page state
        int m_offset = 0;
        int m_size = 0;
        std::vector<ListBoxItem> m_pageItems;

        // temporary data for painting
        static String m_chunk;

        void InitSize();

    protected:
        void OnResize() override;
        void DoPaintListMode(const Rect& rect, DrawParameters& parameters);

    public:
        CListBox(std::shared_ptr<DialogColorProfile> colorProfile, IListBoxOwner* owner);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
        void Destroy() override;

        int GetColumnsCount() const;

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
        std::vector<ListBoxItem>& GetItems();
    };

}