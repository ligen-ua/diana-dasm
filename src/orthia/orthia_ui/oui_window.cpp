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
            m_focused = window->GetParent().lock();
        }
        m_allWindows.erase(window);
    }

    void CWindowsPool::SetFocus(std::shared_ptr<CWindow> window)
    {
        m_focused = window;
    }
    std::shared_ptr<CWindow> CWindowsPool::GetFocus()
    {
        return m_focused;
    }

    // CWindow
    CWindow::CWindow(bool visible)
        :
            m_visible(visible)
    {

    }
    CWindow::~CWindow()
    {
    }

    void CWindow::Init(std::weak_ptr<CWindowsPool> pool)
    {
        m_pool = pool;
    }
    void CWindow::SetParent(std::weak_ptr<CWindow> parent)
    {
        m_parent = parent;
        if (auto ptr = m_parent.lock())
        {
            Init(ptr->m_pool);
        }
    }
    std::weak_ptr<CWindow> CWindow::GetParent()
    {
        return m_parent;
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

    void CWindow::Destroy()
    {
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
        Invalidate();
        m_position = newPt;
    }

    // size
    Size CWindow::GetSize() const
    {
        return m_size;
    }
    void CWindow::Resize(const Size& newSize)
    {
        Invalidate();
        m_size = newSize;
    }

    // draw stuff
    void CWindow::DrawTo(const Rect& rect, CConsole& console)
    {

    }
    void CWindow::Invalidate(bool valid)
    {
        m_valid = valid;
    }
    bool CWindow::IsValid() const
    {
        return m_valid;
    }

    void CWindow::ProcessEvent(InputEvent& evt)
    {

    }

}