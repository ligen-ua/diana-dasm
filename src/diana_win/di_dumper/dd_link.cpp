#include "dd_link.h"
#include "dd_process_utils.h"
#include "psapi.h"
#include "orthia_utils.h"

namespace dd
{

struct ModuleInfo
{
    HMODULE handle;
    std::wstring fullName;
    std::wstring name;

    MODULEINFO info;

    bool IsInside(void * pFunction) const
    {
        return (pFunction > handle && pFunction < (char*)(handle) + info.SizeOfImage);
    }
};

static
void QueryLocalModuleInfo(const std::wstring & name, std::vector<wchar_t> & modNameBuffer, ModuleInfo * pInfo)
{
    HMODULE dll = GetModuleHandleW(name.c_str());
    if (!dll)
    {
        throw std::runtime_error("Module not found: " + orthia::ToAnsiString_Silent(name));
    }
    pInfo->handle = dll;
    pInfo->name = name;
    pInfo->fullName = GetModuleNameLowercase(dll, modNameBuffer);


    if (!GetModuleInformation(GetCurrentProcess(),
                            dll,
                            &pInfo->info,
                            sizeof(pInfo->info)))
    {
        throw std::runtime_error("Can't get module info: " + orthia::ToAnsiString_Silent(name));
    }
}

static const wchar_t * g_dllNames[] = {L"ntdll.dll", L"kernel32.dll", L"kernelbase.dll"};
static const size_t g_dllNamesSize = sizeof(g_dllNames)/sizeof(g_dllNames[0]);


typedef std::map<HMODULE, ModuleInfo*> LocalModulesMap;
typedef std::map<std::wstring, HMODULE> RemoteModulesMap;
class CRemoteFncResolver
{
    const LocalModulesMap & m_localModules;
    const RemoteModulesMap & m_remoteModules;

public:
    CRemoteFncResolver(const LocalModulesMap & localModules_in,
                       const RemoteModulesMap & remoteModules_in)
        : 
            m_localModules(localModules_in),
            m_remoteModules(remoteModules_in)
    {
    }

    void * GetRemoteAddressPtr(HMODULE dll, const char * pFunctionName)
    {
        LocalModulesMap::const_iterator moduleIt = m_localModules.find(dll);
        if (moduleIt == m_localModules.end())
        {
            throw std::runtime_error(std::string("Can't find function module: ") + pFunctionName);
        }

        void * pFunction = ::GetProcAddress(dll, pFunctionName);
        if (!pFunction)
        {
            throw std::runtime_error(std::string("Can't find local function: ") + pFunctionName);
        }
        if (!moduleIt->second->IsInside(pFunction))
        {
            // try nearest
            moduleIt = m_localModules.lower_bound((HMODULE)pFunction);
            if (moduleIt == m_localModules.begin())
            {
                throw std::runtime_error(std::string("Can't find module for a function: ") + pFunctionName);
            }
            --moduleIt;
            if (!moduleIt->second->IsInside(pFunction))
            {
                throw std::runtime_error(std::string("Can't find module for a function: ") + pFunctionName);
            }        
        }
        // link with remote
        RemoteModulesMap::const_iterator remoteIt = m_remoteModules.find(moduleIt->second->fullName);
        if (remoteIt == m_remoteModules.end())
        {
            throw std::runtime_error(std::string("Can't find remote module: ") + orthia::ToAnsiString_Silent(moduleIt->second->fullName));
        }
        HMODULE remoteDll = remoteIt->second;
        size_t localDiff = (const char*)pFunction - (const char*)moduleIt->first;
        return (char*)remoteDll + localDiff;
    }

    template <class FncPtr>
    void ApplyRemoteAddress(FncPtr & outParam, HMODULE dll, const char * pFunctionName)
    {
        outParam = (FncPtr)GetRemoteAddressPtr(dll, pFunctionName);
    }

};

void LoadRemoteFunctions(const ProcessInfo & process, 
                         RegionInfo & region)
{
    std::vector<wchar_t> modNameBuffer;

    // query local modules
    ModuleInfo localModules[g_dllNamesSize];
    LocalModulesMap localModulesMap;
    for(size_t i = 0; i < g_dllNamesSize; ++i)
    {
        QueryLocalModuleInfo(g_dllNames[i], modNameBuffer, &localModules[i]);
        localModulesMap[localModules[i].handle] = &localModules[i];
    }

    // query remote
    std::map<std::wstring, HMODULE> remoteModules;
    LoadModulesFromProcess(process, remoteModules);

    HMODULE ntdll = localModules[0].handle;
    HMODULE kernel32 = localModules[1].handle;

    CRemoteFncResolver remoteFncResolver(localModulesMap, remoteModules);
    remoteFncResolver.ApplyRemoteAddress(region.parts.fnc_itoa, ntdll, "_itoa");
    remoteFncResolver.ApplyRemoteAddress(region.parts.fnc_memcpy, ntdll, "memcpy");

    remoteFncResolver.ApplyRemoteAddress(region.parts.fnc_GetProcessHeap, kernel32, "GetProcessHeap");
    remoteFncResolver.ApplyRemoteAddress(region.parts.fnc_HeapAlloc, kernel32, "HeapAlloc");
    remoteFncResolver.ApplyRemoteAddress(region.parts.fnc_HeapFree, kernel32, "HeapFree");
    remoteFncResolver.ApplyRemoteAddress(region.parts.fnc_CreateFileA, kernel32, "CreateFileA");
    remoteFncResolver.ApplyRemoteAddress(region.parts.fnc_WriteFile, kernel32, "WriteFile");
    remoteFncResolver.ApplyRemoteAddress(region.parts.fnc_CloseHandle, kernel32, "CloseHandle");
    remoteFncResolver.ApplyRemoteAddress(region.parts.fnc_Sleep, kernel32, "Sleep");
}

}