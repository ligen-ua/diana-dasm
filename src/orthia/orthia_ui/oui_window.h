#pragma once

#include "oui_base.h"

namespace oui
{
    class CWindow;
    class CWindowsPool
    {
        std::unordered_map<CWindow*, std::shared_ptr<CWindow>> m_allWindows;
        std::shared_ptr<CWindow> m_focused;
    public:
        CWindowsPool();
        void RegisterWindow(std::shared_ptr<CWindow> window);
        void UnregisterWindow(CWindow* window);

        void SetFocus(std::shared_ptr<CWindow> window);
        std::shared_ptr<CWindow> GetFocus();
    };

    struct InputEvent;
    class CConsole;

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
        explicit CWindow(bool visible = true);
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
        virtual void DrawTo(const Rect& rect, CConsole& console);
        virtual void Invalidate(bool valid = false);
        virtual bool IsValid() const;

        virtual void ProcessEvent(InputEvent& evt);
    };
}