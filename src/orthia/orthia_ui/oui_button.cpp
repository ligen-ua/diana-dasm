#include "oui_button.h"


namespace oui
{
    static const auto g_braceLeft = String::char_type('[');
    static const auto g_braceRight = String::char_type(']');
    String CButton::m_chunk, CButton::m_chunkText;

    CButton::CButton(std::shared_ptr<ButtonColorProfile> colorProfile, std::function<String()> getText)
        :
        m_colorProfile(colorProfile),
        m_getText(getText)
    {
    }
    CButton::~CButton()
    {
    }
    void CButton::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        const auto absClientRect = GetAbsoluteClientRect(this, rect);
        Point target = absClientRect.position;

        // prepare borders and background
        int symbolsLeft = absClientRect.size.width;
        if (symbolsLeft <= 0)
        {
            return;
        }

        m_chunk.native.clear();
        m_chunk.native.resize(absClientRect.size.width, String::symSpace);
        if (m_chunk.native.size() >= 2)
        {
            symbolsLeft -= 2;
            m_chunk.native[0] = g_braceLeft;
            m_chunk.native[m_chunk.native.size() - 1] = g_braceRight;
            if (IsFocused())
            {
                if (m_chunk.native.size() >= 4)
                {
                    symbolsLeft -= 2;
                    m_chunk.native[1] = g_braceLeft;
                    m_chunk.native[m_chunk.native.size() - 2] = g_braceRight;
                }
            }
        }

        m_chunkText = GetText();
        console->GetSymbolsAnalyzer().CutVisibleString(m_chunkText.native, symbolsLeft);

        auto renderPos = m_chunk.native.size() - m_chunkText.native.size();
        renderPos /= 2;
        std::copy(m_chunkText.native.begin(), m_chunkText.native.end(), m_chunk.native.begin() + renderPos);
        
        bool mouseInside = IsMouseOn();
        auto state = &m_colorProfile->normal;
        if (mouseInside)
        {
            state = &m_colorProfile->mouseHighlight;
        }
        else if (IsFocused())
        {
            state = &m_colorProfile->selected;
        }
        parameters.console.PaintText(target,
            state->text,
            state->background,
            m_chunk.native);
    }
    String CButton::GetText() const
    {
        auto console = GetConsole();

        String text = m_getText();
        if (console)
        {
            console->FilterOrReplaceUnreadableSymbols(text);
        }
        return text;
    }
    bool CButton::HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext)
    {
        Invalidate(false);
        return true;
    }
}
