#include "diana_processor_win32_context.h"
#include "diana_processor_core.h"

int DianaProcessor_ConvertContextToIndependent_Win32(const DIANA_CONTEXT_NTLIKE_32 * pContextIn, 
                                                     Diana_Processor_Registers_Context * pContextOut)
{
    DIANA_MEMSET(pContextOut, 0, sizeof(Diana_Processor_Registers_Context));

    pContextOut->reg_DR0.value = pContextIn->Dr0;
    pContextOut->reg_DR1.value = pContextIn->Dr1;
    pContextOut->reg_DR2.value = pContextIn->Dr2;
    pContextOut->reg_DR3.value = pContextIn->Dr3;
    pContextOut->reg_DR6.value = pContextIn->Dr6;
    pContextOut->reg_DR7.value = pContextIn->Dr7;

    pContextOut->fpuStateFlags = DIANA_PROCESSOR_CONTEXT_FPU_STATE_VALID;
    pContextOut->fpuState.controlWord = (DI_UINT16)pContextIn->FloatSave.ControlWord;
    pContextOut->fpuState.statusWord = (DI_UINT16)pContextIn->FloatSave.StatusWord;
    pContextOut->reg_MM0.value = *(DI_UINT64*)&pContextIn->FloatSave.RegisterArea[0];
    pContextOut->reg_MM1.value = *(DI_UINT64*)&pContextIn->FloatSave.RegisterArea[8];
    pContextOut->reg_MM2.value = *(DI_UINT64*)&pContextIn->FloatSave.RegisterArea[16];
    pContextOut->reg_MM3.value = *(DI_UINT64*)&pContextIn->FloatSave.RegisterArea[24];
    pContextOut->reg_MM4.value = *(DI_UINT64*)&pContextIn->FloatSave.RegisterArea[32];
    pContextOut->reg_MM5.value = *(DI_UINT64*)&pContextIn->FloatSave.RegisterArea[40];
    pContextOut->reg_MM6.value = *(DI_UINT64*)&pContextIn->FloatSave.RegisterArea[48];
    pContextOut->reg_MM7.value = *(DI_UINT64*)&pContextIn->FloatSave.RegisterArea[56];


    pContextOut->reg_GS.value = (DI_UINT16)pContextIn->SegGs;
    pContextOut->reg_FS.value = (DI_UINT16)pContextIn->SegFs;
    pContextOut->reg_ES.value = (DI_UINT16)pContextIn->SegEs;
    pContextOut->reg_DS.value = (DI_UINT16)pContextIn->SegDs;

    pContextOut->reg_RDI.value = pContextIn->Edi;
    pContextOut->reg_RSI.value = pContextIn->Esi;
    pContextOut->reg_RBX.value = pContextIn->Ebx;
    pContextOut->reg_RDX.value = pContextIn->Edx;
    pContextOut->reg_RCX.value = pContextIn->Ecx;
    pContextOut->reg_RAX.value = pContextIn->Eax;

    pContextOut->reg_RBP.value = pContextIn->Ebp;
    pContextOut->reg_RIP.value = pContextIn->Eip;
    pContextOut->reg_CS.value = (DI_UINT16)pContextIn->SegCs;
    pContextOut->flags.value = pContextIn->EFlags;
    pContextOut->reg_RSP.value = pContextIn->Esp;
    pContextOut->reg_SS.value = (DI_UINT16)pContextIn->SegSs;
    return DI_SUCCESS;
}

