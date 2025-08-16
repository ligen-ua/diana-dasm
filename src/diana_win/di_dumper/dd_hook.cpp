#include "dd_hook.h"
#include "process.h"
#include "diana_core_cpp.h"
#include "dd_link.h"
#include "orthia_memory_cache.h"
#include "orthia_streams.h"
#include "orthia_allocators.h"
#include "dd_out.h"

namespace dd
{

static const char g_size_check[(sizeof(FormatType)<g_minPageSize)?1:0];

#pragma warning(disable:4748)
#pragma optimize( "", off )
#pragma check_stack( off )
#pragma runtime_checks("", off)

static
void HookAgent(FormatType * pData,
               void * pOriginalRSP,
               void ** ppInputRegisters)
{
    InterlockedIncrement(&pData->liveCounter);
    
    if (pData->samplesToProceed <= 0)
    {
        return;
    }
    // need to send data to consumer
    int newSamplesToProceed = (int)InterlockedDecrement(&pData->samplesToProceed);
    int id = pData->samplesCount - newSamplesToProceed - 1;
    if (id < 0)
    {
        // possible race 
        return;
    }
   
    // analyse input parameteres
    const void * pUserData = ppInputRegisters[pData->addressReg_number];
    const DWORD userDataSize = (DWORD)(size_t)ppInputRegisters[pData->sizeReg_number];

    // allocate data
    const DWORD fullDataSize = userDataSize + sizeof(ListNode);
    ListNode * pNode = (ListNode *)pData->fnc_HeapAlloc(pData->heap, 0, fullDataSize);
    if (!pNode)
    {
        InterlockedIncrement(&pData->samplesToProceed);
        return;
    }
    pNode->id = id;
    pNode->pData = pNode + 1;
    pNode->size = userDataSize;
    pNode->pNext = 0;
    
    pData->fnc_memcpy(pNode->pData, pUserData, userDataSize);
    // push to the list
    ListNode * pRealLast = (ListNode *)InterlockedExchangePointer((PVOID *)&pData->list.m_pLast, pNode);
    InterlockedExchangePointer((PVOID *)&pRealLast->pNext, pNode);
}

static
DWORD WINAPI HookSaver(void * pContext)
{
    FormatType * pData = (FormatType * )pContext;
    // configure the global data
    pData->samplesToProceed = pData->samplesCount;
    pData->list.m_fakeNodeAdded = 1;
    pData->list.m_pFirst = &pData->list.m_fakeNode; 
    pData->list.m_pLast = &pData->list.m_fakeNode;

    // init heap
    pData->heap = pData->fnc_GetProcessHeap();

    // prepare filename
    char outFile[g_maxDirSize];
    int zeroPozition = 0;
    for(;; ++zeroPozition)
    {
        outFile[zeroPozition] = pData->outDir[zeroPozition];
        if (!outFile[zeroPozition])
        {
            break;
        }
    }

    // report the server is ready
    InterlockedIncrement(&pData->saverIsReady);

    if (zeroPozition == 0 || !pData->samplesCount)
    {
        return 0;
    }

    // start server
    for(;;)
    {
        ListNode * pNode;
        for(;;)
        {
            pNode = 0;
            // take a first node
            ListNode * pFirst = (ListNode *)InterlockedExchangePointer((PVOID *)&pData->list.m_pFirst, 
                                                                       &pData->list.m_processingNode);
            if (pFirst == &pData->list.m_processingNode)
                continue;

            // pFirst is valid
            if (!pFirst->pNext)
            {
                // there is no second element
                if (pFirst == &pData->list.m_fakeNode)
                {
                    // cleanup
                    InterlockedExchangePointer((PVOID *)&pData->list.m_pFirst, pFirst);
                    break;
                }

               // push back fake node
               if (InterlockedIncrement(&pData->list.m_fakeNodeAdded) != 1)
               {
                   InterlockedDecrement(&pData->list.m_fakeNodeAdded);

                   // cleanup
                   InterlockedExchangePointer((PVOID *)&pData->list.m_pFirst, pFirst);
                   continue;
               }
            
               {
                    ListNode * pRealLast = (ListNode * )InterlockedExchangePointer((PVOID *)&pData->list.m_pLast, &pData->list.m_fakeNode);
                    pRealLast->pNext = &pData->list.m_fakeNode;
               }

               // still not added
               if (!pFirst->pNext)
               {
                   // cleanup
                   InterlockedExchangePointer((PVOID *)&pData->list.m_pFirst, pFirst);
                   continue;
               }
            }

           pNode = pFirst;
           InterlockedExchangePointer((PVOID *)&pData->list.m_pFirst, pFirst->pNext);
           pFirst->pNext = 0;
           break;
        }
        if (!pNode)
        {
            // no data
            if (pData->exitCommand)
            {
                break;
            }
            pData->fnc_Sleep(300);
            continue;
        }
        if (pNode != &pData->list.m_fakeNode)
        {
            // got the result, write the data
            outFile[zeroPozition] = 0;
            pData->fnc_itoa(pNode->id, outFile + zeroPozition, 10); 
                
            HANDLE hFile = pData->fnc_CreateFileA(outFile, GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 0, CREATE_ALWAYS, 0, 0);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                DWORD written = 0;
                BOOL res = pData->fnc_WriteFile(hFile, pNode->pData, (DWORD)pNode->size, &written, 0); 
                if (res)
                {
                    InterlockedIncrement(&pData->samplesReported);
                }
                pData->fnc_CloseHandle(hFile);
            }
            pData->fnc_HeapFree(pData->heap, 0, pNode);
            continue;
        }   
        InterlockedDecrement(&pData->list.m_fakeNodeAdded);
    }
    // report the exit
    InterlockedIncrement(&pData->exitCommand);
    return 0;
}


