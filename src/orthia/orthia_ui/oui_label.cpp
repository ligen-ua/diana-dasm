#include "oui_label.h"
#include "oui_input.h"

namespace oui
{
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
        auto state = &m_colorProfile->label.normal;

        auto text = GetText();

        bool mouseInside = IsInside(absClientRect, m_lastMouseMovePoint);
        if (mouseInside)
        {
            state = &m_colorProfile->label.mouseHighlight;
        }
        parameters.console.PaintText(target,
            state->text,
            state->background,
            text.native);

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