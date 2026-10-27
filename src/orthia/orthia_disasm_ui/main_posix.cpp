#include "orthia_core.h"
#include "ui_main_window.h"
#include "oui_editbox.h"
#include "orthia_config.h"
#include <iostream>
#include <optional>
#include "diana_core_cpp.h"
#include "orthia_log.h"

extern "C"
{
#include "diana_processor/diana_processor_core.h"
}
#include "orthia_files.h"
#include "console_mode.h"
#include "ui_help.h"
#include "orthia_version.h"
#include "oui_privileges_posix.h"
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
    out << "  --proc-folder <folder|pid>\n";
    out << "                    open a saved process folder (proc/...) after the process\n";
    out << "                    exited: a path, its data.db, or a pid or name of .database list\n";
    out << "  --cmd <command>   run <command> without UI and exit\n";
    out << "                    (repeatable, one command per --cmd, executed in order)\n";
    out << "  --analyze         with --cmd: deep code analysis and symbol loading on open\n";
    out << "                    (default: headers, modules, imports and exports only)\n";
    out << "  --format <format> with --cmd: open the --file as pe, elf or raw (code without\n";
    out << "                    headers) instead of the format it is detected as\n";
    out << "  --base <address>  with --format raw: where the code is placed\n";
    out << "  --arch <arch>     with --format raw: how it is disassembled, x86, x64,\n";
    out << "                    arm64, arm or thumb\n";
    out << "                    (the UI asks for these when it doesn't recognize a file)\n";
    out << "  --capture         save the memory of each analyzed module of a process into\n";
    out << "                    its folder (deep mode: see .capture)\n";
    out << "  --overwrite-db    new, empty databases for the targets opened in this run\n";
    out << "                    (default: reuse the database of an earlier open)\n";
    out << "  --no-sandbox     no privilege drop under sudo and no syscall filter\n";
    out << "  --run-tests       run the built-in tests and exit\n";
    out << "  -h, --help        show this help and exit\n";
    out << "  --version         show the version and exit\n";
    out << "\n";
    out << "Without --cmd the UI is started with the given files and processes opened.\n";
    out << "--file and --pid are repeatable in UI mode, --cmd requires exactly one of them,\n";
    out << "except for the data folder commands, which run without a target:\n";
    out << "  " << programName << " --cmd \".database list\"   (also: delete <sha1 prefix|pid|file>, cleanup)\n";
    out << "\n";
    out << "Exit codes:\n";
    out << "  0  success\n";
    out << "  1  at least one command reported an error\n";
    out << "  2  bad or incomplete argument, or the UI started without a console\n";
    out << "  3  target failed to open, or no target given for a command that needs one\n";
    out << "  4  unexpected error\n";
    out << "\n";
    out << "Environment:\n";
    out << "  ORTHIA_HOME       data folder to use instead of $XDG_DATA_HOME/Orthia\n";
    out << "  ORTHIA_SYMBOL_PATH symbol folders separated by ';' (default ~/sym;~/symbols)\n";
    out << "\n";
    out << "Commands (UI command window and --cmd):\n";
    for (const auto& line : orthia::GetCommandReference("  "))
    {
        out << line << "\n";
    }
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
    std::vector<std::string> procFoldersToOpen;
    std::vector<std::string> commandsToRun;
    const std::string programName = GetProgramName(argc > 0 ? argv[0] : nullptr);
    bool analyze = false;
    bool overwriteDatabases = false;
    bool captureModules = false;

    try
    {
        bool nextIsPid = false;
        bool nextIsCmd = false;
        bool nextIsFile = false;
        bool nextIsProcFolder = false;
        bool dropPrivileges = true;
        std::optional<std::string> formatArg, baseArg, archArg;
        std::optional<std::string>* nextValue = nullptr;      // --format, --base, --arch
        for (int i = 1; i < argc; ++i)
        {
            // checked first, so that a value starting with -- is taken as a value
            if (nextValue)
            {
                *nextValue = argv[i];
                nextValue = nullptr;
                continue;
            }
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
            if (nextIsProcFolder)
            {
                procFoldersToOpen.push_back(argv[i]);
                nextIsProcFolder = false;
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
                else if (!orthia::ParsePidArgument(text, &pid))
                {
                    std::cerr << "Invalid value for --pid: " << text
                              << " (expected a decimal or 0x-prefixed hex number, or \"self\")\n\n";
                    PrintUsage(std::cerr, programName);
                    return orthia::consoleExit_Usage;
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
            if (strcmp(argv[i], "--proc-folder") == 0)
            {
                nextIsProcFolder = true;
                continue;
            }
            if (strcmp(argv[i], "--file") == 0)
            {
                nextIsFile = true;
                continue;
            }
            if (strcmp(argv[i], "--format") == 0)
            {
                nextValue = &formatArg;
                continue;
            }
            if (strcmp(argv[i], "--base") == 0)
            {
                nextValue = &baseArg;
                continue;
            }
            if (strcmp(argv[i], "--arch") == 0)
            {
                nextValue = &archArg;
                continue;
            }
            if (strcmp(argv[i], "--analyze") == 0)
            {
                // the UI always analyzes, so it only matters with --cmd
                analyze = true;
                continue;
            }
            if (strcmp(argv[i], "--capture") == 0)
            {
                captureModules = true;
                continue;
            }
            if (strcmp(argv[i], "--overwrite-db") == 0)
            {
                overwriteDatabases = true;
                continue;
            }
            // --no-privilege-drop: the old name, kept as an undocumented alias
            if (strcmp(argv[i], "--no-sandbox") == 0 || strcmp(argv[i], "--no-privilege-drop") == 0)
            {
                // keeps full root rights and no sandbox
                dropPrivileges = false;
                continue;
            }
            if (IsHelpSwitch(argv[i]))
            {
                PrintUsage(std::cout, programName);
                return orthia::consoleExit_Ok;
            }
            if (strcmp(argv[i], "--version") == 0)
            {
                std::cout << "orthia " ORTHIA_UI_VERSION "\n";
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
        if (nextIsPid || nextIsCmd || nextIsFile || nextIsProcFolder || nextValue)
        {
            std::cerr << "Missing value for " << argv[argc - 1] << "\n\n";
            PrintUsage(std::cerr, programName);
            return orthia::consoleExit_Usage;
        }

        orthia::FileFormatOptions fileFormat;
        {
            std::string error;
            if (!orthia::ParseFileFormatOptions(formatArg ? &*formatArg : nullptr,
                baseArg ? &*baseArg : nullptr,
                archArg ? &*archArg : nullptr,
                fileFormat,
                error))
            {
                std::cerr << error << "\n\n";
                PrintUsage(std::cerr, programName);
                return orthia::consoleExit_Usage;
            }
        }

        const bool consoleMode = !commandsToRun.empty();
        if (!consoleMode && !fileFormat.format.empty())
        {
            std::cerr << "--format needs --cmd (the UI asks how to open a file it doesn't recognize)\n\n";
            PrintUsage(std::cerr, programName);
            return orthia::consoleExit_Usage;
        }
        if (!consoleMode)
        {
            if (!orthia::HasInteractiveConsole())
            {
                std::cerr << "The UI needs an interactive console (stdin/stdout are redirected);"
                             " use --cmd to run commands without the UI\n";
                return orthia::consoleExit_Usage;
            }
            // would pollute the command output otherwise
            std::cout << "Welcome to Orthia Disasm\n\n";
            std::cout.flush();
        }

        if (dropPrivileges)
        {
            // before InitAppCore: the data folder is created as the user that ran sudo
            orthia::DropRootPrivileges();
        }
        else if (geteuid() == 0)
        {
            std::cerr << "Warning: running with full root rights and no sandbox (--no-sandbox)\n";
        }

        auto config = orthia::InitAppCore();

        if (dropPrivileges)
        {
            // still before the model starts its threads: both apply to the calling thread only
            std::vector<std::string> warnings;
            orthia::ApplySandbox(&warnings);
            for (const auto& warning : warnings)
            {
                // only root runs depend on it
                if (orthia::GetPrivilegeState().startedAsRoot)
                {
                    std::cerr << "Warning: " << warning << "\n";
                }
                else
                {
                    ORTHIA_DEV_LOG(orthia::LogSeverity::Info, warning);
                }
            }
        }

        config->SetOverwriteDatabases(overwriteDatabases);
        config->SetCaptureModules(captureModules);
        auto programModel = std::make_shared<orthia::CProgramModel>(config);

        if (consoleMode)
        {
            const int result = orthia::RunConsoleMode(programModel,
                { commandsToRun, filenamesToOpen, processesToOpen, procFoldersToOpen, analyze, fileFormat });
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

        rootWindow->AddInitialTargets(filenamesToOpen, processesToOpen, procFoldersToOpen);
        app.Loop(rootWindow);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << "\n";
        return orthia::consoleExit_Exception;
    }
    return 0;
}
