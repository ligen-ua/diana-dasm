#include "diana_win32_exceptions.h"
#include "windows.h"
#include "diana_core.h"
#include "diana_patchers.h"
#include "diana_allocators.h"
#include "diana_win32_sysenter.h"
#include "diana_win32_executable_heap_old.h"

static ULONG g_TlsProcessorIndex = 0;
static int g_exceptionSupportEnabled = 0;
static void * g_pNtContinue = 0;
static void * g_pNtRaiseException = 0;

#ifdef DIANA_CFG_I386 
#ifdef DIANA_CFG_USE_INLINE_ASSEMBLER

static void __stdcall hooked_KiUserExceptionDispatcher(void * pContext, void * pOriginalESP)
{
    void * pProcessor = TlsGetValue(g_TlsProcessorIndex);
    DianaWin32Processor * pRealProcessor = pProcessor;

    if (!g_exceptionSupportEnabled)
        return;

    if (!pRealProcessor)
        return;

    // need to go to 
    if (pRealProcessor->m_sysenterRetContext)
    {
        DianaProcessor * pCallContext = &pRealProcessor->m_processor;
        DianaProcessor_LoadLiveContext32(&pRealProcessor->m_processor);
        DianaProcessorWin32_ReturnAfterSysenter(pRealProcessor);
    }
}

static void __stdcall hooked_NtRaiseException(void * pContext, void * pOriginalESP)
{
    void * pProcessor = TlsGetValue(g_TlsProcessorIndex);
    DianaWin32Processor * pRealProcessor = pProcessor;

    if (!g_exceptionSupportEnabled)
        return;

    if (!pRealProcessor)
        return;
}

int Diana_EnsureWriteAccess(void * pData)
{
    MEMORY_BASIC_INFORMATION mem;
    ULONG dwOld = 0;

    if (!VirtualQuery(pData, &mem, sizeof(mem)) )
        return DI_WIN32_ERROR;

    if (!VirtualProtect(mem.BaseAddress, mem.RegionSize, PAGE_EXECUTE_READWRITE, &dwOld))
        return DI_WIN32_ERROR;
        
    return DI_SUCCESS;
}

int  DianaWin32Processor_InitExceptions()
{
    HANDLE hNtDll = GetModuleHandleW(L"ntdll.dll");
    void * pKiUserExceptionDispatcher = 0;
    int res  = 0;
    DianaExecutableHeapAllocator allocator;

    Diana_InitHeapAllocator(&allocator);

    g_TlsProcessorIndex = TlsAlloc();
    if (g_TlsProcessorIndex == TLS_OUT_OF_INDEXES)
    {
        return DI_OUT_OF_MEMORY;
    }


    pKiUserExceptionDispatcher = GetProcAddress(hNtDll, "KiUserExceptionDispatcher");
    g_pNtRaiseException = GetProcAddress(hNtDll, "NtRaiseException");
    g_pNtContinue = GetProcAddress(hNtDll, "NtContinue");

    DI_CHECK(Diana_EnsureWriteAccess(pKiUserExceptionDispatcher));

    res = Diana_PatchSomething32(pKiUserExceptionDispatcher,
                                 1024,
                                 hooked_KiUserExceptionDispatcher,
                                 0,
                                 &allocator.m_parent,
                                 0);
    DI_CHECK(res);

    g_exceptionSupportEnabled = 1;
    return DI_SUCCESS;
}

void DianaWin32Processor_SaveCurrentThreadProcessor(DianaWin32Processor * pRealProcessor)
{
    TlsSetValue(g_TlsProcessorIndex, pRealProcessor);
}

static void NtContinue(struct _dianaProcessorFirePoint * pPoint,
                       struct _dianaProcessor * pProcessor)
{
    DianaProcessor * pCallContext = pProcessor;
    unsigned long * pSP = (unsigned long * )GET_REG_RSP;
    
    CONTEXT * pContext = (CONTEXT * )pSP[1];
    DianaProcessor_LoadContext32(pProcessor, pContext);
}

static void * RestoreContextAndGetOriginalEIP()
{
    void * pProcessor = TlsGetValue(g_TlsProcessorIndex);
    DianaWin32Processor * pRealProcessor = pProcessor;
    DianaProcessor * pCallContext = &pRealProcessor->m_processor;

    DianaProcessor_LoadLiveContext32(pCallContext);

    DianaProcessorWin32_ReturnAfterSysenter(pRealProcessor);
    return (void*)pRealProcessor->m_lastContextRIP;
}

static __declspec(naked) void NtRaiseExceptionRetStub()
{
    __asm {

    push ebp
    mov ebp, esp
    pushad

    call RestoreContextAndGetOriginalEIP
    mov [ebp + 4], eax

    popad
    mov esp, ebp
    pop ebp
    ret
    }
}

static void NtRaiseException(struct _dianaProcessorFirePoint * pPoint,
                             struct _dianaProcessor * pProcessor)
{
    DianaProcessor * pCallContext = pProcessor;
    DianaWin32Processor * pWin32Processor = DianaWin32Processor_Cast(pProcessor);
    unsigned long * pSP = (unsigned long * )GET_REG_RSP;
    
    EXCEPTION_RECORD * pExceptionRecord = (EXCEPTION_RECORD * )pSP[1];
    CONTEXT * pContext = (CONTEXT * )pSP[2];

    pWin32Processor->m_lastContextRIP = pContext->Eip;
    pContext->Eip = (DWORD)NtRaiseExceptionRetStub;
}


int DianaWin32Processor_InitProcessorExceptions(DianaWin32Processor * pThis)
{
    DianaProcessorFirePoint point;
    point.pContext = 0;
    point.address = (OPERAND_SIZE)g_pNtContinue;
    point.action = NtContinue;

    DI_CHECK(DianaProcessor_RegisterFirePoint(&pThis->m_processor,
                                              &point));

    point.address = (OPERAND_SIZE)g_pNtRaiseException;
    point.action = NtRaiseException;
    return DianaProcessor_RegisterFirePoint(&pThis->m_processor,
                                            &point);
}


#endif
#endif