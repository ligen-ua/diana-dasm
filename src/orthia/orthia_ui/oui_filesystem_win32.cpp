#include "oui_filesystem.h"
#include "oui_window_thread.h"
#include "oui_base_win32.h"

namespace oui
{

    template<class ContainerStr>
    void GetExtensionOfFile(const ContainerStr& fullName, ContainerStr* pExtension)
    {
        pExtension->clear();
        int pExtensionsize = (int)fullName.size();
        int size = (int)fullName.size();
        for (int i = size - 1; i > 0; --i)
        {
            wchar_t ch = (wchar_t)fullName[i];
            if (ch == '\\' || ch == L'/')
            {
                return;
            }
            if (ch == '.')
            {
                pExtension->assign(fullName.begin() + i + 1, fullName.end());
                return;
            }
        }
    }
    static bool IsFileNameSeparator(wchar_t ch)
    {
        return (ch == L'\\' || ch == L'/');
    }

    static bool SplitFullPathName(const std::wstring& str,
        std::wstring& path,
        std::wstring& name)
    {
        for (int i = (int)str.size() - 1; i >= 0; --i)
        {
            if (IsFileNameSeparator(str[i]))
            {
                name = std::wstring(str.begin() + i + 1, str.end());
                path = std::wstring(str.begin(), str.begin() + i);
                return true;
            }
        }
        return false;
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
    static void ErasePrefix(std::wstring& fullPath)
    {
        if (wcsncmp(fullPath.c_str(), L"\\\\?\\", 4) == 0)
        {
            fullPath.erase(0, 4);
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
        for (size_t i = 2; i < size; ++i)
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
            ErasePrefix(fullPath);
            return;
        }

        // append prefix
        InsertPrefix(fullPath);
    }
    static void ApplyFlags(std::wstring& fullPath, const std::wstring& argument, int queryFlags, std::wstring& highlightName)
    {
        highlightName.clear();
        if (queryFlags & IFileSystem::queryFlags_OpenChild)
        {
            // transition from root -> disk
            if (fullPath.empty())
            {
                fullPath = argument;
                Normalize(fullPath);
                return;
            }
            // regular case
            fullPath.append(L"\\");
            fullPath.append(argument);
            return;
        }
        if (queryFlags & IFileSystem::queryFlags_OpenParent)
        {
            if (fullPath.empty())
            {
                return;
            }
            std::wstring path;
            if (!SplitFullPathName(fullPath, path, highlightName))
            {
                fullPath.clear();
                return;
            }
            fullPath = path;
            Normalize(fullPath);
            return;
        }
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
            info.fileName = path;
            info.flags = FileInfo::flag_disk;
            result.push_back(std::move(info));
            p += path.size();
        }
        return 0;
    }

    class CFile:public IFile, Noncopyable
    {
        String m_fullName;
        HANDLE m_hFile;
    public:
        CFile(const String& fullname, HANDLE hFile)
            :
                m_fullName(fullname),
                m_hFile(hFile)
        {
        }
        ~CFile()
        {
            Reset(String(), 0);
        }
        void Reset(const String& fullname, HANDLE hFile)
        {
            if (hFile == m_hFile)
            {
                return;
            }
            if (m_hFile != 0 && m_hFile != INVALID_HANDLE_VALUE)
            {
                CloseHandle(m_hFile);
            }
            m_hFile = hFile;
            m_fullName = fullname;
        }
        std::tuple<int, unsigned long long> GetSizeInBytes() const override
        {
            LARGE_INTEGER size;
            size.QuadPart = 0;
            if (!::GetFileSizeEx(m_hFile, &size))
            {
                int error = GetLastError();
                return std::make_tuple(error, 0ULL);
            }
            return std::make_tuple(0, size.QuadPart);
        }

