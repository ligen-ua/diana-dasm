#include "oui_button.h"


namespace oui
{
    String CButton::m_chunk;

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

        {
            auto text = GetText();
            m_chunk = text;
        }
        int symbolsLeft = absClientRect.size.width;
        console->GetSymbolsAnalyzer().CutVisibleString(m_chunk.native, symbolsLeft);

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
    bool CButton::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        Invalidate(false);
        return true;
    }
}
