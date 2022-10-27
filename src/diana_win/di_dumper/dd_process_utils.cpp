#include "dd_process_utils.h"
#include "psapi.h"
#include "orthia_utils.h"
#include "orthia_allocators.h"

namespace dd
{
const char g_magic[] = "dd1-1430d49d-b517-43d9-bb14-bc4b1def1ea0";


RegionInfo::RegionInfo()
        :
            baseAddress(0),
            regionSize(g_minPageSize)
{
    memset(page, 0, sizeof(page));
    memcpy(parts.magic, g_magic, sizeof(parts.magic));
}
RegionInfo::RegionInfo(ULONGLONG baseAddress_in, ULONGLONG regionSize_in, const char * page_in)
{
    baseAddress = baseAddress_in;
    regionSize = regionSize_in;
    memcpy(page, page_in, g_minPageSize);
}
void RegionInfo::InitOutDir(const std::string & outDir, ULONG samplesCount)
{
    std::string outDir2 = outDir;
    if (outDir2.empty())
    {
        return;
    }
    if (outDir2[outDir2.size()-1] != '\\' &&
        outDir2[outDir2.size()-1] != '/')
    {
        outDir2.push_back('\\');
    }
    const int minSizeOfInt = 20;
    if (outDir2.size() > (sizeof(parts.outDir) - minSizeOfInt))
    {
        throw std::runtime_error("Path is too long");
    }

    memcpy(parts.outDir, outDir2.c_str(), outDir2.size() + 1);
    parts.samplesCount = samplesCount;
}
std::string RegionInfo::GetDir() const
{
    char outDir[g_maxDirSize];
    memcpy(outDir, parts.outDir, g_maxDirSize);
    outDir[g_maxDirSize-1] = 0;
    return outDir;
}
void RegionInfo::VerifyMagic(ULONG pid) const
{
    if (memcmp(parts.magic, g_magic, sizeof(parts.magic)) != 0 ||
        parts.pid != pid)
    {
        throw std::runtime_error("Magic/PID doesn't match");
    }
}

void OpenProcess(ULONG pid, bool writeMode, ProcessInfo * pProcess)
{
    DWORD desiredAccess = PROCESS_QUERY_INFORMATION|STANDARD_RIGHTS_REQUIRED|SYNCHRONIZE|PROCESS_VM_OPERATION|PROCESS_VM_READ;
    if (writeMode)
    {
        desiredAccess |= PROCESS_VM_WRITE|PROCESS_SUSPEND_RESUME;
    }
    HANDLE hProcess = ::OpenProcess(desiredAccess, 
        FALSE, 
        pid);
    if (!hProcess)
    {
        ORTHIA_THROW_WIN32("Can't open process");
    }
    pProcess->Init(pid, hProcess);
}

ULONG ScanRegions(const ProcessInfo & process, std::vector<RegionInfo> * pRegions)
{
    pRegions->clear();

    char page[g_minPageSize];
    const int magicSize = sizeof(g_magic)-1;

    for(SIZE_T offset = 0;; )
    {
        MEMORY_BASIC_INFORMATION memInfo;
        size_t bytes = VirtualQueryEx(process.GetHandle(),
                     (char*)offset, 
                     &memInfo,

                     sizeof(memInfo));
        if (!bytes)
        {
            if (!offset)
            {
                return ERROR_ACCESS_DENIED;
            }
            break;
        }

        if (memInfo.State == MEM_COMMIT &&
            memInfo.Type == MEM_PRIVATE &&
            (memInfo.Protect & PAGE_READWRITE)== PAGE_READWRITE &&
            memInfo.RegionSize >= g_minPageSize)
        {
            SIZE_T readBytes = 0;
            BOOL res = ReadProcessMemory(process.GetHandle(), 
                             memInfo.BaseAddress,
                             page,
                             g_minPageSize,
                             &readBytes);

            if (res && readBytes == g_minPageSize)
            {
                FormatType * pData = (FormatType * )page;
                if (memcmp(pData->magic, g_magic, magicSize) == 0 &&
                    pData->pid == process.GetPid())
                {
                    // great
                    pRegions->push_back(RegionInfo((ULONGLONG)memInfo.BaseAddress, 
                                                   g_minPageSize,
                                                   page));
                }
            }
        }
        offset = (OPERAND_SIZE)memInfo.BaseAddress +(OPERAND_SIZE)memInfo.RegionSize;
    }
    return 0;
}

void SavePageToProcess(const ProcessInfo & process, RegionInfo & region)
{
    char page[g_minPageSize] = {0, };
    memcpy(page, region.page, sizeof(region.page));

    if (!region.baseAddress || !region.regionSize)
    {
        LPVOID ptr = VirtualAllocEx(process.GetHandle(), 
                        NULL, 
                        g_minPageSize, 
                        MEM_COMMIT|MEM_TOP_DOWN, 
                        PAGE_READWRITE);
        if (!ptr)
        {
            ORTHIA_THROW_WIN32("Can't allocate process memory");
        }
        region.baseAddress = (ULONGLONG)ptr;
        region.regionSize = g_minPageSize;
    }

    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(process.GetHandle(),
                            (LPVOID)region.baseAddress,
                            page,
                            sizeof(page),
                            &bytesWritten))
    {
        ORTHIA_THROW_WIN32("Can't write process memory");
    }
    if (sizeof(page) != bytesWritten)
    {
        throw orthia::CWin32Exception("Partial write", ERROR_BAD_LENGTH);
    }
}

