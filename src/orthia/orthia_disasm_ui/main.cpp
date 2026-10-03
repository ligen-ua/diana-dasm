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
#include "diana_win32.h"
}
#include "orthia_resources.h"
#include "resource.h"
#include "orthia_files.h"
#include "orthia_processes_ex.h"
#include "console_mode.h"
#include "ui_help.h"
#include "orthia_version.h"
#include "oui_sandbox_win32.h"

int RunTests();

static void PrintUsage(std::ostream& out)
{
    out << "Usage: orthia [options]\n";
    out << "\n";
    out << "Options:\n";
    out << "  --file <filename>   open the given executable file\n";
    out << "  --pid <pid>         open the process with the given id (\"self\" for this process)\n";
    out << "  --cmd <command>     run <command> without UI and exit\n";
    out << "                      (repeatable, one command per --cmd, executed in order)\n";
    out << "  --analyze           with --cmd: deep code analysis and symbol loading on open\n";
    out << "                      (default: headers, modules, imports and exports only)\n";
    out << "  --no-sandbox        don't restrict orthia (needed for orthia_proc_win32.dll)\n";
    out << "  --run-tests         run the built-in tests and exit\n";
    out << "  -h, --help, /?      show this help and exit\n";
    out << "  --version           show the version and exit\n";
    out << "\n";
    out << "Without --cmd the UI is started with the given files and processes opened.\n";
    out << "--file and --pid are repeatable in UI mode, --cmd requires exactly one of them,\n";
    out << "except for the data folder commands, which run without a target:\n";
    out << "  orthia --cmd \".database list\"   (also: delete <sha1 prefix|pid|file>, cleanup)\n";
    out << "\n";
    out << "Exit codes:\n";
    out << "  0  success\n";
    out << "  1  at least one command reported an error\n";
    out << "  2  bad or incomplete argument, or the UI started without a console\n";
    out << "  3  target failed to open, or no target given for a command that needs one\n";
    out << "  4  unexpected error\n";
    out << "\n";
    out << "Environment:\n";
    out << "  ORTHIA_HOME         data folder to use instead of %APPDATA%\\Orthia\n";
    out << "  ORTHIA_SYMBOL_PATH  symbol folders separated by ';' (default C:\\Sym;C:\\Symbols)\n";
    out << "\n";
    out << "Commands (UI command window and --cmd):\n";
    for (const auto& line : orthia::GetCommandReference("  "))
    {
        out << line << "\n";
    }
}

static bool IsHelpSwitch(const wchar_t* arg)
{
    // "/help" and "/h" are not accepted: they are valid root-relative paths
    return wcscmp(arg, L"--help") == 0 ||
           wcscmp(arg, L"-h") == 0 ||
           wcscmp(arg, L"/?") == 0 ||
           wcscmp(arg, L"-?") == 0;
}


#if defined(_M_AMD64)

void UpdateHostFile(const std::wstring& targetExe)
{
    orthia::CResource resource;
    resource.Load(orthia::GetCurrentModule(), ORTHIA_WIN32_HOST, L"BINARY");

    orthia::CFile file;
    file.Open(targetExe, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_ALWAYS);
    file.WriteToFile(resource.data(), resource.size());
    file.FlushBuffers();
}

void SetupWin32FSHandlers(std::shared_ptr<orthia::CConfigOptionsStorage> config)
{
    const std::wstring hostFile(L"orthia_win32_host.exe");

    auto bin = config->GetBinFolder();
    orthia::EraseLastSlash(bin);
    auto targetExe = bin + L"\\" + hostFile;

    try
    {
        UpdateHostFile(targetExe);
    }
    catch (std::exception& e)
    {
        ORTHIA_DEV_LOG(orthia::LogSeverity::Error, e.what());
    }
    
    oui::SetupWin32DllLookupHandler([=](const oui::String& name) -> std::tuple<int, oui::String> {

        try
        {
            if (!orthia::IsFileExists(targetExe))
            {
                UpdateHostFile(targetExe);
            }

            orthia::CProcessParams params(targetExe, true); 
            params << L"search";
            params << name.native;
            oui::String result;
            orthia::intrusive_ptr<orthia::CConsoleProcess> process = StartConsoleProcess(params,
                nullptr,
                true,
                64*1024,
                nullptr,
                nullptr);
            process->PerformAll([&](const std::wstring& line) { 

                result.native.append(line);
            });
            process->Join();
            auto code = process->GetExitCode();
            if (code == 0 && result.native.empty())
            {
                code = ERROR_INTERNAL_ERROR;
            }
            return { (int)code, result };
        }
        catch (orthia::CWin32Exception& e)
        {
            return { e.GetErrorCode(), oui::String()};
        }
        catch (std::exception& e)
        {
            &e;
            return { ERROR_FILE_NOT_FOUND, oui::String() };
        }
    });
}
#endif //  M_AMD64


