
#include "diana_processor_core_impl.h"

#include "diana_proc_gen.h"

// special
#include "diana_processor_impl_init.h"
#include "diana_processor_specific.h"

DianaRegInfo * DianaProcessor_FPU_QueryReg(DianaProcessor * pThis, 
                                           DianaUnifiedRegister reg)
{
    int index = 0;
    DianaUnifiedRegister realRegister;
    if (reg < reg_fpu_ST0 || reg > reg_fpu_ST7)
    {
        Diana_FatalBreak();
        return 0;
    }

    index = reg_fpu_ST0 + 16 - reg - Diana_FPU_GetStackTop(pThis);
    --index;
    index = index % 8;
    realRegister = reg_MM_Internal0 + index;
    return DianaProcessor_QueryReg(pThis, realRegister);
}

DianaRegInfo * DianaProcessor_FPU_QueryReg_MM(DianaProcessor * pThis, 
                                              DianaUnifiedRegister reg)
{
    int index = 0;
    DianaUnifiedRegister realRegister;
    if (reg < reg_MM0 || reg > reg_MM7)
    {
        Diana_FatalBreak();
        return 0;
    }

    if (pThis->m_context.iAMD64Mode == 0)
    {
        index = reg_MM0 + 16 - reg - 1;
        index = index % 8;
        realRegister = reg_MM0 + index;
        return pThis->m_registers + realRegister;    
    }

    index = reg_MM0 + 16 - reg - Diana_FPU_GetStackTop(pThis);
    --index;
    index = index % 8;
    realRegister = reg_MM0 + index;
    return pThis->m_registers + realRegister;
}

DianaRegInfo * DianaProcessor_QueryReg(DianaProcessor * pThis, 
                                       DianaUnifiedRegister reg)
{
    if (reg >= reg_fpu_ST0 && reg <= reg_fpu_ST7)
    {
        return DianaProcessor_FPU_QueryReg(pThis, reg);
    }
    if (reg >= reg_MM0 && reg <= reg_MM7)
    {
        return DianaProcessor_FPU_QueryReg_MM(pThis, reg);
    }
    return pThis->m_registers + reg;
}
 
int  DianaProcessor_QueryFlag(DianaProcessor * pThis, 
                              OPERAND_SIZE flag)
{
    return (pThis->m_flags.value & flag) ? 1 : 0;
}

void DianaProcessor_SetFlag(DianaProcessor * pThis, 
                            OPERAND_SIZE flag)
{
    pThis->m_flags.value |= flag;
}
 
void DianaProcessor_ClearFlag(DianaProcessor * pThis, 
                              OPERAND_SIZE flag)
{
    pThis->m_flags.value &= ~flag;
}

void DianaProcessor_SetValue(DianaProcessor * pCallContext,
                             DianaUnifiedRegister regId,
                             DianaRegInfo * pReg,
                             OPERAND_SIZE value)
{
    DianaProcessor_SetValueEx(pCallContext,
                              regId,
                              pReg,
                              value,
                              pReg->m_size);
}

//---
void DianaProcessor_UpdatePSZ(DianaProcessor * pCallContext,
                              OPERAND_SIZE value,
                              int opSize)
{
    pCallContext->m_stateFlags |= DI_PROC_STATE_UPDATE_FLAGS_PSZ;
    switch(opSize)
    {
    case 1:
        UPDATE_PSZ((DI_CHAR)value, 0x80);
        break;
    case 2:
        UPDATE_PSZ((DI_UINT16)value, 0x8000);
        break;
    case 4:
        UPDATE_PSZ((DI_UINT32)value, 0x80000000UL);
        break;
    case 8:
        UPDATE_PSZ(value, 0x8000000000000000ULL);
        break;
    default:
        Diana_FatalBreak();
        break;
    }
    pCallContext->m_stateFlags &= ~DI_PROC_STATE_UPDATE_FLAGS_PSZ;
    
}

