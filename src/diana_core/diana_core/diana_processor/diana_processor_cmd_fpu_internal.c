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
DI_UINT32 DI_FPU_ProcessException(DianaProcessor * pCallContext, DI_UINT32 in_exception)
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
            return unmasked;
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

    unmasked &= ~DI_FPU_EX_PRECISION;
    if (unmasked & (DI_FPU_EX_UNDERFLOW|DI_FPU_EX_OVERFLOW))
    {
        pCallContext->m_fpu.statusWord &= ~DI_FPU_SW_C1;
        if (! (status & DI_FPU_EX_PRECISION))
        {
            pCallContext->m_fpu.statusWord &= ~DI_FPU_EX_PRECISION;
        }
    }
    return unmasked;
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
    #ifndef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
        return InitSoftFloat();
    #endif
    return 0;
}

int Diana_FPU_WriteFloatArgument(struct _dianaContext * pDianaContext,
                                 DianaProcessor * pCallContext,
                                 int number,
                                 floatx80_t * result)
{
    float_status_t status = {0, };
    OPERAND_SIZE doneBytes;
    OPERAND_SIZE selector = 0, address = 0;
    DianaUnifiedRegister segReg = reg_DS;
    DI_CHECK(Diana_QueryAddress(pDianaContext, pCallContext, number, &selector, &address, &segReg));

    switch(pCallContext->m_result.linkedOperands[number].usedSize)
    {
    default:
        Diana_FatalBreak();
        return DI_ERROR;
    case 4:
        {
            float32 ft32 = floatx80_to_float32(*result, &status);
            DI_CHECK(DianaProcessor_WriteMemory(pCallContext, 
                                           selector,
                                           address,
                                           &ft32,
                                           4,
                                           &doneBytes,
                                           0,
                                           segReg));
        }
        break;
    case 8:
        {
            float64 ft64 = floatx80_to_float64(*result, &status);
            DI_CHECK(DianaProcessor_WriteMemory(pCallContext, 
                                           selector,
                                           address,
                                           &ft64,
                                           8,
                                           &doneBytes,
                                           0,
                                           segReg));
        }  
        break;
    case 10:
        DI_CHECK(DianaProcessor_WriteMemory(pCallContext, 
                                       selector,
                                       address,
                                       result,
                                       10,
                                       &doneBytes,
                                       0,
                                       segReg));
        break;
    }
    return DI_SUCCESS;
}

int Diana_FPU_ReadFloatArgument_80(struct _dianaContext * pDianaContext,
                                   DianaProcessor * pCallContext,
                                   int number,
                                   floatx80_t * result)
{
    float_status_t status = {0};
    OPERAND_SIZE doneBytes;
    OPERAND_SIZE selector = 0, address = 0;
    DianaUnifiedRegister segReg = reg_DS;
    DI_CHECK(Diana_QueryAddress(pDianaContext, pCallContext, number, &selector, &address, &segReg));

    switch(pCallContext->m_result.linkedOperands[number].usedSize)
    {
    default:
        Diana_FatalBreak();
        return DI_ERROR;
    case 4:
        {
            float32 ft32 = 0;
            DI_CHECK(DianaProcessor_ReadMemory(pCallContext, 
                                           selector,
                                           address,
                                           &ft32,
                                           4,
                                           &doneBytes,
                                           0,
                                           segReg));


            *result = float32_to_floatx80(ft32, &status);
        }
        break;
    case 8:
        {
            float64 ft64 = 0;
            DI_CHECK(DianaProcessor_ReadMemory(pCallContext, 
                                           selector,
                                           address,
                                           &ft64,
                                           8,
                                           &doneBytes,
                                           0,
                                           segReg));
            *result = float64_to_floatx80(ft64, &status);
        }  
        break;
    case 10:
        DI_CHECK(DianaProcessor_ReadMemory(pCallContext, 
                                       selector,
                                       address,
                                       result,
                                       10,
                                       &doneBytes,
                                       0,
                                       segReg));
        break;
    }
    return DI_SUCCESS;
}
int Diana_FPU_ReadFloatArgument(struct _dianaContext * pDianaContext,
                                   DianaProcessor * pCallContext,
                                   int number,
                                   float64 * result)
{
    float_status_t status = {0};
    OPERAND_SIZE doneBytes;
    OPERAND_SIZE selector = 0, address = 0;
    DianaUnifiedRegister segReg = reg_DS;
    DI_CHECK(Diana_QueryAddress(pDianaContext, pCallContext, number, &selector, &address, &segReg));

    switch(pCallContext->m_result.linkedOperands[number].usedSize)
    {
    default:
        Diana_FatalBreak();
        return DI_ERROR;
    case 4:
        {
            float32 ft32 = 0;
            DI_CHECK(DianaProcessor_ReadMemory(pCallContext, 
                                           selector,
                                           address,
                                           &ft32,
                                           4,
                                           &doneBytes,
                                           0,
                                           segReg));


            *result = float32_to_float64(ft32, &status);
        }
        break;
    case 8:
        {
            DI_CHECK(DianaProcessor_ReadMemory(pCallContext, 
                                           selector,
                                           address,
                                           result,
                                           8,
                                           &doneBytes,
                                           0,
                                           segReg));
        }  
        break;
    case 10:
        {
            floatx80_t value;
            DI_CHECK(DianaProcessor_ReadMemory(pCallContext, 
                                       selector,
                                       address,
                                       &value,
                                       10,
                                       &doneBytes,
                                       0,
                                       segReg));
            *result = floatx80_to_float64(value, &status);
        }
        break;
    }
    return DI_SUCCESS;
}

