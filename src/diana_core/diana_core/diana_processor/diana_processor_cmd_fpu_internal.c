#include "diana_processor_cmd_fpu_internal.h"



float_status_t FPU_pre_exception_handling(DianaProcessor * pCallContext)
{
    float_status_t status;

    int precision = pCallContext->m_fpu.controlWord & DI_FPU_CW_PRECISION_CONTROL;
    switch(precision)
    {
     case DI_FPU_PR_32_BITS:
        status.float_rounding_precision = 32;
        break;
     case DI_FPU_PR_64_BITS:
        status.float_rounding_precision = 64;
        break;
     case DI_FPU_PR_80_BITS:
        status.float_rounding_precision = 80;
        break;
     default:
        status.float_rounding_precision = 80;
    }

    status.float_exception_flags = 0; 
    status.float_nan_handling_mode = float_first_operand_nan;
    status.float_rounding_mode = (pCallContext->m_fpu.controlWord & DI_FPU_CW_ROUNDING_CONTROL)>>10;
    status.flush_underflow_to_zero = 0;
    status.float_suppress_exception = 0;
    status.float_exception_masks = pCallContext->m_fpu.controlWord&DI_FPU_CW_ALL_EXCEPTIONS;
    status.denormals_are_zeros = 0;
    return status;
}
float_status_t Diana_SSE_Status(DianaProcessor * pCallContext)
{
    float_status_t status;
    DI_UINT32 mxcsr = pCallContext->m_mxcsr;
    status.float_rounding_precision = 80;
    status.float_exception_flags = 0;
    status.float_nan_handling_mode = float_first_operand_nan;
    status.float_rounding_mode = (mxcsr & DI_MXCSR_RC) >> 13;
    // FTZ only with the underflow masked
    status.flush_underflow_to_zero = (mxcsr & DI_MXCSR_FZ) && (mxcsr & DI_MXCSR_UM);
    status.float_suppress_exception = 0;
    status.float_exception_masks = (mxcsr & DI_MXCSR_MASKS) >> 7;
    status.denormals_are_zeros = (mxcsr & DI_MXCSR_DAZ) != 0;
    return status;
}

int Diana_SSE_Exceptions(DianaProcessor * pCallContext, const float_status_t * pStatus)
{
    DI_UINT32 flags = pStatus->float_exception_flags & DI_MXCSR_FLAGS;
    pCallContext->m_mxcsr |= flags;
    if (flags & ~((pCallContext->m_mxcsr & DI_MXCSR_MASKS) >> 7))
        return DI_INTERRUPT;
    return DI_SUCCESS;
}

void Diana_FPU_MmxTransition(DianaProcessor * pCallContext)
{
    int i = 0, mmx = 0;
    const DianaLinkedOperand * pOperands = pCallContext->m_result.linkedOperands;
    for (i = 0; i < pCallContext->m_result.iLinkedOpCount; ++i)
    {
        if (pOperands[i].type == diana_register &&
            pOperands[i].value.recognizedRegister >= reg_MM0 && pOperands[i].value.recognizedRegister <= reg_MM7)
        {
            mmx = 1;
        }
    }
    if (!mmx)
        return;
    Diana_FPU_SetStackTop(pCallContext, 0);
    for (i = 0; i < 8; ++i)
        pCallContext->m_fpu.registerFlags[i] |= DI_PROCESSOR_FPU_REGISTER_BUSY;
    // the written MMX register: bits 79:64 all ones (maskmovq only reads its first operand)
    if (pOperands[0].type == diana_register &&
        pOperands[0].value.recognizedRegister >= reg_MM0 && pOperands[0].value.recognizedRegister <= reg_MM7 &&
        pCallContext->m_result.pInfo->m_pGroupInfo->m_commandId != diana_cmd_maskmovq)
    {
        DianaRegInfo * pReg = DianaProcessor_QueryReg(pCallContext, pOperands[0].value.recognizedRegister);
        *(DI_UINT16 *)(pCallContext->m_pRegistersVector + pReg->m_offset + 8) = 0xFFFF;
    }
}

