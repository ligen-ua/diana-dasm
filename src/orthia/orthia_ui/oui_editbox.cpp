#include "oui_editbox.h"
#include "oui_console.h"

// TODO: Add LTR support after switching to new terminal style
namespace oui
{
    String CEditBox::m_chunk;
    String CEditBox::m_chunk2;

    CEditBox::CEditBox(std::shared_ptr<DialogColorProfile> colorProfile)
        :
            m_colorProfile(colorProfile)
    {
        SetBackgroundColor(m_colorProfile->editBox.normal.background); 
    }
    bool CEditBox::SelectionIsActive() const
    {
        return m_selPosStart != m_selPosEnd;
    }
    String CEditBox::ExtractSelected(bool cut)
    {
        if (!SelectionIsActive())
        {
            return String();
        }

        int selPosStart = m_selPosStart;
        int selPosEnd = m_selPosEnd;
        if (selPosStart > selPosEnd)
        {
            selPosStart = m_selPosEnd;
            selPosEnd = m_selPosStart;
        }
        int start = GetSymOffset(selPosStart);
        int end = GetSymOffset(selPosEnd);
        if (end < start)
        {
            return String();
        }
        auto it = m_text.native.begin() + start;
        auto it_end = m_text.native.begin() + end;
        String res(decltype(String::native)(it, it_end));
        if (cut)
        {
            String text = m_text;
            text.native.erase(start, end - start);
            SetTextImpl(text);
            if (m_cursorIterator > (int)m_symbols.size())
            {
                m_cursorIterator = (int)m_symbols.size();
                m_windowRightIterator = m_cursorIterator + 1;
            }
            ResetSelection();
            Invalidate();
        }
        return res;
    }
    int CEditBox::GetSymOffset(int symbol) const
    {
        if (symbol <= 0)
        {
            return 0;
        }
        if (symbol >= (int)m_symbols.size())
        {
            if (m_symbols.empty())
            {
                return 0;
            }
            return m_symbols.back().charOffset + m_symbols.back().sizeInTChars;
        }
        auto& sym = m_symbols[symbol];
        return sym.charOffset;
    }
    void CEditBox::SetLowLevelHandlers(EditBoxLowLevelHandlers&& handlers)
    {
        m_llHandlers = std::move(handlers);
    }
    void CEditBox::SetReadOnly(bool readOnly)
    {
        m_readOnly = readOnly;
    }
    bool CEditBox::IsReadOnly() const
    {
        return m_readOnly;
    }
    void CEditBox::SetEnterHandler(std::function<void(const String& text)> enterHandler)
    {
        m_enterHandler = enterHandler;
    }
    void CEditBox::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        CConsole* console = GetConsole();
        if (!console)
        {
            return;
        }
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
        int chunkStartOffset = 0;
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