void DianaProcessor_CmdUsesNormalRep(DianaProcessor * pCallContext)
{
    pCallContext->m_stateFlags |= DI_PROC_STATE_CMD_USES_NORMAL_REP;
}

int DianaProcessor_RegisterFirePoint(DianaProcessor * pCallContext,
                                     const DianaProcessorFirePoint * pPoint)
{
    if (pCallContext->m_firePointsCount >= DIANA_PROCESSOR_MAX_FIRE_POINTS)
        return DI_OUT_OF_MEMORY;

    pCallContext->m_firePoints[pCallContext->m_firePointsCount++] = *pPoint;
    return DI_SUCCESS;
}

int DianaProcessor_ReadMemory(DianaProcessor * pThis,
                              OPERAND_SIZE selector,
                              OPERAND_SIZE offset,
                              void * pBuffer, 
                              OPERAND_SIZE iBufferSize, 
                              OPERAND_SIZE * readBytes,
                              int flags,
                              DianaUnifiedRegister segReg)
{
    flags;
    return pThis->m_pMemoryStream->pReadFnc(pThis->m_pMemoryStream,
                                            selector, 
                                            offset,
                                            pBuffer, 
                                            iBufferSize, 
                                            readBytes,
                                            pThis,
                                            segReg);
                                     
}
int DianaProcessor_ReadMemory_Exact(DianaProcessor * pThis,
                                    OPERAND_SIZE selector,
                                    OPERAND_SIZE offset,
                                    void * pBuffer, 
                                    OPERAND_SIZE iBufferSize, 
                                    int flags,
                                    DianaUnifiedRegister segReg)
{
    OPERAND_SIZE readBytes = 0;
    flags;
    DI_CHECK(pThis->m_pMemoryStream->pReadFnc(pThis->m_pMemoryStream,
                                            selector, 
                                            offset,
                                            pBuffer, 
                                            iBufferSize, 
                                            &readBytes,
                                            pThis,
                                            segReg));
    if (readBytes != iBufferSize)
    {
        return DI_PARTIAL_READ_WRITE;
    }
    return DI_SUCCESS;
}
int DianaProcessor_WriteMemory(DianaProcessor * pThis,
                               OPERAND_SIZE selector,
                               OPERAND_SIZE offset,
                               void * pBuffer, 
                               OPERAND_SIZE iBufferSize, 
                               OPERAND_SIZE * readed,
                               int flags,
                               DianaUnifiedRegister segReg)
{
    flags;
    return pThis->m_pMemoryStream->pWriteFnc(pThis->m_pMemoryStream,
                                             selector,
                                             offset,
                                             pBuffer, 
                                             iBufferSize, 
                                             readed,
                                             pThis,
                                             segReg);
                                     
}

int Diana_ProcessorSetGetOperand_register(struct _dianaContext * pDianaContext,
                                          DianaProcessor * pCallContext,
                                          DianaLinkedOperand * pLinkedOp,
                                          OPERAND_SIZE * pResult,
                                          int bSet)
{
    pDianaContext;

    if (bSet)
    {
        DianaProcessor_SetValue(pCallContext, 
                                pLinkedOp->value.recognizedRegister,
                                DianaProcessor_QueryReg(pCallContext,
                                                        pLinkedOp->value.recognizedRegister), 
                                *pResult);
    }
    else
    {
        *pResult = DianaProcessor_GetValue(pCallContext, 
                                           DianaProcessor_QueryReg(pCallContext,
                                                                   pLinkedOp->value.recognizedRegister));
    }
    return DI_SUCCESS;
}

