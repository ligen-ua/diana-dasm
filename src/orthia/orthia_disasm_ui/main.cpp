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
int wmain(int argc, const wchar_t* argv[])
{
    // MessageBox(0, 0, 0, 0);
    std::vector<std::wstring> filenamesToOpen;
    std::vector<unsigned long long> processesToOpen;

    try
    {
        bool nextIsPid = false;
        for (int i = 1; i < argc; ++i)
        {
            if (nextIsPid)
            {
                unsigned long long pid = 0;
                orthia::StringToObject(std::wstring(argv[i]), &pid);
                processesToOpen.push_back(pid);
                nextIsPid = false;
                continue;
            }
            if (wcscmp(argv[i], L"--run-tests") == 0)
            {
                return RunTests();
            }
            if (wcscmp(argv[i], L"--pid") == 0)
            {
                nextIsPid = true;
                continue;
            }
            if (wcsncmp(argv[i], L"--", 2) == 0)
            {
                PrintUsage();
                return 1;
            }
            filenamesToOpen.push_back(argv[i]);
        }

        std::cout << "Welcome to Orthia Disasm\n\n";
        std::cout.flush();

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
            std::shared_ptr<oui::IFile2> file;
            std::tie(platformError, file) = programModel->GetFileSystem()->SyncOpenFile(oui::FileUnifiedId(name));
            if (!file)
            {
                throw orthia::CWin32Exception("Can't open file: " + orthia::ToAnsiString_Silent(name), platformError);
            }
            rootWindow->AddInitialArgument({ platformError, name, file, nullptr });
        }
        for (auto& pid : processesToOpen)
        {
            int platformError = 0;
            std::shared_ptr<oui::IProcess> process;
            std::tie(platformError, process) = programModel->GetProcessSystem()->SyncOpenProcess(oui::ProcessUnifiedId(pid));
            if (!process)
            {
                throw orthia::CWin32Exception("Can't open process: " + orthia::ObjectToString_Ansi(pid), platformError);
            }
            auto uiName = process->GetFullFileNameForUI();
            rootWindow->AddInitialArgument({ platformError, uiName, nullptr, process });
        }
        app.Loop(rootWindow);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << "\n";
    }
    return 0;
}
