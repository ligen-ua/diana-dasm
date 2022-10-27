#include "diana_processor_cmd_internal.h"
#include "diana_disable_warnings.h"

char g_diana_parityBuffer[256];

int IsParity(unsigned char value)
{
    return g_diana_parityBuffer[value];
}

static void add128(DI_UINT64 *plow, DI_UINT64 *phigh, DI_UINT64 a, DI_UINT64 b)
{
    *plow += a;
    // carry test
    if (*plow < a)
        (*phigh)++;
    *phigh += b;
}

void neg128(DI_UINT64 *plow, DI_UINT64 *phigh)
{
    *plow = ~*plow;
    *phigh = ~*phigh;
    add128(plow, phigh, 1, 0);
}

void mul64(DI_UINT64 *plow, DI_UINT64 *phigh, DI_UINT64 a, DI_UINT64 b)
{
    DI_UINT32 a0, a1, b0, b1;
    DI_UINT64 v;

    a0 = ( DI_UINT32 )a;
    a1 = (DI_UINT32)(a >> 32);

    b0 = ( DI_UINT32 )b;
    b1 = (DI_UINT32)(b >> 32);

    v = (DI_UINT64)a0 * (DI_UINT64)b0;
    *plow = v;
    *phigh = 0;

    v = (DI_UINT64)a0 * (DI_UINT64)b1;
    add128(plow, phigh, v << 32, v >> 32);

    v = (DI_UINT64)a1 * (DI_UINT64)b0;
    add128(plow, phigh, v << 32, v >> 32);

    v = (DI_UINT64)a1 * (DI_UINT64)b1;
    *phigh += v;
}

void imul64(DI_UINT64 *plow, DI_UINT64 *phigh, DI_INT64 a, DI_INT64 b)
{
    int sa, sb;

    sa = (a < 0);
    if( sa )
        a = -a;
    sb = (b < 0);
    if (sb)
        b = -b;
    mul64( plow, phigh, a, b );
    if( sa ^ sb ) {
        neg128( plow, phigh );
    }
}

int div64(DI_UINT64 *plow, DI_UINT64 *phigh, DI_UINT64 b)
{
    DI_UINT64 q, r, a1, a0;
    int i, qb, ab;

    a0 = *plow;
    a1 = *phigh;
    if (a1 == 0) 
    {
        q = a0 / b;
        r = a0 % b;
        *plow = q;
        *phigh = r;
    } 
    else 
    {
        if (a1 >= b)
            return 1;
        for(i = 0; i < 64; i++) 
        {
            ab = (int)(a1 >> 63);
            a1 = (a1 << 1) | (a0 >> 63);
            if (ab || a1 >= b) {
                a1 -= b;
                qb = 1;
            } else {
                qb = 0;
            }
            a0 = (a0 << 1) | qb;
        }
        *plow = a0;
        *phigh = a1;
    }
    return 0;
}

int idiv64(DI_UINT64 *plow, DI_UINT64 *phigh, DI_INT64 b)
{
    int sa, sb;
    sa = ((DI_INT64)*phigh < 0);
    if (sa)
        neg128(plow, phigh);
    sb = (b < 0);
    if (sb)
        b = -b;
    if (div64(plow, phigh, b) != 0)
        return 1;
    if (sa ^ sb) {
        if (*plow > (1ULL << 63))
            return 1;
        *plow = - *plow;
    } else {
        if (*plow >= (1ULL << 63))
            return 1;
    }
    if (sa)
    {
        *phigh = - *phigh;
    }
    return 0;
}

int diana_internal_push(DianaProcessor * pCallContext,
                        OPERAND_SIZE * pValue)
{
    OPERAND_SIZE rsp = 0;

    rsp = GET_REG_RSP2(pCallContext->m_context.iCurrentCmd_addressSize);

    if (rsp < pCallContext->m_context.iCurrentCmd_opsize)
        return DI_ERROR;

    rsp -= pCallContext->m_context.iCurrentCmd_opsize;
    DI_CHECK(DianaProcessor_SetMemValue(pCallContext, 
                                        GET_REG_SS,
                                        rsp, 
                                        pCallContext->m_context.iCurrentCmd_opsize,
                                        pValue,
                                        0,
                                        reg_SS));
    SET_REG_RSP2(rsp, pCallContext->m_context.iCurrentCmd_addressSize);
    return DI_SUCCESS;
}

