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

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;
void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);
int RunTests();

static void PrintUsage()
{
    std::cout << "Usage: [--run-tests] <filename>\n";
    std::cout << "       --pid <pid-to-open>\n";
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

    try
    {
        bool nextIsPid = false;
        for (int i = 1; i < argc; ++i)
        {
            if (nextIsPid)
            {
                std::wstring text = argv[i];
                unsigned long long pid = 0;
                if (text == L"self")
                {
                    pid = GetCurrentProcessId();
                }
                else
                {
                    orthia::StringToObject(text, &pid);
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
        oui::EditBox_SetContextMenuLabelsProvider([&]() {
            auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.editbox.contextmenu"));
            return std::make_tuple(
                node->QueryValue(ORTHIA_TCSTR("cut")),
                node->QueryValue(ORTHIA_TCSTR("copy")),
                node->QueryValue(ORTHIA_TCSTR("paste"))
            );
        });

        auto config = std::make_shared<orthia::CConfigOptionsStorage>();
        config->Init();

        Diana_Init();
        DianaProcessor_GlobalInit();
        DianaWin32_Init();

#if defined(_M_AMD64)
        SetupWin32FSHandlers(config);
#endif //  M_AMD64

        auto programModel = std::make_shared<orthia::CProgramModel>(config);
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
