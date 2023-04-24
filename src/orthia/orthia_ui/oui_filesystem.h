#pragma once

#include "oui_string.h"
#include "oui_threadpool.h"
#include "oui_window_thread.h"

namespace oui
{

    struct IFile
    {
        virtual ~IFile() {}
    };

    struct FileUnifiedId
    {
        std::vector<char> fileId;
        String fullFileName;
        FileUnifiedId()
        {
        }
        FileUnifiedId(const String& fullFileName_in)
            :
            fullFileName(fullFileName_in)
        {
        }
        bool IsEmpty() const
        {
            return fileId.empty() && fullFileName.native.empty();
        }
    };
    struct FileInfo
    {
        static const int flag_directory = 1;
        static const int flag_disk      = 3;
        static const int flag_uplink    = 4;
        static const int flag_highlight = 8;

        String fileName;
        int flags = 0;
        unsigned long long size = 0;
    };

    using ThreadPtr_type = std::shared_ptr<CWindowThread>;
    using FileRecipientHandler_type = std::function<void(std::shared_ptr<IFile>, int error)>;
    using QueryFilesHandler_type = std::function<void(std::shared_ptr<BaseOperation> operation, const FileUnifiedId& folderId, 
        const std::vector<FileInfo>& data, int error)>;
    using QueryDefaultRootHandler_type = std::function<void(const String& name, int error)>;

    struct IFileSystem
    {
        const static int queryFlags_OpenParent = 0x0001;
        const static int queryFlags_OpenChild  = 0x0002;

        virtual ~IFileSystem() {}
        virtual void AsyncOpenFile(ThreadPtr_type targetThread, const FileUnifiedId& fileId, FileRecipientHandler_type handler) = 0;
        virtual void AsyncStartQueryFiles(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId, 
            const String& argument,
            int queryFlags, 
            OperationPtr_type<QueryFilesHandler_type> handler) = 0;
        virtual void AsyncQueryDefaultRoot(ThreadPtr_type targetThread, QueryDefaultRootHandler_type handler) = 0;
    };

    // default filesystem
    class CFileSystem:public IFileSystem
    {
        std::shared_ptr<IFileSystem> m_fsImpl;
        CThreadPool m_pool;
    public:
        CFileSystem();

        // id-based stuff
        void AsyncOpenFile(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId,
            FileRecipientHandler_type handler) override;

        void AsyncStartQueryFiles(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId,
            const String& argument,
            int queryFlags,
            OperationPtr_type<QueryFilesHandler_type> handler) override;

        // root
        void AsyncQueryDefaultRoot(ThreadPtr_type targetThread, 
            QueryDefaultRootHandler_type handler) override;
    };

    std::shared_ptr<IFileSystem> CreateDefaultFSProvider();
}