        void MoveToBegin() 
        {
            LARGE_INTEGER distance;
            distance.QuadPart = 0;
            LARGE_INTEGER result;
            SetFilePointerEx(m_hFile, distance, &result, FILE_BEGIN);
        }
        oui::String GetFullFileName() const override
        {
            return m_fullName;
        }
        oui::String GetFullFileNameForUI() const override
        {
            auto result = m_fullName;
            ErasePrefix(result.native);
            return result;
        }
        int SaveToVector(std::shared_ptr<BaseOperation> operation, size_t size, std::vector<char>& peFile) override
        {
            MoveToBegin();
            peFile.resize(size);

            DWORD pageSize = 1024 * 1024;
            std::vector<char> page(pageSize);
            
            auto ptr = peFile.data();
            size_t sizeToCopy = size;
            for (; sizeToCopy; )
            {
                DWORD sizeToRead = pageSize;
                if (sizeToCopy < pageSize)
                {
                    sizeToRead = (DWORD)sizeToCopy;
                }
                DWORD readBytes = 0;
                if (!ReadFile(m_hFile, ptr, sizeToRead, &readBytes, 0))
                {
                    return GetLastError();
                }
                ptr += readBytes;
                sizeToCopy -= readBytes;

                if (operation->IsCancelled())
                {
                    return ERROR_CANCELLED;
                }
            }
            return 0;
        }

    };
    class CFileSystemImpl:public IFileSystem
    {
        std::unordered_map<std::wstring, int> m_knownExtensions;
    public:
        CFileSystemImpl()
        {
            m_knownExtensions[L"EXE"] = FileInfo::flag_any_executable;
            m_knownExtensions[L"DLL"] = FileInfo::flag_any_executable;
            m_knownExtensions[L"SYS"] = FileInfo::flag_any_executable;
            m_knownExtensions[L"OCX"] = FileInfo::flag_any_executable;
            m_knownExtensions[L"CPL"] = FileInfo::flag_any_executable;
            m_knownExtensions[L"SCR"] = FileInfo::flag_any_executable;
        }
        std::tuple<int, std::shared_ptr<IFile>> SyncOpenFile(const FileUnifiedId& fileId_in)
        {
            FileUnifiedId fileId = fileId_in;
            Normalize(fileId.fullFileName.native);

            std::wstring folderName;
            std::shared_ptr<IFile> file;
            int error = 0;
            HANDLE hValue = CreateFileW(fileId.fullFileName.native.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                0,
                0);
            if (hValue == INVALID_HANDLE_VALUE)
            {
                auto err = GetLastError();
                return std::make_tuple(err, file);
            }
            file = std::make_shared<CFile>(fileId.fullFileName, hValue);
            return std::make_tuple(0, file);
        }
        void AsyncOpenFile(ThreadPtr_type targetThread, 
            const FileUnifiedId& fileId_in, 
            FileRecipientHandler_type handler) override
        {
            FileUnifiedId fileId = fileId_in;
            Normalize(fileId.fullFileName.native);

            std::wstring folderName;
            std::shared_ptr<IFile> file;
            int error = 0;
            HANDLE hValue = CreateFileW(fileId.fullFileName.native.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                0,
                0);

            if (hValue == INVALID_HANDLE_VALUE)
            {
                // check if it is a directory
                error = GetLastError();
                HANDLE hDir = CreateFileW(fileId.fullFileName.native.c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ,
                    nullptr,
                    OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS,
                    0);
                if (hDir != INVALID_HANDLE_VALUE)
                {
                    folderName = fileId.fullFileName.native;
                    ErasePrefix(folderName);

                    error = 0;
                    CloseHandle(hDir);
                }
            }
            else
            {
                file = std::make_shared<CFile>(fileId.fullFileName, hValue);
            }

            auto operation = std::make_shared<Operation<FileRecipientHandler_type>>(
                targetThread,
                handler);
            
            operation->ReplyWithRetain(operation, file, error, folderName);
        }
        void AsyncStartQueryFiles(ThreadPtr_type targetThread,
            const FileUnifiedId& fileId_in,
            const String& argument,
            int queryFlags,
            const String& tag,
            OperationPtr_type<QueryFilesHandler_type> handler) override
        {
            FileUnifiedId fileId = fileId_in;
            Normalize(fileId.fullFileName.native);

            std::wstring highlightName;
            ApplyFlags(fileId.fullFileName.native, argument.native, queryFlags, highlightName);

            const int pageSize = 30;
            std::vector<FileInfo> result;
            result.reserve(pageSize);
            int error = 0;
            if (fileId.IsEmpty())
            {
                // query disks
                error = QueryDisks(fileId, result);
                handler->Reply(handler, fileId, result, error, tag);
                return;
            }
            std::wstring searchParam = fileId.fullFileName.native + L"\\*";

            WIN32_FIND_DATAW win32Info = { 0, };
            HANDLE hSearch = FindFirstFileW(searchParam.c_str(), &win32Info);
            if (hSearch == INVALID_HANDLE_VALUE)
            {
                handler->Reply(handler, fileId, result, error, tag);
                return;
            }
            oui::ScopedGuard guard([=]() { FindClose(hSearch); });

            FileInfo info;
            std::wstring path;
            ULARGE_INTEGER size;
            bool wasReply = false;
            for (;;)
            {
                path = win32Info.cFileName;
                if (path == L"." || path == L"..")
                {
                    if (!FindNextFileW(hSearch, &win32Info))
                    {
                        break;
                    }
                    continue;
                }
                size.HighPart = win32Info.nFileSizeHigh;
                size.LowPart = win32Info.nFileSizeLow;

                info.fileName = path;
                info.flags = 0;
                info.size = size.QuadPart;
                if (win32Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    info.flags |= FileInfo::flag_directory;
                }
                if (highlightName == path)
                {
                    info.flags |= FileInfo::flag_highlight;
                }
                std::wstring extension;
                GetExtensionOfFile(info.fileName.native, &extension);
                {
                    auto it = m_knownExtensions.find(Uppercase_Silent(extension));
                    if (it != m_knownExtensions.end())
                    {
                        info.flags |= it->second;
                    }
                }
                result.push_back(info);

                // flush the data if ready
                if (result.size() >= pageSize)
                {
                    if (!handler->Reply(handler, fileId, result, 0, tag))
                    {
                        return;
                    }
                    result.clear();
                    wasReply = true;
                }

                if (!FindNextFileW(hSearch, &win32Info))
                {
                    break;
                }
            }
            if (!wasReply || !result.empty())
            {
                handler->Reply(handler, fileId, result, 0, tag);
            }
        }
        String AppendSlash(const String& file_in) override
        {
            String file = file_in;
            EraseLastSlash(file.native);
            if (!file.native.empty())
            {
                file.native.append(L"\\");
            }
            ErasePrefix(file.native);
            return file;
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

        void AsyncExecute(ThreadPtr_type targetThread,
            ExecuteHandler_type handler)
        {
            handler();
        }
    };

    std::shared_ptr<IFileSystem> CreateDefaultFSProvider()
    {
        // TODO: add shell32 here if available
        return std::make_shared<CFileSystemImpl>();
    }
}
