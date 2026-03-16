#include "orthia_files.h"

namespace orthia
{
long long GetSizeOfFile(HANDLE hFile)
{
    ULARGE_INTEGER res;
    res.LowPart = GetFileSize(hFile, &res.HighPart);
    DWORD dwLastError = GetLastError();
    if (res.LowPart == INVALID_FILE_SIZE && dwLastError != NO_ERROR)
    {
        throw orthia::CWin32Exception("Can't get file size", dwLastError);
    }
    return res.QuadPart;
}

std::wstring GetTempPathWithSlash()
{
    std::vector<wchar_t> buf(1024);
    DWORD res = GetTempPathW((ULONG)buf.size(), &buf.front());
    if (!res)
    {
        ORTHIA_THROW_WIN32("Can't get temp path");
    }
    return AddSlash2(std::wstring(buf.begin(), buf.begin() + res));
}

bool IsFileExists(const std::wstring& fileName)
{
    return (GetFileAttributesW(fileName.c_str()) != INVALID_FILE_ATTRIBUTES);
}

void CreateDir(const std::wstring & path, const char * pData)
{
    BOOL res = CreateDirectory(path.c_str(), 0);
    if (res)
    {
        return;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return;
    }
    ORTHIA_THROW_WIN32(pData);
}

bool CreateDir_Silent(const std::wstring & path)
{
    BOOL res = CreateDirectory(path.c_str(), 0);
    if (res)
    {
        return true;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return true;
    }
    return false;
}


}