
#include "diana_processor_core_impl_xmm.h"

// special
#include "diana_processor_specific_xmm.h"

void DianaProcessor_XMM_SetValue(DianaProcessor * pCallContext,
                                 DianaUnifiedRegister regId,
                                 DianaRegInfo * pReg,
                                 DianaRegisterXMM_type * value)
{
    DianaProcessor_XMM_SetValueEx(pCallContext,
                                  regId,
                                  pReg,
                                  value,
                                  pReg->m_size);
}

int Diana_ProcessorSetGetOperand_XMM_register(struct _dianaContext * pDianaContext,
                                              DianaProcessor * pCallContext,
                                              DianaLinkedOperand * pLinkedOp,
                                              DianaRegisterXMM_type * pResult,
                                              int bSet)
{
    pDianaContext;

    if (bSet)
    {
        DianaUnifiedRegister reg64;
        DianaProcessor_XMM_SetValueEx(pCallContext,
                                    pLinkedOp->value.recognizedRegister,
                                    DianaProcessor_QueryReg(pCallContext,
                                                            pLinkedOp->value.recognizedRegister),
                                    pResult,
                                    pLinkedOp->usedSize);
        // 64-bit mode: a 32-bit general register destination (movd r32, xmm) clears the upper half
        if (pCallContext->m_context.iAMD64Mode &&
            DianaProcessor_Query64RegisterFor32(pLinkedOp->value.recognizedRegister, &reg64) == DI_SUCCESS)
        {
            DianaRegInfo * pReg64 = DianaProcessor_QueryReg(pCallContext, reg64);
            *(DI_UINT32 *)(pCallContext->m_pRegistersVector + pReg64->m_offset + 4) = 0;
        }
    }
    else
    {
        *pResult = DianaProcessor_XMM_GetValueEx(pCallContext, 
                                                 DianaProcessor_QueryReg(pCallContext,
                                                                         pLinkedOp->value.recognizedRegister),
                                                 pLinkedOp->usedSize);
    }
    return DI_SUCCESS;
}

// DI_GP for an address that isn't a multiple of alignment, as the alignment options say
static int Di_XmmCheckAlignment(DianaProcessor * pCallContext, OPERAND_SIZE address, int alignment)
{
    if (!alignment || 0 == (address % alignment))
        return DI_SUCCESS;
    if (pCallContext->m_options & DIANA_PROCESSOR_OPTION_CHECK_ALIGNMENT_LEGACY_MODE)
    {
        return DI_GP;
    }
    if (pCallContext->m_options & DIANA_PROCESSOR_OPTION_CHECK_ALIGNMENT_STRICT_MODE)
    {
        OPERAND_SIZE cr0 = GET_REG_CR0;
        OPERAND_SIZE csValue = GET_REG_CS;
        DianaRegisterValue_type callFlags = pCallContext->m_flags;
        if (((csValue & 3) == 3) && (0x40000 & (cr0 | callFlags.value)))
        {
            return DI_GP;
        }
    }
    return DI_SUCCESS;
}

int Diana_ProcessorSetGetOperand_XMM_index(struct _dianaContext * pDianaContext,
                                           DianaProcessor * pCallContext,
                                           int usedSize,
                                           DianaRegisterXMM_type * pResult,
                                           int bSet,
                                           DianaRmIndex * pRmIndex,
                                           int flags)
{
    OPERAND_SIZE selector = GET_REG_DS;
    OPERAND_SIZE address = 0;

    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                      pCallContext,
                                      pRmIndex,
                                      &selector,
                                      &address));


    if (pCallContext->m_result.pInfo->m_flags & DI_FLAG_CMD_MUST_BE_ALIGNED)
    {
        DI_CHECK(Di_XmmCheckAlignment(pCallContext, address, 16));
    }

    if (bSet)
    {
        return DianaProcessor_XMM_SetMemValue(pCallContext, 
                                              selector,
                                              address, 
                                              usedSize,
                                              pResult,
                                              flags,
                                              pRmIndex->seg_reg);
    }

    return DianaProcessor_XMM_GetMemValue(pCallContext, 
                                          selector,
                                          address, 
                                          usedSize,
                                          pResult,
                                          flags,
                                          pRmIndex->seg_reg);
}

int DianaProcessor_XMM_SetGetOperand(struct _dianaContext * pDianaContext,
                                     DianaProcessor * pCallContext,
                                     int opNumber,
                                     DianaRegisterXMM_type * pResult,
                                     int bSet,
                                     int sizeOfOperand,
                                     int flags,
                                     int setNewSizeLegacyMode)
{
    DianaLinkedOperand * pLinkedOp = pCallContext->m_result.linkedOperands + opNumber;

    if (opNumber >= pCallContext->m_result.iLinkedOpCount ||
        opNumber < 0)
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }

    if (setNewSizeLegacyMode)
    {
        pLinkedOp->usedSize = sizeOfOperand;
    }
    switch(pLinkedOp->type)
    {
    case diana_register:
        return Diana_ProcessorSetGetOperand_XMM_register(pDianaContext,
                                                         pCallContext,
                                                         pLinkedOp,
                                                         pResult,
                                                         bSet);
    case diana_index:
        return Diana_ProcessorSetGetOperand_XMM_index(pDianaContext,
                                                      pCallContext,
                                                      pLinkedOp->usedSize,
                                                      pResult,
                                                      bSet,
                                                      &pLinkedOp->value.rmIndex,
                                                      flags);
    default:
        break;
    }

    Diana_FatalBreak();
    return DI_ERROR;
}

