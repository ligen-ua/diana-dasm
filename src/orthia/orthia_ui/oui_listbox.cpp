#include "oui_listbox.h"

namespace oui
{
    String CListBox::m_chunk;

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
        const auto colorProfile = m_colorProfile->listBox;
        const PanelBorderSymbols symbols = GetPanelBorderSymbols();
        const auto absClientRect = GetAbsoluteClientRect(this, rect);

        int relLeftX = (0 * absClientRect.size.width) / m_columnsCount;
        int leftX = absClientRect.position.x + relLeftX;

        auto currentItem = m_pageItems.begin();
        String tmpStr;
        for (int i = 0; i < m_columnsCount; ++i)
        {
            int relRightX = ((i + 1) * absClientRect.size.width) / m_columnsCount;
            int rightX = absClientRect.position.x + relRightX;

            int size = rightX - leftX;
            if (size < 1)
            {
                return;
            }

            for (int u = 0; u < absClientRect.size.height; ++u)
            {
                const int yPos = u + absClientRect.position.y;

                m_chunk.native.clear();
                m_chunk.native.resize(size, String::symSpace);

                int symbolsCount = size - 1;
                bool cutHappens = false;
                if (currentItem != m_pageItems.end())
                {
                    tmpStr = currentItem->text.empty() ? String() : currentItem->text[0];
                    auto prevSize = tmpStr.native.size();
                    CutString(tmpStr.native, symbolsCount);
                    auto newSize = tmpStr.native.size();
                    std::copy(tmpStr.native.begin(), tmpStr.native.end(), m_chunk.native.begin());
                    cutHappens = newSize != prevSize;
                    ++currentItem;
                }

                if ((i + 1) != m_columnsCount)
                {
                    // not a last item, put vertical here
                    if (cutHappens)
                    {
                        m_chunk.native.back() = String::char_type('}');
                    }
                    else
                    {
                        m_chunk.native.back() = symbols.vertical;
                    }
                }

                auto color = &colorProfile.normalText;

                Point pt{ leftX, yPos };
                parameters.console.PaintText(pt, 
                    color->text,
                    color->background,
                    m_chunk.native);
            }
            leftX = rightX;
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
        InitSize();
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
        InitSize();
    }
    void CListBox::InitSize()
    {
        auto rect = GetClientRect();
        if (m_columnsCount)
        {
            // list mode
            m_size = rect.size.height * m_columnsCount;
        }
        else
        {
            // report mode
            m_size = rect.size.height * (int)m_columns.size();
        }
    }
    void CListBox::Clear()
    {
        m_offset = 0;
        m_pageItems.clear();
    }
    int CListBox::GetVisibleSize() const
    {
        return m_size;
    }
    int CListBox::GetOffset() const
    {
        return m_offset;
    }
    std::vector<ListBoxItem> & CListBox::GetItems()
    {
        return m_pageItems;
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
        InitSize();
    }

}