static void SelfTest2_FunctionToHook(void * pData, int size)
{
    std::string data((char*)pData, (char*)pData + size);
    std::cout<<"Test: "<<data<<"\n";
}

#pragma runtime_checks( "sc", restore )
#pragma check_stack( on )
#pragma optimize( "", on )


static void * TEST_GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    void * pRes = GetProcAddress(hModule, lpProcName);
    if (!pRes)
    {
        throw std::runtime_error(std::string("Can't find: ") + lpProcName);
    }
    return pRes;
}

static void SelfTest1()
{
    const int samplesCount = 3;
    DIANA_AUTO_PTR<RegionInfo> regionPtr(new RegionInfo());
    RegionInfo & region = *regionPtr;
    region.InitOutDir(".", samplesCount);

    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    region.parts.fnc_itoa = (fnc__itoa_type)TEST_GetProcAddress(ntdll, "_itoa");
    region.parts.fnc_memcpy = (fnc_memcpy_type)TEST_GetProcAddress(ntdll, "memcpy");

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    region.parts.fnc_GetProcessHeap = (fnc_GetProcessHeap_type)TEST_GetProcAddress(kernel32, "GetProcessHeap");
    region.parts.fnc_HeapAlloc = (fnc_HeapAlloc_type)TEST_GetProcAddress(kernel32, "HeapAlloc");
    region.parts.fnc_HeapFree = (fnc_HeapFree_type)TEST_GetProcAddress(kernel32, "HeapFree");
    region.parts.fnc_CreateFileA = (fnc_CreateFileA_type)TEST_GetProcAddress(kernel32, "CreateFileA");
    region.parts.fnc_WriteFile = (fnc_WriteFile_type)TEST_GetProcAddress(kernel32, "WriteFile");
    region.parts.fnc_CloseHandle = (fnc_CloseHandle_type)TEST_GetProcAddress(kernel32, "CloseHandle");
    region.parts.fnc_Sleep = (fnc_Sleep_type)TEST_GetProcAddress(kernel32, "Sleep");

    region.parts.addressReg_number = 0;
    region.parts.sizeReg_number = 1;

    // yes this thing leaks, but it is important to make sure we work with API:
    DWORD tid = 0;
    HANDLE hThread = (HANDLE)CreateThread(0,0, HookSaver, region.page, 0, &tid);

    // wait for initialization
    int tryCount = 100;
    for(int i = 0; i < tryCount; ++i)
    {
        Sleep(100);
        if (region.parts.saverIsReady)
        {
            break;
        }
    }
    // simulate activity
    void * pRegs[2];
    std::string data[samplesCount] = {
            "part1", "part2", "part3"
    }; 
    for(int i = 0; i < samplesCount; ++i)
    {
        pRegs[0] = (void*)data[i].c_str();
        pRegs[1] = (void*)data[i].size();
        HookAgent(&region.parts, 0, pRegs);
        data[i].clear();
    }

    // wait till done
    for(int i = 0; i < tryCount; ++i)
    {
        Sleep(100);
        if (region.parts.samplesReported == samplesCount)
        {
            break;
        }
    }

    // exit server
    InterlockedExchange(&region.parts.exitCommand, 1);
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
}


