#pragma once

#include "orthia_parser.h"
#include "orthia_model_interfaces.h"

namespace orthia
{

class CCommandProcessor
{
public:
    using ExecuteProgressHandler_type = std::function<void(std::shared_ptr<oui::BaseOperation> operation,
        const oui::String& text,
        bool finalText)>;

protected:

    oui::CThreadPool m_pool;
    std::atomic_bool m_busy = false;
    void ExecuteImpl(ThreadPtr_type targetThread,
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item);
    
    struct CommandArguments
    {
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler;
        CCommandParser& parser;
        std::shared_ptr<IWorkPlaceItem> item;
    };

    void ReportStop(CommandArguments& args);
    void Handle_u(CommandArguments& args);
    void Handle_x(CommandArguments& args);

public:

    CCommandProcessor();
    void AsyncExecute(ThreadPtr_type targetThread, 
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item);
    bool IsBusy() const;
};

}