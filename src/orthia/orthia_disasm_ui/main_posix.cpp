#include "orthia_core.h"
#include "ui_main_window.h"
#include "orthia_config.h"
#include <iostream>
#include "diana_core_cpp.h"
#include "orthia_log.h"

extern "C"
{
#include "diana_processor/diana_processor_core.h"
}
#include "orthia_files.h"
#include <unistd.h>

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;
void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);
int RunTests();

static void PrintUsage()
{
    std::cout << "Usage: [--run-tests]\n";
    std::cout << "       --pid <pid-to-open>\n";
}

int main(int argc, const char* argv[])
{
    orthia::intrusive_ptr<orthia::ILowLevelLog> lowLevelLog(new orthia::CDebugOutputLog());
    orthia::DefLog_Init(new orthia::CProgramLog(lowLevelLog));

    ORTHIA_DEV_LOG(orthia::LogSeverity::Info, "Logging enabled");

    std::vector<unsigned long long> processesToOpen;

    try
    {
        bool nextIsPid = false;
        for (int i = 1; i < argc; ++i)
        {
            if (nextIsPid)
            {
                std::string text = argv[i];
                unsigned long long pid = 0;
                if (text == "self")
                {
                    pid = (unsigned long long)getpid();
                }
                else
                {
                    orthia::StringToObject(text, &pid);
                }
                processesToOpen.push_back(pid);
                nextIsPid = false;
                continue;
            }
            if (strcmp(argv[i], "--run-tests") == 0)
            {
                return RunTests();
            }
            if (strcmp(argv[i], "--pid") == 0)
            {
                nextIsPid = true;
                continue;
            }
            if (strncmp(argv[i], "--", 2) == 0)
            {
                PrintUsage();
                return 1;
            }
            std::cerr << "File mode is not supported in this build\n";
            return 1;
        }

        std::cout << "Welcome to Orthia Disasm\n\n";
        std::cout.flush();

        g_textManager = new orthia::CTextManager();
        InitLanguage_EN(g_textManager);

        auto config = std::make_shared<orthia::CConfigOptionsStorage>();
        config->Init();

        Diana_Init();
        DianaProcessor_GlobalInit();

        auto programModel = std::make_shared<orthia::CProgramModel>(config);
        oui::CConsoleApp app;

        auto rootWindow = std::make_shared<CMainWindow>(programModel);
        programModel->SubscribeUI(rootWindow);

        oui::ScopedGuard handlerGuard([&]() {
            programModel->UnsubscribeUI(rootWindow);
        });

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
