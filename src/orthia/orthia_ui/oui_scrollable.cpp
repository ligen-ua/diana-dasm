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
    bool CScrollable::ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) 
    {
        return m_pTarget->ProcessEvent(evt, evtContext);
    }
}
