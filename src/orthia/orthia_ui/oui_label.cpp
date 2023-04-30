#include "oui_label.h"
#include "oui_input.h"

namespace oui
{
    String CLabel::m_chunk;

    CLabel::CLabel(std::shared_ptr<DialogColorProfile> colorProfile, std::function<String()> getText)
        :
        m_colorProfile(colorProfile),
        m_getText(getText)
    {
    }
    void CLabel::DoPaint(const Rect& rect, DrawParameters& parameters) 
    {
        const auto absClientRect = GetAbsoluteClientRect(this, rect);
        Point target = absClientRect.position;

        {
            auto text = GetText();
            m_chunk = text;
        }
        int symbolsLeft = absClientRect.size.width;
        CutString(m_chunk.native, symbolsLeft);

        bool mouseInside = IsInside(absClientRect, m_lastMouseMovePoint);
        auto state = &m_colorProfile->label.normal;
        if (mouseInside)
        {
            state = &m_colorProfile->label.mouseHighlight;
        }
        parameters.console.PaintText(target,
            state->text,
            state->background,
            m_chunk.native);

        m_lastRect = absClientRect;
    }
    String CLabel::GetText() const
    {
        return m_getText();
    }
    bool CLabel::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        m_lastMouseMovePoint = evt.mouseEvent.point;
        Invalidate(false);
        return true;
    }
}