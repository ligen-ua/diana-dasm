#include "oui_filesystem.h"
#include "oui_window_thread.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <algorithm>

namespace oui
{

static bool IsSeparator(char c)
{
    return c == '/';
}

static bool SplitPath(const std::string& path, std::string& dir, std::string& name)
{
    for (int i = (int)path.size() - 1; i >= 0; --i)
    {
        if (IsSeparator(path[i]))
        {
            name = path.substr(i + 1);
            dir  = path.substr(0, i);
            return true;
        }
    }
    return false;
}

static std::string StripTrailingSlashes(const std::string& path)
{
    size_t end = path.size();
    while (end > 1 && IsSeparator(path[end - 1]))
        --end;
    return path.substr(0, end);
}

class CPosixFile : public IFile2, Noncopyable
{
    String                     m_fullName;
    int                        m_fd;
    std::shared_ptr<IFileSystem> m_fs;
public:
    CPosixFile(const String& fullName, int fd, std::shared_ptr<IFileSystem> fs)
        : m_fullName(fullName), m_fd(fd), m_fs(fs)
    {
    }
    ~CPosixFile()
    {
        if (m_fd >= 0)
            close(m_fd);
    }
    std::shared_ptr<IFileSystem> GetFileSystem() override { return m_fs; }
    oui::String GetFullFileName() const override { return m_fullName; }
    oui::String GetFullFileNameForUI() const override { return m_fullName; }

    std::tuple<int, unsigned long long> GetSizeInBytes() const override
    {
        struct stat st;
        if (fstat(m_fd, &st) != 0)
            return std::make_tuple(errno, 0ULL);
        return std::make_tuple(0, (unsigned long long)st.st_size);
    }

    int ReadExact(std::shared_ptr<BaseOperation> operation,
                  unsigned long long offset,
                  size_t size,
                  std::vector<char>& data) override
    {
        if (offset != IFile::offset_UseCurrent)
        {
            if (lseek(m_fd, (off_t)offset, SEEK_SET) == (off_t)-1)
                return errno;
        }
        data.resize(size);
        size_t total = 0;
        while (total < size)
        {
            ssize_t n = read(m_fd, data.data() + total, size - total);
            if (n < 0)
            {
                if (errno == EINTR) continue;
                return errno;
            }
            if (n == 0) break;
            total += (size_t)n;
            if (operation && operation->IsCancelled())
                return ECANCELED;
        }
        data.resize(total);
        return 0;
    }
};

class CPosixFileSystemImpl
    : public std::enable_shared_from_this<CPosixFileSystemImpl>
    , public IFileSystem
{
    static bool IsExecutableExt(const std::string& name)
    {
        // On Linux, executability is determined by mode bits, not extension.
        // We mark files that have an execute bit set.
        return false;
    }

    static int QueryDir(const std::string& path,
                        const std::string& highlightName,
                        std::vector<FileInfo>& result)
    {
        DIR* dir = opendir(path.c_str());
        if (!dir)
            return errno;

        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr)
        {
            const char* dname = ent->d_name;
            if (strcmp(dname, ".") == 0)
                continue;
            if (strcmp(dname, "..") == 0)
            {
                FileInfo uplink;
                uplink.fileName = String(std::string(".."));
                uplink.flags = FileInfo::flag_uplink;
                result.push_back(std::move(uplink));
                continue;
            }

            std::string fullPath = path + "/" + dname;
            struct stat st;
            if (stat(fullPath.c_str(), &st) != 0)
                continue;

            FileInfo info;
            info.fileName = String(std::string(dname));
            info.size = (unsigned long long)st.st_size;

            if (S_ISDIR(st.st_mode))
            {
                info.flags |= FileInfo::flag_directory;
            }
            else if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
            {
                info.flags |= FileInfo::flag_any_executable;
            }

            if (!highlightName.empty() && info.fileName.native == highlightName)
                info.flags |= FileInfo::flag_highlight;

            result.push_back(std::move(info));
        }
        closedir(dir);
        return 0;
    }

public:
    std::tuple<int, String> SyncLocateFile(const String& fileName, int /*dianaMode*/) override
    {
        struct stat st;
        if (stat(fileName.native.c_str(), &st) == 0)
            return std::make_tuple(0, fileName);
        return std::make_tuple(errno, String());
    }

