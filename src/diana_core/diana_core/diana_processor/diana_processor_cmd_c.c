#include "diana_processor_cmd_c.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

int Diana_Call_call(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    OPERAND_SIZE newSegment = 0;
    int bNewSegmentIs = 0;
    OPERAND_SIZE rsp = 0, retRIP = 0;

    DI_DEF_LOCAL(newRIP);
    oldDestValue;

    rsp = GET_REG_RSP;

    if (pCallContext->m_result.iLinkedOpCount != 1)
    {
        return DI_ERROR;
    }

    // calculate absolute address
    switch(pCallContext->m_result.linkedOperands[0].type)
    {
    case diana_rel:
        newRIP = GET_REG_RIP + 
                 pCallContext->m_result.linkedOperands[0].value.rel.m_value + 
                 pCallContext->m_result.iFullCmdSize;
        break;

    case diana_index:
    case diana_register:
        DI_MEM_GET_DEST(newRIP);
        break;

    case diana_memory:
        {
            OPERAND_SIZE memSelector = GET_REG_DS, memAddress=0;
            DianaRmIndex  * pIndex = &pCallContext->m_result.linkedOperands[0].value.memory.m_index;
            DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                              pCallContext,
                                              pIndex,
                                              &memSelector,
                                              &memAddress));

            // load new IP and CS
            DI_CHECK(DianaProcessor_GetMemValue(pCallContext, 
                                                memSelector,
                                                memAddress,
                                                sizeof(DI_UINT16), 
                                                &newSegment,
                                                0,
                                                pIndex->seg_reg));

            DI_CHECK(DianaProcessor_GetMemValue(pCallContext, 
                                                memSelector,
                                                memAddress + sizeof(DI_UINT16),
                                                pCallContext->m_result.linkedOperands[0].usedAddressSize,
                                                &newRIP,
                                                0,
                                                pIndex->seg_reg));

            bNewSegmentIs = 1;
            break;
        }
        break;
    case diana_call_ptr:
        newSegment = pCallContext->m_result.linkedOperands[0].value.ptr.m_segment;
        bNewSegmentIs = 1;
        newRIP = pCallContext->m_result.linkedOperands[0].value.ptr.m_address;
        break;
    default:
        return DI_ERROR;
    }

    // if cs specified, push it
    if (bNewSegmentIs && pDianaContext->iMainMode_opsize == DIANA_MODE16)
    {
        OPERAND_SIZE oldCS = GET_REG_CS;
        
        DI_CHECK(DianaProcessor_SetMemValue(pCallContext, 
                                            GET_REG_SS,
                                            rsp-pCallContext->m_context.iCurrentCmd_opsize, 
                                            pCallContext->m_context.iCurrentCmd_opsize,
                                            &oldCS,
                                            0,
                                            reg_SS));
        rsp -= pCallContext->m_context.iCurrentCmd_opsize;
    }

    // set ret IP
    retRIP = GET_REG_RIP + pCallContext->m_result.iFullCmdSize;

    DI_CHECK(DianaProcessor_SetMemValue(pCallContext, 
                                        GET_REG_SS,
                                        rsp-pCallContext->m_context.iCurrentCmd_opsize, 
                                        pCallContext->m_context.iCurrentCmd_opsize,
                                        &retRIP,
                                        0,
                                        reg_SS));
    rsp -= pCallContext->m_context.iCurrentCmd_opsize;

    pCallContext->m_lastCallRIP = newRIP;
    pCallContext->m_lastCallRSP = rsp;
    SET_REG_RSP(rsp);
    SET_REG_RIP(newRIP);

    if (bNewSegmentIs)
    {
        SET_REG_CS(newSegment);
    }

    DI_PROC_END;
}