            console->GetSymbolsAnalyzer().CutVisibleString(m_chunk.native, windowsSymbolsCount);
            stringToRender = &m_chunk;
        }
        else
        {
            m_windowSymStart = 0;
            m_windowSymSize = (int)m_symbols.size();

            // fill free space
            Parent_type::DoPaint(rect, parameters);
        }

        if (auto console = GetConsole())
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

        if (!SelectionIsActive())
        {
            // simple case if no selection
            Point target = absClientRect.position;
            auto state = &m_colorProfile->editBox.normal;

            parameters.console.PaintText(target,
                state->text,
                state->background,
                stringToRender->native);
            return;
        }

        struct SelectionRange
        {
            int start = 0;
            int end = 0;
            int symbolsCount = 0;
            bool selected = false;
        };

        int selPosStart = m_selPosStart;
        int selPosEnd = m_selPosEnd;
        if (selPosStart > selPosEnd)
        {
            selPosStart = m_selPosEnd;
            selPosEnd = m_selPosStart;
        }

        SelectionRange ranges[3];
        ranges[0].start = GetSymOffset(0);
        ranges[0].end = GetSymOffset(selPosStart) - chunkStartOffset;
        ranges[0].symbolsCount = 0;
        if (ranges[0].end < 0)
        {
            ranges[0].end = 0;
        }
        if (selPosStart > m_windowSymStart)
        {
            ranges[0].symbolsCount = selPosStart - m_windowSymStart;
        }

        ranges[1].start = GetSymOffset(selPosStart) - chunkStartOffset;
        ranges[1].end = GetSymOffset(selPosEnd) - chunkStartOffset;
        ranges[1].selected = true;
        ranges[1].symbolsCount = selPosEnd - selPosStart;
        if (ranges[1].start < 0)
        {
            ranges[1].start = 0;
        }
        if (selPosStart < m_windowSymStart)
        {
            ranges[1].symbolsCount -= m_windowSymStart - selPosStart;
        }       
        if (selPosEnd > m_windowSymStart + m_windowSymSize)
        {
            ranges[1].symbolsCount -= selPosEnd - (m_windowSymStart + m_windowSymSize);
        }

        ranges[2].start = GetSymOffset(selPosEnd) - chunkStartOffset;
        ranges[2].end = GetSymOffset((int)m_symbols.size());
        ranges[2].symbolsCount = m_windowSymSize - ranges[0].symbolsCount - ranges[1].symbolsCount;

        Point target = absClientRect.position;
        for (int i = 0; i < 3; ++i)
        {
            auto range = ranges[i];

            if (range.symbolsCount <= 0)
            {
                continue;
            }
            int endPos = range.end;
            if (endPos > (int)stringToRender->native.size())
            {
                endPos = (int)stringToRender->native.size();
            }
            if (range.start >= range.end)
            {
                continue;
            }
            m_chunk2.native.assign(stringToRender->native.begin() + range.start,
                stringToRender->native.begin() + endPos);
            
            auto state = &m_colorProfile->editBox.normal;
            if (range.selected)
            {
                state = &m_colorProfile->editBox.selectedText;
            }
            parameters.console.PaintText(target,
                state->text,
                state->background,
                m_chunk2.native);

            target.x += range.symbolsCount;
        }
    }

    void CEditBox::SetVirtualCursorPosition(int newIterator, bool changeSelecton, bool shiftMode)
    {
        if (newIterator < 0)
        {
            newIterator = 0;
        }
        else
        if (newIterator > (int)m_symbols.size())
        {
            newIterator = (int)m_symbols.size();
        }
        m_cursorIterator = newIterator;

        if (changeSelecton)
        {
            if (shiftMode)
            {
                m_selPosEnd = m_cursorIterator;
            }
            else
            {
                ResetSelection();
            }
        }
        Invalidate();
    }
    void CEditBox::SetCursorPosition(int newScreenX, bool changeSelecton, bool shiftMode)
    {
        SetVirtualCursorPosition(m_windowSymStart + newScreenX, changeSelecton, shiftMode);
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
            SetCursorPosition(relativePoint.x, true, evt.keyState.state & evt.keyState.AnyShift);
        }
        if (m_llHandlers.mouseHandler)
        {
            return m_llHandlers.mouseHandler(rect, evt);
        }
        return true;
    }
    void CEditBox::InsertText(const String& text_in)
    {
        if (IsReadOnly())
        {
            return;
        }

        CConsole* console = GetConsole();
        if (!console)
        {
            return;
        }

        String text = text_in;
        console->FilterOrReplaceUnreadableSymbols(text);
        if (text.native.empty())
        {
            return;
        }

        if (SelectionIsActive())
        {
            ExtractSelected(true);
        }

        int symbolsToInsert = console->GetSymbolsAnalyzer().CalculateSymbolsCount(text.native, 0);

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
    void CEditBox::ProcessDelete()
    {
        if (IsReadOnly())
        {
            return;
        }
        if (SelectionIsActive())
        {
            ExtractSelected(true);
            return;
        }
        if (m_cursorIterator >= 0 && m_cursorIterator < (int)m_symbols.size())
        {
            auto newText = m_text;
            newText.native.erase(m_symbols[m_cursorIterator].charOffset, 1);
            SetTextImpl(newText);
        }
    }
    void CEditBox::ProcessBackpace()
    {
        if (IsReadOnly())
        {
            return;
        }
        if (SelectionIsActive())
        {
            ExtractSelected(true);
            return;
        }
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
            int prevCursor = m_cursorIterator;
            bool handled = false;
            bool cursorMove = false;
            switch (evt.keyEvent.virtualKey)
            {
            case oui::VirtualKey::Escape:
                if (SelectionIsActive())
                {
                    ResetSelection();
                    return true;
                }
                break;
            case oui::VirtualKey::kA:
                if (evt.keyState.state & evt.keyState.AnyCtrl)
                {
                    SelectAll();
                    handled = true;
                }
                break;
            case oui::VirtualKey::kV:
                if (evt.keyState.state & evt.keyState.AnyCtrl)
                {
                    String text;

                    if (auto console = GetConsole())                        
                    {
                        text = console->PasteTextFromClipboard();
                    }
                    if (!text.native.empty())
                    {
                        InsertText(text);
                        handled = true;
                    }
                }
                break;
            case oui::VirtualKey::kC:
                if (evt.keyState.state & evt.keyState.AnyCtrl)
                {
                    if (auto console = GetConsole())
                    {
                        console->CopyTextToClipboard(ExtractSelected(false));
                    }
                    handled = true;
                }
                break;
            case oui::VirtualKey::Enter:
                if (m_enterHandler)
                {
                    m_enterHandler(GetText());
                    handled = true;
                }
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
                cursorMove = true;
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
                cursorMove = true;
                break;
            case oui::VirtualKey::End:
                m_cursorIterator = (int)m_symbols.size();
                m_windowRightIterator = m_cursorIterator + 1;
                handled = true;
                cursorMove = true;
                break;
            case oui::VirtualKey::Home:
                m_cursorIterator = 0;
                handled = true;
                cursorMove = true;
                break;
            }

            if (cursorMove)
            {
                if (evt.keyState.state & evt.keyState.AnyShift)
                {
                    if (!SelectionIsActive())
                    {
                        m_selPosStart = prevCursor;
                    }
                    m_selPosEnd = m_cursorIterator;
                }
                else
                {
                    ResetSelection();
                }
            }
            if (!handled && !evt.keyEvent.rawText.native.empty())
            {
                InsertText(evt.keyEvent.rawText);
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
    int CEditBox::GetVirtualCursorPosition() const
    {
        return m_cursorIterator;
    }
    void CEditBox::SetTextImpl(const String& text)
    {
        CConsole* console = GetConsole();
        if (!console)
        {
            return;
        }
        console->GetSymbolsAnalyzer().CalculateSymbolsCount(text.native.c_str(), text.native.size(), m_symbols);
        m_text = text;
    }
    void CEditBox::SetText(const String& text)
    {
        SetTextImpl(text);
        m_cursorIterator = 0;
        m_windowRightIterator = 0;
        ResetSelection();
    }
    void CEditBox::SelectAll()
    {
        m_selPosStart = 0;
        m_selPosEnd = (int)m_symbols.size();
        m_cursorIterator = m_selPosEnd;
        Invalidate();
    }
    void CEditBox::ResetSelection()
    {
        m_selPosStart = m_cursorIterator;
        m_selPosEnd = m_cursorIterator;
        Invalidate();
    }

    void CEditBox::OnFocusLost()
    {
        Invalidate();
       
        if (auto console = GetConsole())
        {
            console->HideCursor();
        }

        ResetSelection();

        Parent_type::OnFocusLost();
    }
    void CEditBox::OnFocusEnter()
    {
        Invalidate();

        Parent_type::OnFocusEnter();
    }


}