int wmain(int argc, const wchar_t* argv[])
{
    orthia::intrusive_ptr<orthia::ILowLevelLog> lowLevelLog(new orthia::CDebugOutputLog());
    orthia::DefLog_Init(new orthia::CProgramLog(lowLevelLog));

    ORTHIA_DEV_LOG(orthia::LogSeverity::Info, "Logging enabled");

    // MessageBox(0, 0, 0, 0);
    std::vector<std::wstring> filenamesToOpen;
    std::vector<unsigned long long> processesToOpen;
    std::vector<std::wstring> commandsToRun;
    bool analyze = false;

    try
    {
        bool nextIsPid = false;
        bool nextIsCmd = false;
        bool nextIsFile = false;
        bool sandbox = true;
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
                std::wstring text = argv[i];
                unsigned long long pid = 0;
                if (text == L"self")
                {
                    pid = GetCurrentProcessId();
                }
                else if (!orthia::ParsePidArgument(text, &pid))
                {
                    std::cerr << "Invalid value for --pid: " << orthia::ToAnsiString_Silent(text)
                              << " (expected a decimal or 0x-prefixed hex number, or \"self\")\n\n";
                    PrintUsage(std::cerr);
                    return orthia::consoleExit_Usage;
                }
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
            if (wcscmp(argv[i], L"--cmd") == 0)
            {
                nextIsCmd = true;
                continue;
            }
            if (wcscmp(argv[i], L"--file") == 0)
            {
                nextIsFile = true;
                continue;
            }
            if (wcscmp(argv[i], L"--analyze") == 0)
            {
                // the UI always analyzes, so it only matters with --cmd
                analyze = true;
                continue;
            }
            if (wcscmp(argv[i], L"--no-sandbox") == 0)
            {
                sandbox = false;
                continue;
            }
            if (IsHelpSwitch(argv[i]))
            {
                PrintUsage(std::cout);
                return orthia::consoleExit_Ok;
            }
            if (wcscmp(argv[i], L"--version") == 0)
            {
                std::cout << "orthia " ORTHIA_UI_VERSION "\n";
                return orthia::consoleExit_Ok;
            }
            if (wcsncmp(argv[i], L"--", 2) == 0)
            {
                std::cerr << "Unknown option: " << orthia::ToAnsiString_Silent(argv[i]) << "\n\n";
                PrintUsage(std::cerr);
                return orthia::consoleExit_Usage;
            }
            std::cerr << "Unexpected argument: " << orthia::ToAnsiString_Silent(argv[i])
                      << " (use --file <filename> to open a file)\n\n";
            PrintUsage(std::cerr);
            return orthia::consoleExit_Usage;
        }
        if (nextIsPid || nextIsCmd || nextIsFile)
        {
            std::cerr << "Missing value for " << orthia::ToAnsiString_Silent(argv[argc - 1]) << "\n\n";
            PrintUsage(std::cerr);
            return orthia::consoleExit_Usage;
        }

        const bool consoleMode = !commandsToRun.empty();
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

        auto config = orthia::InitAppCore();

        DianaWin32_Init();

#if defined(_M_AMD64)
        SetupWin32FSHandlers(config);
#endif //  M_AMD64

        // after the executable heap and the host exe, before the first process open loads the plugin
        if (sandbox)
        {
            std::vector<std::string> warnings;
            const bool signedOnly = orthia::ApplySandbox(&warnings);
            const bool elevated = orthia::IsElevated();
            for (const auto& warning : warnings)
            {
                // only elevated runs depend on it
                if (elevated)
                {
                    std::cerr << "Warning: " << warning << "\n";
                }
                else
                {
                    ORTHIA_DEV_LOG(orthia::LogSeverity::Info, warning);
                }
            }
            if (signedOnly && orthia::IsFileExists(orthia::GetCurrentModuleDir() + L"orthia_proc_win32.dll"))
            {
                std::cerr << "Warning: orthia_proc_win32.dll is not loaded in the sandbox;"
                             " run with --no-sandbox to use it\n";
            }
        }
        else if (orthia::IsElevated())
        {
            std::cerr << "Warning: running elevated with no sandbox (--no-sandbox)\n";
        }

        auto programModel = std::make_shared<orthia::CProgramModel>(config);

        if (consoleMode)
        {
            const int result = orthia::RunConsoleMode(programModel,
                { commandsToRun, filenamesToOpen, processesToOpen, analyze });
            programModel.reset();
            return result;
        }

        oui::CConsoleApp app;

        // create root windows
        auto rootWindow = std::make_shared<CMainWindow>(programModel);
        programModel->SubscribeUI(rootWindow);

        oui::ScopedGuard handlerGuard([&]() {
            programModel->UnsubscribeUI(rootWindow);
            programModel->Stop();
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
