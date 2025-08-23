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
    void CListBox::DoPaintImpl(const Rect& rect, DrawParameters& parameters)
    {
        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        const auto colorProfile = m_colorProfile->listBox;
        const PanelBorderSymbols symbols = GetPanelBorderSymbols();
        const auto absClientRect = GetAbsoluteClientRect(this, rect);

        int columnsCount = GetColumnsCount();
        if (!columnsCount)
        {
            return;
        }
        int relLeftX = (0 * absClientRect.size.width) / columnsCount;
        int leftX = absClientRect.position.x + relLeftX;

        auto currentItem = m_pageItems.begin();
        String tmpStr;
        int pos = 0;

        for (int i = 0; i < columnsCount; ++i)
        {
            if (HasReportMode()) 
            {
                currentItem = m_pageItems.begin();
            }
            int relRightX = ((i + 1) * absClientRect.size.width) / columnsCount;
            int rightX = absClientRect.position.x + relRightX;

            int size = rightX - leftX;
            if (size < 1)
            {
                return;
            }
            int cutSize = 0;
            for (int u = 0; u < absClientRect.size.height; ++u)
            {
                const int yPos = u + absClientRect.position.y;

                m_chunk.native.clear();
                m_chunk.native.resize(size, String::symSpace);

                int symbolsCount = size - 1;
                bool cutHappens = false;
                decltype(ListBoxItem::colorsHandler) colorsHandler;
                if (currentItem != m_pageItems.end())
                {
                    colorsHandler = currentItem->colorsHandler;

                    int textPosition = 0;
                    if (HasReportMode())
                    {
                        textPosition = i;
                    }
                    tmpStr = textPosition >= currentItem->text.size() ? String() : currentItem->text[textPosition];
                    auto prevSize = tmpStr.native.size();

                    const auto info = console->GetSymbolsAnalyzer().CutVisibleString(tmpStr.native, symbolsCount);

                    auto newSize = tmpStr.native.size();
                    std::copy(tmpStr.native.begin(), tmpStr.native.end(), m_chunk.native.begin());

                    if (info.visibleSize > info.symbolsCount)
                    {
                        cutSize = info.visibleSize - info.symbolsCount;
                        m_chunk.native.erase(m_chunk.native.end()-cutSize, m_chunk.native.end());
                    }

                    cutHappens = newSize != prevSize;
                    ++currentItem;
                }

                if ((i + 1) != m_columnsCount)
                {
                    // not a last item, put vertical here
                    m_chunk.native.back() = String::char_type('/');
                    if (cutHappens)
                    {
                        m_chunk.native.push_back(String::char_type('}'));
                    }
                    else
                    {
                        m_chunk.native.push_back(symbols.vertical);
                    }
                }

                auto color = &colorProfile.normalText;
                LabelColorState customColor;
                if (m_selectedPosition == pos)
                {
                    if (IsFocused())
                    {
                        color = &colorProfile.selectedText;
                    }
                    else
                    {
                        color = &colorProfile.selectedNonFocusedText;
                    }
                }
                else
                {
                    if (colorsHandler)
                    {
                        customColor = colorsHandler();
                        color = &customColor;
                    }
                }

                Point pt{ leftX, yPos };
                parameters.console.PaintText(pt, 
                    color->text,
                    color->background,
                    m_chunk.native,
                    String::char_type('/'),
                    colorProfile.borderColor,
                    Color());
                ++pos;
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

        m_paintInProgress = true;

        DoPaintImpl(rect, parameters);

        Parent_type::DoPaint(rect, parameters);
        m_paintInProgress = false;
    }
    void CListBox::OnResize()
    {
        Parent_type::OnResize();
        InitSize();
    }
    void CListBox::Destroy()
    {
        auto guard = GetPtr();
        m_owner->CancelAllQueries();
        Parent_type::Destroy();
    }
    bool CListBox::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        {
            int pageSize = 1;
            if (evt.mouseEvent.button == MouseButton::WheelDown)
            {
                UIShiftWindow(m_offset, m_selectedPosition + pageSize);
                return true;
            }
            if (evt.mouseEvent.button == MouseButton::WheelUp)
            {
                UIShiftWindow(m_offset, m_selectedPosition - pageSize);
                return true;
            }
        }
        if (evt.mouseEvent.button == MouseButton::Left && 
            (evt.mouseEvent.state == MouseState::Pressed ||
            evt.mouseEvent.state == MouseState::DoubleClick))
        {
            auto relativePoint = GetClientMousePoint(this, rect, evt.mouseEvent.point);
            int newPosition = 0;
            if (m_columnsCount)
            {
                // list mode
                const auto absClientRect = GetAbsoluteClientRect(this, rect);
                int relLeftX = (0 * absClientRect.size.width) / m_columnsCount;
                int leftX = absClientRect.position.x + relLeftX;
                int targetColumn = -1;
                for (int i = 0; i < m_columnsCount; ++i)
                {
                    int relRightX = ((i + 1) * absClientRect.size.width) / m_columnsCount;
                    int rightX = absClientRect.position.x + relRightX;

                    if (evt.mouseEvent.point.x == rightX - 1)
                    {
                        // click on separator, do nothing
                        return true;
                    }
                    if (evt.mouseEvent.point.x < rightX - 1)
                    {
                        targetColumn = i;
                        break;
                    }
                    leftX = rightX;
                }
                if (targetColumn == -1)
                {
                    return true;
                }
                newPosition = (targetColumn * absClientRect.size.height) + relativePoint.y;

                if (relativePoint.y < 0 && targetColumn == 0)
                {
                    UIShiftWindow(m_offset, m_selectedPosition - 1);
                    return true;
                }
                if (newPosition >= m_visibleSize && targetColumn == m_columnsCount - 1)
                {
                    UIShiftWindow(m_offset, m_selectedPosition + 1);
                    return true;
                }
            }
            else
            {
                // report mode
                if (relativePoint.x < 0 || relativePoint.y < 0)
                {
                    return true;
                }
                newPosition = relativePoint.y;
            }
            if (newPosition >= (int)m_pageItems.size() && !m_pageItems.empty())
            {
                newPosition = (int)m_pageItems.size() - 1;
            }
            if (newPosition >= 0 && newPosition < (int)m_pageItems.size())
            {
                SetSelectedPosition(newPosition);
                m_owner->OnVisibleItemChanged();
                if (evt.mouseEvent.state == MouseState::DoubleClick)
                {
                    OpenSelectedItem();
                }
            }
            Invalidate();
        }
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
            m_visibleSize = rect.size.height * m_columnsCount;
        }
        else
        {
            // report mode
            m_visibleSize = rect.size.height * (int)m_columns.size();
        }
    }
    void CListBox::Clear()
    {
        if (m_paintInProgress)
        {
            __debugbreak();
        }
        m_offset = 0;
        m_pageItems.clear();
    }
    int CListBox::GetVisibleSize() const
    {
        return m_visibleSize;
    }
    int CListBox::GetOffset() const
    {
        return m_offset;
    }
    void CListBox::SetOffset(int offset)
    {
        m_offset = offset;
    }
    int CListBox::GetSelectedPosition() const
    {
        return m_selectedPosition;
    }
    bool CListBox::GetSelectedItem(ListBoxItem& item) const
    {
        if (m_selectedPosition >= 0 && m_selectedPosition < (int)m_pageItems.size())
        {
            item = m_pageItems[m_selectedPosition];
            return true;
        }
        return false;
    }
    void CListBox::SetSelectedPosition(int selectedPosition)
    {
        m_selectedPosition = selectedPosition;
    }
    std::vector<ListBoxItem> & CListBox::GetItems()
    {
        if (m_paintInProgress)
        {
            __debugbreak();
        }
        return m_pageItems;
    }
    void CListBox::OpenSelectedItem()
    {
        if (m_selectedPosition >=0 && m_selectedPosition < (int)m_pageItems.size())
        {
            auto& item = m_pageItems[m_selectedPosition];
            auto openHandler = item.openHandler;
            if (openHandler)
            {
                openHandler();
            }
        }
    }
    bool CListBox::ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext)
    {
        CConsole* console = GetConsole();
        if (!console)
        {
            return false;
        }

        if (evt.keyEvent.valid)
        {
            int newOffset = m_offset;
            int newPosition = m_selectedPosition;
            bool handled = false;
            switch (evt.keyEvent.virtualKey)
            {
            case VirtualKey::Enter:
                OpenSelectedItem();
                return true;

            case VirtualKey::PageUp:
                handled = true;
                newOffset -= GetVisibleSize();
                break;

            case VirtualKey::PageDown:
                handled = true;
                newOffset += GetVisibleSize();
                break;

            case VirtualKey::Home:
                handled = true;
                newOffset = 0;
                newPosition = 0;
                SetSelectedPosition(0);
                break;

            case VirtualKey::End:
                handled = true;
                newOffset = m_owner->GetTotalCount();
                break;

            // arrows
            case VirtualKey::Left:
                handled = true;
                newPosition -= GetVisibleSize() / 2;
                break;

            case VirtualKey::Right:
                handled = true;
                newPosition += GetVisibleSize() / 2;
                break;

            case VirtualKey::Up:
                handled = true;
                --newPosition;
                break;

            case VirtualKey::Down:
                handled = true;
                ++newPosition;
                break;
            default:
                break;
            }

            if (handled)
            {
                UIShiftWindow(newOffset, newPosition);
                return true;
            }
            // check text
            auto text = evt.keyEvent.rawText;
            console->FilterOrReplaceUnreadableSymbols(text);

            if (!text.native.empty())
            {
                if (m_owner->ShiftViewWindowToSymbol(text.native))
                {
                    return true;
                }
            }
            return false;
        }
        return Parent_type::ProcessEvent(evt, evtContext);
    }
    void CListBox::UIShiftWindow(int newOffset, int newPosition)
    {
        if (m_selectedPosition == newPosition)
        {
            // user requests to change offset
            m_owner->ShiftViewWindow(newOffset);
            return;
        }
        
        const int windowSize = (int)m_pageItems.size();
        if (newPosition < 0 || newPosition >= windowSize)
        {
            const int diff = newPosition - m_selectedPosition;
            newOffset = m_offset + diff;
            m_owner->ShiftViewWindow(newOffset);
        }
        else
        {
            SetSelectedPosition(newPosition);
            m_owner->OnVisibleItemChanged();
        }
        Invalidate();
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

    void DefaultShiftViewWindow(std::shared_ptr<CListBox> filesBox, int newOffset, size_t totalFilesAvailable_in)
    {
        const int visibleSize = filesBox->GetVisibleSize();
        const int totalFilesAvailable = (int)totalFilesAvailable_in;

        int newSelectedPositon = filesBox->GetSelectedPosition();

        int newSelectedOffset = newSelectedPositon + newOffset;
        int maxOffset = totalFilesAvailable - visibleSize;
        if (maxOffset < 0)
        {
            maxOffset = 0;
        }
        if (newOffset > maxOffset)
        {
            if (newSelectedOffset >= totalFilesAvailable)
            {
                auto sizeToProceed = std::min(totalFilesAvailable - maxOffset, visibleSize);
                newSelectedPositon = sizeToProceed - 1;
            }
            else
            {
                newSelectedPositon = visibleSize - (totalFilesAvailable - newSelectedOffset);
            }
            newOffset = maxOffset;
        }
        if (newOffset < 0)
        {
            newOffset = 0;
            newSelectedPositon = newSelectedOffset;
            if (newSelectedPositon < 0)
            {
                newSelectedPositon = 0;
            }
        }
        filesBox->SetSelectedPosition(newSelectedPositon);
        filesBox->SetOffset(newOffset);
    }
}