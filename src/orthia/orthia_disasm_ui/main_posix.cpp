#include "orthia_core.h"
#include "ui_main_window.h"
#include "oui_editbox.h"
#include "orthia_config.h"
#include <iostream>
#include "diana_core_cpp.h"
#include "orthia_log.h"

extern "C"
{
#include "diana_processor/diana_processor_core.h"
}
#include "orthia_files.h"
#include "console_mode.h"
#include <unistd.h>

int RunTests();

static std::string GetProgramName(const char* argv0)
{
    if (!argv0 || !*argv0)
    {
        return "orthia";
    }
    const char* slash = strrchr(argv0, '/');
    return slash ? slash + 1 : argv0;
}

static void PrintUsage(std::ostream& out, const std::string& programName)
{
    out << "Usage: " << programName << " [options]\n";
    out << "\n";
    out << "Options:\n";
    out << "  --file <filename> open the given executable file\n";
    out << "  --pid <pid>       open the process with the given id (\"self\" for this process)\n";
    out << "  --cmd <command>   run <command> without UI and exit\n";
    out << "                    (repeatable, one command per --cmd, executed in order)\n";
    out << "  --run-tests       run the built-in tests and exit\n";
    out << "  -h, --help        show this help and exit\n";
    out << "\n";
    out << "Without --cmd the UI is started with the given files and processes opened.\n";
    out << "--file and --pid are repeatable in UI mode, --cmd requires exactly one of them.\n";
    out << "\n";
    out << "Exit codes (--cmd mode):\n";
    out << "  0  success\n";
    out << "  1  at least one command reported an error\n";
    out << "  2  bad or incomplete argument\n";
    out << "  3  target failed to open, or no target given\n";
    out << "  4  unexpected error\n";
}

static bool IsHelpSwitch(const char* arg)
{
    return strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0;
}

int main(int argc, const char* argv[])
{
    orthia::intrusive_ptr<orthia::ILowLevelLog> lowLevelLog(new orthia::CDebugOutputLog());
    orthia::DefLog_Init(new orthia::CProgramLog(lowLevelLog));

    ORTHIA_DEV_LOG(orthia::LogSeverity::Info, "Logging enabled");

    std::vector<unsigned long long> processesToOpen;
    std::vector<std::string> filenamesToOpen;
    std::vector<std::string> commandsToRun;
    const std::string programName = GetProgramName(argc > 0 ? argv[0] : nullptr);

    try
    {
        bool nextIsPid = false;
        bool nextIsCmd = false;
        bool nextIsFile = false;
        for (int i = 1; i < argc; ++i)
        {
            // checked first, so that a value starting with -- is taken as a value
            if (nextIsCmd)
            {
                commandsToRun.push_back(argv[i]);
                nextIsCmd = false;
                continue;
            }
            if (nextIsFile)
            {
                filenamesToOpen.push_back(argv[i]);
                nextIsFile = false;
                continue;
            }
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
            if (strcmp(argv[i], "--cmd") == 0)
            {
                nextIsCmd = true;
                continue;
            }
            if (strcmp(argv[i], "--file") == 0)
            {
                nextIsFile = true;
                continue;
            }
            if (IsHelpSwitch(argv[i]))
            {
                PrintUsage(std::cout, programName);
                return orthia::consoleExit_Ok;
            }
            if (strncmp(argv[i], "--", 2) == 0)
            {
                std::cerr << "Unknown option: " << argv[i] << "\n\n";
                PrintUsage(std::cerr, programName);
                return orthia::consoleExit_Usage;
            }
            std::cerr << "Unexpected argument: " << argv[i]
                      << " (use --file <filename> to open a file)\n\n";
            PrintUsage(std::cerr, programName);
            return orthia::consoleExit_Usage;
        }
        if (nextIsPid || nextIsCmd || nextIsFile)
        {
            std::cerr << "Missing value for " << argv[argc - 1] << "\n\n";
            PrintUsage(std::cerr, programName);
            return orthia::consoleExit_Usage;
        }

        const bool consoleMode = !commandsToRun.empty();
        if (!consoleMode)
        {
            // would pollute the command output otherwise
            std::cout << "Welcome to Orthia Disasm\n\n";
            std::cout.flush();
        }

        auto config = orthia::InitAppCore();

        auto programModel = std::make_shared<orthia::CProgramModel>(config);

        if (consoleMode)
        {
            const int result = orthia::RunConsoleMode(programModel,
                { commandsToRun, filenamesToOpen, processesToOpen });
            programModel.reset();
            return result;
        }

        oui::CConsoleApp app;

        auto rootWindow = std::make_shared<CMainWindow>(programModel);
        programModel->SubscribeUI(rootWindow);

        oui::ScopedGuard handlerGuard([&]() {
            programModel->UnsubscribeUI(rootWindow);
            programModel->Stop();
        });

        rootWindow->AddInitialTargets(filenamesToOpen, processesToOpen);
        app.Loop(rootWindow);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << "\n";
        return orthia::consoleExit_Exception;
    }
    return 0;
}