int DianaProcessor_CalcIndex(struct _dianaContext * pDianaContext,
                             DianaProcessor * pCallContext,
                             const DianaRmIndex * pIndex,
                             OPERAND_SIZE * pSelector,
                             OPERAND_SIZE * pAddress)
{
    OPERAND_SIZE reg = 0;
    OPERAND_SIZE indexedReg = 0;

    OPERAND_SIZE dispValue = 0;
    OPERAND_SIZE index = 0;
    OPERAND_SIZE address = 0;
    OPERAND_SIZE selector = 0;

    if (pIndex->reg != reg_none)
    {
        reg = DianaProcessor_GetValue(pCallContext,
                                      DianaProcessor_QueryReg(pCallContext, 
                                                              pIndex->reg));
    }

    if (pIndex->indexed_reg != reg_none)
    {
        indexedReg = DianaProcessor_GetValue(pCallContext, 
                                             DianaProcessor_QueryReg(pCallContext, 
                                                                     pIndex->indexed_reg));
    }

    dispValue = (OPERAND_SIZE)pIndex->dispValue;
    index = (OPERAND_SIZE)pIndex->index;

    // RIP-relative addressing
    if( pDianaContext->iAMD64Mode && pIndex->reg == reg_RIP )
    {
        reg += pCallContext->m_result.iFullCmdSize;
    }

    address = reg + indexedReg * index + dispValue;

    switch (pDianaContext->iCurrentCmd_addressSize)
    {
    case 8:
        break;
    case 4:
        address = address & 0xFFFFFFFF;
        break;
    default:
        return DI_ERROR;
    }

    selector = DianaProcessor_GetValue(pCallContext, 
                                       DianaProcessor_QueryReg(pCallContext, 
                                                               pIndex->seg_reg));

    *pSelector = selector;
    *pAddress = address;
    return DI_SUCCESS;
}

int DianaProcessor_GetAddress(struct _dianaContext * pDianaContext,
                              DianaProcessor * pCallContext,
                              DianaLinkedOperand * pLinkedOp,
                              OPERAND_SIZE * pSelector,
                              OPERAND_SIZE * pAddress)
{
    DianaRmIndex * pIndex = 0;
    switch(pLinkedOp->type)
    {
        case diana_register:
            return Diana_ProcessorSetGetOperand_register(pDianaContext, 
                                                         pCallContext, 
                                                         pLinkedOp,
                                                         pAddress,
                                                         0);
        case diana_index:
            pIndex = &pLinkedOp->value.rmIndex;
            break;
        case diana_memory:
            pIndex = &pLinkedOp->value.memory.m_index;
            break;
        default:
            return DI_ERROR;
    }

    return DianaProcessor_CalcIndex(pDianaContext,
                                    pCallContext,
                                    pIndex, 
                                    pSelector, 
                                    pAddress);
}

int Diana_ProcessorSetGetOperand_index(struct _dianaContext * pDianaContext,
                                       DianaProcessor * pCallContext,
                                       int usedSize,
                                       OPERAND_SIZE * pResult,
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
                                      &address))

    if (bSet)
    {
        if (pCallContext->m_stateFlags & DI_PROC_STATE_UPDATE_FLAGS_PSZ)
        {
            DianaProcessor_UpdatePSZ(pCallContext, 
                                     *pResult,
                                     usedSize);
        }
        return DianaProcessor_SetMemValue(pCallContext, 
                                          selector,
                                          address, 
                                          usedSize,
                                          pResult,
                                          flags,
                                          pRmIndex->seg_reg);

    }
        
    return DianaProcessor_GetMemValue(pCallContext, 
                                      selector,
                                      address, 
                                      usedSize,
                                      pResult,
                                      flags,
                                      pRmIndex->seg_reg);
}

int Diana_ProcessorGetOperand_index_ex(struct _dianaContext * pDianaContext,
                                       DianaProcessor * pCallContext,
                                       int usedSize,
                                       OPERAND_SIZE * pAddress,
                                       OPERAND_SIZE * pResult,
                                       DianaRmIndex * pRmIndex)
{
    OPERAND_SIZE selector = GET_REG_DS;
    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                      pCallContext,
                                      pRmIndex,
                                      &selector,
                                      pAddress));

    return DianaProcessor_GetMemValue(pCallContext, 
                                      selector,
                                      *pAddress, 
                                      usedSize,
                                      pResult,
                                      0,
                                      pRmIndex->seg_reg);
}


