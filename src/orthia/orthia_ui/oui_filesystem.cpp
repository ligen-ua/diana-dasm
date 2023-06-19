#include "oui_filesystem.h"

namespace oui
{
    const int g_threadsCount = 2;
    CFileSystem::CFileSystem()
    {
        m_fsImpl = CreateDefaultFSProvider();
        m_pool.Start(g_threadsCount);
    }

    std::tuple<int, std::shared_ptr<IFile>> CFileSystem::SyncOpenFile(const FileUnifiedId& fileId)
    {
        return m_fsImpl->SyncOpenFile(fileId);
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
        const String& argument,
        int queryFlags,
        const String& tag,
        OperationPtr_type<QueryFilesHandler_type> handler)
    {
        if (!targetThread)
        {
            return;
        }
        m_pool.AddTask([=, handler = std::move(handler)]() {
            m_fsImpl->AsyncStartQueryFiles(targetThread, fileId, argument, queryFlags, tag, handler);
        });
    }
    String CFileSystem::AppendSlash(const String& file)
    {
        return m_fsImpl->AppendSlash(file);
    }

    void CFileSystem::AsyncExecute(ThreadPtr_type targetThread,
        ExecuteHandler_type handler)
    {
        if (!targetThread)
        {
            return;
        }
        m_pool.AddTask([=, handler = std::move(handler)]() {
            m_fsImpl->AsyncExecute(targetThread, handler);
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