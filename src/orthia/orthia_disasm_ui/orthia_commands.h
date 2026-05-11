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

        int linesWithoutSync = 0;
        void ReplyLine(const oui::String& text);
        void Sync();
    };
protected:

    oui::CThreadPool m_pool;
    std::atomic_bool m_busy = false;
    void ExecuteImpl(ThreadPtr_type targetThread,
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        oui::OperationPtr_type<SpecialUICommandHandler_type> uiCommandHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<orthia::CProgramModel> model);

    void ReportStop(CommandArguments& args);
    void Handle_u(CommandArguments& args);
    void Handle_x(CommandArguments& args);
    void Handle_d(CommandArguments& args, int itemSize, bool dps = false);
    void Handle_lm(CommandArguments& args);
    void Handle_threads(CommandArguments& args);
    void Handle_reload(CommandArguments& args);
    void Handle_analyze(CommandArguments& args);
    void Handle_symfix(CommandArguments& args);

    int PrepareTokens(CommandArguments& args, std::vector<Token>& tokens, const Address_type maxCountOfItems, Address_type& countOfItems);
    std::shared_ptr<ICalcNode> BuildNodes(CommandArguments& args, std::vector<Token>& tokens, int indexOfLength, std::shared_ptr<ICalcNode> currentNode);

public:

    CCommandProcessor();
    void AsyncExecute(ThreadPtr_type targetThread, 
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler, 
        oui::OperationPtr_type<SpecialUICommandHandler_type> uiCommandHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<orthia::CProgramModel> model);
    bool IsBusy() const;
};

}