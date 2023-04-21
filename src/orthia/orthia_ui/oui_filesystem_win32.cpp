#include "oui_filesystem.h"
#include "oui_window_thread.h"
#include "oui_base_win32.h"

namespace oui
{
    class CFileSystemImpl:public IFileSystem
    {
    public:
        void AsyncOpenFile(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId, 
            FileRecipientHandler_type handler) override
        {

        }
        void AsyncStartQueryFiles(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId, 
            OperationPtr_type<QueryFilesHandler_type> handler) override
        {

        }
        void AsyncQueryDefaultRoot(ThreadPtr_type targetThread, 
            QueryDefaultRootHandler_type handler) override
        {
            std::wstring result;
            int error = 0;
            std::vector<wchar_t> buffer(256);
            if (GetSystemDirectoryW(buffer.data(), (UINT)buffer.size() - 1))
            {
                auto it = std::find(buffer.begin(), buffer.end(), L':');
                if (it != buffer.end())
                {
                    ++it;
                    if (it != buffer.end())
                    {
                        result.assign(buffer.begin(), it + 1);
                    }
                }
            }
            else
            {
                error = GetLastError();
            }
            targetThread->AddTask([=]() { handler(oui::String(result), error);  });
        }
    };

    std::shared_ptr<IFileSystem> CreateDefaultFSProvider()
    {
        // TODO: add shell32 here if available
        return std::make_shared<CFileSystemImpl>();
    }
}
