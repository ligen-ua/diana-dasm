#pragma once

#include "oui_filesystem.h"
#include "orthia_model_interfaces.h"

namespace oui
{

    struct IProcess:IFile
    {
        virtual orthia::WorkAddressData ReadExactEx(unsigned long long offset, size_t size) = 0;
        virtual int ReadExactEx2(unsigned long long offset, void* pBuffer, size_t size) = 0;
        virtual int QueryModules(std::vector<orthia::ModuleInfo>& modules, int& processModuleOffset) = 0;
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
        static const int flag_hasReaderAccess = 0x01;

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
        const static int queryFlags_TryOpenProcessAsReader = 0x0001;

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