int Diana_ProcessorGetOperand_index_ex2(struct _dianaContext * pDianaContext,
                                        DianaProcessor * pCallContext,
                                        int usedSize,
                                        OPERAND_SIZE * pAddress,
                                        OPERAND_SIZE * pResult,
                                        DianaRmIndex * pRmIndex,
                                        OPERAND_SIZE * pSelector)
{
    *pSelector = GET_REG_DS;
    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                      pCallContext,
                                      pRmIndex,
                                      pSelector,
                                      pAddress));

    return DianaProcessor_GetMemValue(pCallContext, 
                                      *pSelector,
                                      *pAddress, 
                                      usedSize,
                                      pResult,
                                      0,
                                      pRmIndex->seg_reg);
}
int Diana_ProcessorSetGetOperand_imm(struct _dianaContext * pDianaContext,
                                     DianaProcessor * pCallContext,
                                     DianaLinkedOperand * pLinkedOp,
                                     OPERAND_SIZE * pResult,
                                     int bSet)
{
    pDianaContext;
    pCallContext;

    if (bSet)
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }
    else
    {
        *pResult = (OPERAND_SIZE)pLinkedOp->value.imm;
    }
    return DI_SUCCESS;
}


int DianaProcessor_GetOperandSize(DianaProcessor * pCallContext,
                                  int opNumber,
                                  int * pSizeOfOperand)
{
    DianaLinkedOperand * pLinkedOp = pCallContext->m_result.linkedOperands + opNumber;

    *pSizeOfOperand = 0;
    if (opNumber >= pCallContext->m_result.iLinkedOpCount ||
        opNumber < 0)
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }
    *pSizeOfOperand = pLinkedOp->usedSize;
    return DI_SUCCESS;
}

int DianaProcessor_SetGetOperand(struct _dianaContext * pDianaContext,
                                 DianaProcessor * pCallContext,
                                 int opNumber,
                                 OPERAND_SIZE * pResult,
                                 int bSet,
                                 int * pSizeOfOperand,
                                 int flags)
{
    DianaLinkedOperand * pLinkedOp = pCallContext->m_result.linkedOperands + opNumber;

    if (opNumber >= pCallContext->m_result.iLinkedOpCount ||
        opNumber < 0)
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }

    
    *pSizeOfOperand = pLinkedOp->usedSize;
    switch(pLinkedOp->type)
    {
        case diana_register:
            return Diana_ProcessorSetGetOperand_register(pDianaContext,
                                                         pCallContext,
                                                         pLinkedOp,
                                                         pResult,
                                                         bSet);

        case diana_index:
            return Diana_ProcessorSetGetOperand_index(pDianaContext,
                                                      pCallContext,
                                                      pLinkedOp->usedSize,
                                                      pResult,
                                                      bSet,
                                                      &pLinkedOp->value.rmIndex,
                                                      flags);

        case diana_imm:
            return Diana_ProcessorSetGetOperand_imm(pDianaContext,
                                                    pCallContext,
                                                    pLinkedOp,
                                                    pResult,
                                                    bSet);

        case diana_call_ptr:
            break;
        case diana_rel:
            break;
        case diana_memory:
            return Diana_ProcessorSetGetOperand_index(pDianaContext,
                                                      pCallContext,
                                                      pLinkedOp->usedSize,
                                                      pResult,
                                                      bSet,
                                                      &pLinkedOp->value.memory.m_index,
                                                      flags);
            // this is command-specific stuff
        case diana_none:
        case diana_reserved_reg:
        default:
            Diana_FatalBreak();
            return DI_ERROR;
    }

    Diana_FatalBreak();
    return DI_ERROR;
}

