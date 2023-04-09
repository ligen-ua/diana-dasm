#include "ui_main_window.h"
#include <iostream>

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;
void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);
int RunTests();

int wmain(int argc, const wchar_t* argv[])
{
   // MessageBox(0, 0, 0, 0);

    if (argc == 2)
    {
        if (wcscmp(argv[1], L"--run-tests") == 0)
        {
            return RunTests();
        }
    }
    std::cout << "Welcome to Orthia Disasm\n\n";
    try
    {
        orthia::CProgramModel programModel;
        std::cout.flush();

        g_textManager = new orthia::CTextManager();
        InitLanguage_EN(g_textManager);

        oui::CConsoleApp app;

        auto rootWindow = std::make_shared<CMainWindow>();
        app.Loop(rootWindow);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << "\n";
    }
    return 0;
}
