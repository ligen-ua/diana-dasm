#include "diana_processor_context.h"
#include "diana_processor_core.h"

int DianaProcessor_InitContext(DianaProcessor * pProcessor, const Diana_Processor_Registers_Context * pContext)
{
    DianaProcessor * pCallContext = pProcessor;

    DI_JUMP_TO_RIP(pContext->reg_RIP.value);

    SET_REG_ES(pContext->reg_ES.value);
    SET_REG_CS(pContext->reg_CS.value);
    SET_REG_SS(pContext->reg_SS.value);
    SET_REG_DS(pContext->reg_DS.value);
    SET_REG_FS(pContext->reg_FS.value);
    SET_REG_GS(pContext->reg_GS.value);

    SET_REG_CR0(pContext->reg_CR0.value);
    SET_REG_CR1(pContext->reg_CR1.value);
    SET_REG_CR2(pContext->reg_CR2.value);
    SET_REG_CR3(pContext->reg_CR3.value);
    SET_REG_CR4(pContext->reg_CR4.value);
    SET_REG_CR5(pContext->reg_CR5.value);
    SET_REG_CR6(pContext->reg_CR6.value);
    SET_REG_CR7(pContext->reg_CR7.value);
    
    SET_REG_DR0(pContext->reg_DR0.value);
    SET_REG_DR1(pContext->reg_DR1.value);
    SET_REG_DR2(pContext->reg_DR2.value);
    SET_REG_DR3(pContext->reg_DR3.value);
    SET_REG_DR4(pContext->reg_DR4.value);
    SET_REG_DR5(pContext->reg_DR5.value);
    SET_REG_DR6(pContext->reg_DR6.value);
    SET_REG_DR7(pContext->reg_DR7.value);

    SET_REG_TR0(pContext->reg_TR0.value);
    SET_REG_TR1(pContext->reg_TR1.value);
    SET_REG_TR2(pContext->reg_TR2.value);
    SET_REG_TR3(pContext->reg_TR3.value);
    SET_REG_TR4(pContext->reg_TR4.value);
    SET_REG_TR5(pContext->reg_TR5.value);
    SET_REG_TR6(pContext->reg_TR6.value);
    SET_REG_TR7(pContext->reg_TR7.value);

    SET_REG_RAX(pContext->reg_RAX.value);
    SET_REG_RCX(pContext->reg_RCX.value);
    SET_REG_RDX(pContext->reg_RDX.value);
    SET_REG_RBX(pContext->reg_RBX.value);
    SET_REG_RSP(pContext->reg_RSP.value);
    SET_REG_RBP(pContext->reg_RBP.value);
    SET_REG_RSI(pContext->reg_RSI.value);
    SET_REG_RDI(pContext->reg_RDI.value);
    SET_REG_R8(pContext->reg_R8.value);
    SET_REG_R9(pContext->reg_R9.value);
    SET_REG_R10(pContext->reg_R10.value);
    SET_REG_R11(pContext->reg_R11.value);
    SET_REG_R12(pContext->reg_R12.value);
    SET_REG_R13(pContext->reg_R13.value);
    SET_REG_R14(pContext->reg_R14.value);
    SET_REG_R15(pContext->reg_R15.value);

    SET_REG_MM0(pContext->reg_MM0.value); // SET_REG_FPU_ST0
    SET_REG_MM1(pContext->reg_MM1.value); // SET_REG_FPU_ST1
    SET_REG_MM2(pContext->reg_MM2.value);
    SET_REG_MM3(pContext->reg_MM3.value);
    SET_REG_MM4(pContext->reg_MM4.value);
    SET_REG_MM5(pContext->reg_MM5.value);
    SET_REG_MM6(pContext->reg_MM6.value);
    SET_REG_MM7(pContext->reg_MM7.value); // SET_REG_FPU_ST7

    DianaProcessor_InitRawRegister(pCallContext, reg_XMM0, pContext->reg_XMM0.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM1, pContext->reg_XMM1.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM2, pContext->reg_XMM2.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM3, pContext->reg_XMM3.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM4, pContext->reg_XMM4.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM5, pContext->reg_XMM5.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM6, pContext->reg_XMM6.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM7, pContext->reg_XMM7.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM8, pContext->reg_XMM8.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM9, pContext->reg_XMM9.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM10, pContext->reg_XMM10.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM11, pContext->reg_XMM11.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM12, pContext->reg_XMM12.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM13, pContext->reg_XMM13.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM14, pContext->reg_XMM14.value, 16);
    DianaProcessor_InitRawRegister(pCallContext, reg_XMM15, pContext->reg_XMM15.value, 16);

    if (pContext->fpuStateFlags & DIANA_PROCESSOR_CONTEXT_FPU_STATE_VALID)
    {
        pCallContext->m_fpu = pContext->fpuState;
    }
    else
    {
        DianaProcessor_ResetFPU(pCallContext);
    }
    pCallContext->m_flags = pContext->flags;
    return DI_SUCCESS;
}
int DianaProcessor_QueryContext(DianaProcessor * pProcessor, Diana_Processor_Registers_Context * pContext)
{
    DianaProcessor * pCallContext = pProcessor;

    DIANA_MEMSET(pContext, 0, sizeof(Diana_Processor_Registers_Context));

    pContext->reg_RIP.value = GET_REG_RIP;

    pContext->reg_ES.value = (DI_UINT16)GET_REG_ES;
    pContext->reg_CS.value =  (DI_UINT16)GET_REG_CS;
    pContext->reg_SS.value = (DI_UINT16)GET_REG_SS;
    pContext->reg_DS.value = (DI_UINT16)GET_REG_DS;
    pContext->reg_FS.value = (DI_UINT16)GET_REG_FS;
    pContext->reg_GS.value = (DI_UINT16)GET_REG_GS;

    pContext->reg_CR0.value = (DI_UINT32)GET_REG_CR0;
    pContext->reg_CR1.value = (DI_UINT32)GET_REG_CR1;
    pContext->reg_CR2.value = (DI_UINT32)GET_REG_CR2;
    pContext->reg_CR3.value = (DI_UINT32)GET_REG_CR3;
    pContext->reg_CR4.value = (DI_UINT32)GET_REG_CR4;
    pContext->reg_CR5.value = (DI_UINT32)GET_REG_CR5;
    pContext->reg_CR6.value = (DI_UINT32)GET_REG_CR6;
    pContext->reg_CR7.value = (DI_UINT32)GET_REG_CR7;
    
    pContext->reg_DR0.value = (DI_UINT32)GET_REG_DR0;
    pContext->reg_DR1.value = (DI_UINT32)GET_REG_DR1;
    pContext->reg_DR2.value = (DI_UINT32)GET_REG_DR2;
    pContext->reg_DR3.value = (DI_UINT32)GET_REG_DR3;
    pContext->reg_DR4.value = (DI_UINT32)GET_REG_DR4;
    pContext->reg_DR5.value = (DI_UINT32)GET_REG_DR5;
    pContext->reg_DR6.value = (DI_UINT32)GET_REG_DR6;
    pContext->reg_DR7.value = (DI_UINT32)GET_REG_DR7;

    pContext->reg_TR0.value = (DI_UINT32)GET_REG_TR0;
    pContext->reg_TR1.value = (DI_UINT32)GET_REG_TR1;
    pContext->reg_TR2.value = (DI_UINT32)GET_REG_TR2;
    pContext->reg_TR3.value = (DI_UINT32)GET_REG_TR3;
    pContext->reg_TR4.value = (DI_UINT32)GET_REG_TR4;
    pContext->reg_TR5.value = (DI_UINT32)GET_REG_TR5;
    pContext->reg_TR6.value = (DI_UINT32)GET_REG_TR6;
    pContext->reg_TR7.value = (DI_UINT32)GET_REG_TR7;

    pContext->reg_RAX.value = GET_REG_RAX;
    pContext->reg_RCX.value = GET_REG_RCX;
    pContext->reg_RDX.value = GET_REG_RDX;
    pContext->reg_RBX.value = GET_REG_RBX;
    pContext->reg_RSP.value = GET_REG_RSP;
    pContext->reg_RBP.value = GET_REG_RBP;
    pContext->reg_RSI.value = GET_REG_RSI;
    pContext->reg_RDI.value = GET_REG_RDI;
    pContext->reg_R8.value = GET_REG_R8;
    pContext->reg_R9.value = GET_REG_R9;
    pContext->reg_R10.value = GET_REG_R10;
    pContext->reg_R11.value = GET_REG_R11;
    pContext->reg_R12.value = GET_REG_R12;
    pContext->reg_R13.value = GET_REG_R13;
    pContext->reg_R14.value = GET_REG_R14;
    pContext->reg_R15.value = GET_REG_R15;

    pContext->reg_MM0.value = GET_REG_MM0; // SET_REG_FPU_ST0
    pContext->reg_MM1.value = GET_REG_MM1; // SET_REG_FPU_ST1
    pContext->reg_MM2.value = GET_REG_MM2;
    pContext->reg_MM3.value = GET_REG_MM3;
    pContext->reg_MM4.value = GET_REG_MM4;
    pContext->reg_MM5.value = GET_REG_MM5;
    pContext->reg_MM6.value = GET_REG_MM6;
    pContext->reg_MM7.value = GET_REG_MM7; // SET_REG_FPU_ST7
    
    DIANA_MEMCPY(pContext->reg_XMM0.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM0), 16);
    DIANA_MEMCPY(pContext->reg_XMM1.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM1), 16);
    DIANA_MEMCPY(pContext->reg_XMM2.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM2), 16);
    DIANA_MEMCPY(pContext->reg_XMM3.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM3), 16);
    DIANA_MEMCPY(pContext->reg_XMM4.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM4), 16);
    DIANA_MEMCPY(pContext->reg_XMM5.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM5), 16);
    DIANA_MEMCPY(pContext->reg_XMM6.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM6), 16);
    DIANA_MEMCPY(pContext->reg_XMM7.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM7), 16);
    DIANA_MEMCPY(pContext->reg_XMM8.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM8), 16);
    DIANA_MEMCPY(pContext->reg_XMM9.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM9), 16);
    DIANA_MEMCPY(pContext->reg_XMM10.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM10), 16);
    DIANA_MEMCPY(pContext->reg_XMM11.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM11), 16);
    DIANA_MEMCPY(pContext->reg_XMM12.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM12), 16);
    DIANA_MEMCPY(pContext->reg_XMM13.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM13), 16);
    DIANA_MEMCPY(pContext->reg_XMM14.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM14), 16);
    DIANA_MEMCPY(pContext->reg_XMM15.value, DianaProcessor_QueryRawRegister(pCallContext, reg_XMM15), 16);

    pContext->flags = pCallContext->m_flags;
    pContext->fpuState = pCallContext->m_fpu;
    pContext->fpuStateFlags = DIANA_PROCESSOR_CONTEXT_FPU_STATE_VALID;
    return DI_SUCCESS;
}
