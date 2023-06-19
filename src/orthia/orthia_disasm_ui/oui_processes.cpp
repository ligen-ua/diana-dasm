#include "oui_processes.h"

namespace oui
{
    const int g_threadsCount = 1;
    CProcessSystem::CProcessSystem()
    {
        m_fsImpl = CreateDefaultProcessProvider();
        m_pool.Start(g_threadsCount);
    }

    // id-based stuff
    void CProcessSystem::AsyncStartQueryProcess(ThreadPtr_type targetThread,
        const ProcessUnifiedId& fileId,
        ProcessRecipientHandler_type openHandler,
        OperationPtr_type<QueryProcessHandler_type> filterHandler,
        int flags)
    {
        if (!targetThread)
        {
            return;
        }
        m_pool.AddTask([=, 
            filterHandler = std::move(filterHandler),
            openHandler = std::move(openHandler)]() {

            m_fsImpl->AsyncStartQueryProcess(targetThread, fileId, openHandler, filterHandler, flags);
        });
    }

}