int DianaProcessor_XMM_SetGetOperandEx(struct _dianaContext * pDianaContext,
                                       DianaProcessor * pCallContext,
                                       int opNumber,
                                       DianaRegisterXMM_type * pResult,
                                       int bSet,
                                       int * pSize,
                                       int flags,
                                       int setNewSizeLegacyMode)
{
    DianaLinkedOperand * pLinkedOp = pCallContext->m_result.linkedOperands + opNumber;
    int size = *pSize;
    if (opNumber >= 0 && opNumber < pCallContext->m_result.iLinkedOpCount &&
        pLinkedOp->type == diana_register &&
        pLinkedOp->value.recognizedRegister >= reg_MM0 && pLinkedOp->value.recognizedRegister <= reg_MM7)
    {
        // an MMX register: 64 bits (16 would reach the next x87 register)
        size = 8;
        setNewSizeLegacyMode = 1;
    }
    DI_CHECK(DianaProcessor_XMM_SetGetOperand(pDianaContext, pCallContext, opNumber, pResult, bSet, size, flags,
                                              setNewSizeLegacyMode));
    *pSize = pLinkedOp->usedSize;
    return DI_SUCCESS;
}

// ---------------------------------------------------------------------------------------------------------------
// YMM

static int Di_IsVectorRegister(DianaUnifiedRegister reg)
{
    return (reg >= reg_XMM0 && reg <= reg_XMM15) || (reg >= reg_YMM0 && reg <= reg_YMM15);
}

DianaUnifiedRegister DianaProcessor_YMM_Register(DianaUnifiedRegister reg)
{
    if (reg >= reg_XMM0 && reg <= reg_XMM15)
        return (DianaUnifiedRegister)(reg_YMM0 + (reg - reg_XMM0));
    if (reg >= reg_YMM0 && reg <= reg_YMM15)
        return reg;
    Diana_FatalBreak();
    return reg_YMM0;
}

DianaRegisterYMM_type DianaProcessor_YMM_GetValue(DianaProcessor * pCallContext,
                                                  DianaUnifiedRegister reg)
{
    DianaRegisterYMM_type r;
    DianaRegInfo * pReg = DianaProcessor_QueryReg(pCallContext, DianaProcessor_YMM_Register(reg));
    DIANA_MEMCPY(r.u8, pCallContext->m_pRegistersVector + pReg->m_offset, 32);
    return r;
}

void DianaProcessor_YMM_SetValue(DianaProcessor * pCallContext,
                                 DianaUnifiedRegister reg,
                                 const DianaRegisterYMM_type * value)
{
    DianaRegInfo * pReg = DianaProcessor_QueryReg(pCallContext, DianaProcessor_YMM_Register(reg));
    DIANA_MEMCPY(pCallContext->m_pRegistersVector + pReg->m_offset, value->u8, 32);
}

void DianaProcessor_YMM_ClearUpper(DianaProcessor * pCallContext,
                                   DianaUnifiedRegister reg)
{
    DianaProcessor_ZMM_ClearAbove(pCallContext, reg, 16);
}

DianaUnifiedRegister DianaProcessor_ZMM_Register(DianaUnifiedRegister reg)
{
    int size = 0;
    int number = Diana_VectorRegisterNumber(reg, &size);
    return number < 0 ? reg_none : (DianaUnifiedRegister)(reg_ZMM0 + number);
}

DianaRegisterZMM_type DianaProcessor_ZMM_GetValue(DianaProcessor * pCallContext,
                                                  DianaUnifiedRegister reg)
{
    DianaRegisterZMM_type r;
    DianaUnifiedRegister zmm = DianaProcessor_ZMM_Register(reg);
    DIANA_MEMSET(&r, 0, sizeof(r));
    if (zmm == reg_none)
    {
        Diana_FatalBreak();
        return r;
    }
    DIANA_MEMCPY(r.u8, pCallContext->m_pRegistersVector + DianaProcessor_QueryReg(pCallContext, zmm)->m_offset, 64);
    return r;
}