void Diana_FPU_SetStackTop(DianaProcessor * pCallContext, int stackTop)
{
    pCallContext->m_fpu.statusWord &= ~DI_FPU_SW_TOP;
    pCallContext->m_fpu.statusWord |= (stackTop << 11) & DI_FPU_SW_TOP;
}
int Diana_FPU_GetStackTop(DianaProcessor * pCallContext)
{
    return ((pCallContext->m_fpu.statusWord & DI_FPU_SW_TOP)>>11) & 7;
}
int Diana_FPU_IsRegEmpty(DianaProcessor * pCallContext, int number)
{
    int currentStackTop = Diana_FPU_GetStackTop(pCallContext);
    return !(pCallContext->m_fpu.registerFlags[(currentStackTop+(number))&7]&DI_PROCESSOR_FPU_REGISTER_BUSY);
}
void Diana_FPU_MarkRegState(DianaProcessor * pCallContext, int number, int isEmpty)
{
    int currentStackTop = Diana_FPU_GetStackTop(pCallContext);
    DI_CHAR * pFlags = pCallContext->m_fpu.registerFlags + ((currentStackTop+(number))&7);
    if (isEmpty)
    {
        *pFlags &= ~DI_PROCESSOR_FPU_REGISTER_BUSY;
    }
    else
    {
        *pFlags |= DI_PROCESSOR_FPU_REGISTER_BUSY;
    }

}
void Diana_FPU_Push(DianaProcessor * pCallContext)
{
    int currentStackTop = Diana_FPU_GetStackTop(pCallContext);
    if (currentStackTop == 0)
    {
        currentStackTop = 8;
    }
    Diana_FPU_SetStackTop(pCallContext, currentStackTop - 1);
}
void Diana_FPU_Pop(DianaProcessor * pCallContext)
{
    int currentStackTop = Diana_FPU_GetStackTop(pCallContext);
    int newTop = currentStackTop+1;
    pCallContext->m_fpu.registerFlags[currentStackTop] &= ~DI_PROCESSOR_FPU_REGISTER_BUSY;
    if (newTop == 8)
    {
        newTop = 0;
    }
    Diana_FPU_SetStackTop(pCallContext, newTop);
}


// 80-bit  API
void DianaProcessor_FPU_GetSTRegister_80(DianaProcessor * pCallContext,
                                         DianaUnifiedRegister recognizedRegister,
                                         floatx80_t * value)
{
    DianaRegInfo * pRealRegister = DianaProcessor_FPU_QueryReg(pCallContext, recognizedRegister);
    const char * pStart = (pCallContext->m_pRegistersVector + pRealRegister->m_offset);
    DIANA_MEMCPY(value, pStart, sizeof(floatx80_t));
}

void DianaProcessor_FPU_SetSTRegister_80(DianaProcessor * pCallContext,
                                         DianaUnifiedRegister recognizedRegister,
                                         const floatx80_t * value)
{
    int stNumber = recognizedRegister - reg_fpu_ST0;
    DianaRegInfo * pRealRegister = DianaProcessor_FPU_QueryReg(pCallContext, recognizedRegister);
    char * pStart = (pCallContext->m_pRegistersVector + pRealRegister->m_offset);
    DIANA_MEMCPY(pStart, value, sizeof(floatx80_t));
    Diana_FPU_MarkRegState(pCallContext, stNumber, 0);
}

// 64-bit API
void DianaProcessor_FPU_SetSTRegister(DianaProcessor * pCallContext,
                                              DianaUnifiedRegister recognizedRegister,
                                              const OPERAND_SIZE * pValue)
{
    float_status_t status = {0, };
    floatx80_t value80 = float64_to_floatx80(*pValue, &status);
    DianaProcessor_FPU_SetSTRegister_80(pCallContext, recognizedRegister, &value80);
}
OPERAND_SIZE DianaProcessor_FPU_GetSTRegister(DianaProcessor * pCallContext,
                                               DianaUnifiedRegister recognizedRegister)
{
    float_status_t status = {0, };
    floatx80_t value80;
    float64 ft64;
    DianaProcessor_FPU_GetSTRegister_80(pCallContext, recognizedRegister, &value80);
    ft64 = floatx80_to_float64(value80, &status);
    return ft64;
}

