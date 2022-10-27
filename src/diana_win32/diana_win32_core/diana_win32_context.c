#include "diana_win32_context.h"

#ifdef DIANA_CFG_I386 
#ifdef DIANA_CFG_USE_INLINE_ASSEMBLER

void __declspec(naked) __stdcall
Di_SetThreadContext(DI_WIN32_CONTEXT context)
{
    __asm pop eax

    __asm pop edi
    __asm sub edi, 40
    __asm push edi
    __asm mov esi, esp
    __asm add esi, 4
    __asm mov ecx, 40
    __asm rep movsb
    
    __asm pop esp

    __asm popfd
    __asm popad

    __asm ret
}


// Di_GetThreadContextImpl stack:
//
    // eflags
    // EDI
    // ESI
    // EBP
    // ESP
    // EBX
    // EDX
    // ECX
    // EAX
    // old_ebp
    // ret_address
    // pThis

static 
void  __stdcall Di_GetThreadContextImpl(DI_WIN32_CONTEXT * pCallContext, 
                                        DI_UINT32 * pParams)
{
    int paramNum = 0;    

    pCallContext->m_EFlags = pParams[paramNum++];
    
    pCallContext->m_EDI = pParams[paramNum++];
    pCallContext->m_ESI = pParams[paramNum++];
    paramNum++;
    pCallContext->m_ESP = pParams[paramNum++]+0xC;
    pCallContext->m_EBX = pParams[paramNum++];
    pCallContext->m_EDX = pParams[paramNum++];
    pCallContext->m_ECX = pParams[paramNum++];
    pCallContext->m_EAX = pParams[paramNum++];

    pCallContext->m_EBP = pParams[paramNum++];
    pCallContext->m_Eip = pParams[paramNum++];

    pCallContext->m_retESP = pCallContext->m_ESP;
}
__declspec(naked) void  __stdcall Di_GetThreadContext(DI_WIN32_CONTEXT * pThis)
{
    __asm push ebp
    __asm mov ebp, esp

    __asm pushad
    __asm pushfd

    // call
    __asm push esp
    __asm push pThis
    __asm call Di_GetThreadContextImpl

    __asm mov esp, ebp
    __asm pop ebp
    __asm ret 4
}


#endif
#endif