static
size_t CalcFunctionSize(const void * pFnc)
{
    const size_t * p = (const size_t * )pFnc;
    for(int i = 0; i < 1024; ++i)
    {
        if (p[i] == (size_t)0xCCCCCCCCCCCCCCCCULL)
        {
            return (const char *)(p+i)-(const char *)pFnc;
        }
    }
    throw std::runtime_error("Can't find the end of function");
}


class CRemoteProcessorMemoryReader: public orthia::IMemoryReader
{
    HANDLE m_hProcess;
public:
    CRemoteProcessorMemoryReader(HANDLE hProcess)
        :
            m_hProcess(hProcess)
    {
    }
    virtual void Read(orthia::Address_type offset, 
                      orthia::Address_type bytesToRead,
                      void * pBuffer,
                      orthia::Address_type * pBytesRead,
                      int flags,
                      orthia::Address_type selectorValue,
                      DianaUnifiedRegister selectorHint)
    {

        SIZE_T bytesWritten = 0;
        if (ReadProcessMemory(m_hProcess,
                            (LPVOID)offset,
                            pBuffer,
                            (ULONG)bytesToRead,
                            &bytesWritten))
        {
            *pBytesRead = bytesWritten;
        }
        else
        {
            dd::error_out()<<"Can't access memory at "<<std::hex<<offset<<", "<<bytesToRead<<" bytes";
        }
    }
};

class CProcessPatcher:public orthia::IVmMemoryRangesTarget
{
    HANDLE m_hProcess;
public:
    CProcessPatcher(HANDLE hProcess)
        :
            m_hProcess(hProcess)
    {
    }
    virtual void OnRange(const orthia::VmMemoryRangeInfo & vmRange,
                         const char * pDataStart)
    {
        if (!vmRange.HasData())
            return;
     
        SIZE_T bytesWritten = 0;
        if (!WriteProcessMemory(m_hProcess,
                                (LPVOID)vmRange.address,
                                pDataStart,
                                (ULONG)vmRange.size,
                                &bytesWritten))
        {
            ORTHIA_THROW_WIN32("Can't write remote memory");
        }
        if (vmRange.size != bytesWritten)
        {
            throw std::runtime_error("Can't write remote memory");
        }
        if (!FlushInstructionCache(m_hProcess, 
                                   (LPVOID)vmRange.address, 
                                   (ULONG)vmRange.size))
        {
            ORTHIA_THROW_WIN32("FlushInstructionCache failed");
        }

        dd::debug_out()<<" - patched: "<<std::hex<<vmRange.address<<", "<<(ULONG)vmRange.size<<" bytes";
    }
};

