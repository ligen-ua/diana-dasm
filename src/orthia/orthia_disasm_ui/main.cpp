#include "ui_main_window.h"
#include <iostream>

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;
void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);
int RunTests();

static void PrintUsage()
{
    std::cout << "Usage: [--run-tests] <filename>\n";
}
int wmain(int argc, const wchar_t* argv[])
{
    // MessageBox(0, 0, 0, 0);
    std::vector<std::wstring> filenamesToOpen;
    if (argc == 2)
    {
        if (wcscmp(argv[1], L"--run-tests") == 0)
        {
            return RunTests();
        }
        filenamesToOpen.push_back(argv[1]);
    }
    else
    {
        if (argc > 2)
        {
            // there is no support of that currently
            PrintUsage();
            return 1;
        }
    }

    std::cout << "Welcome to Orthia Disasm\n\n";
    std::cout.flush();

    try
    {
        auto programModel = std::make_shared<orthia::CProgramModel>();

        g_textManager = new orthia::CTextManager();
        InitLanguage_EN(g_textManager);

        oui::CConsoleApp app;

        // create root windows
        auto rootWindow = std::make_shared<CMainWindow>(programModel);
#if 0
        rootWindow->AddInitialTextOutputInfo(L"this");
        rootWindow->AddInitialTextOutputInfo(L"is");
        rootWindow->AddInitialTextOutputInfo(L"test");
        rootWindow->AddInitialTextOutputInfo(L"data");
        rootWindow->AddInitialTextOutputInfo(L"yep");
        rootWindow->AddInitialTextOutputInfo(L"it is");
        rootWindow->AddInitialTextOutputInfo(L"a");
        rootWindow->AddInitialTextOutputInfo(L"test");
#endif

        // pass arguments
        for (auto& name : filenamesToOpen)
        {
            int platformError = 0;
            std::shared_ptr<oui::IFile> file;
            std::tie(platformError, file) = programModel->GetFileSystem()->SyncOpenFile(oui::FileUnifiedId(name));
            rootWindow->AddInitialArgument({platformError, name, file});
        }

        app.Loop(rootWindow);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << "\n";
    }
    return 0;
}
