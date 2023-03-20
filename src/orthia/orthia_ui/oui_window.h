#pragma once

#include "oui_console.h"

namespace oui
{
    class CWindow;
    class CWindowsPool
    {
        std::unordered_map<CWindow*, std::shared_ptr<CWindow>> m_allWindows;
        std::shared_ptr<CWindow> m_focused;
        std::atomic<bool> m_exitRequested = false;

        std::shared_ptr<CWindow> m_rootWindow;
    public:
        CWindowsPool();
        void RegisterRootWindow(std::shared_ptr<CWindow> window);
        void RegisterWindow(std::shared_ptr<CWindow> window);
        void UnregisterWindow(CWindow* window);
        std::shared_ptr<CWindow> GetWindow(CWindow* window);

        void SetFocus(std::shared_ptr<CWindow> window, bool invalidate = true);
        std::shared_ptr<CWindow> GetFocus();
    
        void ExitLoop();
        bool IsExitRequested() const;
        std::shared_ptr<CWindow> GetRootWindow();
    };

    struct InputEvent;
    struct DrawParameters
    {
        CConsoleDrawAdapter console;
    };
    class CWindow:Noncopyable
    {
    protected:
        std::weak_ptr<CWindow> m_parent;
        std::weak_ptr<CWindowsPool> m_pool;

        Point m_position;
        Size m_size;

        bool m_visible = true;
        bool m_valid = false;

        bool m_active = false;

        std::list<std::shared_ptr<CWindow>> m_childs;
        std::function<void()> m_onResize = nullptr;

        void RemoveChild(CWindow* child);

        virtual void ConstuctChilds();
        virtual void OnResize();

        template<class Type>
        Type AddChild_t(Type child)
        {
            AddChild(child);
            return child;
        }

        virtual void OnInit(std::shared_ptr<CWindowsPool> pool);

    public:
        CWindow();
        virtual ~CWindow();

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

        // destroy
        virtual void Destroy();

        // address
        virtual Point GetPosition() const;
        virtual void MoveTo(const Point& newPt);

        // size
        virtual Size GetSize() const;
        virtual void Resize(const Size& newSize);

        virtual Rect GetClientRect() const;

        // draw stuff
        virtual void DrawTo(const Rect& rect, DrawParameters & parameters, bool force);
        virtual void Invalidate(bool valid = false);
        virtual bool IsValid() const;

        virtual bool ProcessEvent(InputEvent& evt);

        // paint
        virtual void DoPaint(const Rect& rect, DrawParameters& parameters);

        std::shared_ptr<CWindow> GetPtr();
        std::shared_ptr<CWindowsPool> GetPool();

        virtual void Activate();
        virtual void Deactivate();
        virtual bool IsActive() const;
        virtual bool IsActiveOrFocused() const;

        std::shared_ptr<CWindow> GetRootWindow();
        void AddChild(std::shared_ptr<CWindow> child);
    };

    template<class Type>
    std::shared_ptr<Type> Cast_t(std::shared_ptr<CWindow> window)
    {
        return std::static_pointer_cast<Type>(window);
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
    Size GetBorderSize(Type ptr)
    {
        auto size = ptr->GetSize();
        auto clientRect = ptr->GetClientRect();
        return { size.width - clientRect.size.width, size.height - clientRect.size.height };
    }
}