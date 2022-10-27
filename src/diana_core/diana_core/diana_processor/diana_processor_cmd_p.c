#include "diana_processor_cmd_p.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

/* DI_FLAG_CMD_PUSH_SEG commands:
PUSH CS 0E
PUSH SS 16 
PUSH DS 1E 
PUSH ES 06 
PUSH FS 0F A0 
PUSH GS 0F A8 */

int Diana_Call_push(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    // DEST 
    int usedSize = 0;
    DI_DEF_LOCAL(src);
    oldDestValue;
    DI_MEM_GET_DEST(src);

    if (pCallContext->m_result.pInfo->m_flags & DI_FLAG_CMD_PUSH_SEG)
    {
        OPERAND_SIZE rsp = 0;

        rsp = GET_REG_RSP2(pCallContext->m_context.iCurrentCmd_addressSize);

        if (rsp < pCallContext->m_context.iCurrentCmd_opsize)
            return DI_ERROR;

        rsp -= pCallContext->m_context.iCurrentCmd_opsize;
        DI_CHECK(DianaProcessor_SetMemValue(pCallContext, 
                                            GET_REG_SS,
                                            rsp, 
                                            2,
                                            &src,
                                            0,
                                            reg_SS));
        SET_REG_RSP2(rsp, pCallContext->m_context.iCurrentCmd_addressSize);
        
        DI_PROC_END
    }

    usedSize = pCallContext->m_result.linkedOperands->usedSize;
    if (usedSize == 1)
    {
        usedSize = pCallContext->m_context.iCurrentCmd_opsize;

        // processor signextends this, proof:
        // 6A F5            push        0FFFFFFF5h
        DI_CHECK(DianaProcessor_SignExtend(&src, 1, usedSize));
    }
    if (pCallContext->m_context.iCurrentCmd_opsize == 8)
    {
        if (pCallContext->m_result.pInfo->m_flags & DI_FLAG_CMD_AMD64_SIGN_EXTENDS)
        {
            DI_CHECK(DianaProcessor_SignExtend( &src, src_size, 8 ));
        }
    }

    DI_CHECK(diana_internal_push( pCallContext, &src ));
    DI_PROC_END
}

int Diana_Call_pushf(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    // DEST 
    OPERAND_SIZE rsp = 0;

    pDianaContext;

    rsp = GET_REG_RSP;

    if (rsp < pCallContext->m_context.iCurrentCmd_opsize)
        return DI_ERROR;

    DI_CHECK(DianaProcessor_SetMemValue(pCallContext, 
                                        GET_REG_SS,
                                        rsp-pCallContext->m_context.iCurrentCmd_opsize, 
                                        pCallContext->m_context.iCurrentCmd_opsize,
                                        &pCallContext->m_flags.value,
                                        0,
                                        reg_SS));

    SET_REG_RSP(rsp-pCallContext->m_context.iCurrentCmd_opsize);
    DI_PROC_END
}

int Diana_Call_pop(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL(dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);

    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_popa32(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    OPERAND_SIZE dest = 0;

    pDianaContext;

    // EDI
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_EDI(dest);

    // ESI
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_ESI(dest);

    // EBP
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_EBP(dest);

    // ESP
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));

    // EBX
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_EBX(dest);

    // EDX
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_EDX(dest);

    // ECX
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_ECX(dest);

    // EAX
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_EAX(dest);
    return DI_SUCCESS;
}

int Diana_Call_popa16(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    OPERAND_SIZE dest = 0;

    pDianaContext;

    // DI
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_DI(dest);

    // SI
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_SI(dest);

    // BP
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_BP(dest);

    // SP
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));

    // BX
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_BX(dest);

    // DX
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_DX(dest);

    // CX
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_CX(dest);

    // AX
    DI_CHECK(diana_internal_pop(pCallContext,
                                &dest));
    SET_REG_AX(dest);
    return DI_SUCCESS;
}

int Diana_Call_popa(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    switch(pCallContext->m_context.iCurrentCmd_opsize)
    {
    case DIANA_MODE64:
        return DI_UNSUPPORTED_COMMAND;

    case DIANA_MODE32:
        return Diana_Call_popa32(pDianaContext,
                                 pCallContext);
    case DIANA_MODE16:
        return Diana_Call_popa16(pDianaContext,
                                 pCallContext);
    default:
        return DI_ERROR;
    }
}

int Diana_Call_popf(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    // In 64-bit mode, this instruction defaults to a 64-bit operand size; there is no prefix available to encode
    // a 32-bit operand size.
    OPERAND_SIZE dest = 0;
    if (pDianaContext->iAMD64Mode)
    {
        DI_CHECK(diana_internal_pop(pCallContext,
                                    &pCallContext->m_flags.value));
        DianaProcessor_SetResetDefaultFlags(pCallContext);
        return DI_SUCCESS;
    }

    switch(pCallContext->m_context.iCurrentCmd_opsize)
    {
    case DIANA_MODE32:
        DI_CHECK(diana_internal_pop(pCallContext,
                                    &dest));

        pCallContext->m_flags.impl.l.value = (DI_UINT32)dest;
        DianaProcessor_SetResetDefaultFlags(pCallContext);
        return DI_SUCCESS;

    case DIANA_MODE16:
        DI_CHECK(diana_internal_pop(pCallContext,
                                    &dest));

        pCallContext->m_flags.impl.l.impl.l.value = (DI_UINT16)dest;
        DianaProcessor_SetResetDefaultFlags(pCallContext);
        return DI_SUCCESS;

    default:
        return DI_ERROR;
    }
}

int Diana_Call_pusha32(struct _dianaContext * pDianaContext,
                       DianaProcessor * pCallContext)
{
    OPERAND_SIZE esp = GET_REG_ESP;
    OPERAND_SIZE dest = 0;

    pDianaContext;

    // EAX
    dest = GET_REG_EAX;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // ECX
    dest = GET_REG_ECX;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // EDX
    dest = GET_REG_EDX;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // EBX
    dest = GET_REG_EBX;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // ESP
    DI_CHECK(diana_internal_push(pCallContext,
                                 &esp));

    // EBP
    dest = GET_REG_EBP;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // ESI
    dest = GET_REG_ESI;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // EDI
    dest = GET_REG_EDI;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    return DI_SUCCESS;
}

int Diana_Call_pusha16(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    OPERAND_SIZE sp = GET_REG_SP;
    OPERAND_SIZE dest = 0;

    pDianaContext;

    // AX
    dest = GET_REG_AX;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // CX
    dest = GET_REG_CX;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // DX
    dest = GET_REG_DX;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // BX
    dest = GET_REG_BX;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // SP
    DI_CHECK(diana_internal_push(pCallContext,
                                 &sp));

    // BP
    dest = GET_REG_BP;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // SI
    dest = GET_REG_SI;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    // DI
    dest = GET_REG_DI;
    DI_CHECK(diana_internal_push(pCallContext,
                                 &dest));

    return DI_SUCCESS;
}

int Diana_Call_pusha(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    switch(pCallContext->m_context.iCurrentCmd_opsize)
    {
    case DIANA_MODE64:
        return DI_UNSUPPORTED_COMMAND;

    case DIANA_MODE32:
        return Diana_Call_pusha32(pDianaContext,
                                  pCallContext);
    case DIANA_MODE16:
        return Diana_Call_pusha16(pDianaContext,
                                  pCallContext);
    default:
        return DI_ERROR;
    }
}

int Diana_Call_pause(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    pDianaContext;
    pCallContext;

    DI_PROC_END
}