int Diana_Call_cbw(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    OPERAND_SIZE reg = 0;

    switch(pDianaContext->iCurrentCmd_opsize)
    {
    case DIANA_MODE64:
        reg = GET_REG_EAX;
        DI_CHECK(DianaProcessor_SignExtend(&reg,
                                           4,
                                           8));
        SET_REG_RAX(reg);
        break;

    case DIANA_MODE32:
        reg = GET_REG_AX;
        DI_CHECK(DianaProcessor_SignExtend(&reg,
                                           2,
                                           4));
        // zero-extended!
        SET_REG_RAX(reg);
        break;

    case DIANA_MODE16:
        reg = GET_REG_AL;
        DI_CHECK(DianaProcessor_SignExtend(&reg,
                                           1,
                                           2));
        SET_REG_AX(reg);
        break;
    default:
        return DI_ERROR;
    }
    DI_PROC_END;
}

int Diana_Call_clc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;

    CLEAR_FLAG_CF;
    DI_PROC_END;
}

int Diana_Call_cld(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;

    CLEAR_FLAG_DF;
    DI_PROC_END;
}

int Diana_Call_cli(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;

    CLEAR_FLAG_IF;
    DI_PROC_END;
}

int Diana_Call_cmc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;

    if (GET_FLAG_CF)
    {
        CLEAR_FLAG_CF;
    }
    else
    {
        SET_FLAG_CF;
    }
    DI_PROC_END;
}


