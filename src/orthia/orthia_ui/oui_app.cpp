#include "oui_app.h"
#include "oui_console.h"
#include "oui_input.h"

namespace oui
{
    CConsoleApp::CConsoleApp()
    {
    }
    void CConsoleApp::Loop(std::shared_ptr<CWindow> rootWindow)
    {
        rootWindow->Init(m_pool);

        oui::CConsole mainConsole;
        oui::CConsoleInputReader reader;

        std::vector<InputEvent> data;

        rootWindow->Resize(mainConsole.GetSize());
        for (;;)
        {
            Rect rect;
            rect.size = mainConsole.GetSize();
            rootWindow->DrawTo(rect, mainConsole);

            if (!reader.Read(data))
            {
                return;
            }
            // process event
            for (auto& evt : data)
            {
                rootWindow->ProcessEvent(evt);
            }
        }
    }
}