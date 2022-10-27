#include "diana_processor_cmd_l.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

int Diana_Call_lahf(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    unsigned char ah = 0;

    pDianaContext;

    ah = pCallContext->m_flags.impl.l.impl.l.impl.l;

    SET_REG_AH(ah);
    DI_PROC_END;
}


int Diana_Call_lea(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //DEST := offst of SRC
    OPERAND_SIZE memSelector = GET_REG_DS, memAddress=0;
    DianaRmIndex  * pIndex = 0;
    DI_DEF_LOCALS(src, dest);
    src;
    src_size;
    oldDestValue;
    
    DI_MEM_GET_DEST(dest);

    if (pCallContext->m_result.linkedOperands[1].type != diana_index)
    {
        return DI_ERROR;
    }
    pIndex = &pCallContext->m_result.linkedOperands[1].value.rmIndex;

    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                      pCallContext,
                                      pIndex,
                                      &memSelector,
                                      &memAddress));
    dest = memAddress;

    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}


int Diana_Call_leave(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    OPERAND_SIZE rbp = 0;
    OPERAND_SIZE value = 0;

    pDianaContext;

    rbp = GET_REG_RBP;
    SET_REG_RSP2(rbp, pCallContext->m_context.iCurrentCmd_addressSize);

    DI_CHECK(diana_internal_pop(pCallContext, &value));
    SET_REG_RBP2(value, pCallContext->m_context.iCurrentCmd_opsize);
    DI_PROC_END
}

int Diana_Call_lods(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    OPERAND_SIZE rsi = 0;
    OPERAND_SIZE value = 0;
    OPERAND_SIZE selector = 0;

    DianaProcessor_CmdUsesNormalRep(pCallContext);

    rsi = GET_REG_RSI2( pCallContext->m_context.iCurrentCmd_addressSize );
    selector = (OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, 
                                                     DianaProcessor_QueryReg(pCallContext,
                                                                             pDianaContext->currentCmd_sreg));

    DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                        selector,
                                        rsi,
                                        pCallContext->m_result.linkedOperands->usedSize,
                                        &value,
                                        0,
                                        pDianaContext->currentCmd_sreg));

    SET_REG_RAX2(value, pCallContext->m_result.linkedOperands->usedSize);

    if (GET_FLAG_DF)
    {
        rsi -= pCallContext->m_result.linkedOperands->usedSize;
    }
    else
    {
        // increment
        rsi += pCallContext->m_result.linkedOperands->usedSize;
    }

    SET_REG_RSI2(rsi, pCallContext->m_context.iCurrentCmd_addressSize);
    DI_PROC_END
}

int Diana_Call_loop(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    OPERAND_SIZE rcx = 0;
    rcx = GET_REG_RCX2(pCallContext->m_context.iCurrentCmd_addressSize);

    rcx -= 1;

    SET_REG_RCX2(rcx, pCallContext->m_context.iCurrentCmd_addressSize);
    if (rcx)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_loope(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    OPERAND_SIZE rcx = 0;
    rcx = GET_REG_RCX2(pCallContext->m_context.iCurrentCmd_addressSize);

    rcx -= 1;

    SET_REG_RCX2(rcx, pCallContext->m_context.iCurrentCmd_addressSize);
    if (rcx && GET_FLAG_ZF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_loopne(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    OPERAND_SIZE rcx = 0;
    rcx = GET_REG_RCX2(pCallContext->m_context.iCurrentCmd_addressSize);

    rcx -= 1;

    SET_REG_RCX2(rcx, pCallContext->m_context.iCurrentCmd_addressSize);
    if (rcx && !GET_FLAG_ZF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}