int DianaProcessor_SetCOA_Add(struct _dianaContext * pDianaContext,
                              DianaProcessor * pCallContext,
                              const OPERAND_SIZE * pOldValue,
                              const OPERAND_SIZE * pNewValue,
                              const OPERAND_SIZE * pOperand,
                              int opSize,
                              int flags
                             )
{
 //PF     Parity Flag -- Set if low-order eight bits of result contain
 //           an even number of 1 bits; cleared otherwise.
 //ZF     Zero Flag -- Set if result is zero; cleared otherwise.
 //SF     Sign Flag -- Set equal to high-order bit of result (0 is
 //           positive, 1 if negative).
 //OF     Overflow Flag -- Set if result is too large a positive number
 //       or too small a negative number (excluding sign-bit) to fit in
 //       destination operand; cleared otherwise.
    pDianaContext;

    if (flags & DI_FLAG_OF)
    {
        if ((*pOldValue & DianaProcessor_GetSignMask(opSize)) ==
            (*pOperand & DianaProcessor_GetSignMask(opSize)))
        {
            if ((*pOldValue & DianaProcessor_GetSignMask(opSize)) !=
                (*pNewValue & DianaProcessor_GetSignMask(opSize)))
            {
                SET_FLAG_OF;
            }
        }
    }
    if (flags & DI_FLAG_AF)
    {
        if (((unsigned int)*pNewValue & 0xF) < ((unsigned int)*pOldValue & 0xF))
        {
            SET_FLAG_AF;
        }
    }
    if (flags & DI_FLAG_CF)
    {
        if (DianaProcessor_CutValue(*pNewValue, opSize) < *pOldValue)
        {
            // was overflow
            SET_FLAG_CF;
        }
    }
    return DI_SUCCESS;
}

int DianaProcessor_SetCOA_AddCF(struct _dianaContext * pDianaContext,
                                DianaProcessor * pCallContext,
                                const OPERAND_SIZE * pOldValue,
                                const OPERAND_SIZE * pNewValue,
                                const OPERAND_SIZE * pOperand,
                                int opSize,
                                int flags
                                )
{
    pDianaContext;

    if (flags & DI_FLAG_OF)
    {
        if ((*pOldValue & DianaProcessor_GetSignMask(opSize)) ==
            (*pOperand & DianaProcessor_GetSignMask(opSize)))
        {
            if ((*pOldValue & DianaProcessor_GetSignMask(opSize)) !=
                (*pNewValue & DianaProcessor_GetSignMask(opSize)))
            {
                SET_FLAG_OF;
            }
        }
    }
    if (flags & DI_FLAG_AF)
    {
        if (((unsigned int)*pNewValue & 0xF) <= ((unsigned int)*pOldValue & 0xF))
        {
            SET_FLAG_AF;
        }
    }
    if (flags & DI_FLAG_CF)
    {
        if (DianaProcessor_CutValue( *pNewValue, opSize ) <= *pOldValue )
        {
            SET_FLAG_CF;
        }
    }
    return DI_SUCCESS;
}

int DianaProcessor_SetCOA_Sub(struct _dianaContext * pDianaContext,
                              DianaProcessor * pCallContext,
                              const OPERAND_SIZE * pOldValue,
                              const OPERAND_SIZE * pNewValue,
                              const OPERAND_SIZE * pOperand,
                              int opSize,
                              int flags
                             )
{
    pDianaContext;

    if (flags & DI_FLAG_OF)
    {
        if ((*pOldValue & DianaProcessor_GetSignMask(opSize)) !=
            (*pOperand & DianaProcessor_GetSignMask(opSize)))
        {
            if ((*pOldValue & DianaProcessor_GetSignMask(opSize)) !=
                (*pNewValue & DianaProcessor_GetSignMask(opSize)))
            {
                SET_FLAG_OF;
            }
        }
    }
    if (flags & DI_FLAG_AF)
    {
        if (((unsigned  int)*pNewValue & 0xF) > ((unsigned int)*pOldValue & 0xF))
        {
            SET_FLAG_AF;
        }
    }    
    if (flags & DI_FLAG_CF)
    {
        if (DianaProcessor_CutValue(*pNewValue, opSize) > *pOldValue)
        {
            // was overflow
            SET_FLAG_CF;
        }
    }
   
    return DI_SUCCESS;
}

