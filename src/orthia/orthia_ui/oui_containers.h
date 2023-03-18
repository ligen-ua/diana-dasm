#pragma once
#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{
    class CContainerWindow:public CWindow
    {
        std::vector<std::shared_ptr<CWindow>> m_childs;
    protected:
        Color m_color;
    public:
        void SetForegroundColor(Color color)
        {
            m_color = color;
        }
        void DrawTo(DrawParameters& parameters) override
        {
            if (!this->IsValid())
            {
                parameters.console.PaintRect(parameters.rect, m_color, false);
                this->Invalidate(true);
            }
        }
    };
}