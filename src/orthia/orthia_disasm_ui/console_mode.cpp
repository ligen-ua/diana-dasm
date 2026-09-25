#include "console_mode.h"
#include "orthia_core.h"
#include "oui_editbox.h"
#include "orthia_commands.h"
#include "diana_core_cpp.h"
#include <cstdio>

extern "C"
{
#include "diana_processor/diana_processor_core.h"
}

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;
void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);

namespace orthia
{
    std::shared_ptr<CConfigOptionsStorage> InitAppCore()
    {
        g_textManager = new orthia::CTextManager();
        InitLanguage_EN(g_textManager);
        oui::EditBox_SetContextMenuLabelsProvider([]() {
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
        return config;
    }

namespace
{
    void WriteLine(FILE* stream, const PlatformString_type& text)
    {
#ifdef WIN32
        const std::string utf8 = orthia::PlatformStringToUtf8(text);
#else
        const std::string& utf8 = text;
#endif
        fwrite(utf8.data(), 1, utf8.size(), stream);
        fputc('\n', stream);
    }

    // Model and analyzer progress goes to stderr, so that stdout stays parseable.
    class CConsoleLog:public IUILogInterface
    {
    public:
        void WriteLog(const oui::String& line) override
        {
            std::vector<orthia::PlatformString_type> lines;
            orthia::SplitStringWithoutWhitespace(line.native, ORTHIA_TCSTR("\x0A"), &lines);
            for (auto& text : lines)
            {
                WriteLine(stderr, text);
            }
            fflush(stderr);
        }
    };

    // Drives an oui::CWindowThread from the main thread: everything the model, the
    // analyzer and the command processor reply through needs someone to drain the
    // task queue. Operation::Sync() blocks until that happens, so this is mandatory.
    class CHeadlessPump:oui::Noncopyable
    {
        std::shared_ptr<oui::CWindowThread> m_thread;
        oui::CEvent m_wakeup;
    public:
        CHeadlessPump()
            :
                m_thread(std::make_shared<oui::CWindowThread>()),
                m_wakeup(oui::EventType::Auto)
        {
            m_thread->SetWakeupHandler([this]() { m_wakeup.Set(); });
        }
        ~CHeadlessPump()
        {
            // Lifetime barrier: WakeUpUI() invokes the handler while holding
            // CWindowThread::m_handlerLock and SetWakeupHandler takes the same lock,
            // so once this returns no background thread can touch this object.
            m_thread->SetWakeupHandler(nullptr);
        }
        const std::shared_ptr<oui::CWindowThread>& GetThread() const { return m_thread; }

        void PumpOnce(unsigned int waitMs = 10)
        {
            // auto-reset event: a Set() arriving during the drain is not lost
            m_wakeup.Wait(waitMs);
            m_thread->GUI_ProcessTasks();
        }
        void PumpUntil(const std::function<bool()>& done)
        {
            while (!done())
            {
                PumpOnce();
            }
            m_thread->GUI_ProcessTasks();
        }
    };

    // Waits until nothing is queued or running in the command processor and the
    // analyzer. Chained work (EnqueueLoadSymbols -> EnqueueAnalyzePrivateSymbols) is
    // re-enqueued from inside the still-active pool task, and CThreadPool counts
    // active tasks, so the count never dips to zero across such a hand-off.
    void WaitUntilIdle(CHeadlessPump& pump, const std::shared_ptr<CProgramModel>& model)
    {
        auto idle = [&]() {
            return !model->GetCommandProcessor()->IsBusy() && !model->GetAnalyzer().IsBusy();
        };
        int consecutive = 0;
        while (consecutive < 2)
        {
            pump.PumpOnce();
            consecutive = idle() ? consecutive + 1 : 0;
        }
        pump.GetThread()->GUI_ProcessTasks();
        fflush(stderr);
        fflush(stdout);
    }

    // Mirrors CMainWindow::AsyncOpenFile/AsyncOpenProcess: the model call itself runs
    // on the file system pool, the completion handler comes back on the pump thread.
    template<class CompleteHandler_type, class Submit_type>
    bool OpenTargetAndWait(CHeadlessPump& pump,
        const std::shared_ptr<CProgramModel>& model,
        Submit_type submit)
    {
        struct State
        {
            bool done = false;
            oui::fsui::OpenResult result;
        };
        // shared_ptr and not stack references: the operation can outlive this frame
        auto state = std::make_shared<State>();

        auto handler = std::make_shared<oui::Operation<CompleteHandler_type>>(
            pump.GetThread(),
            [state](std::shared_ptr<oui::BaseOperation>, auto, const oui::fsui::OpenResult& result) {
                state->result = result;
                state->done = true;
            });

        submit(handler);
        pump.PumpUntil([state]() { return state->done; });

        if (!state->result.error.native.empty())
        {
            WriteLine(stderr, state->result.error.native);
            return false;
        }
        auto it = state->result.extraInfo.find(model_OpenResult_extraInfo_WorkspaceId);
        if (it == state->result.extraInfo.end())
        {
            WriteLine(stderr, ORTHIA_TCSTR("Open failed"));
            return false;
        }

        // The UI does this in CMainWindow::OnWorkspaceItemChanged: items are registered
        // inactive, so without this GetActiveItem() stays null and commands refuse to run.
        model->SetActiveItem(std::any_cast<int>(it->second));
        return true;
    }

    bool OpenFile(CHeadlessPump& pump,
        const std::shared_ptr<CProgramModel>& model,
        const PlatformString_type& name_in)
    {
        // on error the name is returned as given, SyncOpenFile reports the real problem
        const PlatformString_type name = std::get<1>(model->GetFileSystem()->SyncGetFullPathName(name_in)).native;
        int platformError = 0;
        std::shared_ptr<oui::IFile2> file;
        std::tie(platformError, file) = model->GetFileSystem()->SyncOpenFile(oui::FileUnifiedId(name));
        if (!file)
        {
            WriteLine(stderr, ORTHIA_TCSTR("Can't open file: ") + name);
            return false;
        }
        return OpenTargetAndWait<oui::fsui::FileCompleteHandler_type>(pump, model,
            [&](std::shared_ptr<oui::Operation<oui::fsui::FileCompleteHandler_type>> handler) {
                model->GetFileSystem()->AsyncExecute(pump.GetThread(), [file, model, handler]() {
                    model->AddExecutable(file, handler);
                });
            });
    }

    bool OpenProcess(CHeadlessPump& pump,
        const std::shared_ptr<CProgramModel>& model,
        unsigned long long pid)
    {
        int platformError = 0;
        std::shared_ptr<oui::IProcess> process;
        std::tie(platformError, process) = model->GetProcessSystem()->SyncOpenProcess(oui::ProcessUnifiedId(pid));
        if (!process)
        {
            PlatformString_type pidStr;
            orthia::ObjectToString_t(pid, pidStr);
            WriteLine(stderr, ORTHIA_TCSTR("Can't open process: ") + pidStr);
            return false;
        }
        return OpenTargetAndWait<oui::fsui::ProcessCompleteHandler_type>(pump, model,
            [&](std::shared_ptr<oui::Operation<oui::fsui::ProcessCompleteHandler_type>> handler) {
                model->GetFileSystem()->AsyncExecute(pump.GetThread(), [process, model, handler]() {
                    model->AddProcess(process, handler);
                });
            });
    }

    // Returns false when the command reported an error.
    bool RunOneCommand(CHeadlessPump& pump,
        const std::shared_ptr<CProgramModel>& model,
        const std::shared_ptr<IWorkPlaceItem>& item,
        const PlatformString_type& cmdText)
    {
        struct State
        {
            bool finished = false;
            bool failed = false;
        };
        auto state = std::make_shared<State>();

        auto progress = std::make_shared<oui::Operation<CCommandProcessor::ExecuteProgressHandler_type>>(
            pump.GetThread(),
            [state](std::shared_ptr<oui::BaseOperation>, const oui::String& text, bool finalText) {
                if (finalText)
                {
                    // a non-empty final reply carries the error text
                    if (!text.native.empty())
                    {
                        state->failed = true;
                        WriteLine(stdout, text.native);
                    }
                    state->finished = true;
                    return;
                }
                WriteLine(stdout, text.native);
            });

        auto uiCommandHandler = std::make_shared<oui::Operation<CCommandProcessor::SpecialUICommandHandler_type>>(
            pump.GetThread(),
            [](std::shared_ptr<oui::BaseOperation>, CCommandProcessor::SpecialUICommands) {
                // cls has no meaning here
            });

        model->GetCommandProcessor()->AsyncExecute(pump.GetThread(), progress, uiCommandHandler, cmdText, item, model);

        // Never cancel the progress operation: a command sitting in Operation::Sync()
        // would never wake up, and .reload would lose its symbol load.
        pump.PumpUntil([state]() { return state->finished; });
        WaitUntilIdle(pump, model);
        return !state->failed;
    }
}

    int RunConsoleMode(std::shared_ptr<CProgramModel> model, const ConsoleModeOptions& options)
    {
#ifdef WIN32
        ::SetConsoleOutputCP(CP_UTF8);
#endif
        CHeadlessPump pump;

        auto log = std::make_shared<CConsoleLog>();
        model->SetUILog(log);   // also initializes the analyzer

        // declared after the pump, so it runs before the pump is destroyed
        oui::ScopedGuard stopGuard([&]() {
            model->GetAnalyzer().CancelAll();
            model->Stop();
            pump.GetThread()->GUI_ProcessTasks();
            fflush(stdout);
            fflush(stderr);
        });

        // commands run against the single active item, so more targets would be ambiguous
        if (options.files.size() + options.pids.size() > 1)
        {
            WriteLine(stderr, ORTHIA_TCSTR("--cmd accepts a single target: one <filename> or one --pid"));
            return consoleExit_Usage;
        }
        if (options.files.empty() && options.pids.empty())
        {
            WriteLine(stderr, ORTHIA_TCSTR("No target: a file name or --pid is required"));
            return consoleExit_OpenFail;
        }

        for (auto& name : options.files)
        {
            if (!OpenFile(pump, model, name))
            {
                return consoleExit_OpenFail;
            }
        }
        for (auto& pid : options.pids)
        {
            if (!OpenProcess(pump, model, pid))
            {
                return consoleExit_OpenFail;
            }
        }
        WaitUntilIdle(pump, model);

        auto item = model->GetActiveItem();
        if (!item)
        {
            WriteLine(stderr, ORTHIA_TCSTR("No active workspace"));
            return consoleExit_OpenFail;
        }

        int result = consoleExit_Ok;
        for (auto& cmdText : options.commands)
        {
            if (!RunOneCommand(pump, model, item, cmdText))
            {
                result = consoleExit_CommandFail;
            }
        }
        return result;
    }
}