int DianaProcessor_SetCOA_SubCF(struct _dianaContext * pDianaContext,
                                DianaProcessor * pCallContext,
                                const OPERAND_SIZE * pOldValue,
                                const OPERAND_SIZE * pNewValue,
                                const OPERAND_SIZE * pOperand,
                                int opSize,
                                int flags
                                )
{
    pDianaContext;

    if (flags & DI_FLAG_OF)
    {
        if ((*pOldValue & DianaProcessor_GetSignMask(opSize)) !=
            (*pOperand & DianaProcessor_GetSignMask(opSize)))
        {
            if ((*pOldValue & DianaProcessor_GetSignMask(opSize)) !=
                (*pNewValue & DianaProcessor_GetSignMask(opSize)))
            {
                SET_FLAG_OF;
            }
        }
    }
    if (flags & DI_FLAG_AF)
    {
        if (((unsigned  int)*pNewValue & 0xF) >= ((unsigned int)*pOldValue & 0xF))
        {
            SET_FLAG_AF;
        }
    }
    if (flags & DI_FLAG_CF)
    {
        if (DianaProcessor_CutValue(*pNewValue, opSize) >= *pOldValue)
        {
            // was overflow
            SET_FLAG_CF;
        }
    }

    return DI_SUCCESS;
}

int DianaProcessor_SignExtend(OPERAND_SIZE * pVariable, 
                              int size, 
                              int newSize)
{
    if (size == newSize)
        return DI_SUCCESS;

    switch(size)
    {
    case 1:
        *pVariable = (long long)(char)*pVariable;
        break;
    case 2:
        *pVariable = (long long)(short)*pVariable;
        break;
    case 4:
        *pVariable = (long long)(int)*pVariable;
        break;
    case 8:
        break;
    default:
        Diana_FatalBreak();
        return DI_ERROR;
    }
    *pVariable = DianaProcessor_CutValue(*pVariable, newSize);
    return 0;
}


int DianaProcessor_Query64RegisterFor32(DianaUnifiedRegister registerIn,
                                        DianaUnifiedRegister * pUsedReg)
{
    *pUsedReg = reg_none;
    switch( registerIn ) {
        case reg_EAX:
        case reg_ECX:
        case reg_EDX:
        case reg_EBX:
        case reg_ESP:
        case reg_EBP:
        case reg_ESI:
        case reg_EDI:
            *pUsedReg = reg_RAX - reg_EAX + registerIn;
            break;
        case reg_R8D:
        case reg_R9D:
        case reg_R10D:
        case reg_R11D:
        case reg_R12D:
        case reg_R13D:
        case reg_R14D:
        case reg_R15D:
            *pUsedReg = reg_R8 - reg_R8D + registerIn;
            break;
        default:
            return DI_ERROR;
    }
    return DI_SUCCESS;
}
int DianaProcessor_QueryRaxRegister(int size, 
                                    DianaUnifiedRegister * pUsedReg)
{
    switch(size)
    {
    case DIANA_MODE64:
        *pUsedReg = reg_RAX;
        break;

    case DIANA_MODE32:
        *pUsedReg = reg_EAX;
        break;

    case DIANA_MODE16:
        *pUsedReg = reg_AX;
        break;

    case 1:
        *pUsedReg = reg_AL;
        break;
    default:
        return DI_ERROR;
    }
    return DI_SUCCESS;
}


int DianaProcessor_QueryRdxRegister(int size, 
                                    DianaUnifiedRegister * pUsedReg)
{
    switch(size)
    {
    case DIANA_MODE64:
        *pUsedReg = reg_RDX;
        break;

    case DIANA_MODE32:
        *pUsedReg = reg_EDX;
        break;

    case DIANA_MODE16:
        *pUsedReg = reg_DX;
        break;

    case 1:
        *pUsedReg = reg_DL;
        break;
    default:
        return DI_ERROR;
    }
    return DI_SUCCESS;
}


