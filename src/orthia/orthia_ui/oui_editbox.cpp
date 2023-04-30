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

                // scroll left if necessary
                if (cursorOffsetToUse >= m_windowRightIterator)
                {
                    m_windowRightIterator = cursorOffsetToUse + 1;
                }
                int chunkStartOffset = 0;
                for (;;)
                {
                    const int symbolBegin = m_windowRightIterator - windowsSymbolsCount;
                    if (symbolBegin < 0)
                    {
                        // inconsistency here
                        chunkStartOffset = 0; 
                        break;
                    }
                    chunkStartOffset = m_symbols[symbolBegin].charOffset;
                    cursorOffsetToUse = cursorOffset - chunkStartOffset;

                    if (cursorOffsetToUse >= 0)
                    {
                        break;
                    }
                    --m_windowRightIterator;
                }
                // erase begin
                m_chunk.native.erase(0, chunkStartOffset);
            }

            m_windowSymStart = m_windowRightIterator - windowsSymbolsCount;
            m_windowSymSize = windowsSymbolsCount;

            CutString(m_chunk.native, windowsSymbolsCount);
            stringToRender = &m_chunk;
        }
        else
        {
            m_windowSymStart = 0;
            m_windowSymSize = (int)m_symbols.size();

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
        if (evt.mouseEvent.button == MouseButton::Left && evt.mouseEvent.state == MouseState::Pressed)
        {
            auto relativePoint = GetClientMousePoint(this, rect, evt.mouseEvent.point);
            if (relativePoint.x < 0 || relativePoint.y < 0 || relativePoint.y != 0)
            {
                return false;
            }
            int newIterator = m_windowSymStart + relativePoint.x;
            if (newIterator >= 0 && newIterator <= (int)m_symbols.size())
            {
                m_cursorIterator = newIterator;
                Invalidate();
            }
        }
        return true;
    }
    void CEditBox::InsertText(const String& text)
    {
        if (text.native.empty())
        {
            return;
        }

        int symbolsToInsert = CalculateSymbolsCount(text.native, 0);

        // if no selection just insert
        if (m_cursorIterator >= 0 && m_cursorIterator < (int)m_symbols.size())
        {
            auto newText = m_text;
            newText.native.insert(m_symbols[m_cursorIterator].charOffset, text.native);
            SetTextImpl(newText);
        }
        else
        {
            SetTextImpl(m_text.native + text.native);
        }
        m_cursorIterator += symbolsToInsert;
    }

    void CEditBox::ProcessEnter()
    {
    }
    void CEditBox::ProcessDelete()
    {
        if (m_cursorIterator >= 0 && m_cursorIterator < (int)m_symbols.size())
        {
            auto newText = m_text;
            newText.native.erase(m_symbols[m_cursorIterator].charOffset, 1);
            SetTextImpl(newText);
        }
    }
    void CEditBox::ProcessBackpace()
    {
        if (m_cursorIterator > 0 && m_cursorIterator <= (int)m_symbols.size())
        {
            auto newText = m_text;
            newText.native.erase(m_symbols[m_cursorIterator-1].charOffset, 1);
            SetTextImpl(newText);
            --m_cursorIterator;
        }
    }

    bool CEditBox::ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext)
    {
        if (evt.keyEvent.valid)
        {
            bool handled = false;
            switch (evt.keyEvent.virtualKey)
            {
            case oui::VirtualKey::Enter:
                ProcessEnter();
                handled = true;
                break;

            case oui::VirtualKey::Delete:
                ProcessDelete();
                handled = true;
                break;
            case oui::VirtualKey::Backspace:
                ProcessBackpace();
                handled = true;
                break;

            case oui::VirtualKey::Left:
                if (m_cursorIterator > 0)
                {
                    --m_cursorIterator;
                }
                handled = true;
                break;
            case oui::VirtualKey::Right:
                if (m_cursorIterator < (int)m_symbols.size())
                {
                    ++m_cursorIterator;
                    if (m_cursorIterator >= (int)m_symbols.size())
                    {
                        m_windowRightIterator = m_cursorIterator + 1;
                    }
                }
                handled = true;
                break;
            }
            if (!handled && !evt.keyEvent.rawText.native.empty())
            {
                auto text = evt.keyEvent.rawText;
                FilterUnreadableSymbols(text.native);
                InsertText(text);
                handled = true;
            }
            Invalidate();
        }
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
    void CEditBox::SetTextImpl(const String& text)
    {
        CalculateSymbolsCount(text.native.c_str(), text.native.size(), m_symbols);
        m_text = text;
    }
    void CEditBox::SetText(const String& text)
    {
        SetTextImpl(text);
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