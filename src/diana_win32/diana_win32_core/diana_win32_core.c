#include "diana_win32_core.h"

#ifdef DIANA_CFG_I386 
#ifdef DIANA_CFG_USE_INLINE_ASSEMBLER

// DianaProcessor_InitAsLive32_Impl stack:
//
    // gs
    // ss
    // fs
    // es
    // ds
    // cs
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
void  __stdcall DianaProcessor_LoadLiveContext32_Impl(DianaProcessor * pCallContext, 
                                                      DI_UINT32 * pParams)
{
    int paramNum = 0;    
    SET_REG_GS(pParams[paramNum++]);
    SET_REG_SS(pParams[paramNum++]);
    SET_REG_FS(pParams[paramNum++]);
    SET_REG_ES(pParams[paramNum++]);
    SET_REG_DS(pParams[paramNum++]);
    SET_REG_CS(pParams[paramNum++]);

    pCallContext->m_flags.impl.l.value = pParams[paramNum++];
    
    SET_REG_RDI(pParams[paramNum++]);
    SET_REG_RSI(pParams[paramNum++]);
    paramNum++;
    SET_REG_RSP(pParams[paramNum++]+0xC);
    SET_REG_RBX(pParams[paramNum++]);
    SET_REG_RDX(pParams[paramNum++]);
    SET_REG_RCX(pParams[paramNum++]);
    SET_REG_RAX(pParams[paramNum++]);

    SET_REG_RBP(pParams[paramNum++]);
    DI_JUMP_TO_RIP(pParams[paramNum++]);
    pCallContext->m_stateFlags |= DI_PROC_STATE_RIP_CHANGED;
}

__declspec(naked) void  __stdcall DianaProcessor_LoadLiveContext32(DianaProcessor * pThis)
{
    __asm push ebp
    __asm mov ebp, esp

    __asm pushad
    __asm pushfd

    __asm xor eax, eax
    
    __asm mov ax, cs
    __asm push eax

    __asm mov ax, ds
    __asm push eax

    __asm mov ax, es
    __asm push eax

    __asm mov ax, fs 
    __asm push eax

    __asm mov ax, ss 
    __asm push eax

    __asm mov ax, gs 
    __asm push eax

    // call
    __asm push esp
    __asm push pThis
    __asm call DianaProcessor_LoadLiveContext32_Impl

    __asm mov esp, ebp
    __asm pop ebp
    __asm ret 4
}


void __stdcall DianaProcessor_LoadContext32(DianaProcessor * pCallContext,
                                            const CONTEXT * pContext)
{
    SET_REG_GS(pContext->SegGs);
    SET_REG_SS(pContext->SegSs);
    SET_REG_FS(pContext->SegFs);
    SET_REG_ES(pContext->SegEs);
    SET_REG_DS(pContext->SegDs);
    SET_REG_CS(pContext->SegCs);

    pCallContext->m_flags.impl.l.value = pContext->EFlags;
    
    SET_REG_RDI(pContext->Edi);
    SET_REG_RSI(pContext->Esi);
    SET_REG_RSP(pContext->Esp);
    SET_REG_RBX(pContext->Ebx);
    SET_REG_RDX(pContext->Edx);
    SET_REG_RCX(pContext->Ecx);
    SET_REG_RAX(pContext->Eax);

    SET_REG_RBP(pContext->Ebp);
    DI_JUMP_TO_RIP(pContext->Eip);
}

void __stdcall DianaProcessor_SaveContext32(DianaProcessor * pCallContext,
                                            CONTEXT * pContext)
{
    pContext->SegGs = (unsigned long)GET_REG_GS;
    pContext->SegSs = (unsigned long)GET_REG_SS;
    pContext->SegFs = (unsigned long)GET_REG_FS;
    pContext->SegEs = (unsigned long)GET_REG_ES;
    pContext->SegDs = (unsigned long)GET_REG_DS;
    pContext->SegCs = (unsigned long)GET_REG_CS;

    pContext->EFlags = pCallContext->m_flags.impl.l.value;
    
    pContext->Edi = (unsigned long)GET_REG_RDI;
    pContext->Esi = (unsigned long)GET_REG_RSI;
    pContext->Esp = (unsigned long)GET_REG_RSP;
    pContext->Ebx = (unsigned long)GET_REG_RBX;
    pContext->Edx = (unsigned long)GET_REG_RDX;
    pContext->Ecx = (unsigned long)GET_REG_RCX;
    pContext->Eax = (unsigned long)GET_REG_RAX;

    pContext->Ebp = (unsigned long)GET_REG_RBP;
    pContext->Eip = (unsigned long)GET_REG_RIP;
}



#endif
#endif