void DianaProcessor_ZMM_SetValue(DianaProcessor * pCallContext,
                                 DianaUnifiedRegister reg,
                                 const DianaRegisterZMM_type * value)
{
    DianaUnifiedRegister zmm = DianaProcessor_ZMM_Register(reg);
    if (zmm == reg_none)
    {
        Diana_FatalBreak();
        return;
    }
    DIANA_MEMCPY(pCallContext->m_pRegistersVector + DianaProcessor_QueryReg(pCallContext, zmm)->m_offset, value->u8, 64);
}

void DianaProcessor_ZMM_ClearAbove(DianaProcessor * pCallContext,
                                   DianaUnifiedRegister reg,
                                   int bytes)
{
    DianaUnifiedRegister zmm = DianaProcessor_ZMM_Register(reg);
    if (zmm == reg_none || bytes < 0 || bytes > 64)
    {
        Diana_FatalBreak();
        return;
    }
    DIANA_MEMSET(pCallContext->m_pRegistersVector + DianaProcessor_QueryReg(pCallContext, zmm)->m_offset + bytes, 0,
                 64 - bytes);
}

static int Di_YmmMemory(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext,
                        DianaRmIndex * pRmIndex,
                        DianaRegisterYMM_type * pValue,
                        int bSet,
                        int size,
                        int alignment)
{
    OPERAND_SIZE selector = GET_REG_DS;
    OPERAND_SIZE address = 0;
    OPERAND_SIZE done = 0;

    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext, pCallContext, pRmIndex, &selector, &address));
    DI_CHECK(Di_XmmCheckAlignment(pCallContext, address, alignment));
    if (bSet)
    {
        DI_CHECK(pCallContext->m_pMemoryStream->pWriteFnc(pCallContext->m_pMemoryStream, selector, address,
                                                          pValue->u8, size, &done, pCallContext,
                                                          pRmIndex->seg_reg));
    }
    else
    {
        DIANA_MEMSET(pValue, 0, sizeof(*pValue));
        DI_CHECK(pCallContext->m_pMemoryStream->pReadFnc(pCallContext->m_pMemoryStream, selector, address,
                                                         pValue->u8, size, &done, pCallContext,
                                                         pRmIndex->seg_reg));
    }
    return done == (OPERAND_SIZE)size ? DI_SUCCESS : DI_ERROR;
}

int DianaProcessor_YMM_SetGetOperand(struct _dianaContext * pDianaContext,
                                     DianaProcessor * pCallContext,
                                     int opNumber,
                                     DianaRegisterYMM_type * pValue,
                                     int bSet,
                                     int size,
                                     int alignment)
{
    DianaLinkedOperand * pLinkedOp = pCallContext->m_result.linkedOperands + opNumber;
    if (opNumber >= pCallContext->m_result.iLinkedOpCount || opNumber < 0 || size <= 0 || size > 32)
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }

    switch (pLinkedOp->type)
    {
    case diana_register:
        if (Di_IsVectorRegister(pLinkedOp->value.recognizedRegister))
        {
            DianaUnifiedRegister reg = pLinkedOp->value.recognizedRegister;
            DianaRegInfo * pReg = DianaProcessor_QueryReg(pCallContext, DianaProcessor_YMM_Register(reg));
            char * pStart = pCallContext->m_pRegistersVector + pReg->m_offset;
            if (reg >= reg_XMM0 && reg <= reg_XMM15 && size > 16)
                size = 16;
            if (bSet)
            {
                // a VEX write: the bits above the register up to 511 are zero
                DIANA_MEMCPY(pStart, pValue->u8, size);
                DianaProcessor_ZMM_ClearAbove(pCallContext, reg, reg >= reg_XMM0 && reg <= reg_XMM15 ? 16 : 32);
            }
            else
            {
                DIANA_MEMSET(pValue, 0, sizeof(*pValue));
                DIANA_MEMCPY(pValue->u8, pStart, size);
            }
            return DI_SUCCESS;
        }
        else
        {
            // a general register: its own size, a 32-bit write zero-extends in 64-bit mode
            OPERAND_SIZE value = 0;
            int registerSize = 0;
            if (bSet)
            {
                DIANA_MEMCPY(&value, pValue->u8, sizeof(value));
                return DianaProcessor_SetGetOperand(pDianaContext, pCallContext, opNumber, &value, 1,
                                                    &registerSize, 0);
            }
            DI_CHECK(DianaProcessor_SetGetOperand(pDianaContext, pCallContext, opNumber, &value, 0,
                                                  &registerSize, 0));
            DIANA_MEMSET(pValue, 0, sizeof(*pValue));
            DIANA_MEMCPY(pValue->u8, &value, sizeof(value));
            return DI_SUCCESS;
        }
    case diana_index:
        return Di_YmmMemory(pDianaContext, pCallContext, &pLinkedOp->value.rmIndex, pValue, bSet, size, alignment);
    case diana_memory:
        return Di_YmmMemory(pDianaContext, pCallContext, &pLinkedOp->value.memory.m_index, pValue, bSet, size,
                            alignment);
    default:
        break;
    }
    Diana_FatalBreak();
    return DI_ERROR;
}
