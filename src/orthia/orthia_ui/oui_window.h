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
    public:
        CWindowsPool();
        void RegisterWindow(std::shared_ptr<CWindow> window);
        void UnregisterWindow(CWindow* window);
        std::shared_ptr<CWindow> GetWindow(CWindow* window);

        void SetFocus(std::shared_ptr<CWindow> window);
        std::shared_ptr<CWindow> GetFocus();
    
        void ExitLoop();
        bool IsExitRequested() const;
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

        std::list<std::shared_ptr<CWindow>> m_childs;
        std::function<void()> m_onResize = nullptr;

        void RemoveChild(CWindow* child);
        void AddChild(std::shared_ptr<CWindow> child);

        virtual void ConstuctChilds();
        virtual void OnResize();

        template<class Type>
        Type AddChild_t(Type child)
        {
            AddChild(child);
            return child;
        }

    public:
        CWindow();
        virtual ~CWindow();

        // init
        virtual void Init(std::shared_ptr<CWindowsPool> pool);
        void SetOnResize(std::function<void()> fnc);

        virtual void SetParent(std::shared_ptr<CWindow> parent);
        virtual std::shared_ptr<CWindow> GetParent();


        // visible
        virtual bool IsVisible() const;
        virtual void SetVisible(bool value);

        // focused
        virtual bool IsFocused() const;

        // destroy
        virtual void Destroy();

        // address
        virtual Point GetPosition() const;
        virtual void MoveTo(const Point& newPt);

        // size
        virtual Size GetSize() const;
        virtual void Resize(const Size& newSize);

        // draw stuff
        virtual void DrawTo(const Rect& rect, DrawParameters & parameters, bool force);
        virtual void Invalidate(bool valid = false);
        virtual bool IsValid() const;

        virtual bool ProcessEvent(InputEvent& evt);

        // paint
        virtual void DoPaint(const Rect& rect, DrawParameters& parameters);

        std::shared_ptr<CWindow> GetPtr();
    };
}