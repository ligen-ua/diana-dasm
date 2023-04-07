#include "oui_window.h"
#include "oui_input.h"

namespace oui
{
    // CWindowsPool
    CWindowsPool::CWindowsPool()
    {

    }
    void CWindowsPool::RegisterRootWindow(std::shared_ptr<CWindow> window)
    {
        m_rootWindow = window;
        RegisterWindow(window);
    }
    void CWindowsPool::RegisterWindow(std::shared_ptr<CWindow> window)
    {
        m_allWindows.insert(std::make_pair(window.get(), window));
    }
    void CWindowsPool::UnregisterWindow(CWindow* window)
    {
        if (m_focused.get() == window)
        {
            m_focused = window->GetParent();
        }
        if (m_lastMouseWindow.get() == window)
        {
            m_lastMouseWindow = 0;
        }
        m_allWindows.erase(window);
    }
    std::shared_ptr<CWindow> CWindowsPool::GetWindow(CWindow* window)
    {
        auto it = m_allWindows.find(window);
        if (it == m_allWindows.end())
        {
            return nullptr;
        }
        return it->second;
    }

    void CWindowsPool::SetFocus(std::shared_ptr<CWindow> window, bool invalidate)
    {
        auto oldFocused = m_focused;

        if (m_focused != window)
        {
            m_focused = window;
        }
        if (invalidate)
        {
            if (window)
            {
                window->Invalidate();
            }
            if (oldFocused)
            {
                oldFocused->Invalidate();
            }
        }
        if (oldFocused && oldFocused->IsPopup())
        {
            oldFocused->Destroy();
        }
    }
    std::shared_ptr<CWindow> CWindowsPool::GetFocus()
    {
        return m_focused;
    }
    void CWindowsPool::ExitLoop()
    {
        m_exitRequested = true;
    }
    bool CWindowsPool::IsExitRequested() const
    {
        return m_exitRequested;
    }
    std::shared_ptr<CWindow> CWindowsPool::GetRootWindow()
    {
        return m_rootWindow;
    }
    void CWindowsPool::SetLastMouseWindow(std::shared_ptr<CWindow> window)
    {
        m_lastMouseWindow = window;
    }
    std::shared_ptr<CWindow> CWindowsPool::GetLastMouseWindow()
    {
        return m_lastMouseWindow;
    }

    // CWindow
    CWindow::CWindow()
    {

    }
    CWindow::~CWindow()
    {
    }
    void CWindow::ConstuctChilds()
    {
    }

    void CWindow::OnMouseLeave() 
    { 
        Invalidate(false);  
        m_mouseIsOn = false; 
    }
    void CWindow::OnMouseEnter() 
    { 
        Invalidate(false);  
        m_mouseIsOn = true; 
    }
    static void InvalidateParent(CWindow* window)
    {
        if (auto parent = window->GetParent())
        {
            parent->Invalidate();
        }
    }

    std::shared_ptr<CWindowsPool> CWindow::GetPool()
    {
        return m_pool.lock();
    }

    std::shared_ptr<CWindow> CWindow::GetPtr()
    {
        if (auto poolPtr = m_pool.lock())
        {
            return poolPtr->GetWindow(this);

        }
        return nullptr;
    }

    void CWindow::AddChild(std::shared_ptr<CWindow> child)
    {
        if (auto poolPtr = m_pool.lock())
        {
            if (auto me = GetPtr())
            {
                poolPtr->RegisterWindow(child);
                m_childs.push_back(child);
                child->SetParent(me);
            }
        }
    }
    void CWindow::SetOnResize(std::function<void()> fnc)
    {
        m_onResize = fnc;
    }
    void CWindow::OnInit(std::shared_ptr<CWindowsPool> pool)
    {
    }
    void CWindow::Init(std::shared_ptr<CWindow> parent)
    {
        if (auto pool = parent->GetPool())
        {
            Init(pool);
        }
    }
    void CWindow::Init(std::shared_ptr<CWindowsPool> pool)
    {
        m_pool = pool;
        ConstuctChilds();
        OnInit(pool);
        for (auto& child : m_childs)
        {
            child->Init(pool);
        }
    }
    void CWindow::SetParent(std::shared_ptr<CWindow> parent)
    {
        m_parent = parent;
    }
    std::shared_ptr<CWindow> CWindow::GetParent()
    {
        return m_parent.lock();
    }