//--------------------------------------
// Bochs FPU_exception: the flags of an x87 operation into the status word; the unmasked exceptions are returned
// (nothing is written then). An unmasked overflow or underflow writes a register result anyway; for a store (memory
// destination) it writes nothing, clears C1 and reports no new PE.
DI_UINT32 DI_FPU_ProcessExceptionEx(DianaProcessor * pCallContext, DI_UINT32 in_exception, int isStore)
{
    DI_UINT32 exception = in_exception & DI_FPU_SW_ALL_EXCEPTIONS;
    DI_UINT32 status = pCallContext->m_fpu.statusWord;
    DI_UINT32 unmasked = exception & ~pCallContext->m_fpu.controlWord&DI_FPU_CW_ALL_EXCEPTIONS;
    if (exception & (DI_FPU_EX_INVALID|DI_FPU_EX_ZERO_DIV))
        unmasked &= (DI_FPU_EX_INVALID|DI_FPU_EX_ZERO_DIV);
    
    if (unmasked)
    {
        pCallContext->m_fpu.statusWord |= DI_FPU_SW_SUMMARY | DI_FPU_SW_BACKWARD;
    }

    if (exception & DI_FPU_EX_INVALID) 
    {
        // FPU_EX_Invalid cannot come with any other exception but x87 stack fault
        pCallContext->m_fpu.statusWord |= exception;
        if (exception & DI_FPU_SW_STACK_FAULT) 
        {
            if (! (exception & DI_FPU_SW_C1)) 
            {
                /* This bit distinguishes over- from underflow for a stack fault,
                and roundup from round-down for precision loss. */
                pCallContext->m_fpu.statusWord &= ~DI_FPU_SW_C1;
            }
        }
        return unmasked;
    }

    if (exception & DI_FPU_EX_ZERO_DIV) 
    {
        pCallContext->m_fpu.statusWord |= DI_FPU_EX_ZERO_DIV;
        return unmasked;
    }

    if (exception & DI_FPU_EX_DENORMAL) 
    {
        pCallContext->m_fpu.statusWord |= DI_FPU_EX_DENORMAL;
        if (unmasked & DI_FPU_EX_DENORMAL)
            return unmasked & DI_FPU_EX_DENORMAL;
    }

    // set the corresponding exception bits */
    pCallContext->m_fpu.statusWord |= exception;

    if (exception & DI_FPU_EX_PRECISION)
    {
        if (! (exception & DI_FPU_SW_C1)) 
        {
            pCallContext->m_fpu.statusWord&= ~DI_FPU_SW_C1;
        }
    }

    // an unmasked #P still writes the result
    unmasked &= ~DI_FPU_EX_PRECISION;
    if (unmasked & (DI_FPU_EX_UNDERFLOW|DI_FPU_EX_OVERFLOW))
    {
        if (!isStore)
        {
            unmasked &= ~(DI_FPU_EX_UNDERFLOW|DI_FPU_EX_OVERFLOW);
        }
        else
        {
            pCallContext->m_fpu.statusWord &= ~DI_FPU_SW_C1;
            if (! (status & DI_FPU_EX_PRECISION))
            {
                pCallContext->m_fpu.statusWord &= ~DI_FPU_EX_PRECISION;
            }
        }
    }
    return unmasked;
}

DI_UINT32 DI_FPU_ProcessException(DianaProcessor * pCallContext, DI_UINT32 in_exception)
{
    return DI_FPU_ProcessExceptionEx(pCallContext, in_exception, 0);
}

void DI_FPU_StackUnderflow(DianaProcessor * pCallContext, int stnr, int pop)
{
    // the masked response: the default NaN in the destination
    if (pCallContext->m_fpu.controlWord & DI_FPU_CW_INVALID)
    {
        DianaProcessor_FPU_SetSTRegister_80(pCallContext, (DianaUnifiedRegister)(reg_fpu_ST0 + stnr),
                                            &floatx80_default_nan);
        if (pop)
            Diana_FPU_Pop(pCallContext);
    }
    DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
}

void DI_FPU_SetCompareFlags(DianaProcessor * pCallContext, int relation)
{
    DI_UINT16 cc = 0;
    switch (relation)
    {
    case float_relation_less:
        cc = DI_FPU_SW_C0;
        break;
    case float_relation_equal:
        cc = DI_FPU_SW_C3;
        break;
    case float_relation_greater:
        break;
    default:
        cc = DI_FPU_SW_C0 | DI_FPU_SW_C2 | DI_FPU_SW_C3;
        break;
    }
    pCallContext->m_fpu.statusWord = (DI_UINT16)((pCallContext->m_fpu.statusWord &
                                                  ~(DI_FPU_SW_C0 | DI_FPU_SW_C2 | DI_FPU_SW_C3)) | cc);
}

void DI_FPU_Overflow(DianaProcessor * pCallContext)
{
    if (pCallContext->m_fpu.controlWord & DI_FPU_CW_INVALID)
    {
        Diana_FPU_Push(pCallContext);
        DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &floatx80_default_nan);
    }
    DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_OVERFLOW);
}


int Diana_GlobalInitFPU()
{
    // the softfloat constants (floatx80_default_nan: the x87 indefinite)
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    return InitSoftFloat();
#else
    return 0;
#endif
}
