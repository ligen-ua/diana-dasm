#include "oui_editbox.h"
#include "oui_console.h"

namespace oui
{

    CEditBox::CEditBox(std::shared_ptr<DialogColorProfile> colorProfile)
        :
            m_colorProfile(colorProfile)
    {
        SetBackgroundColor(m_colorProfile->editBox.normal.background);        
    }
    void CEditBox::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        Parent_type::DoPaint(rect, parameters);

        const auto absClientRect = GetAbsoluteClientRect(this, rect);
  
        m_lastRect = absClientRect;

        if (auto pool = GetPool())
        {
            if (auto console = pool->GetConsole())
            {
                if (IsFocused())
                {
                    const int offset = GetCursorPosition();
                    const int cursorX = m_lastRect.position.x + offset;
                    console->SetCursorPositon(Point{ cursorX, m_lastRect.position.y });

                    console->ShowCursor();
                }
                else
                {
                    console->HideCursor();
                }
            }
        }

        {
            Point target = absClientRect.position;
            auto state = &m_colorProfile->editBox.normal;

            auto text = GetText();

            parameters.console.PaintText(target,
                state->text,
                state->background,
                text.native);
        }
    }
    bool CEditBox::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        return true;
    }
    bool CEditBox::ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext)
    {
        return Parent_type::ProcessEvent(evt, evtContext);
    }
    void CEditBox::Clear()
    {
        m_text.native.clear();
    }
    String CEditBox::GetText() const
    {
        return m_text;
    }
    void CEditBox::ScrollRight()
    {
        m_cursorIterator = (int)m_symbols.size();
    }
    int CEditBox::GetCursorPosition() const
    {
        if (!m_cursorIterator)
        {
            return 0;
        }
        int arrayOffset = m_cursorIterator - 1;
        if (arrayOffset >= (int)m_symbols.size())
        {
            arrayOffset = (int)m_symbols.size() - 1;;
        }
        auto& sym = m_symbols[arrayOffset];
        return sym.charOffset + sym.sizeInTChars;
    }
    void CEditBox::SetText(const String& text)
    {
        CalculateSymbolsCount(text.native.c_str(), text.native.size(), m_symbols);
        m_text = text;
    }
    void CEditBox::OnFocusLost()
    {
        Invalidate();
       
        if (auto pool = GetPool())
        {
            if (auto console = pool->GetConsole())
            {
                console->HideCursor();
            }
        }

        Parent_type::OnFocusLost();
    }
    void CEditBox::OnFocusEnter()
    {
        Invalidate();

        Parent_type::OnFocusEnter();
    }


}