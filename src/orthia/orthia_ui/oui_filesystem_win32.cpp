#include "oui_filesystem.h"
#include "oui_window_thread.h"
#include "oui_base_win32.h"

namespace oui
{
    static bool IsFileNameSeparator(wchar_t ch)
    {
        return (ch == L'\\' || ch == L'/');
    }

    static void EraseLastSlash(std::wstring& str)
    {
        for (;;)
        {
            if (str.empty())
                return;

            if (!IsFileNameSeparator(*str.rbegin()))
                break;

            str.resize(str.size() - 1);
        }
    }
    static void InsertPrefix(std::wstring& fullPath)
    {
        if (wcsncmp(fullPath.c_str(), L"\\\\", 2) != 0)
        {
            fullPath.insert(0, L"\\\\?\\");
        }
    }
    static void Normalize(std::wstring& fullPath)
    {
        // remove the possible duplications and switch to windows style slash
        size_t size = fullPath.size();
        if (!size)
            return;

        if (fullPath[0] == '/')
            fullPath[0] = '\\';

        int delta = 0;
        for (size_t i = 1; i < size; ++i)
        {
            if (fullPath[i] == '/')
                fullPath[i] = '\\';

            if ((fullPath[i] == '\\') && (fullPath[i - 1 - delta] == '\\'))
            {
                ++delta;
            }
            fullPath[i - delta] = fullPath[i];
        }
        fullPath.erase(size - delta);

        EraseLastSlash(fullPath);

        // MSDN:
        // Note  Prepending the string "\\?\" does not allow access to the root directory.
        if (fullPath.empty())
        {
            return;
        }
        if (fullPath.back() == L':')
        {
            return;
        }

        // append prefix
        InsertPrefix(fullPath);
    }

    static int QueryDisks(const FileUnifiedId& fileId,
        std::vector<FileInfo>& result)
    {
        // query disks
        std::vector<wchar_t> buffer;
        buffer.resize(1024);
        DWORD dwRes = GetLogicalDriveStringsW((DWORD)buffer.size() - 1, &buffer.front());
        if (!dwRes)
        {
            return GetLastError();
        }
        std::wstring path;
        for (auto p = buffer.data(); *p; ++p)
        {
            path = p;

            FileInfo info;
            info.id = FileUnifiedId(path);
            info.fileName = path;
            info.flags = FileInfo::flag_disk;
            result.push_back(std::move(info));
        }
        return 0;
    }
    class CFileSystemImpl:public IFileSystem
    {
    public:
        void AsyncOpenFile(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId, 
            FileRecipientHandler_type handler) override
        {

        }
        void AsyncStartQueryFiles(ThreadPtr_type targetThread,
            const FileUnifiedId& fileId_in,
            OperationPtr_type<QueryFilesHandler_type> handler) override
        {
            FileUnifiedId fileId = fileId_in;
            Normalize(fileId.fullFileName.native);

            const int pageSize = 30;
            std::vector<FileInfo> result;
            result.reserve(pageSize);
            int error = 0;
            if (fileId.IsEmpty())
            {
                // query disks
                error = QueryDisks(fileId, result);
                handler->Reply(handler, fileId, result, error);
                return;
            }
            std::wstring searchParam = fileId.fullFileName.native + L"\\*";

            WIN32_FIND_DATAW win32Info = { 0, };
            HANDLE hSearch = FindFirstFileW(searchParam.c_str(), &win32Info);
            if (hSearch == INVALID_HANDLE_VALUE)
            {
                handler->Reply(handler, fileId, result, error);
                return;
            }
            oui::ScopedGuard guard([=]() { FindClose(hSearch); });

            FileInfo info;
            std::wstring fullPath, path;
            ULARGE_INTEGER size;
            for (;;)
            {
                path = win32Info.cFileName;

                fullPath.clear();
                fullPath.append(fileId.fullFileName.native);
                fullPath.append(L"\\");
                fullPath.append(win32Info.cFileName);
                
                size.HighPart = win32Info.nFileSizeHigh;
                size.LowPart = win32Info.nFileSizeLow;

                info.id = FileUnifiedId(fullPath);
                info.fileName = path;
                info.flags = 0;
                info.size = size.QuadPart;
                if (win32Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    info.flags = FileInfo::flag_directory;

                }
                result.push_back(info);

                // flush the data if ready
                if (result.size() >= pageSize)
                {
                    if (!handler->Reply(handler, fileId, result, 0))
                    {
                        return;
                    }
                    result.clear();
                }

                if (!FindNextFileW(hSearch, &win32Info))
                {
                    break;
                }
            }
            if (!result.empty())
            {
                handler->Reply(handler, fileId, result, 0);
            }
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
