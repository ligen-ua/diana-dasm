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

        void SetFocus(std::shared_ptr<CWindow> window);
        std::shared_ptr<CWindow> GetFocus();
    
        void ExitLoop();
        bool IsExitRequested() const;
    };

    struct InputEvent;
    struct DrawParameters
    {
        Rect rect; 
        CConsoleDrawAdapter console;
    };
    class CWindow
    {
    protected:
        std::weak_ptr<CWindow> m_parent;
        std::weak_ptr<CWindowsPool> m_pool;

        Point m_position;
        Size m_size;

        bool m_visible = true;
        bool m_valid = false;

    public:
        CWindow();
        virtual ~CWindow();

        // init
        virtual void Init(std::weak_ptr<CWindowsPool> pool);

        virtual void SetParent(std::weak_ptr<CWindow> parent);
        virtual std::weak_ptr<CWindow> GetParent();

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
        virtual void DrawTo(DrawParameters & parameters);
        virtual void Invalidate(bool valid = false);
        virtual bool IsValid() const;

        virtual bool ProcessEvent(InputEvent& evt);
    };
}