ULONGLONG HookProcess(const ProcessInfo & process, 
                 ULONGLONG patchAddress, 
                 int addressReg_number,
                 int sizeReg_number,
                 const std::string & outPath,
                 ULONG samplesCount)
{    
    dd::debug_out()<<"Patch address: "<<std::hex<<patchAddress;

    RegionInfo region;
    region.InitOutDir(outPath, samplesCount);

    if (region.parts.outDir[0])
    {
        dd::debug_out()<<"Working directory: "<<region.parts.outDir;
    }
    region.parts.pid = process.GetPid();
    region.parts.addressReg_number = addressReg_number;
    region.parts.sizeReg_number = sizeReg_number;
    
    LoadRemoteFunctions(process, region);
    dd::debug_out()<<"Remote functions OK";

    const size_t hookAgentSize = CalcFunctionSize(HookAgent);
    dd::debug_out()<<"Size of agent: "<<std::hex<<hookAgentSize;

    const size_t hookSaverSize = CalcFunctionSize(HookSaver);
    dd::debug_out()<<"Size of server thread: "<<std::hex<<hookSaverSize;

    // deploy handler
    if (region.parts.saverIsReady)
    {
        throw std::runtime_error("Internal error");
    }

    SavePageToProcess(process, region);

    dd::debug_out()<<"Control block deployed at "<<std::hex<<region.baseAddress;
    ULONGLONG hookSaverRemoteAddress = SaveCodeToProcess(process, 
                            HookSaver,
                            hookSaverSize);

    DWORD tid = 0;
    HANDLE hThread = CreateRemoteThread(process.GetHandle(), 
        0, 
        0, 
        (LPTHREAD_START_ROUTINE)hookSaverRemoteAddress, 
        (LPVOID)region.baseAddress, 
        0, 
        &tid); 
    if (!hThread)
    {
        ORTHIA_THROW_WIN32("Can't create remote thread");
    }
    dd::debug_out()<<"Thread ID: "<<tid;
    diana::Guard<diana::Win32Handle> threadGuard(hThread);

    for(;;)
    {
        LoadPageFromProcess(process, region);
        
        if (region.parts.saverIsReady)
        {
            break;
        }
        if (WaitForSingleObject(hThread, 300) == WAIT_OBJECT_0)
        {
            throw std::runtime_error("Thread has exited");
        }
    }
    dd::debug_out()<<"Thread is OK";
    try
    {
        // deploy the hook body
        ULONGLONG agentRemoteAddress = SaveCodeToProcess(process, 
                                                        HookAgent,
                                                        hookAgentSize);

        // hook the target
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);

        CRemoteProcessorMemoryReader memReader(process.GetHandle());
        orthia::CMemoryStorageOfModifiedData writeCache(&memReader, sysInfo.dwPageSize);

        orthia::DianaAnalyzerReadWriteStream rwStream(&writeCache);
        
        orthia::CProcessAllocator processAllocator(process.GetHandle(), true);
        orthia::PatcherHookAllocator allocator(&processAllocator);
        DianaHook_TargetMemoryProvider memProvider;
        DianaHook_TargetMemoryProvider_Init(&memProvider,
                                            &rwStream,
                                            &allocator);

        DianaHook_CustomOptions customOptions = {0,};
        customOptions.patchContext = region.baseAddress;
        int res = DianaHook_PatchStream(&memProvider,
                                        sizeof(void*),
                                        patchAddress,
                                        agentRemoteAddress, 
                                        &customOptions);
        if (res)
        {
            throw diana::CException(res, "Can't hook function");
        }


        dd::debug_out()<<"Patching...";

        dd::CPausedProcess pausedProcess;
        if (process.GetPid() != GetCurrentProcessId())
        {
            pausedProcess.Init(process.GetHandle());
        }
        CProcessPatcher patcher(process.GetHandle());
        if (!writeCache.ReportRegions(0,
                                      MAXUINT64,
                                      &patcher,
                                      false)) 
        {
            throw std::runtime_error("Can't patch the process");
        }
        dd::debug_out()<<"Patching OK";

        processAllocator.MakeExecutable();
        processAllocator.Release();

        return region.baseAddress;
    }
    catch(...)
    {
        // cleanup the resoruces
        try
        {
            dd::debug_out()<<"Error happens, stopping the thread";

            region.parts.exitCommand = 1;
            SaveControlFieldToProcess(process, region, (const char*)&region.parts.exitCommand, sizeof(region.parts.exitCommand));

            if (WaitForSingleObject(hThread, 5000) == WAIT_OBJECT_0)
            {   
                dd::debug_out()<<"Thread stopped";

                if (VirtualFreeEx(process.GetHandle(), 
                                 (PVOID)region.baseAddress,
                                 0,
                                 MEM_RELEASE))
                {
                    dd::debug_out()<<"Resources are clean";
                }
                else
                {
                    dd::debug_out()<<"Can't clean the region: "<<GetLastError();
                }
            }
            else
            {
                dd::debug_out()<<"Can't stop the thread, timeout reached";
            }
        }
        catch(std::exception & )
        {
        }
        throw;
    }
}

static void SelfTest2()
{
    ProcessInfo processInfo;
    OpenProcess(GetCurrentProcessId(), true, &processInfo);
    ULONGLONG baseAddress = HookProcess(processInfo, 
                (ULONGLONG)&SelfTest2_FunctionToHook,
                7,
                6,
                ".",
                2);
    SelfTest2_FunctionToHook("hello", 5);
    SelfTest2_FunctionToHook("world", 5);

    for(;;)
    {
        dd::RegionInfo region;
        region.baseAddress = baseAddress;
        LoadPageFromProcess(processInfo, region);
    
        if (region.parts.samplesReported == 2)
            break;

        Sleep(300);
    }
}

void SelfTest()
{
    dd::VerboseDebugOn();
    SelfTest1();
    SelfTest2();
}

}