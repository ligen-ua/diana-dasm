#pragma once

#include "oui_input.h"
#include "oui_color.h"
#include "oui_console.h"

namespace oui
{

    template<class Base>
    class Fullscreen:public Base
    {
    public:
        void MoveTo(const Point&) override
        {
            // do nothing
        }

        bool ProcessEvent(InputEvent& evt) override
        {
            if (evt.resizeEvent.valid)
            {
                Size size;
                size.width = evt.resizeEvent.newWidth;
                size.height = evt.resizeEvent.newHeight;
                this->Resize(size);
            }
            return Base::ProcessEvent(evt);
        }
    };

    template<class Base>
    class SimpleBrush:public Base
    {
    protected:
        Color m_color;
    public:
        void SetBackgroundColor(Color color)
        {
            m_color = color;
        }
        void DoPaint(const Rect& rect, DrawParameters& parameters) override
        {
            parameters.console.PaintRect(rect, m_color, false);
        }
    };


    template<class Base>
    class ExitOnControlC:public Base
    {
    public:
        bool ProcessEvent(InputEvent& evt) override
        {
            if (evt.keyEvent.valid && evt.keyEvent.virtualKey == VirtualKey::CtrlC)
            {
                if (auto pool = this->m_pool.lock())
                {
                    pool->ExitLoop();
                }
            }
            return Base::ProcessEvent(evt);
        }
    };
}
