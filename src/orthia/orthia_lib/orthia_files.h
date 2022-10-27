#ifndef ORTHIA_FILES_H
#define ORTHIA_FILES_H

#include "windows.h"
#include "algorithm"
#include "orthia_utils.h"

namespace orthia
{

inline long long GetSizeOfFile(HANDLE hFile)
{
    ULARGE_INTEGER res;
    res.LowPart = GetFileSize(hFile, &res.HighPart);
    DWORD dwLastError = GetLastError();
    if (res.LowPart == INVALID_FILE_SIZE  &&  dwLastError!= NO_ERROR)
    {
        throw orthia::CWin32Exception("Can't get file size", dwLastError);
    }
    return res.QuadPart;
}

const ULONG g_share_all = FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
const ULONG g_share_read = FILE_SHARE_READ;

class CFile
{
    HANDLE m_handle;

    CFile(const CFile & );
    CFile & operator = (const CFile&);
public:
    explicit CFile(HANDLE handle=0)
        : m_handle(handle)
    {
        if (handle == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Can't open file");
    }
    void swap(CFile & file)
    {
        std::swap(m_handle, file.m_handle);
    }
    void Open(const std::wstring & fullname, 
              DWORD dwAccessRights, 
              DWORD dwShareMode,
              DWORD dwCreationDisposition,
              DWORD dwFlagsAndAttributes = 0)
    {
        HANDLE handle = CreateFileW(fullname.c_str(), 
            dwAccessRights, 
            dwShareMode,
            0,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            0);
        if (handle == INVALID_HANDLE_VALUE)
        {
            ThrowError(GetLastError());
        }
        Close();
        m_handle = handle;
    }

    void ThrowError(ULONG dwLastError)
    {
        throw orthia::CWin32Exception("Can't open file", dwLastError);
    }

    ULONG Open_Silent(const std::wstring & fullname, 
              DWORD dwAccessRights, 
              DWORD dwShareMode,
              DWORD dwCreationDisposition,
              DWORD dwFlagsAndAttributes = 0)
    {
        HANDLE handle = CreateFileW(fullname.c_str(), 
            dwAccessRights, 
            dwShareMode,
            0,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            0);
        if (handle == INVALID_HANDLE_VALUE)
        {
            DWORD dwLastError = GetLastError();
            return dwLastError;
        }
        Close();
        m_handle = handle;
        return 0;
    }
    ~CFile()
    {
        Close();
    }
    unsigned __int64 GetSize() const 
    {
        return GetSizeOfFile(m_handle);
    }
    HANDLE GetHandle()
    {
        return m_handle;
    }
    void Close()
    {
        if (m_handle)
        {
            CloseHandle( m_handle );
        }
        m_handle = 0;
    }

    void SetEOF()
    {
        if (!SetEndOfFile(m_handle))
        {
            ORTHIA_THROW_WIN32("Can't set EOF");
        }
    }
    LONG SetEOF_Silent()
    {
        if (!SetEndOfFile(m_handle))
        {
            return GetLastError();
        }
        return 0;
    }
    void MoveToFirst(long long offset)
    {
        LARGE_INTEGER distance, newFp;
        distance.QuadPart = offset;

        if (!SetFilePointerEx(m_handle, 
                         distance,
                         &newFp,
                         FILE_BEGIN))
        {
            ORTHIA_THROW_WIN32("Can't access file");
        }
    }

    void WriteToFile(const void * pBegin, size_t size)
    {
        WriteToFile(pBegin, (char*)pBegin+size);
    }
    void WriteToFile(const void * pBegin, const void * pEnd)
    {
        if (pBegin > pEnd)
            throw std::runtime_error("Can't write file: invalid arguments");

        DWORD dwSize = (DWORD)((char*)pEnd-(char*)pBegin);
        DWORD dwWasSize = 0;
        if (!WriteFile(m_handle, pBegin, dwSize, &dwWasSize, 0))
        {
            DWORD dwLastError = GetLastError();
            throw orthia::CWin32Exception("Can't write file", dwLastError);
        }
        if (dwWasSize!=dwSize)
            throw std::runtime_error("Can't write file: partial write");
    }
    ULONG WriteToFile_Silent(const void * pBegin, size_t size)
    {
        DWORD dwSize = (DWORD)size;
        DWORD dwWasSize = 0;
        if (!WriteFile(m_handle, pBegin, dwSize, &dwWasSize, 0))
        {
            DWORD dwLastError = GetLastError();
            return dwLastError;
        }
        if (dwWasSize!=dwSize)
        {
            return ERROR_RECEIVE_PARTIAL;
        }
        return NO_ERROR;
    }
    ULONG Read(void * pData, ULONG dwSize)
    {
        DWORD read = 0;
        if (!ReadFile(m_handle, pData, dwSize, &read, 0))
        {
            ORTHIA_THROW_WIN32("ReadFile failed");
        }
        return read;
    }

    void ExactRead(void * pData, ULONG dwSize)
    {
        DWORD read = 0;
        if (!ReadFile(m_handle, pData, dwSize, &read, 0))
        {
            ORTHIA_THROW_WIN32("Can't read file");
        }
        if (read != dwSize)
        {
            throw std::runtime_error("Can't read file: partial read");
        }
    }

    void FlushBuffers()
    {
        if (!FlushFileBuffers(m_handle))
        {
            ORTHIA_THROW_WIN32("FlushFileBuffers failed");
        }
    }
};

inline 
void LoadFileToVector(const std::wstring& fileName, 
                      std::vector<char> & data)
{
    data.clear();
    CFile file;
    file.Open(fileName, 
              GENERIC_READ, 
              FILE_SHARE_READ|FILE_SHARE_WRITE, 
              OPEN_EXISTING);
    

    long long fileSize = file.GetSize();

    if (fileSize > (long long)(1024*1024*1024))
    {
        throw std::runtime_error("File too big");
    }
    data.resize((size_t)fileSize);

    if (!fileSize)
        return;

    char * pData = &data[0];
    file.ExactRead(pData, (ULONG)fileSize);
}

inline bool IsFileExists(const std::wstring& fileName)
{
    return (GetFileAttributesW(fileName.c_str()) != INVALID_FILE_ATTRIBUTES);
}
}
#endif