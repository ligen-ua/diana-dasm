#include "oui_listbox.h"

namespace oui
{
    CListBox::CListBox(std::shared_ptr<DialogColorProfile> colorProfile, IListBoxOwner* owner)
        :
            m_colorProfile(colorProfile),
            m_owner(owner)
    {
        SetBorderStyle(oui::BorderStyle::Thin);
        SetColors(colorProfile->listBox.borderColor, oui::Color());
    }
    void CListBox::DoPaintListMode(const Rect& rect, DrawParameters& parameters)
    {
        const auto absClientRect = GetAbsoluteClientRect(this, rect);

        for (int i = 0; i < m_columnsCount; ++i)
        {
            int relXPos = (i * absClientRect.size.width) / m_columnsCount;
            int xpos = absClientRect.position.x + relXPos;
        }
    }
    void CListBox::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        if (rect.size.width <= 0 || rect.size.height <= 0)
        {
            return;
        }

        if (m_columnsCount)
        {
            DoPaintListMode(rect, parameters);
        }

        Parent_type::DoPaint(rect, parameters);
;    }
    void CListBox::OnResize()
    {
        Parent_type::OnResize();
    }
    void CListBox::Destroy()
    {
        m_owner->CancelAllQueries();
        Parent_type::Destroy();
    }
    bool CListBox::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        return true;
    }
    int CListBox::GetColumnsCount() const
    {
        if (m_columnsCount)
        {
            return m_columnsCount;
        }
        return (int)m_columns.size();
    }
    void CListBox::InitColumns(int columnsCount)
    {
        m_columnsCount = columnsCount;
    }
    void CListBox::InitColumns(const ColumnParam& param1,
        const ColumnParam& param2,
        const ColumnParam& param3,
        const ColumnParam& param4,
        const ColumnParam& param5,
        const ColumnParam& param6,
        const ColumnParam& param7,
        const ColumnParam& param8,
        const ColumnParam& param9,
        const ColumnParam& param10,
        const ColumnParam& param11,
        const ColumnParam& param12,
        const ColumnParam& param13,
        const ColumnParam& param14)
    {
        m_columns.clear();
        const ColumnParam* items[] = { &param1, &param2, &param3, &param4, &param5,
                                       &param6, &param7, &param8, &param9, &param10,
                                       &param11, &param12, &param13, &param14 };
        int itemsCount = sizeof(items) / sizeof(items[0]);
        for (int i = 0; i < itemsCount; ++i)
        {
            if (items[i]->IsEmpty())
                break;

            m_columns.push_back(*items[i]);
        }
    }

}