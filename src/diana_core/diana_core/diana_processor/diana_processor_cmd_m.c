#include "diana_processor_cmd_m.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

int Diana_Call_mov(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    
    DI_MEM_GET_DEST_SIZE(dest)
    DI_MEM_GET_SRC(src);

    dest = src;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_movs(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    // SRC := DEST;
    OPERAND_SIZE selector = GET_REG_DS;
    DianaRmIndex srcIndex, destIndex;
    OPERAND_SIZE srcRegAddress = 0, destRegAddress = 0;
    DI_DEF_LOCALS(src, dest);
    dest;
    oldDestValue;
    dest_size;
    
    DianaProcessor_CmdUsesNormalRep(pCallContext);

    DI_CHECK(Di_PrepareSIDI_regs(pCallContext, &srcIndex, &destIndex));
        
    DI_CHECK(Diana_ProcessorGetOperand_index_ex(pDianaContext,
                                                pCallContext,
                                                pCallContext->m_result.linkedOperands->usedSize,
                                                &srcRegAddress,
                                                &src,
                                                &srcIndex));

    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                      pCallContext,
                                      &destIndex,
                                      &selector,
                                      &destRegAddress));

    // move src to dest
    DI_CHECK(DianaProcessor_SetMemValue(pCallContext, 
                                        selector,
                                        destRegAddress, 
                                        pCallContext->m_result.linkedOperands->usedSize,
                                        &src,
                                        0,
                                        destIndex.seg_reg));

    // shift registers
    Di_UpdateSIDI(pCallContext,
                  &srcRegAddress, 
                  &destRegAddress);

    src_size = pCallContext->m_result.linkedOperands->usedSize;

    //---
    DianaProcessor_SetValue(pCallContext, 
                            srcIndex.reg,
                            DianaProcessor_QueryReg(pCallContext,
                                                    srcIndex.reg), 
                            srcRegAddress);

    DianaProcessor_SetValue(pCallContext, 
                            destIndex.reg,
                            DianaProcessor_QueryReg(pCallContext,
                                                    destIndex.reg), 
                            destRegAddress);

    DI_PROC_END
}

int Diana_Call_movsx(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    
    DI_MEM_GET_DEST(dest)
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, dest_size);

    dest = src;

    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_movzx(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    
    DI_MEM_GET_DEST(dest)
    DI_MEM_GET_SRC(src);

    dest = src;

    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// MUL
static
int Diana_Call_mul8(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext,
                    OPERAND_SIZE * pArgument)
{
    DianaRegisterValue16_type result;
    DI_UINT16 arg1 = (DI_UINT16)GET_REG_AL;
    DI_UINT16 arg2 = (DI_UINT16)*pArgument;

    pDianaContext;

    result.value = arg1 * arg2;

    if (result.impl.h)
    {
       SET_FLAG_CF;
       SET_FLAG_OF;
    }

    SET_REG_AX(result.value);
    return DI_SUCCESS;
}

static
int Diana_Call_mul16(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext,
                     OPERAND_SIZE * pArgument)
{
    DianaRegisterValue32_type result;
    DI_UINT32 arg1 = (DI_UINT32)GET_REG_AX;
    DI_UINT32 arg2 = (DI_UINT32)*pArgument;

    pDianaContext;

    result.value = arg1 * arg2;

    if (result.impl.h)
    {
       SET_FLAG_CF;
       SET_FLAG_OF;
    }

    SET_REG_AX(result.impl.l.value);
    SET_REG_DX(result.impl.h);
    return DI_SUCCESS;
}

static
int Diana_Call_mul32(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext,
                     OPERAND_SIZE * pArgument)
{
    DianaRegisterValue_type result;
    DI_UINT64 arg1 = (DI_UINT64)GET_REG_EAX;
    DI_UINT64 arg2 = (DI_UINT64)*pArgument;

    pDianaContext;

    result.value = arg1 * arg2;

    if (result.impl.h) 
    {
       SET_FLAG_CF;
       SET_FLAG_OF;
    }

    SET_REG_EAX(result.impl.l.value);
    SET_REG_EDX(result.impl.h);
    return DI_SUCCESS;
}

static
int Diana_Call_mul64(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext,
                     OPERAND_SIZE * pArgument)
{
    DI_UINT64 r0, r1;
    DI_UINT64 argument = *pArgument;
    DI_UINT64 rax = GET_REG_RAX;

    pDianaContext;

    mul64( &r0, &r1, rax, argument );

    if( r1 )
    {
        SET_FLAG_CF;
        SET_FLAG_OF;
    }

    SET_REG_RAX( r0 );
    SET_REG_RDX( r1 );
    return DI_SUCCESS;
}

int Diana_Call_mul(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL(argument);
    oldDestValue;
    DI_MEM_GET_DEST(argument);

    CLEAR_FLAG_CF;
    CLEAR_FLAG_OF;

    switch(pCallContext->m_result.linkedOperands->usedSize)
    {
    case DIANA_MODE64:
        DI_CHECK(Diana_Call_mul64(pDianaContext, pCallContext, &argument));
        break;

    case DIANA_MODE32:
        DI_CHECK(Diana_Call_mul32(pDianaContext, pCallContext, &argument));
        break;

    case DIANA_MODE16:
        DI_CHECK(Diana_Call_mul16(pDianaContext, pCallContext, &argument));
        break;

    case 1:
        DI_CHECK(Diana_Call_mul8(pDianaContext, pCallContext, &argument));
        break;

    default:
        return DI_ERROR;
    }

    DI_PROC_END
}

int Diana_Call_movsxd(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    
    DI_MEM_GET_DEST(dest)
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, dest_size);

    if (pCallContext->m_context.iCurrentCmd_opsize == DIANA_MODE16)
    {
        dest &= ~0xFFFFULL;
        dest |= src & 0xFFFFULL;
    }
    else
    {
        dest = src;
    }

    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}