int Diana_Call_cmova(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if ((GET_FLAG_CF == 0) && (GET_FLAG_ZF == 0))
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovae(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_CF == 0)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovb(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_CF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovbe(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_CF || GET_FLAG_ZF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}


int Diana_Call_cmove(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_ZF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovg(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if ((GET_FLAG_SF == GET_FLAG_OF) && (GET_FLAG_ZF == 0))
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovge(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_SF == GET_FLAG_OF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovl(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_SF != GET_FLAG_OF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovle(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_ZF || (GET_FLAG_SF != GET_FLAG_OF))
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovne(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (!GET_FLAG_ZF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovno(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (!GET_FLAG_OF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovnp(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (!GET_FLAG_PF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovns(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (!GET_FLAG_SF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovo(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_OF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_PF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmovs(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := SRC
    DI_DEF_LOCAL(src);
    oldDestValue;

    DI_MEM_GET_SRC(src);

    if (GET_FLAG_SF)
    {
        DI_MEM_SET_DEST(src);
    }
    DI_PROC_END
}

int Diana_Call_cmp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //DEST - SRC;
    DI_DEF_LOCALS(src, dest);
        
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, DI_VAR_SIZE(dest));

    // update AF, OF, CF
    DI_START_UPDATE_COA_FLAGS(dest);

    dest -= src;

    DI_END_UPDATE_COA_FLAGS_SUB(dest, src);

    DianaProcessor_UpdatePSZ(pCallContext, 
                             dest, 
                             pCallContext->m_result.linkedOperands->usedSize);
    DI_PROC_END
}

int Diana_Call_cmps(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //SRC - DEST;
    DianaRmIndex srcIndex, destIndex;
    OPERAND_SIZE srcRegAddress = 0, destRegAddress = 0;
    DI_DEF_LOCALS(src, dest);
    dest_size;
    
    DI_CHECK(Di_PrepareSIDI_regs(pCallContext, 
                                 &srcIndex, 
                                 &destIndex));
        
    DI_CHECK(Diana_ProcessorGetOperand_index_ex(pDianaContext,
                                                pCallContext,
                                                pCallContext->m_result.linkedOperands->usedSize,
                                                &srcRegAddress,
                                                &src,
                                                &srcIndex));

    DI_CHECK(Diana_ProcessorGetOperand_index_ex(pDianaContext,
                                                pCallContext,
                                                pCallContext->m_result.linkedOperands->usedSize,
                                                &destRegAddress,
                                                &dest,
                                                &destIndex));

    Di_UpdateSIDI(pCallContext,
                  &srcRegAddress, 
                  &destRegAddress);

    // update AF, OF, CF
    DI_START_UPDATE_COA_FLAGS(src);

    src -= dest;

    src_size = pCallContext->m_result.linkedOperands->usedSize;
    DI_END_UPDATE_COA_FLAGS_SUB(src, dest);

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

    DianaProcessor_UpdatePSZ(pCallContext, 
                             src, 
                             pCallContext->m_result.linkedOperands->usedSize);

    DI_PROC_END
}


int Diana_Call_cmpxchg(struct _dianaContext * pDianaContext,
                       DianaProcessor * pCallContext)
{
    DianaUnifiedRegister usedReg = 0;
    int usedRegValue_size = 0;

    OPERAND_SIZE usedRegValue = 0, usedRegValue_saved = 0;
    DI_DEF_LOCALS(src, dest);

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    DI_CHECK(DianaProcessor_QueryRaxRegister(src_size, &usedReg));
    usedRegValue = DianaProcessor_GetValue(pCallContext,
                                           DianaProcessor_QueryReg(pCallContext, usedReg));
    usedRegValue_saved = usedRegValue;
    usedRegValue_size = dest_size;

    // update flags
    DI_START_UPDATE_COA_FLAGS(usedRegValue);
    usedRegValue -= dest;
    DI_END_UPDATE_COA_FLAGS_SUB(usedRegValue,dest);
    DianaProcessor_UpdatePSZ(pCallContext,
                             usedRegValue,
                             dest_size);

    // update end
    if (usedRegValue_saved == dest)
    {
        dest = src;
        DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
        DI_MEM_SET_DEST(dest);
        SET_FLAG_ZF;
    }
    else
    {
        DianaProcessor_SetValue(pCallContext,
                                usedReg,
                                DianaProcessor_QueryReg(pCallContext, usedReg),
                                dest);
        CLEAR_FLAG_ZF;
    }
    DI_PROC_END
}

int Diana_Call_cwd(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    // DX:AX := sign-extend of AX 
    DianaUnifiedRegister usedReg = 0, usedDestReg = 0;
    OPERAND_SIZE usedRegValue = 0;
  
    DI_CHECK(DianaProcessor_QueryRaxRegister(pCallContext->m_context.iCurrentCmd_opsize, 
                                             &usedReg));

    
    DI_CHECK(DianaProcessor_QueryRdxRegister(pCallContext->m_context.iCurrentCmd_opsize, 
                                             &usedDestReg));

    usedRegValue = 
        DianaProcessor_GetValue(pCallContext, 
                                DianaProcessor_QueryReg(pCallContext, usedReg));

    
    if (usedRegValue & DianaProcessor_GetSignMask(pCallContext->m_context.iCurrentCmd_opsize))
    {
        // dirty hack
        if( pDianaContext->iAMD64Mode && pCallContext->m_context.iCurrentCmd_opsize == 4 ) {
            SET_REG_RDX( 0x00000000FFFFFFFFULL );
     } 
     else 
     {
        DianaProcessor_SetValue(pCallContext,
                                usedDestReg,
                                DianaProcessor_QueryReg(pCallContext, usedDestReg),
                                0xFFFFFFFFFFFFFFFFULL);
    }

    }
    else
    {
        DianaProcessor_SetValue(pCallContext,
                                usedDestReg,
                                DianaProcessor_QueryReg(pCallContext, usedDestReg),
                                0);
    }
    DI_PROC_END
}

static
int Diana_Call_cmpxchg8b_impl(struct _dianaContext * pDianaContext,
                              DianaProcessor * pCallContext)
{
    DianaRegisterValue_type mem;
    OPERAND_SIZE selector = 0, address = 0;
 
    mem.value = 0;
    if (pCallContext->m_result.iLinkedOpCount != 1 ||
        pCallContext->m_result.linkedOperands[0].type != diana_index)
    {
        return DI_ERROR;
    }
    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                      pCallContext,
                                      &pCallContext->m_result.linkedOperands[0].value.rmIndex,
                                      &selector,
                                      &address));

    DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                        selector,
                                        address,
                                        sizeof(mem), 
                                        &mem.value,
                                        0,
                                        pCallContext->m_result.linkedOperands[0].value.rmIndex.seg_reg));

    if (mem.impl.l.value != GET_REG_EAX || mem.impl.h != GET_REG_EDX)
    {
        CLEAR_FLAG_ZF;
        SET_REG_EDX(mem.impl.h);
        SET_REG_EAX(mem.impl.l.value);
        DI_PROC_END
    }

    // equal
    SET_FLAG_ZF;
    mem.impl.l.value = (DI_UINT32)GET_REG_EBX;
    mem.impl.h = (DI_UINT32)GET_REG_ECX;

    DI_CHECK(DianaProcessor_SetMemValue(pCallContext,
                                        selector,
                                        address,
                                        sizeof(mem), 
                                        &mem.value,
                                        0,
                                        pCallContext->m_result.linkedOperands[0].value.rmIndex.seg_reg));
    DI_PROC_END
}


static
int Diana_Call_cmpxchg16b_impl(struct _dianaContext * pDianaContext,
                               DianaProcessor * pCallContext)
{
    OPERAND_SIZE low = 0, high= 0;
    OPERAND_SIZE selector = 0, address = 0;

    if (pCallContext->m_result.iLinkedOpCount != 1 ||
        pCallContext->m_result.linkedOperands[0].type != diana_index)
    {
        return DI_ERROR;
    }
    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                      pCallContext,
                                      &pCallContext->m_result.linkedOperands[0].value.rmIndex,
                                      &selector,
                                      &address));

    DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                        selector,
                                        address,
                                        sizeof(low), 
                                        &low,
                                        0,
                                        pCallContext->m_result.linkedOperands[0].value.rmIndex.seg_reg));

    DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                        selector,
                                        address+8,
                                        sizeof(high), 
                                        &high,
                                        0,
                                        pCallContext->m_result.linkedOperands[0].value.rmIndex.seg_reg));

    if (low != GET_REG_RAX || high != GET_REG_RDX)
    {
        CLEAR_FLAG_ZF;
        SET_REG_RDX(high);
        SET_REG_RAX(low);
        DI_PROC_END
    }

    // equal
    SET_FLAG_ZF;
    low = GET_REG_RBX;
    high = GET_REG_RCX;

    DI_CHECK(DianaProcessor_SetMemValue(pCallContext,
                                        selector,
                                        address,
                                        sizeof(low), 
                                        &low,
                                        0,
                                        pCallContext->m_result.linkedOperands[0].value.rmIndex.seg_reg));
    DI_CHECK(DianaProcessor_SetMemValue(pCallContext,
                                        selector,
                                        address+8,
                                        sizeof(high), 
                                        &high,
                                        0,
                                        pCallContext->m_result.linkedOperands[0].value.rmIndex.seg_reg));
    DI_PROC_END
}

int Diana_Call_cmpxchg16b(struct _dianaContext * pDianaContext,
                          DianaProcessor * pCallContext)
{
    if (pDianaContext->iCurrentCmd_opsize != DIANA_MODE64)
    {
        return Diana_Call_cmpxchg8b_impl(pDianaContext,
                                         pCallContext);
    }
    // 16b
    return Diana_Call_cmpxchg16b_impl(pDianaContext,
                                         pCallContext);

}

int Diana_Call_cpuid(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{  
    OPERAND_SIZE eaxValue = GET_REG_EAX;

    &pDianaContext;
    SET_REG_EAX(0);
    SET_REG_EBX(0);
    SET_REG_ECX(0);
    SET_REG_EDX(0);

    switch(eaxValue) 
    {
    case 0:
        SET_REG_EAX(0x00000001);
        SET_REG_EBX(0x756E6547);
        SET_REG_ECX(0x6C65746E);
        SET_REG_EDX(0x49656E69);
        break;
    case 1:
        SET_REG_EAX(0x000306A9);
        SET_REG_EBX(0x03100800);
        SET_REG_ECX(0x7FBAE3BF);
        SET_REG_EDX(0xBFEBFBFF);
        break;
    case 0x80000000:
        SET_REG_EAX(0x80000001);
        break;
    case 0x80000001:
        SET_REG_EDX(0x28100000);
        break;
    }
    DI_PROC_END
}