int diana_internal_pop(DianaProcessor * pCallContext,
                       OPERAND_SIZE * pValue)
{
    OPERAND_SIZE rsp = 0;

    rsp = GET_REG_RSP2(pCallContext->m_context.iCurrentCmd_addressSize);

    DI_CHECK(DianaProcessor_GetMemValue(pCallContext, 
                                        GET_REG_SS,
                                        rsp, 
                                        pCallContext->m_context.iCurrentCmd_opsize,
                                        pValue,
                                        0,
                                        reg_SS));

    rsp += pCallContext->m_context.iCurrentCmd_opsize;

    SET_REG_RSP2(rsp, pCallContext->m_context.iCurrentCmd_addressSize);
    return DI_SUCCESS;
}

int Di_CheckZeroExtends(DianaProcessor * pCallContext,
                        OPERAND_SIZE * pDest,
                        int src_size,
                        int * dest_size)
{
    dest_size;

    if (!pCallContext->m_context.iAMD64Mode)
    {
        return DI_SUCCESS;
    }

    if (src_size == 8)
    {
        return DI_SUCCESS;
    }

    if (pCallContext->m_context.iCurrentCmd_opsize == 8)
    {
        if (pCallContext->m_result.pInfo->m_flags & DI_FLAG_CMD_AMD64_SIGN_EXTENDS)
        {
            DI_CHECK(DianaProcessor_SignExtend(pDest, 
                                               src_size,
                                               8));
        }
    }
    else
    if (pCallContext->m_context.iCurrentCmd_opsize == 4)
    {
        if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            DianaUnifiedRegister reg64;
            if( DianaProcessor_Query64RegisterFor32( pCallContext->m_result.linkedOperands->value.recognizedRegister,
                                                     &reg64) == DI_ERROR )
                                                     // dirty hack, check this!
                                                     return DI_SUCCESS;

            DianaProcessor_SetValue(pCallContext, reg64, DianaProcessor_QueryReg(pCallContext, reg64), 0);
        }
    }
    return DI_SUCCESS;
}

int Di_CheckZeroExtends2(DianaProcessor * pCallContext,
                         OPERAND_SIZE * pDest,
                         int src_size,
                         int * dest_size)
{
    dest_size;

    if (!pCallContext->m_context.iAMD64Mode)
        return DI_SUCCESS;
    if (src_size == 8)
        return DI_SUCCESS;

    if (pCallContext->m_context.iCurrentCmd_opsize == 8) {
        if (pCallContext->m_result.pInfo->m_flags & DI_FLAG_CMD_AMD64_SIGN_EXTENDS) {
            DI_CHECK(DianaProcessor_SignExtend(pDest, src_size, 8));
        }
    }
    else if (pCallContext->m_context.iCurrentCmd_opsize == 4) {
        if (pCallContext->m_result.linkedOperands->type == diana_register) {
            DianaUnifiedRegister reg64;
            if( DianaProcessor_Query64RegisterFor32(pCallContext->m_result.linkedOperands[ 1 ].value.recognizedRegister,
                                                    &reg64) == DI_ERROR )
                                                    // dirty hack, check this!
                                                    return DI_SUCCESS;

            DianaProcessor_SetValue(pCallContext, reg64, DianaProcessor_QueryReg(pCallContext, reg64), 0);
        }
    }
    return DI_SUCCESS;
}

int Diana_Call_jcc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext,
                   int opCount,
                   int relOp)
{
    OPERAND_SIZE newRIP = 0; 

    pDianaContext;

    if (pCallContext->m_result.iLinkedOpCount != opCount)
    {
        return DI_ERROR;
    }

    // calculate absolute address
    if (pCallContext->m_result.linkedOperands[relOp].type != diana_rel)
    {
        return DI_ERROR;
    }

    newRIP = GET_REG_RIP2(pCallContext->m_context.iCurrentCmd_opsize) + 
             pCallContext->m_result.linkedOperands[relOp].value.rel.m_value + 
             pCallContext->m_result.iFullCmdSize;

    SET_REG_RIP2(newRIP, pCallContext->m_context.iMainMode_addressSize);
    DI_PROC_END
}

