#include "oui_multiline_view.h"
#include "oui_console.h"

namespace oui
{
      
     void CSelfHostedMultiLineViewOwner::CancelAllQueries()
     {
     }
     bool CSelfHostedMultiLineViewOwner::SelectAll()
     {
         SF_GetView()->SelectAllCached();
         return true;
     }
     void CSelfHostedMultiLineViewOwner::OnEnter()
     {
     }
     void CSelfHostedMultiLineViewOwner::CopySelected(const oui::MultiLineSelPoint&, const oui::MultiLineSelPoint&)
     {
         auto selectedText = SF_GetView()->ExtractSelected();
         auto console = SF_GetConsole();
         if (console)
         {
             console->CopyTextToClipboard(selectedText);
         }
     }
     oui::LineIndex CSelfHostedMultiLineViewOwner::GetLineIndex(int offsetInPage) const
     {
         return oui::LineIndex(offsetInPage, 0);
     }
     bool CSelfHostedMultiLineViewOwner::ScrollUp(oui::MultiLineViewItem* item, int count)
     {
         return false;
     }
     bool CSelfHostedMultiLineViewOwner::ScrollDown(oui::MultiLineViewItem* item, int count)
     {
         return false;
     }


    void CMultiLineView::SetupHandlers()
    {
        EditBoxLowLevelHandlers handlers;
        handlers.mouseHandler = [&](const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext) {
            // translate edit box rect to my rect
            auto myPosition = rect.position;
            myPosition.y -= m_yCursopPos;

            Rect myRect = this->GetWndRect();
            myRect.position = myPosition;

            return this->HandleMouseEventImpl(myRect, evt, true, mouseEventContext);
        };
        handlers.ctrlCHandler = [&](InputEvent& evt) {
            return this->CopySelected();
        };
        handlers.ctrlAHandler = [&](InputEvent& evt) {
            return m_owner->SelectAll();
        };
        handlers.onPaintStart = [&](std::shared_ptr<CEditBox> editBox) {
            m_owner->OnPaintStart(editBox);
        };
        handlers.onPaintDone = [&]() {
            OnEditBoxPaintDone();
        };
        m_editBox->SetLowLevelHandlers(std::move(handlers));

        m_editBox->SetEnterHandler([&](const String&) {
            if (this->SelectionIsActive() || m_editBox->SelectionIsActive()) {
                this->CopySelected();
                this->CancelSelection();
                return;
            }
            m_owner->OnEnter();
        });
        EditBoxLowLevelHandlers handlers2;
        handlers2.onPaintStart = [&](std::shared_ptr<CEditBox> editBox) {
            m_owner->OnPaintStart(editBox);
        };
        m_paintBox->SetLowLevelHandlers(std::move(handlers2));
    }
    CMultiLineView::CMultiLineView(std::shared_ptr<DialogColorProfile> colorProfile,
        IMultiLineViewOwner* owner,
        bool dynamicLogMode)
        :
        m_colorProfile(colorProfile),
        m_owner(owner),
        m_dynamicLogMode(dynamicLogMode)
    {
        m_editBox = std::make_shared<CEditBox>(m_colorProfile);
        m_editBox->SetReadOnly(true);

        m_paintBox = std::make_shared<CEditBox>(m_colorProfile);
        m_paintBox->SetReadOnly(true);

        SetupHandlers();
        SetBackgroundColor(m_colorProfile->editBox.normal.background);
    }
    void CMultiLineView::OnEditBoxPaintDone()
    {
        auto range = m_editBox->GetLastSelectedRange();
        if (range.id)
        {
            SelectedRangeInfo info;
            info.index = GetCurrentLineIndex();
            info.range = range;
            info.offsetInPage = m_yCursopPos + m_firstVisibleLineIndex;
            m_lastSelectedRanges.push_back(info);
        }
        std::sort(m_lastSelectedRanges.begin(), m_lastSelectedRanges.end(), [](auto &x, auto &y) { return x.index < y.index; });

        // compare selection
        bool needInvalidate = false;
        if (m_prevSelectedRanges.size() != m_lastSelectedRanges.size())
        {
            needInvalidate = true;
        }
        else
        {
            auto it = m_prevSelectedRanges.begin();
            for (auto& info : m_lastSelectedRanges)
            {
                if (it->index != info.index || it->range.id != info.range.id)
                {
                    needInvalidate = true;
                    break;
                }
                ++it;
            }
        }
        if (needInvalidate)
        {
            Invalidate();
        }
    }
    bool CMultiLineView::PaintInProgress() const
    {
        return m_paintIsProgress;
    }
    void CMultiLineView::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        m_paintIsProgress = true;
        DoPaintImpl(rect, parameters);
        m_paintIsProgress = false;
    }
    void CMultiLineView::DoPaintImpl(const Rect& rect, DrawParameters& parameters)
    {
        m_prevSelectedRanges = m_lastSelectedRanges;
        m_lastSelectedRanges.clear();

        const auto absClientRect = GetAbsoluteClientRect(this, rect);
        if (absClientRect.size.height <= 0)
        {
            return;
        }

        int availableHeight = absClientRect.size.height;
        if (m_cursorOutOfText)
        {
            --availableHeight;
        }

        // check available size
        int availableSize = (int)m_lines.size() - m_firstVisibleLineIndex;
        int yResizeCorrection = 0;
        if (availableSize <= 0)
        {
            m_firstVisibleLineIndex = 0;
        }
        else
        {
            if (!m_cursorOutOfText && m_yCursopPos >= absClientRect.size.height)
            {
                yResizeCorrection = m_yCursopPos - absClientRect.size.height + 1;
                m_firstVisibleLineIndex += yResizeCorrection;
            }
        }

        if (m_firstVisibleLineIndex < 0)
        {
            m_firstVisibleLineIndex = 0;
        }
        if (!m_lines.empty() && m_firstVisibleLineIndex >= (int)m_lines.size())
        {
            m_firstVisibleLineIndex = (int)m_lines.size() - 1;
        }
        if (m_cursorOutOfText)
        {
            m_yCursopPos = std::min(availableHeight, (int)m_lines.size() - m_firstVisibleLineIndex);
            if ((m_yCursopPos + m_firstVisibleLineIndex) < (int)m_lines.size())
            {
                m_firstVisibleLineIndex += (int)m_lines.size() - (m_yCursopPos + m_firstVisibleLineIndex);
            }
        }
        else
        {
            m_yCursopPos -= yResizeCorrection;
            if (m_yCursopPos < 0)
            {
                m_yCursopPos = 0;
            }
        }

        auto selPosStart = m_selPosStart;
        auto selPosEnd = m_selPosEnd;
        if (selPosEnd < selPosStart)
        {
            std::swap(selPosEnd, selPosStart);
        }

        auto target = absClientRect.position;
        auto it = m_lines.begin() + m_firstVisibleLineIndex;
        for (int i = 0; i < availableHeight; ++i)
        {
            if (it == m_lines.end())
            {
                break;
            }
            if (i != m_yCursopPos)
            {
                m_paintBox->SetMarkup(it->markup);
                m_paintBox->SetText(it->text);
                bool lineIndexValid = false;
                if (SelectionIsActive())
                {
                    lineIndexValid = true;
                    m_currentPaintedLineIndex = m_owner->GetLineIndex(m_firstVisibleLineIndex + i);
                    int res1 = m_currentPaintedLineIndex.CompareWith(selPosStart.y);
                    int res2 = m_currentPaintedLineIndex.CompareWith(selPosEnd.y);
                    if (res1 > 0 && res2 < 0)
                    {
                        m_paintBox->SelectAll();
                    }
                    else
                    if (res1 == 0)
                    {
                        m_paintBox->Select(selPosStart.x, -1);
                    }
                    else
                    if (res2 == 0)
                    {
                        m_paintBox->Select(0, selPosEnd.x);
                    }
                }
                oui::Rect childRect{ target, { absClientRect.size.width, 1 } };

                if (m_lastMouseMovePoint.y == i)
                {
                    auto point = m_lastMouseMovePoint;
                    point.y = 0;
                    m_paintBox->SetLastMousePoint(point);
                }
                else
                {
                    m_paintBox->SetLastMousePoint(Point());
                }
                if (!lineIndexValid)
                {
                    m_currentPaintedLineIndex = m_owner->GetLineIndex(m_firstVisibleLineIndex + i);
                }
                m_paintBox->DoPaint(childRect, parameters);

                auto lastRange = m_paintBox->GetLastSelectedRange();
                if (lastRange.id)
                {
                    SelectedRangeInfo info;
                    info.index = m_currentPaintedLineIndex;
                    info.range = lastRange;
                    info.offsetInPage = m_firstVisibleLineIndex + i;
                    m_lastSelectedRanges.push_back(info);
                }
            }
            ++it;
            ++target.y;
        }
        Point pt = { 0, m_yCursopPos };
        m_editBox->MoveTo(pt);
        if (m_lastMouseMovePoint.y != m_yCursopPos)
        {
            m_editBox->SetLastMousePoint(Point());
        }
        if (m_cursorOutOfText)
        {
            m_editBox->SetText(String());
        }
        if (SelectionIsActive())
        {
            // adjust selections
            auto lineIndex = m_owner->GetLineIndex(m_firstVisibleLineIndex + m_yCursopPos);
            int res1 = lineIndex.CompareWith(selPosStart.y);
            int res2 = lineIndex.CompareWith(selPosEnd.y);
            if (res1 > 0 && res2 < 0)
            {
                // select all case when edit box is inside the selection
                m_editBox->SelectAll();
            }
            else
            if (res1 == 0 && res2 == 0)
            {
                // special case when user selects a lot and then returns back
                m_editBox->Select(m_selPosStart.x, m_selPosEnd.x);
            }
            else
            {
                if (res1 == 0)
                {
                    m_editBox->Select(-1, selPosStart.x);
                }
                else
                {
                    m_editBox->Select(0, selPosEnd.x);
                }
            }
        }
    }

    void CMultiLineView::ScrollUp(int count)
    {
        int newCursor = m_yCursopPos - count;
        if (newCursor < 0)
        {
            int requestCount = -newCursor;
            int newFirstVisibleLineIndex = m_firstVisibleLineIndex;
            newFirstVisibleLineIndex -= requestCount;
            if (newFirstVisibleLineIndex < 0)
            {
                requestCount = -newFirstVisibleLineIndex;
                newFirstVisibleLineIndex = 0;

                if (requestCount)
                {
                    // ask owner, need some data
                    MultiLineViewItem* item = m_lines.empty() ? nullptr : &m_lines[0];
                    if (m_owner->ScrollUp(item, count))
                    {
                        return;
                    }
                }
            }
            m_firstVisibleLineIndex = newFirstVisibleLineIndex;
            newCursor = 0;
        }

        m_cursorOutOfText = false;
        SetNewYCursorPosImpl(newCursor);
        Invalidate();
    }

    void CMultiLineView::ScrollDown(int count)
    {
        const auto clientRect = GetClientRect();
        const int availableScreenHeight = clientRect.size.height;
        int availableItemsCount = (int)m_lines.size() - m_firstVisibleLineIndex;
        int lastPossibleCursor = std::min(availableScreenHeight, availableItemsCount);
        if (m_dynamicLogMode)
        {
            ++availableItemsCount;
        }
        else
        {
            --lastPossibleCursor;
        }
        int newCursor = m_yCursopPos + count;
        if (newCursor > lastPossibleCursor)
        {
            int availableItemsCountAfterCursor = availableItemsCount - m_yCursopPos - 1;
            if (availableItemsCountAfterCursor < 0)
            {
                availableItemsCountAfterCursor = 0;
            }
            if (count > availableItemsCountAfterCursor)
            {
                // ask owner, need some data
                MultiLineViewItem* item = m_lines.empty() ? nullptr : &m_lines[m_lines.size() - 1];
                if (m_owner->ScrollDown(item, count))
                {
                    return;
                }
            }
            int requestCount = std::min(availableItemsCountAfterCursor, count);
            m_firstVisibleLineIndex += requestCount;
            newCursor = lastPossibleCursor;
        }

        SetNewYCursorPosImpl(newCursor);
        Invalidate();
    }
    bool CMultiLineView::CopySelected()
    {
        if (SelectionIsActive())
        {
            m_owner->CopySelected(m_selPosStart, m_selPosEnd);
            return true;
        }
        else
        {
            // local selection 
            if (m_editBox->SelectionIsActive())
            {
                if (auto console = GetConsole())
                {
                    console->CopyTextToClipboard(m_editBox->ExtractSelected(false));
                }
            }
        }
        return false;
    }
    void CMultiLineView::FixupTopSelectionRange() 
    {
        if (SelectionIsActive())
        {
            m_selPosEnd.y = GetCurrentLineIndex();
        }
    }
    bool CMultiLineView::SelectionIsActive() const
    {
        return m_selectionIsActive;
    }

    bool CMultiLineView::ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext)
    {
        CConsole* console = GetConsole();
        if (!console)
        {
            return false;
        }

        if (evt.keyEvent.valid)
        {
            m_lastKeyState = evt.keyState;
            bool handled = false;
            switch (evt.keyEvent.virtualKey)
            {
            case VirtualKey::Up:
                handled = true;
                CancelSelectionIfNecessary();
                ScrollUp(1);
                break;

            case VirtualKey::Down:
                handled = true;
                CancelSelectionIfNecessary();
                ScrollDown(1);
                break;

            case VirtualKey::PageUp:
                handled = true;
                CancelSelectionIfNecessary();
                ScrollUp(GetClientRect().size.height);
                break;

            case VirtualKey::PageDown:
                handled = true;
                CancelSelectionIfNecessary();
                ScrollDown(GetClientRect().size.height);
                break;

            default:
                break;
            }
            m_lastKeyState = KeyState();
            if (handled)
            {
                Invalidate();
            }
            return handled;
        }
        return Parent_type::ProcessEvent(evt, evtContext);
    }
    void CMultiLineView::OnFocusLost()
    {
        Invalidate();

        if (auto console = GetConsole())
        {
            console->HideCursor();
        }

        Parent_type::OnFocusLost();
    }
    void CMultiLineView::OnFocusEnter()
    {
        Invalidate();

        Parent_type::OnFocusEnter();
    }
    void CMultiLineView::ConstructChilds()
    {
        AddChild(m_editBox);
        GetPool()->RegisterWindow(m_paintBox);
        m_paintBox->Init(GetPool());
    }
    void CMultiLineView::OnResize()
    {
        const Rect clientRect = GetClientRect();

        Size size = clientRect.size;
        size.height = 1;
        m_editBox->Resize(size);
    }
    void CMultiLineView::Destroy()
    {
        auto guard = GetPtr();
        m_owner->CancelAllQueries();
        Parent_type::Destroy();
    }
    void CMultiLineView::Clear()
    {
        std::vector<MultiLineViewItem> lines;
        Init(std::move(lines));
    }
    void CMultiLineView::Init(std::vector<MultiLineViewItem>&& lines, bool cancelSelection)
    {
        m_cursorOutOfText = false;
        m_firstVisibleLineIndex = 0;
        m_lines = std::move(lines);
        if (cancelSelection)
        {
            CancelSelection();
        }
        SetNewYCursorPosImpl(m_yCursopPos);
        Invalidate();
    }

    bool CMultiLineView::KeyStateHasSelection() const
    {
        return m_lastKeyState.HasShift();
    }

    void CMultiLineView::ActivateSelection()
    {
        if (m_selectionIsActive)
        {
            return;
        }
        m_selPosStart.y = GetCurrentLineIndex();
        m_selPosEnd.y = GetCurrentLineIndex();
        m_selPosStart.x = m_editBox->GetVirtualCursorPosition();
        m_selPosEnd.x = m_editBox->GetVirtualCursorPosition();
        m_selectionIsActive = true;
    }
    void CMultiLineView::CancelSelection()
    {
        m_selectionIsActive = false;
        m_editBox->ResetSelection();
        Invalidate();
    }
    void CMultiLineView::CancelSelectionIfNecessary()
    {
        if (SelectionIsActive() && !KeyStateHasSelection()) 
        {
            CancelSelection();
        }
    }
    bool CMultiLineView::SetCursorYPos(const oui::LineIndex& index)
    {
        CancelSelection();

        const auto clientRect = GetClientRect();
        if (clientRect.size.height <= 0)
        {
            return false;
        }

        int availableHeight = clientRect.size.height;
        if (m_cursorOutOfText)
        {
            --availableHeight;
        }

        auto it = m_lines.begin() + m_firstVisibleLineIndex;
        for (int i = 0; i < availableHeight; ++i)
        {
            if (it == m_lines.end())
            {
                break;
            }
            auto curIndex = m_owner->GetLineIndex(m_firstVisibleLineIndex + i);
            if (curIndex == index)
            {
                SetNewYCursorPosImpl(i);
                return true;
            }
            ++it;
        }
        return false;
    }
    void CMultiLineView::SetNewYCursorPosImpl(int newCursor)
    {
        if (KeyStateHasSelection())
        {
            ActivateSelection();
        }
        else
        {
            CancelSelection();
        }
        bool selectionActive = SelectionIsActive();

        m_yCursopPos = newCursor;
        bool outOfBounds = m_firstVisibleLineIndex + m_yCursopPos >= (int)m_lines.size();
        if (((int)m_lines.size() - m_firstVisibleLineIndex <= 0) || (outOfBounds && m_dynamicLogMode))
        {
            m_cursorOutOfText = true;
        }
        else
        {
            if (outOfBounds)
            {
                m_yCursopPos = (int)m_lines.size() - m_firstVisibleLineIndex - 1;
            }
            m_cursorOutOfText = false;
            int offset = m_firstVisibleLineIndex + m_yCursopPos;
            int cursorPos = m_editBox->GetVirtualCursorPosition();
            m_selPosEnd.y = m_owner->GetLineIndex(offset);
            m_selPosEnd.x = cursorPos;

            auto &line = m_lines[offset];
            m_editBox->SetText(line.text);
            m_editBox->SetMarkup(line.markup);
            m_editBox->SetVirtualCursorPosition(cursorPos, true, false);
        }
    }

    oui::LineIndex CMultiLineView::GetCurrentPaintedLineIndex() const
    {
        return m_currentPaintedLineIndex;
    }
    LineIndex CMultiLineView::GetCurrentLineIndex() const
    {
        return m_owner->GetLineIndex(m_yCursopPos + m_firstVisibleLineIndex);
    }

    void CMultiLineView::SetNewCursor(const Point& pt)
    {
        if (m_yCursopPos == pt.y)
        {
            return;
        }
        int newCursopPos = pt.y;
        int availableHeight = GetClientRect().size.height;

        int maxPos = std::min(availableHeight, (int)m_lines.size() - m_firstVisibleLineIndex);
        if (newCursopPos > maxPos)
        {
            newCursopPos = maxPos;
            m_cursorOutOfText = true;
        }
        else
        {
            m_cursorOutOfText = false;
        }
        SetNewYCursorPosImpl(newCursopPos);

        if (!m_cursorOutOfText)
        {
            m_editBox->SetCursorPosition(pt.x, true, false);
        }
    }
    bool CMultiLineView::HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext)
    {
        auto relativePoint = GetClientMousePoint(mouseEventContext, this, rect, evt.mouseEvent.point);
        m_lastMouseMovePoint = relativePoint;
        m_lastKeyState = evt.keyState;
        bool res = HandleMouseEventImpl(rect, evt, false, mouseEventContext);
        m_lastKeyState = KeyState();
        return res;
    }
    bool CMultiLineView::HandleMouseEventImpl(const Rect& rect, InputEvent& evt, bool fromEditBox, MouseEventContext& mouseEventContext)
    {
        {
            int pageSize = 1;
            if (evt.mouseEvent.button == MouseButton::WheelDown)
            {
                ScrollDown(pageSize);
                return true;
            }
            if (evt.mouseEvent.button == MouseButton::WheelUp)
            {
                ScrollUp(pageSize);
                return true;
            }
        }
        auto clientRect = GetClientRect();
        auto point = GetRelativeMousePoint(mouseEventContext, rect, evt.mouseEvent.point);
        bool handled = false;


        bool tryGoLink = 0;
        if (evt.mouseEvent.button == MouseButton::Left && evt.mouseEvent.state == MouseState::Pressed)
        {
            if (point.y < 0 || point.y >= clientRect.size.height)
            {
                return false;
            }
            CancelSelectionIfNecessary();

            if (!fromEditBox)
            {
                // got click
                SetNewCursor(point);
                handled = true;
            }
            tryGoLink = evt.keyState.HasJustCtrl();
        }        
        if (tryGoLink)
        {
            m_owner->OnEnter();
        }
        Invalidate(false);
        return handled;
    }
    void CMultiLineView::SetFocusImpl()
    {
        m_editBox->SetFocus();
    }
    std::pair<MultiLineViewItem, bool> CMultiLineView::GetItem(int offsetInPage)
    {
        if (m_lines.empty())
        {
            return std::make_pair(MultiLineViewItem(), false);
        }
        if (offsetInPage >= (int)m_lines.size())
        {
            return std::make_pair(m_lines[m_lines.size() - 1], false);
        }
        return std::make_pair(m_lines[offsetInPage], true);
    }
    void CMultiLineView::AddLine(MultiLineViewItem&& item)
    {
        m_lines.push_back(std::move(item));
        Invalidate();
    }
    TextMarkup::Range CMultiLineView::GetCurrentItemRange() const
    {
        return m_editBox->GetCursorRange();
    }
    MultiLineViewItem CMultiLineView::GetCurrentItem() const
    {
        bool outOfBounds = (m_firstVisibleLineIndex + m_yCursopPos) >= (int)m_lines.size();
        if (outOfBounds)
        {
            return MultiLineViewItem();
        }
        return m_lines[(m_firstVisibleLineIndex + m_yCursopPos)];
    }
    std::vector<MultiLineViewItem>::iterator CMultiLineView::VisibleItemsBegin()
    {
        if (m_firstVisibleLineIndex > (int)m_lines.size())
        {
            return m_lines.end();
        }
        return m_lines.begin() + m_firstVisibleLineIndex;
    }
    std::vector<MultiLineViewItem>::iterator CMultiLineView::VisibleItemsEnd()
    {
        if (m_firstVisibleLineIndex > (int)m_lines.size())
        {
            return m_lines.end();
        }
        int avaliableItemsCount = (int)m_lines.size() - m_firstVisibleLineIndex;
        int availableHeight = GetClientRect().size.height;
        return m_lines.begin() + std::min(avaliableItemsCount, availableHeight);
    }

    int CMultiLineView::GetCursorYPos() const
    {
        return m_yCursopPos;
    }
    void CMultiLineView::SetCursorYPos(int newPos)
    {
        SetNewYCursorPosImpl(newPos);
    }
    void CMultiLineView::SelectAllCached()
    {
        if (m_lines.empty())
        {
            CancelSelection();
            return;
        }
        m_selPosStart.y = m_owner->GetLineIndex(0);
        m_selPosEnd.y = m_owner->GetLineIndex((int)m_lines.size());
        m_selPosStart.x = 0;
        m_selPosEnd.x = 0;
        m_selectionIsActive = true;
        Invalidate();
    }
    void CMultiLineView::GoToLastLine()
    {
        auto offsetInPage = m_yCursopPos + m_firstVisibleLineIndex;
        ScrollDown((int)m_lines.size() - offsetInPage);
        Invalidate();
    }

    String CMultiLineView::ExtractSelected()
    {
        if (!SelectionIsActive())
        {
            return m_editBox->ExtractSelected(false);
        }

        auto startPos = m_selPosStart, endPos = m_selPosEnd;
        if (endPos < startPos)
        {
            std::swap(startPos, endPos);
        }

        if (startPos.y == endPos.y)
        {
            return m_editBox->ExtractSelected(false);
        }
        
        int yPos = -1;
        String result;
        for (auto& line : m_lines)
        {
            ++yPos;
            auto index = m_owner->GetLineIndex(yPos);
            auto res1 = index.CompareWith(startPos.y);
            auto res2 = index.CompareWith(endPos.y);
            if (res1 < 0)
            {
                // current line is above selection
                continue;
            }
            if (res2 > 0)
            {
                // current line is below selection
                break;
            }
            if (res1 == 0)
            {
                m_paintBox->SetMarkup(line.markup);
                m_paintBox->SetText(line.text.native);
                m_paintBox->Select(startPos.x, -1);
                result.native += m_paintBox->ExtractSelected(false).native;
                result.native += OUI_TCSTR("\n");
                continue;
            }
            if (res2 == 0)
            {
                m_paintBox->SetMarkup(line.markup); 
                m_paintBox->SetText(line.text.native);
                m_paintBox->Select(-1, endPos.x);
                result.native += m_paintBox->ExtractSelected(false).native;
                result.native += OUI_TCSTR("\n");
                break;
            }
            result.native += line.text.native;
            result.native += OUI_TCSTR("\n");
        }
        return result;
    }

    const std::vector<CMultiLineView::SelectedRangeInfo>& CMultiLineView::GetPrevSelectedRanges()
    {
        return m_prevSelectedRanges;
    }
}