    bool CWindow::IsVisible() const
    {
        return m_visible;
    }
    void CWindow::SetVisible(bool value)
    {
        m_visible = value;
    }
    void CWindow::Activate()
    {
        m_active = true;
        Invalidate();
    }
    void CWindow::Deactivate()
    {
        m_active = false;
        Invalidate();
    }
    bool CWindow::IsActive() const
    {
        return m_active;
    }
    bool CWindow::IsActiveOrFocused() const
    {
        return m_active || IsFocused();
    }
    void CWindow::SetFocus()
    {
        if (auto poolPtr = m_pool.lock())
        {
            if (auto me = GetPtr())
            {
                poolPtr->SetFocus(me);
            }
        }
    }

    bool CWindow::IsFocused() const
    {
        if (auto poolPtr = m_pool.lock())
        {
            auto focused = poolPtr->GetFocus();
            if (focused.get() == this)
            {
                return true;
            }
        }
        return false;
    }
    void CWindow::RemoveChild(CWindow* child)
    {
        for (auto it = m_childs.rbegin(), it_end = m_childs.rend(); it != it_end; ++it)
        {
            if (it->get() == child)
            {
                m_childs.erase(std::next(it).base());
                return;
            }
        }
    }
    void CWindow::Destroy()
    {
        for (auto it = m_childs.begin(), it_end = m_childs.end(); it != it_end; )
        {
            auto oldIt = it++;
            (*oldIt)->Destroy();
        }
        if (auto parent = GetParent())
        {
            parent->RemoveChild(this);
            parent->Invalidate();
        }
        if (auto poolPtr = m_pool.lock())
        {
            poolPtr->UnregisterWindow(this);
        }
    }
    Point CWindow::GetPosition() const
    {
        return m_position;
    }
    void CWindow::MoveTo(const Point& newPt)
    {
        if (m_position == newPt)
        {
            return;
        }
        
        InvalidateParent(this);
        m_position = newPt;
    }
    // size
    Size CWindow::GetSize() const
    {
        return m_size;
    }
    Rect CWindow::GetClientRect() const
    {
        {
            return { {0, 0}, m_size};
        }
    }
    void CWindow::Resize(const Size& newSize)
    {
        if (m_size == newSize)
        {
            return;
        }

        Invalidate();
        InvalidateParent(this);
        m_size = newSize;
        OnResize();
    }
    void CWindow::OnResize()
    {
        if (m_onResize)
        {
            m_onResize();
        }
    }
    // draw stuff
    void CWindow::DrawTo(const Rect& rect, DrawParameters& parameters, bool force_in)
    {
        bool force = force_in;
        if (force || !this->IsValid())
        {
            DoPaint(rect, parameters);
            this->Invalidate(true);
            force = true;
        }

        RenderChilds(rect, [&](std::shared_ptr<CWindow> child, const Rect& childRect) {
            child->DrawTo(childRect, parameters, force);
            return true;
        });
    }

