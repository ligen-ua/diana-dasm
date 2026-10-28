#pragma once

#include "orthia_parser.h"
#include "orthia_model_interfaces.h"

namespace orthia
{
struct ICalcNode;
class CProgramModel;
class CCommandProcessor
{
public:
    struct RequestCanceledException:std::runtime_error
    {
        RequestCanceledException();
    };

    enum class SpecialUICommands
    {
        None,
        ClearScreen
    };
    using ExecuteProgressHandler_type = std::function<void(std::shared_ptr<oui::BaseOperation> operation,
        const oui::String& text,
        bool finalText)>;
    using SpecialUICommandHandler_type = std::function<void(std::shared_ptr<oui::BaseOperation> operation,
        SpecialUICommands cmd)>;

    struct CommandArguments
    {
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler;
        CCommandParser& parser;
        std::shared_ptr<IWorkPlaceItem> item;
        std::shared_ptr<orthia::CProgramModel> model;
        int workspaceId = 0;
        oui::OperationPtr_type<SpecialUICommandHandler_type> uiCommandHandler;
        // the whole command line, for commands that split their arguments themselves
        orthia::PlatformString_type text;

        int linesWithoutSync = 0;
        // a pause every few lines, so that the UI can draw them
        bool pauseOutput = true;
        // non-empty when the command failed, sent as the final reply
        oui::String errorText;
        void ReplyLine(const oui::String& text);
        void Sync();
    };
protected:

    oui::CThreadPool m_pool;
    std::atomic_bool m_busy = false;
    std::atomic_bool m_pauseOutput = true;
    void ExecuteImpl(ThreadPtr_type targetThread,
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        oui::OperationPtr_type<SpecialUICommandHandler_type> uiCommandHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<orthia::CProgramModel> model);

    void ReportStop(CommandArguments& args);
    void Handle_u(CommandArguments& args);
    void Handle_x(CommandArguments& args);
    void Handle_xr(CommandArguments& args);
    void Handle_d(CommandArguments& args, int itemSize, bool dps = false);
    // floating: lmf, the floating modules; lm: the others
    void Handle_lm(CommandArguments& args, bool floating);
    void Handle_threads(CommandArguments& args);
    void Handle_reload(CommandArguments& args);
    void Handle_analyze(CommandArguments& args);
    void Handle_capture(CommandArguments& args);
    void Handle_symfix(CommandArguments& args);
    void Handle_mod_info(CommandArguments& args);
    void Handle_sections(CommandArguments& args);
    void Handle_database(CommandArguments& args);
    void Handle_database_list(CommandArguments& args);
    void Handle_database_delete(CommandArguments& args, const std::vector<orthia::PlatformString_type>& selectors);
    void Handle_database_cleanup(CommandArguments& args);

public:

    CCommandProcessor();
    void AsyncExecute(ThreadPtr_type targetThread, 
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler, 
        oui::OperationPtr_type<SpecialUICommandHandler_type> uiCommandHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<orthia::CProgramModel> model);
    bool IsBusy() const;
    // false without a UI (--cmd): long output is written at full speed
    void SetOutputPause(bool pause);

    // true for the commands that work on the data folder and need no open item
    static bool IsTargetless(const orthia::PlatformString_type& text);
};

}