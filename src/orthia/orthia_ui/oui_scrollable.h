#pragma once

#include "oui_window.h"

namespace oui
{

    class CScrollable : public CWindow
    {
        std::shared_ptr<CWindow> m_pTarget;

        void ConstructChilds() override;

    public:
        CScrollable(std::shared_ptr<CWindow> pTarget);
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override;

    };
}
