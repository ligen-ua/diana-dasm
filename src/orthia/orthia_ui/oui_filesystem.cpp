#include "oui_filesystem.h"

namespace oui
{
    const int g_tasksCount = 2;
    CFileSystem::CFileSystem()
    {
        m_fsImpl = CreateDefaultFSProvider();
        m_pool.Start(g_tasksCount);
    }

    // id-based stuff
    void CFileSystem::AsyncOpenFile(ThreadPtr_type targetThread, 
        const FileUnifiedId& fileId,
        FileRecipientHandler_type handler)
    {
        if (!targetThread)
        {
            return;
        }
        m_pool.AddTask([=, handler = std::move(handler)]() {
            m_fsImpl->AsyncOpenFile(targetThread, fileId, handler);
        });
    }

    void CFileSystem::AsyncStartQueryFiles(ThreadPtr_type targetThread, 
        const FileUnifiedId& fileId,
        OperationPtr_type<QueryFilesHandler_type> handler)
    {
        if (!targetThread)
        {
            return;
        }
        m_pool.AddTask([=, handler = std::move(handler)]() {
            m_fsImpl->AsyncStartQueryFiles(targetThread, fileId, handler);
        });
    }
 
    void CFileSystem::AsyncQueryDefaultRoot(ThreadPtr_type targetThread, 
        QueryDefaultRootHandler_type handler)
    {
        if (!targetThread)
        {
            return;
        }
        m_pool.AddTask([=, handler = std::move(handler)]() {
            m_fsImpl->AsyncQueryDefaultRoot(targetThread, handler);
        });
    }
}