#pragma once

#include "oui_filesystem.h"

namespace oui
{

    struct IProcess:IFile
    {
    };

    struct ProcessUnifiedId
    {
        unsigned long long pid = 0;
        String namePart;
        ProcessUnifiedId()
        {
        }
        ProcessUnifiedId(const String& namePart_in)
            :
            namePart(namePart_in)
        {
        }
        ProcessUnifiedId(unsigned long long pid_in)
            :
            pid(pid_in)
        {
        }
        bool IsEmpty() const
        {
            return pid == 0 && namePart.native.empty();
        }
    };
    struct ProcessInfo
    {
        unsigned long long pid = 0;
        String processName;
        int flags = 0;
        int pointerSize = 0;
    };

    using ProcessRecipientHandler_type = std::function<void(std::shared_ptr<IProcess>, int error)>;
    using QueryProcessHandler_type = std::function<void(std::shared_ptr<BaseOperation> operation, 
        const ProcessUnifiedId& filter, 
        const std::vector<ProcessInfo>& data, 
        int error)>;

    struct IProcessSystem
    {
        virtual ~IProcessSystem() {}
        virtual void AsyncStartQueryProcess(ThreadPtr_type targetThread, 
            const ProcessUnifiedId& fileId,
            ProcessRecipientHandler_type openHandler,
            OperationPtr_type<QueryProcessHandler_type> filterHandler,
            int flags) = 0;
    };

    // default filesystem
    class CProcessSystem:public IProcessSystem
    {
        std::shared_ptr<IProcessSystem> m_fsImpl;
        CThreadPool m_pool;
    public:
        CProcessSystem();

        void AsyncStartQueryProcess(ThreadPtr_type targetThread,
            const ProcessUnifiedId& fileId,
            ProcessRecipientHandler_type openHandler,
            OperationPtr_type<QueryProcessHandler_type> filterHandler, 
            int flags) override;
    };

    std::shared_ptr<IProcessSystem> CreateDefaultProcessProvider();

    namespace fsui
    {
        using ProcessCompleteHandler_type = std::function<void(std::shared_ptr<BaseOperation>, std::shared_ptr<IProcess>, const OpenResult&)>;
    }
}
