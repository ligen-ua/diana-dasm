#include "oui_multiline_view.h"
#include "oui_console.h"

namespace oui
{
    CMultiLineView::CMultiLineView(std::shared_ptr<DialogColorProfile> colorProfile, 
        IMultiLineViewOwner* owner)
        :
            m_colorProfile(colorProfile),
            m_owner(owner)
    {
        m_editBox = std::make_shared<CEditBox>(m_colorProfile);
        m_editBox->SetReadOnly(true);

        m_paintBox = std::make_shared<CEditBox>(m_colorProfile);
        m_paintBox->SetReadOnly(true);
        SetBackgroundColor(m_colorProfile->editBox.normal.background); 
    }
    void CMultiLineView::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
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
        if ((availableSize + 1) < availableHeight)
        {
            yResizeCorrection = availableHeight - availableSize;
            m_firstVisibleLineIndex -= yResizeCorrection;
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
                m_paintBox->SetText(it->text);

                oui::Rect childRect{ target, {absClientRect.size.width, 1} };
                m_paintBox->DoPaint(childRect, parameters);
            }
            ++it;
            ++target.y;
        }
        Point pt = { 0, m_yCursopPos };
        m_editBox->MoveTo(pt);
        if (m_cursorOutOfText)
        {
            m_editBox->SetText(String());
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
                    m_owner->ScrollUp(item, requestCount);
                    return;
                }
            }
            m_firstVisibleLineIndex = newFirstVisibleLineIndex;
            newCursor = 0;
        }

        m_cursorOutOfText = false;
        m_yCursopPos = newCursor;
        int offset = m_firstVisibleLineIndex + m_yCursopPos;
        if (offset >= (int)m_lines.size() || offset < 0)
        {
            offset = 0;
        }
        m_editBox->SetText(m_lines[offset].text);
        Invalidate();
    }

    void CMultiLineView::ScrollDown(int count)
    {
        const auto clientRect = GetClientRect();
        const int availableScreenHeight = clientRect.size.height;
        const int availableItemsCount = 1 + (int)m_lines.size() - m_firstVisibleLineIndex;

        const int lastPossibleCursor = std::min(availableScreenHeight, availableItemsCount);
        int newCursor = m_yCursopPos + count;
        if (newCursor > lastPossibleCursor)
        {
            int requestCount = newCursor - lastPossibleCursor;
            if (requestCount > availableItemsCount)
            {
                // ask owner, need some data
                MultiLineViewItem* item = m_lines.empty() ? nullptr : &m_lines[m_lines.size()-1];
                m_owner->ScrollDown(item, requestCount - availableItemsCount);
                return;
            }
            m_firstVisibleLineIndex += requestCount;
            newCursor = lastPossibleCursor;
        }

        m_yCursopPos = newCursor;
        if (m_firstVisibleLineIndex + m_yCursopPos >= (int)m_lines.size())
        {
            m_cursorOutOfText = true;
        }
        else
        {
            int offset = m_firstVisibleLineIndex + m_yCursopPos;
            m_editBox->SetText(m_lines[offset].text);
        }
        Invalidate();
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
            bool handled = false;
            switch (evt.keyEvent.virtualKey)
            {
            case VirtualKey::Up:
                handled = true;
                ScrollUp(1);
                return true;

            case VirtualKey::Down:
                handled = true;
                ScrollDown(1);
                break;

            default:
                break;
            }
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
    void CMultiLineView::Init(std::vector<MultiLineViewItem>&& lines)
    {
        m_firstVisibleLineIndex = 0;
        m_lines = std::move(lines);
        Invalidate();
    }
    bool CMultiLineView::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        Invalidate(false);
        return true;
    }
    void CMultiLineView::SetFocusImpl()
    {
        m_editBox->SetFocus();
    }
    void CMultiLineView::AddLine(MultiLineViewItem&& item)
    {
        m_lines.push_back(std::move(item));
        Invalidate();
    }
}