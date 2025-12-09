struct IUnknown;
#include <windows.h>

typedef
size_t (__cdecl *wcslen_ptr)(
    _In_z_ wchar_t const* _String
); 

typedef
size_t(__cdecl* wcscmp_ptr)(
    _In_z_ wchar_t const* _String1,
    _In_z_ wchar_t const* _String2
    );


typedef
void* (__cdecl* memset_ptr)(void* ptr, int x, size_t n);

wcslen_ptr _wcslen;
wcscmp_ptr _wcscmp;
memset_ptr _memset;
void Print(const wchar_t * text)
{
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),
        text,
        _wcslen(text)*2,
        &written,
        NULL);
}
typedef struct _ExtraNameInfo
{
    FILE_NAME_INFO parent;
    wchar_t maxpath[MAX_PATH+1];
}ExtraNameInfo;

int mainCRTStartup(void)
{
    HMODULE hMod = GetModuleHandle(L"ntdll.dll");
    _wcslen = (wcslen_ptr)GetProcAddress(hMod, "wcslen");
    _wcscmp = (wcscmp_ptr)GetProcAddress(hMod, "wcscmp");
    _memset = (memset_ptr)GetProcAddress(hMod, "memset");
    if (!_wcslen || !_wcscmp || !_memset)
    {
        return 1;
    }
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL) 
    {
        Print(L"Can't parse arguments\n");
        return GetLastError();
    }

    if (argc != 3)
    {
        Print(L"Usage: search <filename>\n");
        return 1;
    }
    if (_wcscmp(argv[1], L"search") == 0)
    {
        WCHAR name[MAX_PATH + 1];
        HMODULE hMod = LoadLibraryEx(argv[2], 0, DONT_RESOLVE_DLL_REFERENCES);
        if (hMod == 0)
        {
            return GetLastError();
        }
        if (!GetModuleFileNameW(hMod,
            name,
            MAX_PATH))
        {
            return GetLastError();
        }
        HANDLE hFile = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ| FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            return GetLastError();
        }

        ExtraNameInfo info;
        if (!GetFileInformationByHandleEx(hFile, FileNameInfo,
            &info, sizeof(info)))
        {
            CloseHandle(hFile);
            return GetLastError();
        }
        CloseHandle(hFile);

        info.parent.FileName[info.parent.FileNameLength/2] = 0;
        for (int i = 0; name[i]; ++i)
        {
            if (name[i] == L'\\')
            {
                name[i] = 0;
                break;
            }
        }
        Print(name);
        Print(info.parent.FileName);
        Print(L"\n");
        FlushFileBuffers(GetStdHandle(STD_OUTPUT_HANDLE));
        return 0;
    }
    Print(L"Usage: unknown command\n");
    FlushFileBuffers(GetStdHandle(STD_OUTPUT_HANDLE));
    return ERROR_INVALID_FUNCTION;
}

