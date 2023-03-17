#pragma once

#include "oui_window.h"

namespace oui
{

    class CConsoleApp
    {
        std::shared_ptr<CWindowsPool> m_pool;
    public:
        CConsoleApp();
        void Loop(std::shared_ptr<CWindow> rootWnd);
    };
}