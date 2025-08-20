#pragma once

#include "oui_console.h"
#include "oui_window_thread.h"

namespace oui
{
    struct InputEvent;
    class CWindow;

    enum class DragEvent
    {
        None = 0,
        Progress,
        Drop,
        Cancel
    };
    using DragHandler_type = std::function<bool(DragEvent event, 
        const Point& initialPoint, 
        const Point& currentPoint,
        std::shared_ptr<CWindow> wnd)>;

    class CConsole;
    class CWindowsPool:Noncopyable
    {
        std::unordered_map<CWindow*, std::shared_ptr<CWindow>> m_allWindows;
        std::shared_ptr<CWindow> m_focused;
        std::atomic<bool> m_exitRequested = false;

        std::shared_ptr<CWindow> m_rootWindow;
        std::shared_ptr<CWindow> m_lastMouseWindow;

        std::shared_ptr<CWindow> m_dragCaller;
        Point m_dragInitialPoint; 
        DragHandler_type m_dragHandler;

        std::shared_ptr<CWindow> m_modalWindow;
        std::shared_ptr<CWindowThread> m_thread;
        CConsole* m_pConsole = 0;
    public:
        CWindowsPool();

        void RegisterConsole(CConsole* pConsole);
        CConsole* GetConsole();

        void RegisterRootWindow(std::shared_ptr<CWindow> window,
            std::shared_ptr<CWindowThread> thread);
        std::shared_ptr<CWindowThread> GetThread();

        void RegisterWindow(std::shared_ptr<CWindow> window);
        void UnregisterWindow(CWindow* window);
        std::shared_ptr<CWindow> GetWindow(CWindow* window);

        void SetFocus(std::shared_ptr<CWindow> window, bool invalidate = true);
        std::shared_ptr<CWindow> GetFocus();
    
        void ExitLoop();
        bool IsExitRequested() const;
        std::shared_ptr<CWindow> GetRootWindow();

        void SetLastMouseWindow(std::shared_ptr<CWindow> window);
        std::shared_ptr<CWindow> GetLastMouseWindow();

        bool RegisterDragEvent(std::shared_ptr<CWindow> caller, const Point& pt, DragHandler_type handler);
        bool HandleDragEvent(InputEvent& evt);
        void CancelDragEvent();

        void SetModalWindow(std::shared_ptr<CWindow> window);
        std::shared_ptr<CWindow> GetModalWindow();
    };

    struct DrawParameters
    {
        CConsoleDrawAdapter console;
    };

    struct WindowEventContext
    {
        std::function<void(std::shared_ptr<CWindow>)> onMouseEventCallback;
        bool finishedProcessing = false;
    };

    class CWindow:Noncopyable
    {
    protected:
        std::weak_ptr<CWindow> m_parent;
        mutable std::weak_ptr<CWindowsPool> m_pool;

        Point m_position;
        Size m_size;

        bool m_visible = true;
        bool m_valid = false;

        bool m_active = false;
        bool m_mouseIsOn = false;
        bool m_destroyed = false;
        std::list<std::shared_ptr<CWindow>> m_childs;
        std::function<void()> m_onResize = nullptr;

        void RemoveChild(CWindow* child);
        bool IsMouseOn() const { return m_mouseIsOn;  }
        virtual void SetFocusImpl();

        virtual void ConstructChilds();
        virtual void OnResize();

        virtual bool HandleMouseEvent(const Rect& rect, InputEvent& evt);

        virtual bool ProcessMouseEvent(const Rect& rect, InputEvent& evt, WindowEventContext& evtContext);

        virtual void OnInit(std::shared_ptr<CWindowsPool> pool);
        virtual void OnAfterInit(std::shared_ptr<CWindowsPool> pool);
        virtual void OnHandleMouseEvent(bool result, const Rect& rect, InputEvent& evt);

        void RenderChilds(const Rect& rect, std::function<bool(std::shared_ptr<CWindow> child, const Rect& childRect)> handler);
        void ReverseRenderChilds(const Rect& rect, std::function<bool(std::shared_ptr<CWindow> child, const Rect& childRect)> handler);