void SaveControlFieldToProcess(const ProcessInfo & process, 
                               RegionInfo & region,
                               const char * pField,
                               size_t fieldSize)
{
    if (!region.baseAddress)
    {
        throw orthia::CWin32Exception("Can't save control field", ERROR_BAD_ARGUMENTS);
    }
    size_t offset = (pField - region.page);
    if (offset > region.regionSize)
    {
        throw orthia::CWin32Exception("Can't save control field", ERROR_BAD_ARGUMENTS);
    }
    LPVOID pTarget = (char*)region.baseAddress + offset;
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(process.GetHandle(),
                            pTarget,
                            pField,
                            fieldSize,
                            &bytesWritten))
    {
        ORTHIA_THROW_WIN32("Can't write process memory");
    }
    if (fieldSize != bytesWritten)
    {
        throw orthia::CWin32Exception("Partial write", ERROR_BAD_LENGTH);
    }
}
void LoadPageFromProcess(const ProcessInfo & process, RegionInfo & region)
{
    if (!region.baseAddress)
    {
        throw orthia::CWin32Exception("Can't access data region", ERROR_BAD_ARGUMENTS);
    }
    SIZE_T bytesWritten = 0;
    if (!ReadProcessMemory(process.GetHandle(),
                            (LPVOID)region.baseAddress,
                            region.page,
                            sizeof(region.page),
                            &bytesWritten))
    {
        ORTHIA_THROW_WIN32("Can't read process memory");
    }
    region.VerifyMagic(process.GetPid());
}

void LoadModulesFromProcess(const ProcessInfo & process, std::vector<HMODULE> & modules)
{
    const int initialModulesCount = 1024;
    if (modules.empty())
    {
        modules.resize(initialModulesCount);
    }
    for(;;)
    {
        DWORD sizeNeeded = 0;
        BOOL res = EnumProcessModules(process.GetHandle(), 
            &modules[0], 
            (DWORD)(modules.size()*sizeof(HMODULE)), 
            &sizeNeeded);
        if (!res)
        {
            ORTHIA_THROW_WIN32("Can't enumerate process modules");
        }
        DWORD maxCount = sizeNeeded/sizeof(HMODULE);
        if (maxCount < modules.size())
        {
            modules.resize(maxCount);
            break;
        }
        modules.resize(maxCount*2);
    }
}

std::wstring Downcase(std::vector<wchar_t> & temp, DWORD dwSize)
{
    if (CharLowerBuffW( &temp.front(), dwSize)!=dwSize)
        throw std::runtime_error("Can't convert string");

    return std::wstring(&temp.front(), &temp.front() + dwSize);
}

std::wstring Downcase(const std::wstring & str, std::vector<wchar_t> & temp)
{
    if (str.empty())
        return std::wstring();

    temp.assign(str.c_str(), str.c_str() + str.size());
    DWORD dwSize = (DWORD)(str.size());

    return Downcase(temp, dwSize);
}
std::wstring Downcase(const std::wstring & str)
{
    std::vector<wchar_t> temp;
    return Downcase(str, temp);
}


std::wstring GetModuleNameLowercase(HMODULE hModule, std::vector<wchar_t> & modName)
{
    if (modName.empty())
    {
        modName.resize(4096);
    }
    if ( GetModuleFileName( hModule,
                              &modName[0],
                              (DWORD)modName.size()))
    {
        return Downcase(modName, (DWORD)wcslen(&modName[0]));
    }
    return std::wstring();
}

void LoadModulesFromProcess(const ProcessInfo & process, std::map<std::wstring, HMODULE> & modules)
{
    std::vector<wchar_t> tempBuffer;

    std::vector<HMODULE> modulesHandles;
    LoadModulesFromProcess(process, modulesHandles);

    std::vector<wchar_t> modName(4096);
    std::wstring name;
    for(std::vector<HMODULE>::iterator it = modulesHandles.begin(), it_end = modulesHandles.end();
        it != it_end;
        ++it)
    {
        // Get the full path to the module's file.
        if ( GetModuleFileNameEx( process.GetHandle(), 
                                  *it, 
                                  &modName[0],
                                  (DWORD)modName.size()))
        {
            name = Downcase(modName, (DWORD)wcslen(&modName[0]));
            modules[name] = *it;
        }
    }
}

ULONGLONG SaveCodeToProcess(const ProcessInfo & process, 
                            const void * pData,
                            size_t size)
{
    LPVOID ptr = VirtualAllocEx(process.GetHandle(), 
                    NULL, 
                    size, 
                    MEM_COMMIT|MEM_TOP_DOWN, 
                    PAGE_READWRITE);
    if (!ptr)
    {
       ORTHIA_THROW_WIN32("Can't alloc remote memory");
    }
    ULONGLONG baseAddress = (ULONGLONG)ptr;
    
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(process.GetHandle(),
                            (LPVOID)baseAddress,
                            pData,
                            size,
                            &bytesWritten))
    {
        ORTHIA_THROW_WIN32("Can't write remote memory");
    }
    if (size != bytesWritten)
    {
        throw std::runtime_error("Can't write remote memory");
    }
    orthia::RemoteMakeExecutable(process.GetHandle(), 
                                 baseAddress,
                                 size);
    return baseAddress;
}

}