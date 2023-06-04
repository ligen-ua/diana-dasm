#pragma once

#include "oui_string.h"
#include "oui_threadpool.h"
#include "oui_window_thread.h"

namespace oui
{

    struct IFile
    {
        virtual ~IFile() {}
        virtual std::tuple<int, unsigned long long> GetSizeInBytes() const = 0;
        virtual int SaveToVector(std::shared_ptr<BaseOperation> operation, size_t size, std::vector<char>& peFile) = 0;
        virtual oui::String GetFullFileName() const = 0;
        virtual oui::String GetFullFileNameForUI() const = 0;
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
        static const int flag_directory      = 0x01;
        static const int flag_disk           = 0x03;
        static const int flag_uplink         = 0x04;
        static const int flag_highlight      = 0x08;
        static const int flag_any_executable = 0x10;

        String fileName;
        int flags = 0;
        unsigned long long size = 0;
    };

    using ThreadPtr_type = std::shared_ptr<CWindowThread>;
    using FileRecipientHandler_type = std::function<void(std::shared_ptr<IFile>, int error, const String& folderName)>;
    using QueryFilesHandler_type = std::function<void(std::shared_ptr<BaseOperation> operation, 
        const FileUnifiedId& folderId, 
        const std::vector<FileInfo>& data, 
        int error,
        const String& tag)>;
    using QueryDefaultRootHandler_type = std::function<void(const String& name, int error)>;
    using ExecuteHandler_type = std::function<void()>;

    struct IFileSystem
    {
        const static int queryFlags_OpenParent = 0x0001;
        const static int queryFlags_OpenChild  = 0x0002;

        virtual ~IFileSystem() {}
        virtual std::tuple<int, std::shared_ptr<IFile>> SyncOpenFile(const FileUnifiedId& fileId) = 0;

        virtual void AsyncOpenFile(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId, 
            FileRecipientHandler_type handler) = 0;
        virtual void AsyncStartQueryFiles(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId, 
            const String& argument,
            int queryFlags,
            const String& tag,
            OperationPtr_type<QueryFilesHandler_type> handler) = 0;
        virtual void AsyncQueryDefaultRoot(ThreadPtr_type targetThread, QueryDefaultRootHandler_type handler) = 0;
        virtual String AppendSlash(const String& file) = 0;

        // async execute
        virtual void AsyncExecute(ThreadPtr_type targetThread,
            ExecuteHandler_type handler) = 0;
    };

    // default filesystem
    class CFileSystem:public IFileSystem
    {
        std::shared_ptr<IFileSystem> m_fsImpl;
        CThreadPool m_pool;
    public:
        CFileSystem();

        std::tuple<int, std::shared_ptr<IFile>> SyncOpenFile(const FileUnifiedId& fileId);

        // id-based stuff
        void AsyncOpenFile(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId,
            FileRecipientHandler_type handler) override;

        void AsyncStartQueryFiles(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId,
            const String& argument,
            int queryFlags,
            const String& tag,
            OperationPtr_type<QueryFilesHandler_type> handler) override;

        // root
        void AsyncQueryDefaultRoot(ThreadPtr_type targetThread, 
            QueryDefaultRootHandler_type handler) override;

        String AppendSlash(const String& file) override;

        // execute FS task
        void AsyncExecute(ThreadPtr_type targetThread,
            ExecuteHandler_type handler) override;

    };

    std::shared_ptr<IFileSystem> CreateDefaultFSProvider();

    namespace fsui
    {
        struct OpenResult
        {
            String error;
            OpenResult()
            {
            }
            OpenResult(const String& error_in)
                :
                error(error_in)
            {
            }
        };

        using FileCompleteHandler_type = std::function<void(std::shared_ptr<BaseOperation>, std::shared_ptr<IFile>, const OpenResult&)>;
    }
}