        bool RenderChild(std::shared_ptr<CWindow> child,
            const Rect& rect,
            std::function<bool(std::shared_ptr<CWindow> child, const Rect& childRect)> handler,
            int endX,
            int endY);

        static CConsole* g_defConsole;
    public:
        CConsole* GetConsole() const;
 
        template<class Type>
        Type AddChild_t(Type child)
        {
            AddChild(child);
            return child;
        }
        template<class Type>
        Type AddChildAndInit_t(Type child)
        {
            auto ptr = GetPtr();
            if (!ptr)
            {
                return nullptr;
            }
            AddChild(child);
            child->Init(ptr);
            return child;
        }

        CWindow();
        virtual ~CWindow();
        virtual void OnChildFocused();

        static void InitDefConsole(CConsole* defConsole);

        bool IsDestroyed() const { return m_destroyed; }
        virtual bool IsPopup() const { return false;  }
        virtual void OnMouseLeave();
        virtual void OnMouseEnter();

        // init
        virtual void Init(std::shared_ptr<CWindowsPool> pool);
        virtual void Init(std::shared_ptr<CWindow> parent);
        void SetOnResize(std::function<void()> fnc);

        virtual void SetParent(std::shared_ptr<CWindow> parent);
        virtual std::shared_ptr<CWindow> GetParent();


        // visible
        virtual bool IsVisible() const;
        virtual void SetVisible(bool value);

        // focused
        virtual void SetFocus();
        virtual bool IsFocused() const;
        virtual void OnFocusLost();
        virtual void OnFocusEnter();
        virtual std::shared_ptr<CWindow> GetPopupPrevFocusTarget();

        // destroy
        virtual void Destroy();

        // address
        virtual Point GetPosition() const;
        virtual void MoveTo(const Point& newPt);

        // size
        virtual Size GetSize() const;
        virtual bool Resize(const Size& newSize);
        void ForceResize();

        Rect GetWndRect() const;
        virtual Rect GetClientRect() const;

        // draw stuff
        virtual void DrawTo(const Rect& rect, DrawParameters & parameters, bool& force);
        virtual void Invalidate(bool valid = false);
        virtual bool IsValid() const;

        virtual bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext);

        // paint
        virtual void DoPaint(const Rect& rect, DrawParameters& parameters);

        std::shared_ptr<CWindow> GetPtr();
        std::shared_ptr<CWindowsPool> GetPool() const;

        virtual void Activate();
        virtual void Deactivate();
        virtual bool IsActive() const;
        virtual bool IsActiveOrFocused() const;

        std::shared_ptr<CWindow> GetRootWindow();
        void AddChild(std::shared_ptr<CWindow> child);

        bool RegisterDragEvent(const Point& pt, DragHandler_type handler);

        std::shared_ptr<CWindowThread> GetThread();
    };

    template<class Type>
    std::shared_ptr<Type> Cast_t(std::shared_ptr<CWindow> window)
    {
        return std::dynamic_pointer_cast<Type>(window);
    }
    template<class Type>
    std::shared_ptr<Type> GetParent_t(std::shared_ptr<CWindow> window)
    {
        auto parent = window->GetParent();
        if (!parent)
        {
            return nullptr;
        }
        return Cast_t<Type>(parent);
    }
    template<class Type>
    std::shared_ptr<Type> GetParent_t(CWindow* window)
    {
        auto parent = window->GetParent();
        if (!parent)
        {
            return nullptr;
        }
        return Cast_t<Type>(parent);
    }

    template<class Type>
    std::shared_ptr<Type> GetPtr_t(CWindow* window)
    {
        auto me = window->GetPtr();
        if (!me)
        {
            return nullptr;
        }
        return Cast_t<Type>(me);
    }

    void InvalidateParent(CWindow* window);

    Rect GetAbsoluteClientRect(CWindow* pWindow, const Rect& rect);
    Point GetRelativeMousePoint(const Rect& rect, const Point& point);
    Point GetClientMousePoint(CWindow* pWindow, const Rect& rect, const Point& point);
}