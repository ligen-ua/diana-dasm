#pragma once

#include "orthia_parser.h"
#include "orthia_model_interfaces.h"

namespace orthia
{
struct ICalcNode;
class CCommandProcessor
{
public:
    using ExecuteProgressHandler_type = std::function<void(std::shared_ptr<oui::BaseOperation> operation,
        const oui::String& text,
        bool finalText)>;

    struct CommandArguments
    {
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler;
        CCommandParser& parser;
        std::shared_ptr<IWorkPlaceItem> item;

        void ReplyLine(const oui::String& text);
    };
protected:

    oui::CThreadPool m_pool;
    std::atomic_bool m_busy = false;
    void ExecuteImpl(ThreadPtr_type targetThread,
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item);
    


    void ReportStop(CommandArguments& args);
    void Handle_u(CommandArguments& args);
    void Handle_x(CommandArguments& args);
    void Handle_d(CommandArguments& args, int itemSize, bool dps = false);
    void Handle_lm(CommandArguments& args);

    int PrepareTokens(CommandArguments& args, std::vector<Token>& tokens, const Address_type maxCountOfItems, Address_type& countOfItems);
    std::shared_ptr<ICalcNode> BuildNodes(CommandArguments& args, std::vector<Token>& tokens, int indexOfLength, std::shared_ptr<ICalcNode> currentNode);

public:

    CCommandProcessor();
    void AsyncExecute(ThreadPtr_type targetThread, 
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item);
    bool IsBusy() const;
};

}