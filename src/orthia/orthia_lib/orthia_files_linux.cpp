#include "orthia_files.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>

namespace orthia
{

static EnvironmentPaths g_paths;

static std::string CheckNotEmpty(const std::string & val)
{
    if (val.empty())
    {
        throw std::runtime_error("The app is not initialized");
    }
    return val;
}

std::string GetApplicationDataPathWithSlash()
{
   return CheckNotEmpty(g_paths.m_ApplicationDataPath);
}
std::string GetTempPathWithSlash()
{
   return CheckNotEmpty(g_paths.m_TempPath);
}

void InitEnvironmentPaths(const EnvironmentPaths & paths)
{
   g_paths = paths;
   EnsureLastSlash(g_paths.m_ApplicationDataPath);
   EnsureLastSlash(g_paths.m_TempPath);
}
///
CFile::CFile()
   :
      m_descriptor(-1)
{
}
CFile::~CFile()
{
    Close();
}

void CFile::Close()
{
    if (m_descriptor < 0)
    {
        return;
    }
    close(m_descriptor);
    m_descriptor = -1;
}
void CFile::swap(CFile & file)
{
    std::swap(m_descriptor, file.m_descriptor);
}

//---- 
void CFile::OpenExistingRead(const std::string & fullname)
{
    Close();
    m_descriptor = open(fullname.c_str(), O_RDONLY | O_CLOEXEC);
    ORTHIA_CHECK_LESS_ZERO(m_descriptor, "Can't open file: " + fullname);
    MoveToFirst(0);

}
void CFile::CreateNewAlways(const std::string & fullname)
{
    Close();
    m_descriptor = open(fullname.c_str(), O_CREAT|O_RDWR|O_TRUNC| O_CLOEXEC, S_IRUSR | S_IWUSR);
    ORTHIA_CHECK_LESS_ZERO(m_descriptor, "Can't open file: " + fullname);
}
bool CFile::OpenAsLock(const std::string & fullname)
{
    CreateNewAlways(fullname);
    return true;
}

long long CFile::GetCurrentPointer() const
{
    off64_t res = lseek64(m_descriptor, 0, SEEK_CUR);
    ORTHIA_CHECK_LESS_ZERO(res, "Can't get file size");
    return (long long)res;

}
unsigned long long CFile::GetSize() const
{
    long long prevPointer = GetCurrentPointer();
    off64_t res = lseek64(m_descriptor, 0, SEEK_END);
    ORTHIA_CHECK_LESS_ZERO(res, "Can't get file size");

    CFile * pThis = const_cast<CFile*>(this);
    pThis->MoveToFirst(prevPointer);
    return (unsigned long long)res;
}
void CFile::MoveToFirst(long long offset)
{
    off64_t res = lseek64(m_descriptor, offset, SEEK_SET);
    ORTHIA_CHECK_LESS_ZERO(res, "Can't seek file");
}
void CFile::WriteToFile(const void * pBegin, size_t size)
{
    if (!size)
    {
        return;
    }
    if (size >= std::numeric_limits<unsigned int>::max())
    {
        throw std::runtime_error("Invalid size");
    }
    ssize_t res = HANDLE_EINTR(write(m_descriptor, pBegin, (unsigned int)size));
    ORTHIA_CHECK_LESS_ZERO(res, "Can't write file");
    if ((size_t)res != size)
    {
        throw std::runtime_error("Can't write file");
    }
}
void CFile::WriteToFile(const void * pBegin, const void * pEnd)
{
    WriteToFile(pBegin, (const char *)pEnd - (const char *)pBegin);
}
unsigned int CFile::Read(void * pData, unsigned int dwSize)
{
    if (!dwSize)
    {
        return 0;
    }
    ssize_t res = HANDLE_EINTR(read(m_descriptor, pData, dwSize));
    ORTHIA_CHECK_LESS_ZERO(res, "Can't read file");
    return (unsigned long)res;
}
void CFile::ExactRead(void * pData, unsigned int dwSize)
{
    unsigned int readBytes = Read(pData, dwSize);
    if (readBytes != dwSize)
    {
        throw std::runtime_error("Read failed");
    }
}
// other
void CFile::FlushBuffers()
{
    ORTHIA_CHECK_LESS_ZERO(FlushBuffers_Silent(), "Can't flush file buffer");
}
int CFile::FlushBuffers_Silent()
{
    return fsync(m_descriptor);

}

void CreateDir(const std::string & fileName, const char * pData)
{
    if (mkdir(fileName.c_str(), S_IRWXU|S_IRWXG|S_IRWXO) != 0 && errno != EEXIST)
    {
        int err = errno;
        throw orthia::CWin32Exception(pData, err);
    }
}
bool CreateDir_Silent(const std::string & path)
{
    int res = mkdir(path.c_str(), S_IRWXU|S_IRWXG|S_IRWXO);
    if (res != 0)
    {
        if (errno != EEXIST)
        {
            return false;
        }
    }
    return true;
}
bool IsFileExists(const char *pFileName)
{
    struct stat buffer;
    return (stat(pFileName, &buffer) == 0);
}
bool IsFileExists(const std::string & fileName)
{
    struct stat buffer;
    return (stat(fileName.c_str(), &buffer) == 0);
}
bool IsDirectoryExists(const std::string & dirPath)
{
    DIR * pDir = opendir(dirPath.c_str());
    if (!pDir)
    {
        return false;
    }
    closedir(pDir);
    return true;
}

static int PlatformDeleteFile_Silent(const std::string & fileName)
{
    if (unlink(fileName.c_str()) == -1)
    {
        int errorCode = errno;
        // no such file
        // as deleting is in silent mode
        // no need to return this error
        if (errorCode != ENOENT)
        {
            return errorCode;
        }
    }
    return 0;
}

void PlatformDeleteFile(const std::string & fullFileName)
{
    int status = PlatformDeleteFile_Silent(fullFileName);
    if (status)
    {
        std::stringstream res;
        res<<"Can't access file: "<<fullFileName<<", code: "<<status;
        throw std::runtime_error(res.str());
    }
}



bool DeleteFolderSilent(const orthia::PlatformString_type & sPath,  bool bDeleteRoot)
{
    int ret = 0;
    FTS *ftsp = NULL;
    FTSENT *curr;

    // Cast needed (in C) because fts_open() takes a "char * const *", instead
    // of a "const char * const *", which is only allowed in C++. fts_open()
    // does not modify the argument.
    char *files[] = { (char *) sPath.c_str(), NULL };

    // FTS_NOCHDIR  - Avoid changing cwd, which could cause unexpected behavior
    //                in multithreaded programs
    // FTS_PHYSICAL - Don't follow symlinks. Prevents deletion of files outside
    //                of the specified directory
    // FTS_XDEV     - Don't cross filesystem boundaries
    ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
    if (!ftsp) {
        ret = -1;
        goto finish;
    }

    while ((curr = fts_read(ftsp))) {
        switch (curr->fts_info) {
        case FTS_NS:
        case FTS_DNR:
        case FTS_ERR:
            break;

        case FTS_DC:
        case FTS_DOT:
        case FTS_NSOK:
            // Not reached unless FTS_LOGICAL, FTS_SEEDOT, or FTS_NOSTAT were
            // passed to fts_open()
            break;

        case FTS_D:
            // Do nothing. Need depth-first search, so directories are deleted
            // in FTS_DP
            break;

        case FTS_DP:
           if (!bDeleteRoot && curr->fts_level == FTS_ROOTLEVEL) {
               continue;
           }
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            if (remove(curr->fts_accpath) < 0) {
                ret = -1;
            }
            break;
        }
    }

finish:
    if (ftsp) {
        fts_close(ftsp);
    }
    return  (ret != -1);
}

void DeleteFolder(const orthia::PlatformString_type & sPath,  bool bDeleteRoot)
{
    if (!DeleteFolderSilent(sPath, bDeleteRoot))
    {
        throw std::runtime_error("Can't delete folder: "+sPath);
    }
}

}