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

        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override
        {
            if (evt.resizeEvent.valid)
            {
                Size size;
                size.width = evt.resizeEvent.newWidth;
                size.height = evt.resizeEvent.newHeight;
                this->Resize(size);
            }
            return Base::ProcessEvent(evt, evtContext);
        }
    };

    template<class Base>
    class SimpleBrush:public Base
    {
    protected:
        Color m_color;
    public:
        SimpleBrush()
        {
        }
        template<class Type>
        SimpleBrush(Type&& obj)
            :
            Base(std::forward<Type>(obj))
        {
        }
        void SetBackgroundColor(Color color)
        {
            m_color = color;
        }
        void DoPaint(const Rect& rect, DrawParameters& parameters) override
        {
            auto paintRect = rect;
            auto size = this->GetSize();
            auto clientRect = this->GetClientRect();

            int rightPosX = size.width - clientRect.size.width - clientRect.position.x;
            int bottomPosY = size.height - clientRect.size.height - clientRect.position.y;
            paintRect.position.x += clientRect.position.x;
            paintRect.position.y += clientRect.position.y;

            paintRect.size.width -= clientRect.position.x + rightPosX;
            paintRect.size.height -= clientRect.position.y + bottomPosY;

            parameters.console.PaintRect(paintRect, m_color, false);
            Base::DoPaint(rect, parameters);
        }
    };

    template<class Base>
    class WithBorder:public Base
    {
    protected:
        Color m_frontColor, m_backgroundColor;
        BorderStyle m_style = BorderStyle::Thick;
    public:
        WithBorder()
        {
            m_frontColor = ColorWhite();
        }
        template<class Type>
        WithBorder(Type && obj)
            :
                Base(std::forward<Type>(obj))
        {
        }
        void SetColors(Color frontColor, Color backgroundColor)
        {
            m_frontColor = frontColor;
            m_backgroundColor = backgroundColor;
        }
        void SetBorderStyle(BorderStyle style)
        {
            m_style = style;
        }
        Rect GetClientRect() const override
        {
            Rect rect = Base::GetClientRect();
            if (m_style == BorderStyle::None)
            {
                return rect;
            }
            ++rect.position.x;
            ++rect.position.y;
            rect.size.width -= 2;
            rect.size.height -= 2;
            return rect;
        }
        void DoPaint(const Rect& rect, DrawParameters& parameters) override
        {
            if (m_style == BorderStyle::None)
            {
                Base::DoPaint(rect, parameters);
                return;
            }
            parameters.console.PaintBorder(rect, m_frontColor, m_backgroundColor, this->m_style);
            Base::DoPaint(rect, parameters);
        }
    };


    template<class Base>
    class ExitOnControlC:public Base
    {
    public:
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override
        {
            if (evt.keyEvent.valid && evt.keyEvent.virtualKey == VirtualKey::CtrlC)
            {
                if (auto pool = this->m_pool.lock())
                {
                    pool->ExitLoop();
                }
            }
            return Base::ProcessEvent(evt, evtContext);
        }
    };

    template<class Base>
    class MouseFocusable:public Base
    {
    public:
        void OnHandleMouseEvent(bool res, const Rect& rect, InputEvent& evt) override
        {
            if (res)
            {
                if (evt.mouseEvent.state == MouseState::Pressed)
                {
                    if (!this->IsFocused())
                    {
                        this->SetFocus();
                    }
                }
            }
            Base::OnHandleMouseEvent(res, rect, evt);
        }
    };



    template<class Base>
    class ChildSwitcher:public Base
    {
        std::vector<std::shared_ptr<CWindow>> m_childs;
    public:
        void RegisterSwitch(std::shared_ptr<CWindow> child)
        {
            m_childs.push_back(child);
        }
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override
        {
            if (evt.keyEvent.valid && evt.keyEvent.virtualKey == VirtualKey::Tab && !m_childs.empty())
            {
                if (auto poolPtr = this->GetPool())
                {
                    auto focused = poolPtr->GetFocus();
                    auto it = std::find(m_childs.begin(), m_childs.end(), focused);
                    if (it == m_childs.end())
                    {
                        m_childs[0]->SetFocus();
                        return true;
                    }
                    if (++it == m_childs.end())
                    {
                        it = m_childs.begin();
                    }
                    (*it)->SetFocus();
                    return true;
                }
            }
            return Base::ProcessEvent(evt, evtContext);
        }
    };
}
