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
        if (availableSize <= 0)
        {
            m_firstVisibleLineIndex = 0;
        }
        else
        if (availableSize < availableHeight)
        {
            m_firstVisibleLineIndex -= availableHeight - availableSize;
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
    void CMultiLineView::AddLine(MultiLineViewItem&& item)
    {
        const Rect clientRect = GetClientRect();
            
        int visibleItemsCount = (int)m_lines.size() - m_firstVisibleLineIndex;
        int emptyLinesCount = clientRect.size.height - visibleItemsCount;

        m_lines.push_back(std::move(item));
        if (m_cursorOutOfText)
        {
            if (emptyLinesCount <= 0)
            {
                m_firstVisibleLineIndex = (int)m_lines.size() - clientRect.size.height;
            }
        }
        Invalidate();
    }
}