int DianaProcessor_QueryRcxRegister(int size, 
                                    DianaUnifiedRegister * pUsedReg)
{
    switch(size)
    {
    case DIANA_MODE64:
        *pUsedReg = reg_RCX;
        break;

    case DIANA_MODE32:
        *pUsedReg = reg_ECX;
        break;

    case DIANA_MODE16:
        *pUsedReg = reg_CX;
        break;

    case 1:
        *pUsedReg = reg_CL;
        break;
    default:
        return DI_ERROR;
    }
    return DI_SUCCESS;
}


int Diana_QueryAddress(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext, 
                           int argument,
                           OPERAND_SIZE * pSelector,
                           OPERAND_SIZE * pAddress,
                           DianaUnifiedRegister * pSeg_reg)
{
    DianaRmIndex * pIndex = 0;
    if (pCallContext->m_result.iLinkedOpCount <= argument)
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }

    if (pCallContext->m_result.linkedOperands[argument].type == diana_index)
    {
        pIndex = &pCallContext->m_result.linkedOperands[argument].value.rmIndex;
    }
    else
    if (pCallContext->m_result.linkedOperands[argument].type == diana_memory)
    {
        pIndex = &pCallContext->m_result.linkedOperands[argument].value.memory.m_index;
    }
    else
    {
        return DI_ERROR;
    }    

    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                      pCallContext,
                                      pIndex,
                                      pSelector,
                                      pAddress));
    *pSeg_reg = pIndex->seg_reg;
    return DI_SUCCESS;
}

int Diana_WriteRawBufferToArgMem(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext, 
                           int argument,
                           void * pBuffer,
                           OPERAND_SIZE size,
                           OPERAND_SIZE * doneBytes,
                           int flags)
{

    DianaUnifiedRegister segReg = reg_DS;
    OPERAND_SIZE selector = 0;
    OPERAND_SIZE address = 0;

    DI_CHECK(Diana_QueryAddress(pDianaContext, 
                           pCallContext,
                           argument,
                           &selector,
                           &address,
                           &segReg));
                    
    DI_CHECK(DianaProcessor_WriteMemory(pCallContext, 
                               selector,
                               address,
                               pBuffer,
                               size,
                               doneBytes,
                               flags,
                               segReg));

    if (*doneBytes != size)
        return DI_PARTIAL_READ_WRITE;

    return DI_SUCCESS;
}

int Diana_ReadRawBufferFromArgMem(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext, 
                           int argument,
                           void * pBuffer,
                           OPERAND_SIZE size,
                           OPERAND_SIZE * doneBytes,
                           int flags)
{

    DianaUnifiedRegister segReg = reg_DS;
    OPERAND_SIZE selector = 0;
    OPERAND_SIZE address = 0;

    DI_CHECK(Diana_QueryAddress(pDianaContext, 
                           pCallContext,
                           argument,
                           &selector,
                           &address,
                           &segReg));
                    
    DI_CHECK(DianaProcessor_ReadMemory(pCallContext, 
                               selector,
                               address,
                               pBuffer,
                               size,
                               doneBytes,
                               flags,
                               segReg));

    if (*doneBytes != size)
        return DI_PARTIAL_READ_WRITE;

    return DI_SUCCESS;
}

/// FPU
int Diana_FPU_CheckExceptions(DianaProcessor * pCallContext)
{
    if(pCallContext->m_fpu.statusWord & DI_FPU_SW_SUMMARY)
    {
        return DI_INTERRUPT;
    }
    return DI_SUCCESS;
}

DI_UINT16 Diana_FPU_QueryStatusWord(DianaProcessor * pCallContext) 
{
    return pCallContext->m_fpu.statusWord;
}

int Diana_FPU_CheckFPU(DianaProcessor * pCallContext, int bIgnoreExceptions)
{
    if (!bIgnoreExceptions)
    {
        DI_CHECK(Diana_FPU_CheckExceptions(pCallContext));
    }
    return DI_SUCCESS;
}

void DianaProcessor_SetOptions(DianaProcessor * pThis, int optionsToSet, int optionsToRemove)
{
    pThis->m_options |= optionsToSet;
    pThis->m_options &=  ~optionsToRemove;
}

