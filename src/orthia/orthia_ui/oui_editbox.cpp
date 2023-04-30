#include "oui_editbox.h"
#include "oui_console.h"

namespace oui
{
    String CEditBox::m_chunk;

    CEditBox::CEditBox(std::shared_ptr<DialogColorProfile> colorProfile)
        :
            m_colorProfile(colorProfile)
    {
        SetBackgroundColor(m_colorProfile->editBox.normal.background);        
    }
    void CEditBox::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        const auto absClientRect = GetAbsoluteClientRect(this, rect);
        m_lastRect = absClientRect;

        // calculate real text view window
        // [01234567[123_567]]
        //              ^-- cursorIterator
        // [[123_567]01234567]
        //      ^-- cursorIterator

        const int cursorOffset = m_cursorIterator;
        
        bool cursorAsSymbol = false;
        int totalSymbolsToDisplay = (int)m_symbols.size();
        if (cursorOffset >= totalSymbolsToDisplay)
        {
            cursorAsSymbol = true;
            ++totalSymbolsToDisplay;
        }

        int cursorOffsetToUse = cursorOffset;
        String* stringToRender = &m_text;
        const int windowsSymbolsCount = rect.size.width;
        if (windowsSymbolsCount < totalSymbolsToDisplay)
        {
            // windowed mode
            if (m_windowRightIterator < windowsSymbolsCount)
            {
                m_windowRightIterator = windowsSymbolsCount;
            }
            else
            if (m_windowRightIterator >= totalSymbolsToDisplay)
            {
                m_windowRightIterator = totalSymbolsToDisplay;
            }

            {
                auto text = GetText();
                m_chunk = text;
                if (cursorAsSymbol)
                {
                    m_chunk.native.push_back(String::symSpace);
                }

                const int symbolBegin = m_windowRightIterator - windowsSymbolsCount;
                int chunkStartOffset = 0;
                for (int pos = 0; pos < symbolBegin; ++pos)
                {
                    chunkStartOffset += m_symbols[pos].sizeInTChars;
                }

                m_chunk.native.erase(0, chunkStartOffset);
                cursorOffsetToUse -= chunkStartOffset;

            }
            stringToRender = &m_chunk;
        }
        else
        {
            // fill free space
            Parent_type::DoPaint(rect, parameters);
        }

        if (auto pool = GetPool())
        {
            if (auto console = pool->GetConsole())
            {
                if (IsFocused())
                {
                    const int cursorX = m_lastRect.position.x + cursorOffsetToUse;
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
                stringToRender->native);
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
        m_windowRightIterator = m_cursorIterator + 1;
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
        m_cursorIterator = 0;
        m_windowRightIterator = 0;
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