#include "oui_window.h"

namespace oui
{
    // CWindowsPool
    CWindowsPool::CWindowsPool()
    {

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

    void CWindowsPool::SetFocus(std::shared_ptr<CWindow> window)
    {
        m_focused = window;
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

    static void InvalidateParent(CWindow* window)
    {
        if (auto parent = window->GetParent())
        {
            parent->Invalidate();
        }
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
    void CWindow::Init(std::shared_ptr<CWindowsPool> pool)
    {
        m_pool = pool;
        ConstuctChilds();
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
        // draw childs anyway
        int endX = rect.position.x + rect.size.width;
        int endY = rect.position.y + rect.size.height;
        for (auto& child : m_childs)
        {
            if (!child->IsVisible())
            {
                continue;
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
                continue;
            }

            Rect childRect;
            childRect.position = childAbs;
            childRect.size = childSize;
            child->DrawTo(childRect, parameters, force);
        }
    }
    void CWindow::Invalidate(bool valid)
    {
        m_valid = valid;
    }
    bool CWindow::IsValid() const
    {
        return m_valid;
    }

    bool CWindow::ProcessEvent(InputEvent& evt)
    {
        return false;
    }
    void CWindow::DoPaint(const Rect& rect, DrawParameters& parameters)
    {

    }
}