int DianaProcessor_ConvertContextToIndependent_X64(const DIANA_CONTEXT_NTLIKE_64 * pContextIn, 
                                                   Diana_Processor_Registers_Context * pContextOut)
{
    DIANA_MEMSET(pContextOut, 0, sizeof(Diana_Processor_Registers_Context));
    
    pContextOut->reg_CS.value = (DI_UINT16)pContextIn->SegCs;
    pContextOut->reg_DS.value = (DI_UINT16)pContextIn->SegDs;
    pContextOut->reg_ES.value = (DI_UINT16)pContextIn->SegEs;
    pContextOut->reg_FS.value = (DI_UINT16)pContextIn->SegFs;
    pContextOut->reg_GS.value = (DI_UINT16)pContextIn->SegGs;
    pContextOut->reg_SS.value = (DI_UINT16)pContextIn->SegSs;
    pContextOut->flags.value = pContextIn->EFlags;


    pContextOut->reg_DR0.value = pContextIn->Dr0;
    pContextOut->reg_DR1.value = pContextIn->Dr1;
    pContextOut->reg_DR2.value = pContextIn->Dr2;
    pContextOut->reg_DR3.value = pContextIn->Dr3;
    pContextOut->reg_DR6.value = pContextIn->Dr6;
    pContextOut->reg_DR7.value = pContextIn->Dr7;

    //
    // Integer registers.
    //

    pContextOut->reg_RAX.value = pContextIn->Rax;
    pContextOut->reg_RBX.value = pContextIn->Rbx;
    pContextOut->reg_RCX.value = pContextIn->Rcx;
    pContextOut->reg_RDX.value = pContextIn->Rdx;
    pContextOut->reg_RBP.value = pContextIn->Rbx;
    pContextOut->reg_RSP.value = pContextIn->Rsp;
    pContextOut->reg_RBP.value = pContextIn->Rbp;
    pContextOut->reg_RSI.value = pContextIn->Rsi;
    pContextOut->reg_RDI.value = pContextIn->Rdi;
    pContextOut->reg_R8.value = pContextIn->R8;
    pContextOut->reg_R9.value = pContextIn->R9;
    pContextOut->reg_R10.value = pContextIn->R10;
    pContextOut->reg_R11.value = pContextIn->R11;
    pContextOut->reg_R12.value = pContextIn->R12;
    pContextOut->reg_R13.value = pContextIn->R13;
    pContextOut->reg_R14.value = pContextIn->R14;
    pContextOut->reg_R15.value = pContextIn->R15;

    pContextOut->reg_RIP.value = pContextIn->Rip;
   
    pContextOut->fpuStateFlags = DIANA_PROCESSOR_CONTEXT_FPU_STATE_VALID;
    pContextOut->fpuState.controlWord = (DI_UINT16)pContextIn->FltSave.ControlWord;
    pContextOut->fpuState.statusWord = (DI_UINT16)pContextIn->FltSave.StatusWord;
    pContextOut->reg_MM0.value = pContextIn->FltSave.FloatRegisters[0].Low;
    pContextOut->reg_MM1.value = pContextIn->FltSave.FloatRegisters[1].Low;
    pContextOut->reg_MM2.value = pContextIn->FltSave.FloatRegisters[2].Low;
    pContextOut->reg_MM3.value = pContextIn->FltSave.FloatRegisters[3].Low;
    pContextOut->reg_MM4.value = pContextIn->FltSave.FloatRegisters[4].Low;
    pContextOut->reg_MM5.value = pContextIn->FltSave.FloatRegisters[5].Low;
    pContextOut->reg_MM6.value = pContextIn->FltSave.FloatRegisters[6].Low;
    pContextOut->reg_MM7.value = pContextIn->FltSave.FloatRegisters[7].Low;

    DIANA_MEMCPY(&pContextOut->reg_XMM0, &pContextIn->FltSave.XmmRegisters[0], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM1, &pContextIn->FltSave.XmmRegisters[1], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM2, &pContextIn->FltSave.XmmRegisters[2], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM3, &pContextIn->FltSave.XmmRegisters[3], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM4, &pContextIn->FltSave.XmmRegisters[4], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM5, &pContextIn->FltSave.XmmRegisters[5], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM6, &pContextIn->FltSave.XmmRegisters[6], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM7, &pContextIn->FltSave.XmmRegisters[7], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM8, &pContextIn->FltSave.XmmRegisters[8], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM9, &pContextIn->FltSave.XmmRegisters[9], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM10, &pContextIn->FltSave.XmmRegisters[10], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM11, &pContextIn->FltSave.XmmRegisters[11], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM12, &pContextIn->FltSave.XmmRegisters[12], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM13, &pContextIn->FltSave.XmmRegisters[13], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM14, &pContextIn->FltSave.XmmRegisters[14], 16);
    DIANA_MEMCPY(&pContextOut->reg_XMM15, &pContextIn->FltSave.XmmRegisters[15], 16);
    return DI_SUCCESS;
}

