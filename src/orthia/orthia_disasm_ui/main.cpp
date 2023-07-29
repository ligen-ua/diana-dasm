#include "ui_main_window.h"
#include "orthia_config.h"
#include <iostream>
#include "diana_core_cpp.h"
extern "C"
{
#include "diana_processor/diana_processor_core.h"
#include "diana_win32.h"
}

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;
void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);
int RunTests();

static void PrintUsage()
{
    std::cout << "Usage: [--run-tests] <filename>\n";
}
typedef int FIRMWARE_TYPE;
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
        g_textManager = new orthia::CTextManager();
        InitLanguage_EN(g_textManager);

        auto config = std::make_shared<orthia::CConfigOptionsStorage>();
        config->Init();

        Diana_Init();
        DianaProcessor_GlobalInit();
        DianaWin32_Init();

        auto programModel = std::make_shared<orthia::CProgramModel>(config);
        oui::CConsoleApp app;

        // create root windows
        auto rootWindow = std::make_shared<CMainWindow>(programModel);
        programModel->SubscribeUI(rootWindow);

        oui::ScopedGuard handlerGuard([&]() {
            programModel->UnsubscribeUI(rootWindow);
        });
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
