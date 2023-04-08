#include "oui_app.h"
#include "oui_console.h"
#include "oui_input.h"
#include <iostream>

namespace oui
{
    CConsoleApp::CConsoleApp()
    {
        m_pool = std::make_shared<CWindowsPool>();
    }
    CConsoleApp::~CConsoleApp()
    {

    }
    void CConsoleApp::FinalCleanup(oui::CConsole& mainConsole)
    {
        // cleanup data
        Rect rect;
        rect.size = mainConsole.GetSize();
        mainConsole.PaintRect(rect, oui::ColorBlack(), false);
    }

    void CConsoleApp::Loop(std::shared_ptr<CWindow> rootWindow)
    {
        m_pool->RegisterRootWindow(rootWindow);
        rootWindow->Init(m_pool);

        oui::CConsole mainConsole;
        try
        {
            oui::CConsoleStateSaver stateSaver;

            try
            {
                DoMainLoop(rootWindow, mainConsole);
            }
            catch(...)
            {
                FinalCleanup(mainConsole);
                throw;
            }
            FinalCleanup(mainConsole);

            //std::cout << "\n";
        }
        catch (const std::exception& err)
        {
            std::cerr << "Error: " << err.what() << "\n";
        }
    }

    void CConsoleApp::DoMainLoop(std::shared_ptr<CWindow> rootWindow, oui::CConsole &mainConsole)
    {
        mainConsole.HideCursor();

        oui::CConsoleInputReader reader;

        std::vector<InputEvent> data;

        mainConsole.Init();

        auto consoleSize = mainConsole.GetSize();
        InputEvent initialEvent;
        initialEvent.resizeEvent.valid = true;
        initialEvent.resizeEvent.newWidth = consoleSize.width;
        initialEvent.resizeEvent.newHeight = consoleSize.height;
        WindowEventContext evtContext;
        rootWindow->ProcessEvent(initialEvent, evtContext);
        
        DrawParameters parameters;
        bool forceRedrewNext = true;
        for (; !m_pool->IsExitRequested(); )
        {
            Rect rect;
            rect.size = mainConsole.GetSize();
            parameters.console.StartDraw(rect.size, &mainConsole);

            rootWindow->DrawTo(rect, parameters, forceRedrewNext);
            parameters.console.FinishDraw();
            forceRedrewNext = false;

            if (!reader.Read(data))
            {
                return;
            }
            // process event
            std::shared_ptr<CWindow> mouseHandler;
            for (auto& evt : data)
            {

                // check drag logic
                if (m_pool->HandleDragEvent(evt))
                {
                    continue;
                }
                // go regular event loop
                WindowEventContext evtContext;
                evtContext.onMouseEventCallback = [&](auto wnd) {
                    if (!mouseHandler)
                    {
                        mouseHandler = wnd;
                    }
                };

                if (evt.resizeEvent.valid)
                {
                    mainConsole.FixupAfterResize();
                }
                rootWindow->ProcessEvent(evt, evtContext);

                // generate on mouse leave
                if (mouseHandler)
                {
                    auto prev = m_pool->GetLastMouseWindow();
                    if (mouseHandler != prev)
                    {
                        m_pool->SetLastMouseWindow(mouseHandler);
                        if (prev)
                        {
                            prev->OnMouseLeave();
                        }
                        mouseHandler->OnMouseEnter();
                    }
                }

                // check focus
                if (evt.focusEvent.valid)
                {
                    if (evt.focusEvent.focusSet)
                    {
                        forceRedrewNext = true;
                    }
                    else
                    {
                        m_pool->CancelDragEvent();
                    }
                }
            }
        }
    }
}