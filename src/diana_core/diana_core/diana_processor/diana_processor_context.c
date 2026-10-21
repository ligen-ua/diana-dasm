#include "diana_processor_context.h"
#include "diana_processor_core.h"
#include "diana_processor_core_impl_xmm.h"

// reg_MM0..7 of the context, 64 bits of each x87 register: ST(i) in every mode, the order of FSAVE/FXSAVE and of
// the Windows CONTEXT (32 and 64 bits); the physical register k (MMk of the instructions) is ST((k - TOP) & 7)
static DianaRegInfo * DianaProcessor_ContextMMRegister(DianaProcessor * pCallContext, int i)
{
    return DianaProcessor_FPU_QueryReg(pCallContext, (DianaUnifiedRegister)(reg_fpu_ST0 + i));
}

static void DianaProcessor_SetContextMM(DianaProcessor * pCallContext, int i, OPERAND_SIZE value)
{
    DianaRegInfo * pReg = DianaProcessor_ContextMMRegister(pCallContext, i);
    DIANA_MEMCPY(pCallContext->m_pRegistersVector + pReg->m_offset, &value, sizeof(value));
}

static OPERAND_SIZE DianaProcessor_GetContextMM(DianaProcessor * pCallContext, int i)
{
    OPERAND_SIZE value = 0;
    DianaRegInfo * pReg = DianaProcessor_ContextMMRegister(pCallContext, i);
    DIANA_MEMCPY(&value, pCallContext->m_pRegistersVector + pReg->m_offset, sizeof(value));
    return value;
}

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
    if (pContext->fpuStateFlags & DIANA_PROCESSOR_CONTEXT_YMM_STATE_VALID)
    {
        int i;
        for (i = 0; i < 16; ++i)
        {
            DianaRegisterYMM_type value = DianaProcessor_YMM_GetValue(pCallContext, (DianaUnifiedRegister)(reg_YMM0 + i));
            DIANA_MEMCPY(&value.xmm[1], pContext->reg_YMM_High[i].value, 16);
            DianaProcessor_YMM_SetValue(pCallContext, (DianaUnifiedRegister)(reg_YMM0 + i), &value);
        }
    }
    if (pContext->fpuStateFlags & DIANA_PROCESSOR_CONTEXT_ZMM_STATE_VALID)
    {
        int i;
        for (i = 0; i < 8; ++i)
            DianaProcessor_InitRawRegister(pCallContext, (DianaUnifiedRegister)(reg_K0 + i),
                                            (const DI_CHAR *)&pContext->reg_K[i].value, 8);
        for (i = 0; i < 16; ++i)
        {
            DianaRegisterZMM_type value = DianaProcessor_ZMM_GetValue(pCallContext, (DianaUnifiedRegister)(reg_ZMM0 + i));
            DIANA_MEMCPY(&value.ymm[1], pContext->reg_ZMM_High[i].value, 32);
            DianaProcessor_ZMM_SetValue(pCallContext, (DianaUnifiedRegister)(reg_ZMM0 + i), &value);
            DIANA_MEMCPY(value.u8, pContext->reg_ZMM16_31[i].value, 64);
            DianaProcessor_ZMM_SetValue(pCallContext, (DianaUnifiedRegister)(reg_ZMM16 + i), &value);
        }
    }

    if (pContext->fpuStateFlags & DIANA_PROCESSOR_CONTEXT_FPU_STATE_VALID)
    {
        pCallContext->m_fpu = pContext->fpuState;
    }
    else
    {
        DianaProcessor_ResetFPU(pCallContext);
    }
    // after TOP: ST(i)
    DianaProcessor_SetContextMM(pCallContext, 0, pContext->reg_MM0.value);
    DianaProcessor_SetContextMM(pCallContext, 1, pContext->reg_MM1.value);
    DianaProcessor_SetContextMM(pCallContext, 2, pContext->reg_MM2.value);
    DianaProcessor_SetContextMM(pCallContext, 3, pContext->reg_MM3.value);
    DianaProcessor_SetContextMM(pCallContext, 4, pContext->reg_MM4.value);
    DianaProcessor_SetContextMM(pCallContext, 5, pContext->reg_MM5.value);
    DianaProcessor_SetContextMM(pCallContext, 6, pContext->reg_MM6.value);
    DianaProcessor_SetContextMM(pCallContext, 7, pContext->reg_MM7.value);
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

    pContext->reg_MM0.value = DianaProcessor_GetContextMM(pCallContext, 0);
    pContext->reg_MM1.value = DianaProcessor_GetContextMM(pCallContext, 1);
    pContext->reg_MM2.value = DianaProcessor_GetContextMM(pCallContext, 2);
    pContext->reg_MM3.value = DianaProcessor_GetContextMM(pCallContext, 3);
    pContext->reg_MM4.value = DianaProcessor_GetContextMM(pCallContext, 4);
    pContext->reg_MM5.value = DianaProcessor_GetContextMM(pCallContext, 5);
    pContext->reg_MM6.value = DianaProcessor_GetContextMM(pCallContext, 6);
    pContext->reg_MM7.value = DianaProcessor_GetContextMM(pCallContext, 7);
    
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
    {
        int i;
        for (i = 0; i < 16; ++i)
        {
            DianaRegisterYMM_type value = DianaProcessor_YMM_GetValue(pCallContext, (DianaUnifiedRegister)(reg_YMM0 + i));
            DIANA_MEMCPY(pContext->reg_YMM_High[i].value, &value.xmm[1], 16);
        }
        for (i = 0; i < 8; ++i)
            DIANA_MEMCPY(&pContext->reg_K[i].value, DianaProcessor_QueryRawRegister(pCallContext, (DianaUnifiedRegister)(reg_K0 + i)), 8);
        for (i = 0; i < 16; ++i)
        {
            DianaRegisterZMM_type value = DianaProcessor_ZMM_GetValue(pCallContext, (DianaUnifiedRegister)(reg_ZMM0 + i));
            DIANA_MEMCPY(pContext->reg_ZMM_High[i].value, &value.ymm[1], 32);
            value = DianaProcessor_ZMM_GetValue(pCallContext, (DianaUnifiedRegister)(reg_ZMM16 + i));
            DIANA_MEMCPY(pContext->reg_ZMM16_31[i].value, value.u8, 64);
        }
    }

    pContext->flags = pCallContext->m_flags;
    pContext->fpuState = pCallContext->m_fpu;
    pContext->fpuStateFlags = DIANA_PROCESSOR_CONTEXT_FPU_STATE_VALID | DIANA_PROCESSOR_CONTEXT_YMM_STATE_VALID |
                              DIANA_PROCESSOR_CONTEXT_ZMM_STATE_VALID;
    return DI_SUCCESS;
}
