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
        oui::CConsoleInputReader reader;

        std::vector<InputEvent> data;

        mainConsole.Init();

        {
            auto size = mainConsole.GetSize();
            InputEvent initialEvent;
            initialEvent.resizeEvent.valid = true;
            initialEvent.resizeEvent.newWidth = size.width;
            initialEvent.resizeEvent.newHeight = size.height;
            rootWindow->ProcessEvent(initialEvent);
        }
        DrawParameters parameters;
        for (; !m_pool->IsExitRequested(); )
        {
            parameters.rect.size = mainConsole.GetSize();
            parameters.console.StartDraw(parameters.rect.size, &mainConsole);

            rootWindow->DrawTo(parameters);

            if (!m_pool->GetFocus())
            {
                mainConsole.HideCursor();
            }

            parameters.console.FinishDraw();

            if (!reader.Read(data))
            {
                return;
            }
            // process event
            for (auto& evt : data)
            {
                if (evt.resizeEvent.valid)
                {
                    mainConsole.FixupAfterResize();
                }
                rootWindow->ProcessEvent(evt);
            }
        }
    }
}