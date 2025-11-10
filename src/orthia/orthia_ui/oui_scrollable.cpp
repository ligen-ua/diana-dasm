#include "oui_scrollable.h"


namespace oui
{
    CScrollable::CScrollable(std::shared_ptr<CWindow> pTarget)
        :
            m_pTarget(pTarget)
    {
    }
    void CScrollable::ConstructChilds()
    {
        AddChild(m_pTarget);
    }
    void CScrollable::ScrollHorizontally(int step)
    {
        auto myRect = GetWndRect();
        auto targetSize = m_pTarget->GetSize();

        if (myRect.size.width >= targetSize.width) 
        {
            return;
        }
        m_xPosPercentage += 10 * step;
        if (m_xPosPercentage > 100)
        {
            m_xPosPercentage = 100;
        }
        else if (m_xPosPercentage < 0)
        {
            m_xPosPercentage = 0;
        }
        Invalidate();
    }

    bool CScrollable::ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) 
    {
        if (evt.keyEvent.valid)
        {
            bool handled = false;
            switch (evt.keyEvent.virtualKey)
            {
                // arrows
            case VirtualKey::Left:
                if (evt.keyState.AnyCtrl)
                {
                    ScrollHorizontally(-1);
                    handled = true;
                }
                break;

            case VirtualKey::Right:
                if (evt.keyState.AnyCtrl)
                {
                    ScrollHorizontally(1);
                    handled = true;
                }
                break;

            default:
                break;
            }

            if (handled)
            {
                return true;
            }
        }
        return Parent_type::ProcessEvent(evt, evtContext);
    }
    void CScrollable::SetFocus()
    {
        m_pTarget->SetFocus();
    }
    bool CScrollable::IsFocused() const
    {
        return m_pTarget->IsFocused();
    }
    void CScrollable::OnFocusLost()
    {
        m_pTarget->OnFocusLost();
    }
    void CScrollable::OnFocusEnter()
    {
        m_pTarget->OnFocusEnter();
    }
    std::shared_ptr<CWindow> CScrollable::GetPopupPrevFocusTarget()
    {
        return m_pTarget->GetPopupPrevFocusTarget();
    }

    void CScrollable::Activate()
    {
        m_pTarget->Activate();
    }
    void CScrollable::Deactivate()
    {
        m_pTarget->Deactivate();
    }
    bool CScrollable::IsActive() const
    {
        return m_pTarget->IsActive();
    }
    bool CScrollable::IsActiveOrFocused() const
    {
        return m_pTarget->IsActiveOrFocused();
    }
    void CScrollable::OnResize()
    {
        auto myRect = GetWndRect();
        m_pTarget->MoveTo(myRect.position);

        auto targetSize = m_pTarget->GetSize();
        targetSize.height = myRect.size.height;
        m_pTarget->Resize(targetSize);
    }

    void CScrollable::DrawChilds(const Rect& rect, DrawParameters& parameters, bool& force)
    {
        auto targetSize = m_pTarget->GetSize();
        if (targetSize.width == 0 || targetSize.width <= rect.size.width)
        {
            CWindow::DrawChilds(rect, parameters, force);
            return;
        }
        // do custom draw
        if (!force && m_pTarget->IsValid())
        {
            return;
        }
        
        static DrawParameters tmpParams;
        tmpParams.console.StartDraw(targetSize, parameters.console.GetConsole());

        Rect grabRect = { Point(), targetSize };
        m_pTarget->DrawTo(grabRect, tmpParams, force);


        auto widthDifference = targetSize.width - rect.size.width;
        auto shiftPos = (widthDifference * m_xPosPercentage) / 100;
        
        grabRect.position.x += shiftPos;
        grabRect.size.width = rect.size.width;

        oui::LabelColorProfile colorProfile;
        QueryScrollBarColorProfile(colorProfile);

        LabelColorState* colorState = &colorProfile.normal;
        if (grabRect.size.width)
        {
            Point markPoint;
            if (m_xPosPercentage > 0)
            {
                markPoint.x = grabRect.position.x;
                tmpParams.console.PaintScrollMark(markPoint, CConsoleDrawAdapter::ScrollMarkType::Left,
                    colorState->text, colorState->background);
            }

            if (m_xPosPercentage < 100)
            {
                markPoint.x = grabRect.position.x + grabRect.size.width - 1;
                tmpParams.console.PaintScrollMark(markPoint, CConsoleDrawAdapter::ScrollMarkType::Right,
                    colorState->text, colorState->background);
            }

            tmpParams.console.CopyRectWindow(grabRect, rect.position, parameters.console);
        }
    }
    bool CScrollable::ProcessMouseEvent(const Rect& rect, InputEvent& evt, WindowEventContext& evtContext)
    {
        return m_pTarget->ProcessMouseEvent(rect, evt, evtContext);
    }
}
