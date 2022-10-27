#pragma once

#include "windows.h"
#include "vector"
#include "string"
#include "map"
#include "dd_list.h"
#include "diana_win32_cpp.h"

namespace dd
{
 
const int g_minPageSize = 4096;
const int g_maxDirSize = 256;

typedef char (__cdecl * fnc__itoa_type)( int value, char *buffer, int radix);  
typedef HANDLE (WINAPI * fnc_CreateFileA_type)(LPCSTR lpFileName, 
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPSECURITY_ATTRIBUTES lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        HANDLE hTemplateFile
    );
typedef BOOL (WINAPI * fnc_WriteFile_type)(HANDLE hFile,
        LPCVOID lpBuffer,
        DWORD nNumberOfBytesToWrite,
        LPDWORD lpNumberOfBytesWritten,
        LPOVERLAPPED lpOverlapped
    );    
typedef BOOL (WINAPI * fnc_CloseHandle_type)(HANDLE hFile);
typedef VOID (WINAPI * fnc_Sleep_type)(DWORD dwMilliseconds);

typedef HANDLE (WINAPI * fnc_GetProcessHeap_type)();

typedef LPVOID (WINAPI * fnc_HeapAlloc_type)(
  HANDLE hHeap,
  DWORD  dwFlags,
  SIZE_T dwBytes
);
typedef BOOL (WINAPI * fnc_HeapFree_type)(
  HANDLE hHeap,
  DWORD  dwFlags,
  LPVOID lpMem
);
typedef void * (__cdecl * fnc_memcpy_type)( void * _Dst, const void * _Src,  size_t _Size);


struct FormatType
{
    char magic[40];
    ULONG pid;
    ULONG flagsAndVersion;
    ULONGLONG patchedAddress;
    int addressReg_number;
    int sizeReg_number;
    LONG liveCounter;
    ULONG samplesCount;
    ULONG errorCode;
    char outDir[g_maxDirSize];

    // runtime info data
    HANDLE heap;
    LONG samplesToProceed;
    LONG samplesReported;
    LONG exitCommand;
    LONG saverIsReady;
    List list;

    // functions
    fnc__itoa_type fnc_itoa;
    fnc_memcpy_type fnc_memcpy;

    fnc_GetProcessHeap_type fnc_GetProcessHeap;
    fnc_HeapAlloc_type fnc_HeapAlloc;
    fnc_HeapFree_type fnc_HeapFree;
    fnc_CreateFileA_type fnc_CreateFileA;
    fnc_WriteFile_type fnc_WriteFile;
    fnc_CloseHandle_type fnc_CloseHandle;
    fnc_Sleep_type fnc_Sleep;
};

struct RegionInfo
{
    ULONGLONG baseAddress, regionSize;

    union {
        char page[g_minPageSize];
        FormatType parts;
    };
    
    RegionInfo();
    RegionInfo(ULONGLONG baseAddress_in, ULONGLONG regionSize_in, const char * page_in);
    void InitOutDir(const std::string & outDir, ULONG samplesCount);
    void VerifyMagic(ULONG pid) const;
    std::string GetDir() const;
};

class ProcessInfo
{
    ULONG m_pid;
    mutable HANDLE m_handle;

    ProcessInfo(const ProcessInfo&);
    ProcessInfo & operator =(const ProcessInfo&);

    void Close()
    {
        if (m_handle) 
        {
            CloseHandle(m_handle);
        }
    }
public:
    ProcessInfo()
        :
            m_pid(0),
            m_handle(0)
    {
    }
    ~ProcessInfo()
    {
        Close();
    }
    void Init(ULONG pid,
              HANDLE handle)
    {
        Close();
        m_pid = pid;
        m_handle = handle;
    }
    ULONG GetPid() const { return m_pid; }
    HANDLE GetHandle() const { return m_handle; }
};

void OpenProcess(ULONG pid, bool writeMode, ProcessInfo * pProcess);
ULONG ScanRegions(const ProcessInfo & process, std::vector<RegionInfo> * pRegions);
void SavePageToProcess(const ProcessInfo & process, RegionInfo & region);
void LoadPageFromProcess(const ProcessInfo & process, RegionInfo & region);

std::wstring GetModuleNameLowercase(HMODULE hModule, std::vector<wchar_t> & modNameBuffer);
void LoadModulesFromProcess(const ProcessInfo & process, std::vector<HMODULE> & modules);
void LoadModulesFromProcess(const ProcessInfo & process, std::map<std::wstring, HMODULE> & modules);
ULONGLONG SaveCodeToProcess(const ProcessInfo & process, 
                            const void * pData,
                            size_t size);
void SaveControlFieldToProcess(const ProcessInfo & process, 
                               RegionInfo & region, 
                               const char * pField,
                               size_t fieldSize);

class CPausedProcess
{
    CPausedProcess(const CPausedProcess&);
    CPausedProcess & operator = (const CPausedProcess&);

    typedef LONG (NTAPI *NtSuspendProcess)(IN HANDLE ProcessHandle);

    HANDLE m_processHandle;
    NtSuspendProcess m_NtSuspendProcess, m_NtResumeProcess;
public:
    CPausedProcess(HANDLE processHandle = 0)
        :
            m_processHandle(0)
    {
        HMODULE hNTDLL = GetModuleHandleW(L"ntdll");
        m_NtSuspendProcess = (NtSuspendProcess)GetProcAddress(hNTDLL, "NtSuspendProcess");
        if (!m_NtSuspendProcess)
        {
            throw std::runtime_error("NtSuspendProcess not found");
        }

        m_NtResumeProcess = (NtSuspendProcess)GetProcAddress(hNTDLL, "NtResumeProcess");
        if (!m_NtResumeProcess)
        {
            throw std::runtime_error("NtResumeProcess not found");
        }

        if (m_processHandle)
        {
            Init(processHandle);
        }
    }
    void Init(HANDLE processHandle)
    {
        if (m_NtSuspendProcess(m_processHandle) < 0)
        {
            throw std::runtime_error("Can't suspend process");
        }
        m_processHandle = processHandle;
    }
    ~CPausedProcess()
    {
        if (m_processHandle)
        {
            m_NtResumeProcess(m_processHandle);
        }
    }
};
}