    String SyncSanitizeName(const String& fileName)
    {
        // On POSIX only '/' and '\0' are truly forbidden,
        // but also strip common problematic characters.
        const std::string invalid = "/\\:*?\"<>|";
        std::string result = fileName.native;
        std::replace_if(result.begin(), result.end(),
            [&invalid](unsigned char c) {
                return invalid.find(static_cast<char>(c)) != std::string::npos
                    || c < 32; // control characters
            },
            '_');
        return result;
    }

    std::tuple<int, String> SyncNormalizeName(const String& fileName, bool expectSo) override
    {
        String result = fileName;
        // lowercase
        std::transform(result.native.begin(), result.native.end(),
                       result.native.begin(), ::tolower);
        if (expectSo && result.native.find('.') == std::string::npos)
            result.native += ".so";
        return std::make_tuple(0, result);
    }

    std::tuple<int, std::shared_ptr<IFile2>> SyncOpenFile(const FileUnifiedId& fileId) override
    {
        std::shared_ptr<IFile2> file;
        int fd = open(fileId.fullFileName.native.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0)
            return std::make_tuple(errno, file);
        file = std::make_shared<CPosixFile>(fileId.fullFileName, fd, shared_from_this());
        return std::make_tuple(0, file);
    }

    void AsyncOpenFile(ThreadPtr_type targetThread,
                       const FileUnifiedId& fileId,
                       FileRecipientHandler_type handler) override
    {
        std::shared_ptr<IFile2> file;
        String folderName;
        int error = 0;

        int fd = open(fileId.fullFileName.native.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0)
        {
            // Check if it's a directory
            struct stat st;
            if (stat(fileId.fullFileName.native.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                folderName = fileId.fullFileName;
            }
            else
            {
                error = errno;
            }
        }
        else
        {
            file = std::make_shared<CPosixFile>(fileId.fullFileName, fd, shared_from_this());
        }

        auto operation = std::make_shared<Operation<FileRecipientHandler_type>>(
            targetThread, handler);
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
        std::string highlightName;

        if (queryFlags & IFileSystem::queryFlags_OpenChild)
        {
            if (fileId.fullFileName.native.empty())
                fileId.fullFileName = argument;
            else
            {
                fileId.fullFileName.native = StripTrailingSlashes(fileId.fullFileName.native)
                    + "/" + argument.native;
            }
        }
        else if (queryFlags & IFileSystem::queryFlags_OpenParent)
        {
            std::string dir, name;
            if (SplitPath(fileId.fullFileName.native, dir, name))
            {
                highlightName = name;
                fileId.fullFileName.native = dir.empty() ? "/" : dir;
            }
            else
            {
                fileId.fullFileName.native.clear();
            }
        }

        std::vector<FileInfo> result;
        int error = 0;

        if (fileId.IsEmpty() || fileId.fullFileName.native.empty())
        {
            // Root — just show "/"
            FileInfo info;
            info.fileName = String(std::string("/"));
            info.flags = FileInfo::flag_directory;
            result.push_back(std::move(info));
        }
        else
        {
            error = QueryDir(fileId.fullFileName.native, highlightName, result);
        }
        handler->ReplyWithRetain(handler, handler, fileId, result, error, tag);
    }

    void AsyncQueryDefaultRoot(ThreadPtr_type targetThread,
                               QueryDefaultRootHandler_type handler) override
    {
        auto op = std::make_shared<Operation<QueryDefaultRootHandler_type>>(
            targetThread, handler);
        op->ReplyWithRetain(op, String(std::string("/")), 0);
    }

    String AppendSlash(const String& file) override
    {
        String result = file;
        if (!result.native.empty() && result.native.back() != '/')
            result.native += '/';
        return result;
    }

    void AsyncExecute(ThreadPtr_type /*targetThread*/, ExecuteHandler_type handler) override
    {
        handler();
    }
};

std::shared_ptr<IFileSystem> CreateDefaultFSProvider()
{
    return std::make_shared<CPosixFileSystemImpl>();
}

} // namespace oui