    void CWindow::RenderChilds(const Rect& rect, std::function<bool(std::shared_ptr<CWindow> child, const Rect& childRect)> handler)
    {
        // draw childs anyway
        int endX = rect.position.x + rect.size.width;
        int endY = rect.position.y + rect.size.height;
        for (auto& child : m_childs)
        {
            if (!RenderChild(child, rect, handler, endX, endY))
            {
                break;
            }
        }
    }
    void CWindow::ReverseRenderChilds(const Rect& rect, std::function<bool(std::shared_ptr<CWindow> child, const Rect& childRect)> handler)
    {
        // draw childs anyway
        int endX = rect.position.x + rect.size.width;
        int endY = rect.position.y + rect.size.height;

        for (auto rit = m_childs.rbegin(), rend = m_childs.rend(); rit != rend; ++rit)
        {
            if (!RenderChild(*rit, rect, handler, endX, endY))
            {
                break;
            }
        }
    }
    bool CWindow::RenderChild(std::shared_ptr<CWindow> child,
        const Rect& rect,
        std::function<bool(std::shared_ptr<CWindow> child, const Rect& childRect)> handler,
        int endX,
        int endY)
    {
        if (!child->IsVisible())
        {
            return true;
        }
        // prepare child offset
        auto childOffset = child->GetPosition();

        auto childAbs = childOffset;
        childAbs.x = rect.position.x + childOffset.x;
        childAbs.y = rect.position.y + childOffset.y;

        // prepare child position
        auto childSize = child->GetSize();
        int childEndX = childAbs.x + childSize.width;
        int childEndY = childAbs.y + childSize.height;

        if (childEndX > endX)
        {
            childSize.width -= childEndX - endX;
        }
        if (childEndY > endY)
        {
            childSize.height -= childEndY - endY;
        }

        if (childSize.width <= 0 || childSize.height <= 0)
        {
            return true;
        }

        Rect childRect;
        childRect.position = childAbs;
        childRect.size = childSize;
        if (!handler(child, childRect))
        {
            return false;
        }
        return true;
    }
    void CWindow::Invalidate(bool valid)
    {
        if (!valid)
        {
            if (auto pool = GetPool())
            {
                if (auto focused = pool->GetFocus())
                {
                    if (focused.get() != this)
                    {
                        focused->Invalidate();
                    }
                }
            }
        }
        m_valid = valid;
    }
    bool CWindow::IsValid() const
    {
        return m_valid;
    }
    bool CWindow::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        return false;
    }

    bool CWindow::ProcessMouseEvent(const Rect& rect, InputEvent& evt, WindowEventContext& evtContext)
    {
        bool handled = false;
        ReverseRenderChilds(rect, [&](std::shared_ptr<CWindow> child, const Rect& childRect) {

            if (IsInside(childRect, evt.mouseEvent.point))
            {
                handled = child->ProcessMouseEvent(childRect, evt, evtContext);
                if (handled)
                {
                    return false;
                }
            }
            return true;
        });
        if (handled)
        {
            return true;
        }
        if (!IsInside(rect, evt.mouseEvent.point))
        {
            return false;
        }
        
        handled = HandleMouseEvent(rect, evt);
        if (handled && evtContext.onMouseEventCallback)
        {
            if (auto me = GetPtr())
            {
                evtContext.onMouseEventCallback(me);
            }
        }
        return handled;
    }

    bool CWindow::ProcessEvent(InputEvent& evt, WindowEventContext& evtContext)
    {
        if (evt.mouseEvent.valid)
        {
            Rect rect;
            rect.size = this->GetSize();

            if (ProcessMouseEvent(rect, evt, evtContext))
            {
                return true;
            }
        }
        return false;
    }
    void CWindow::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
    }

    std::shared_ptr<CWindow> CWindow::GetRootWindow()
    {
        if (auto pool = GetPool())
        {
            return pool->GetRootWindow();
        }
        return nullptr;
    }
    Point GetClientMousePoint(CWindow* pWindow, const Rect& rect, const Point& point)
    {
        auto relPoint = GetRelativeMousePoint(rect, point);
        auto clientRect = pWindow->GetClientRect();
        return { relPoint.x - clientRect.position.x, relPoint.y - clientRect.position.y };
    }
    Point GetRelativeMousePoint(const Rect& rect, const Point& point)
    {
        return Point{ point.x - rect.position.x, point.y - rect.position.y };
    }
    Rect GetAbsoluteClientRect(CWindow* pWindow, const Rect& rect)
    {
        auto paintRect = rect;

        auto size = pWindow->GetSize();

        auto clientRect = pWindow->GetClientRect();

        int rightPosX = size.width - clientRect.size.width - clientRect.position.x;
        int bottomPosY = size.height - clientRect.size.height - clientRect.position.y;
        paintRect.position.x += clientRect.position.x;
        paintRect.position.y += clientRect.position.y;

        paintRect.size.width -= clientRect.position.x + rightPosX;
        paintRect.size.height -= clientRect.position.y + bottomPosY;

        return paintRect;
    }
}