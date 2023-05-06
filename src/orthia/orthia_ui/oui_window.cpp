#include "oui_window.h"
#include "oui_input.h"

namespace oui
{
    // CWindowsPool
    CWindowsPool::CWindowsPool()
    {

    }
    void CWindowsPool::SetModalWindow(std::shared_ptr<CWindow> window)
    {
        m_modalWindow = window;
    }
    std::shared_ptr<CWindow> CWindowsPool::GetModalWindow()
    {
        return m_modalWindow;
    }
    std::shared_ptr<CWindowThread> CWindowsPool::GetThread()
    {
        return m_thread;
    }
    void CWindowsPool::RegisterConsole(CConsole* pConsole)
    {
        m_pConsole = pConsole;
    }
    CConsole* CWindowsPool::GetConsole()
    {
        return m_pConsole;
    }
    void CWindowsPool::RegisterRootWindow(std::shared_ptr<CWindow> window,
        std::shared_ptr<CWindowThread> thread)
    {
        m_rootWindow = window;
        m_thread = thread;

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
        if (oldFocused)
        {
            oldFocused->OnFocusLost();
        }
        if (auto focused = m_focused)
        {
            focused->OnFocusEnter();
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
    bool CWindowsPool::RegisterDragEvent(std::shared_ptr<CWindow> caller, const Point& pt, DragHandler_type handler)
    {
        if (m_dragCaller)
        {
            return false;
        }
        m_dragCaller = caller;
        m_dragInitialPoint = pt;
        m_dragHandler = handler;
        return true;
    }
    bool CWindowsPool::HandleDragEvent(InputEvent& evt)
    {
        if (!m_dragCaller)
        {
            return false;
        }
        if (evt.keyEvent.valid && 
            evt.keyEvent.virtualKey == oui::VirtualKey::Escape)
        {
            CancelDragEvent();
            return true;
        }
        if (!evt.mouseEvent.valid)
        {
            return false;
        }
        DragEvent event = DragEvent::None;
        bool finalEvent = false;
        if (evt.mouseEvent.state == MouseState::Released)
        {
            finalEvent = true;
            event = DragEvent::Drop;
        }
        else
        {
            event = DragEvent::Progress;
        }

        if (!m_dragHandler(event, m_dragInitialPoint, evt.mouseEvent.point, m_dragCaller))
        {
            finalEvent = true;
        }
        if (finalEvent)
        {
            m_dragCaller = nullptr;
            m_dragInitialPoint = Point();
            m_dragHandler = nullptr;
        }
        return true;
    }
    void CWindowsPool::CancelDragEvent()
    {
        if (m_dragCaller && m_dragHandler)
        {
            m_dragHandler(DragEvent::Cancel, m_dragInitialPoint, m_dragInitialPoint, m_dragCaller);
        }
        m_dragCaller = nullptr;
        m_dragInitialPoint = Point();
        m_dragHandler = nullptr;
    }

    // CWindow
    CConsole* CWindow::g_defConsole = 0;
    void CWindow::InitDefConsole(CConsole* defConsole)
    {
        g_defConsole = defConsole;
    }
    CWindow::CWindow()
    {

    }
    CWindow::~CWindow()
    {
    }
    void CWindow::ConstructChilds()
    {
        // this MUST be empty here
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
    void InvalidateParent(CWindow* window)
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

    std::shared_ptr<CWindowThread> CWindow::GetThread()
    {
        auto poolPtr = m_pool.lock();
        if (!poolPtr)
        {
            return nullptr;
        }
        return poolPtr->GetThread();
    }

    bool CWindow::RegisterDragEvent(const Point& pt, DragHandler_type handler)
    {
        auto poolPtr = m_pool.lock();
        if (!poolPtr)
        {
            return false;
        }
        auto me = GetPtr();
        if (!me)
        {
            return false;
        }
        return poolPtr->RegisterDragEvent(me, pt, handler);
    }
    void CWindow::SetOnResize(std::function<void()> fnc)
    {
        m_onResize = fnc;
    }
    void CWindow::OnInit(std::shared_ptr<CWindowsPool> pool)
    {
    }
    void CWindow::OnAfterInit(std::shared_ptr<CWindowsPool> pool)
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
        ConstructChilds();
        OnInit(pool);
        for (auto& child : m_childs)
        {
            child->Init(pool);
        }
        OnAfterInit(pool);
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
    CConsole* CWindow::GetConsole()
    {
        if (auto pool = GetPool())
        {
            if (auto console = pool->GetConsole())
            {
                return console;
            }
        }
        return g_defConsole;
    }

    static bool CheckModalInput(std::shared_ptr<CWindowsPool> poolPtr,
        std::shared_ptr<CWindow> me)
    {
        if (auto modal = poolPtr->GetModalWindow())
        {
            if (!modal->IsPopup())
            {
                bool found = false;
                auto ptr = me;
                for (; ptr;)
                {
                    if (ptr == modal)
                    {
                        found = true;
                        break;
                    }
                    ptr = ptr->GetParent();
                }
                if (!found)
                {
                    return false;
                }
            }
        }
        return true;
    }

    void CWindow::SetFocus()
    {
        if (auto poolPtr = m_pool.lock())
        {
            if (auto me = GetPtr())
            {
                if (!IsVisible())
                {
                    return;
                }

                // don't try to focus inactive items
                if (!CheckModalInput(poolPtr, me))
                {
                    return;
                }
                poolPtr->SetFocus(me);
            }
        }
    }
    void CWindow::OnFocusLost()
    {
        if (IsPopup())
        {
            Destroy();
        }
    }
    void CWindow::OnFocusEnter()
    {
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

        if (m_position.x < 0)
        {
            m_position.x = 0;
        }
        if (m_position.y < 0)
        {
            m_position.y = 0;
        }
    }
    // size
    Rect CWindow::GetWndRect() const
    {
        return { GetPosition(), GetSize() };
    }
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
        if (m_size.width < 0)
        {
            m_size.width = 0;
        }
        if (m_size.height < 0)
        {
            m_size.height = 0;
        }
        OnResize();
    }
    void CWindow::ForceResize()
    {
        Invalidate();
        InvalidateParent(this);
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
    void CWindow::DrawTo(const Rect& rect, DrawParameters& parameters, bool& force)
    {
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
        m_valid = valid;
        if (!valid)
        {
            if (auto thread = GetThread())
            {
                thread->WakeUpUI();
            }
        }
    }
    bool CWindow::IsValid() const
    {
        return m_valid;
    }
    void CWindow::OnHandleMouseEvent(bool result, const Rect& rect, InputEvent& evt)
    {
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
            else
            {
                // check if child is a modal dialog
                if (auto poolPtr = m_pool.lock())
                {
                    if (auto modal = poolPtr->GetModalWindow())
                    {
                        if ((!modal->IsPopup()) && child == modal)
                        {
                            // no more mouse processing
                            return false;
                        }
                    }
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
        OnHandleMouseEvent(handled, rect, evt);
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
            Rect rect = this->GetWndRect();

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