int Di_PrepareSIDI_regs(DianaProcessor * pCallContext,
                        DianaRmIndex * pSrcIndex, 
                        DianaRmIndex * pDestIndex)
{
    if (pSrcIndex)
    {
        if (pCallContext->m_result.iLinkedOpCount != 2)
        {
            return DI_ERROR;
        }

        DIANA_MEMSET(pSrcIndex, 0, sizeof(*pSrcIndex));
        pSrcIndex->seg_reg = reg_DS;
    }
    else
    {
        if (pCallContext->m_result.iLinkedOpCount != 1)
        {
            return DI_ERROR;
        }
    }
    DIANA_MEMSET(pDestIndex, 0, sizeof(*pDestIndex));
    pDestIndex->seg_reg = reg_ES;

    switch(pCallContext->m_context.iCurrentCmd_addressSize)
    {
    case DIANA_MODE64:
        if (pSrcIndex)
        {
            pSrcIndex->reg = reg_RSI;
        }
        pDestIndex->reg = reg_RDI;
        break;

    case DIANA_MODE32:
        if (pSrcIndex)
        {
            pSrcIndex->reg = reg_ESI;
        }
        pDestIndex->reg = reg_EDI;
        break;

    case DIANA_MODE16:
        if (pSrcIndex)
        {
            pSrcIndex->reg = reg_SI;
        }
        pDestIndex->reg = reg_DI;
        break;

    default:
        return DI_ERROR;
    }

    return DI_SUCCESS;
}

void Di_UpdateSIDI(DianaProcessor * pCallContext,
                   OPERAND_SIZE * pSrcRegAddress, 
                   OPERAND_SIZE * pDestRegAddress)
{
    if (GET_FLAG_DF)
    {
        *pDestRegAddress -= pCallContext->m_result.linkedOperands->usedSize;
        if (pSrcRegAddress)
        {
            *pSrcRegAddress -= pCallContext->m_result.linkedOperands->usedSize;
        }
    }
    else
    {
        // increment
        *pDestRegAddress += pCallContext->m_result.linkedOperands->usedSize;
        if (pSrcRegAddress)
        {
            *pSrcRegAddress += pCallContext->m_result.linkedOperands->usedSize;
        }
    }
}


//__asm
//{
//    mov eax, 0
//    mov ax, cs
//    push eax
//    push offset function
//    retf
//}
int Diana_Call_internal_ret(struct _dianaContext * pDianaContext,
                            DianaProcessor * pCallContext,
                            int bPopCS)
{
    // DEST 
    OPERAND_SIZE newCs = 0;
    OPERAND_SIZE rsp = 0, dropBytes = 0;
    OPERAND_SIZE newRIP = 0;
    DI_DEF_LOCAL(src);
    oldDestValue;
    
    if (pCallContext->m_result.iLinkedOpCount == 1)
    {
        DI_MEM_GET_DEST(src);

        if (pCallContext->m_result.linkedOperands[0].type != diana_imm)
        {
            return DI_ERROR;
        }
        dropBytes = pCallContext->m_result.linkedOperands[0].value.imm;
    }
    else
    if (pCallContext->m_result.iLinkedOpCount)
    {
        return DI_ERROR;
    }

    rsp = GET_REG_RSP;

    DI_CHECK(DianaProcessor_GetMemValue(pCallContext, 
                                        GET_REG_SS,
                                        rsp, 
                                        pCallContext->m_context.iCurrentCmd_opsize,
                                        &newRIP,
                                        0,
                                        reg_SS));

    rsp += pCallContext->m_context.iCurrentCmd_opsize;
    if (bPopCS)
    {
        DI_CHECK(DianaProcessor_GetMemValue(pCallContext, 
                                            GET_REG_SS,
                                            rsp, 
                                            pCallContext->m_context.iCurrentCmd_opsize,
                                            &newCs,
                                            0,
                                            reg_SS));

        rsp += pCallContext->m_context.iCurrentCmd_opsize;
    }

    rsp += dropBytes;

    SET_REG_RSP(rsp);
    SET_REG_RIP(newRIP);

    if (bPopCS)
    {
        SET_REG_CS(newCs);
    }
    DI_PROC_END
}
