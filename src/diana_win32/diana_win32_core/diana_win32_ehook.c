#include "diana_win32_ehook.h"
#include "diana_win32_executable_heap_old.h"
#include "diana_patchers.h"
#include "diana_win32_exceptions.h"
#include "diana_processor.h"
#include "diana_streams.h"
#include "diana_gen.h"
#include "diana_allocators.h"
#include "diana_processor/diana_processor_cmd_internal.h"

#define DEFAULT_STACKSIZE  (1024*1024)

#ifdef DIANA_CFG_I386 
#ifdef DIANA_CFG_USE_INLINE_ASSEMBLER

struct HookContext
{
    int argSize; // must be first
    void * pFunction;
    int defaultErrorCode;
};

static
int EmulateFunctionCall(void * pFunction,
                        int argSize,
                        void * pOriginalESP,
                        int * pNewEAX)
{
    OPERAND_SIZE exitOp = 0x87654321;
    int status = 0;
    DianaWin32Processor proc;
    size_t newStackSize = DEFAULT_STACKSIZE;
    int i = 0;
    char * pNewStack =0, *pNewStackEnd = 0;
    DianaProcessor * pCallContext = 0;

    pNewStack = malloc(newStackSize);
    pNewStackEnd = pNewStack + newStackSize;
    DI_CHECK_ALLOC(pNewStack);
    *pNewEAX = 0;

    DI_CHECK_GOTO(DianaWin32Processor_Init(&proc, 
                                         (OPERAND_SIZE)pNewStack, 
                                         (OPERAND_SIZE)pNewStackEnd));
    
    pCallContext = &proc.m_processor;

    DianaProcessor_LoadLiveContext32(pCallContext);

    SET_REG_RSP((OPERAND_SIZE)pNewStackEnd);
  
    {
        // pass arguments
        int u = 0;
        int count = argSize/4;
        for(;u<count;++u)
        {
            OPERAND_SIZE value = ((int*)pOriginalESP)[count - u];
            DI_CHECK_GOTO(diana_internal_push(pCallContext, &value));
        }
    }

    DI_CHECK_GOTO(diana_internal_push(pCallContext, &exitOp));
    DI_JUMP_TO_RIP((OPERAND_SIZE)pFunction);

    for(i = 0; ; ++i)
    {
        OPERAND_SIZE rip = GET_REG_RIP;
        if (rip == exitOp)
            break;

        status = DianaProcessor_ExecOnce(pCallContext);
        if (status != DI_SUCCESS)
        {
            OPERAND_SIZE failedRip  = GET_REG_RIP;
            break;
        }
    }
    *pNewEAX = (int)GET_REG_EAX;
cleanup:
    if (pNewStack)
        free(pNewStack);
    return status;
}

static
void __stdcall ehook_gate(void * pContext_in, 
                          void * pOriginalESP, 
                          void * pInputRegisters)
{
    struct HookContext * pContext = (struct HookContext * )pContext_in;
    int newEax = 0;
    
    if (EmulateFunctionCall(pContext->pFunction, pContext->argSize, pOriginalESP, &newEax))
    {
        newEax = pContext->defaultErrorCode;
    }

    __asm 
    {
         // save error code
         mov eax, newEax

         // move out from our function
         mov esp, ebp
         pop ebp

         // clean: 3 params, EIP
         add esp, 0x10

         // restore flags and all registers but save new EAX
         // see Diana_Hook for more information
         popfd
         mov [esp + 0x1C], eax
         popad

         // move out from Diana_Hook
         mov esp, ebp
         pop ebp
         add esp, 0xc

         // start to emulate retn
         push eax

         // <get argument size>
         mov eax, [esp - 4] 
         mov eax, [eax] 
         mov eax, [eax] // eax = m_argumentsSize
         // </get argument size>

         pushfd
         push esi
         push edi    
         push ecx

         lea esi, [esp+0x14]
         lea edi, [esp+0x14]
         add edi, eax

         // copy params
         std
         mov ecx, eax 
         add ecx, 6
         rep movsd

         add esp, eax
         pop ecx
         pop edi
         pop esi
         popfd
         pop eax
         ret
    }
}


int DianaWin32Processor_EhookSTD(void * pPlaceToHook,
                                 int argSize,
                                 int defaultErrorCode)
{
    int res = 0;
    struct HookContext * pHookContext = 0;
    DianaExecutableHeapAllocator allocator;
    Diana_InitHeapAllocator(&allocator);

    DI_CHECK(Diana_EnsureWriteAccess(pPlaceToHook));

    pHookContext = malloc(sizeof(struct HookContext));
    DI_CHECK_ALLOC(pHookContext);
    pHookContext->pFunction = 0;
    pHookContext->argSize = argSize;
    pHookContext->defaultErrorCode = defaultErrorCode;

    res = Diana_PatchSomethingEx(pPlaceToHook,
                                 1024,
                                 ehook_gate,
                                 pHookContext,
                                 &allocator.m_parent,
                                 &pHookContext->pFunction);
    return res;
}



#endif
#endif