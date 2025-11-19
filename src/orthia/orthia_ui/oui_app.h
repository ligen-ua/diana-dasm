#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{

    class CConsoleApp
    {
        std::shared_ptr<CWindowsPool> m_pool;
        void FinalCleanup(oui::CConsole& mainConsole);

    public:
        CConsoleApp();
        ~CConsoleApp();
        void Loop(std::shared_ptr<CWindow> rootWnd);
        void DoMainLoop(std::shared_ptr<CWindow> rootWindow, oui::CConsole& mainConsole);
    };
}