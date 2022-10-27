#include "diana_win32_sysenter.h"
#include "diana_win32_core_init.h"
#include "diana_processor/diana_processor_commands.h"
#include "diana_processor/diana_processor_core_impl.h"
#include "diana_processor/diana_proc_gen.h"
#include "diana_gen.h"
#include "string.h"
#include "diana_core_gen_tags.h"
#include "diana_win32_processor.h"

#include "diana_processor/diana_processor_cmd_p.h"
#include "windows.h"
#include "diana_win32_core.h"
#include "diana_processor/diana_processor_cmd_internal.h"
#include "diana_win32_context.h"
#include "diana_win32_exceptions.h"
#include "diana_win32_executable_heap_old.h"

#define STUB_SIZE 18


#ifdef DIANA_CFG_I386 
#ifdef DIANA_CFG_USE_INLINE_ASSEMBLER

typedef struct _CallContextInfo
{
    unsigned char m_stub[STUB_SIZE]; // MUST BE FIRST!
    DI_WIN32_CONTEXT m_retContext;
    DI_WIN32_CONTEXT m_newContext;
    DianaWin32Processor * m_pProcessor;
    OPERAND_SIZE m_sysentersRIP;
    OPERAND_SIZE m_oldRSP;
}CallContextInfo;

//                         0x68, 0, 0, 0, 0,   // push $+2

static
unsigned char g_stub[]= {0x90,               // popad
                         0xCD, 0x2E,         //0x0F, 0x34,         // sysenter
                         0x68, 0, 0, 0, 0,   // push context
                         0x68, 0, 0, 0, 0,   // push processor
                         0xe8, 0, 0, 0, 0    // call out_of
};  



static 
void __stdcall sysenter_exit_impl(CallContextInfo * pInfo)
{
    DianaProcessor * pCallContext = &pInfo->m_pProcessor->m_processor;
    OPERAND_SIZE rsp = 0;

    // i want to return
    //DI_JUMP_TO_RIP(pInfo->m_sysentersRIP);
    SET_REG_RIP(pInfo->m_sysentersRIP);
    rsp = GET_REG_RSP;
    SET_REG_RSP(pInfo->m_oldRSP);

    Di_SetThreadContext(pInfo->m_retContext);
}

void DianaProcessorWin32_ReturnAfterSysenter(DianaWin32Processor * pThis)
{
    if (pThis->m_sysenterRetContext)
    {
        CallContextInfo * pInfo = (CallContextInfo * )pThis->m_sysenterRetContext;
        Di_SetThreadContext(pInfo->m_retContext);
    }
}

//------
static 
void __declspec(naked) __stdcall sysenter_exit(CallContextInfo * pInfo)
{
    __asm push [esp+4]
    __asm call DianaProcessor_LoadLiveContext32
    __asm push [esp+8]
    __asm call sysenter_exit_impl
}

static
void InitStub(CallContextInfo * pInfo)
{
    char * pCallOffset = 0;

    memcpy(pInfo->m_stub, g_stub, STUB_SIZE);
    
    *(DWORD*)(pInfo->m_stub+4) = (DWORD)pInfo;

    *(DWORD*)(pInfo->m_stub+9) = (DWORD)&pInfo->m_pProcessor->m_processor;
    
    pCallOffset = pInfo->m_stub+13;
    *(DWORD*)(pCallOffset+1) = (char*)sysenter_exit - pCallOffset - 5;
}

int Diana_Call_sysenter_ex(DianaWin32Processor * pThis)
{
    DianaProcessor * pCallContext = &pThis->m_processor;
    HANDLE hThread = 0;
    CallContextInfo * pCallContextInfo = 0;
    int result = DI_SUCCESS;
    unsigned int exit_offset = 0;

    __asm mov exit_offset, offset exit_label

        
    // -- 
    pCallContextInfo = (CallContextInfo * )
            DianaExecutableHeap_Alloc(sizeof(CallContextInfo));
    if (!pCallContextInfo)
    {
        result = DI_OUT_OF_MEMORY;
        goto clean;
    }
    memset(pCallContextInfo, 0, sizeof(*pCallContextInfo));
  
    pThis->m_sysenterRetContext = pCallContextInfo;
    DianaWin32Processor_SaveCurrentThreadProcessor(pThis);

    pCallContextInfo->m_pProcessor = pThis;
    pCallContextInfo->m_oldRSP = GET_REG_RSP;
    pCallContextInfo->m_sysentersRIP = GET_REG_RIP + pCallContextInfo->m_pProcessor->m_processor.m_result.iFullCmdSize;
    hThread = GetCurrentThread();
    
    Di_GetThreadContext(&pCallContextInfo->m_newContext);
 
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_INTEGER.
    pCallContextInfo->m_newContext.m_EDI = (DWORD)GET_REG_EDI;
    pCallContextInfo->m_newContext.m_ESI = (DWORD)GET_REG_ESI;
    pCallContextInfo->m_newContext.m_EBX = (DWORD)GET_REG_EBX;
    pCallContextInfo->m_newContext.m_EDX = (DWORD)GET_REG_EDX+8;
    pCallContextInfo->m_newContext.m_ECX = (DWORD)GET_REG_ECX;
    pCallContextInfo->m_newContext.m_EAX = (DWORD)GET_REG_EAX;
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_CONTROL.
    pCallContextInfo->m_newContext.m_EBP = (DWORD)GET_REG_EBP;
    pCallContextInfo->m_newContext.m_EFlags = (DWORD)pCallContext->m_flags.impl.l.value;

    {
    DWORD newRspValue = (DWORD)GET_REG_ESP;
    pCallContextInfo->m_newContext.m_ESP = newRspValue;
    pCallContextInfo->m_newContext.m_retESP = newRspValue;
    }
    pCallContextInfo->m_newContext.m_Eip = (DWORD)pCallContextInfo;

    InitStub(pCallContextInfo);

    Di_GetThreadContext(&pCallContextInfo->m_retContext);

    pCallContextInfo->m_retContext.m_Eip = exit_offset;

    Di_SetThreadContext(pCallContextInfo->m_newContext);
    
exit_label:

    
clean:

    if (pCallContextInfo)
    {
        DianaExecutableHeap_Free(pCallContextInfo);
    }

    pThis->m_sysenterRetContext = 0;
    DianaWin32Processor_SaveCurrentThreadProcessor(0);
    DI_CHECK(result);

    DI_PROC_END
}

int Diana_Call_sysenter(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DianaWin32Processor * pWin32Processor = DianaWin32Processor_Cast(pCallContext);
    if (!pWin32Processor)
        return DI_INVALID_CONFIGURATION;

    return Diana_Call_sysenter_ex(pWin32Processor);
}


#endif
#endif