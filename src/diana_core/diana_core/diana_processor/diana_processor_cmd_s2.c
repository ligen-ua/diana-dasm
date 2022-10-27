#include "diana_processor_cmd_s2.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"
  
int Diana_Call_sub(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //DEST := DEST - SRC;
    DI_DEF_LOCALS(src, dest);
        
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, DI_VAR_SIZE(dest));


    // update AF, OF, CF
    DI_START_UPDATE_COA_FLAGS(dest);

    dest -= src;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    DI_END_UPDATE_COA_FLAGS_SUB(dest, src);

    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));

    DI_PROC_END
}

int Diana_Call_shl(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //temp := COUNT;
    //WHILE (temp  0)
    //DO
    //   tmpcf := high-order bit of (r/m);
    //   r/m := r/m * 2 + (tmpcf);
    //   temp := temp - 1;
    //OD;
    //IF COUNT = 1
    //THEN
    //   IF high-order bit of r/m  CF
    //   THEN OF := 1;
    //   ELSE OF := 0;
    //   FI;
    //ELSE OF := undefined;
    //FI; 

    OPERAND_SIZE i = 0;
    OPERAND_SIZE signMask = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_SRC(src);
    DI_MEM_GET_DEST(dest);

    if( pDianaContext->iAMD64Mode == 1 && DI_REX_HAS_FLAG_W( pDianaContext->iRexPrefix ) )
    {
        src &= 0x3FULL;
    }
    else
    {
        src &= 0x1FULL;
    }
    if (!src)
    {
        DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
        DI_MEM_SET_DEST(dest);
        DI_PROC_END;
    }
    signMask = DianaProcessor_GetSignMask(dest_size);
    CLEAR_FLAG_OF;
    for(i = 0; i < src; ++i)
    {
        CLEAR_FLAG_CF;
        if (dest&signMask)
        {
            SET_FLAG_CF;
        }
        dest = (dest<<1);            
    }
    
    if (src == 1)
    {
        int tmp = 0;
        if (signMask & dest)
        {
            tmp = 1;
        }
        if (GET_FLAG_CF ^ tmp)
        {
            SET_FLAG_OF;
        }
    }

    DI_CHECK(Di_CheckZeroExtends(pCallContext,
                                 &dest,
                                 src_size,
                                 &dest_size));

    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}


int Diana_Call_sar(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //temp := COUNT;
    //WHILE (temp  0 )
    //DO
    //   tmpcf := low-order bit of (r/m);
    //   r/m := r/m / 2 + (tmpcf * 2^(width(r/m)));
    //   temp := temp - 1;
    //DO;
    //IF COUNT = 1
    //THEN
    //   IF (high-order bit of r/m)  (bit next to high-order bit of r/m)
    //   THEN OF := 1;
    //   ELSE OF := 0;
    //   FI;
    //ELSE OF := undefined; 
    OPERAND_SIZE i = 0;
    OPERAND_SIZE signMask = 0, prevSign = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_SRC(src);
    DI_MEM_GET_DEST(dest);

    if( pDianaContext->iAMD64Mode == 1 && DI_REX_HAS_FLAG_W( pDianaContext->iRexPrefix ) )
    {
        src &= 0x3FULL;
    }
    else
    {
        src &= 0x1FULL;
    }
    if (!src)
    {
        DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
        DI_MEM_SET_DEST(dest);
        DI_PROC_END;
    }

    signMask = DianaProcessor_GetSignMask(dest_size);
    prevSign = signMask & dest;

    CLEAR_FLAG_OF;
    for(i = 0; i < src; ++i)
    {
        CLEAR_FLAG_CF;
        if (dest&1)
        {
            SET_FLAG_CF;
        }
        dest = (dest>>1) | prevSign;
    }

    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}


int Diana_Call_sbb(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //DEST := DEST - SRC - CF;
    int cfValue = 0;
    DI_DEF_LOCALS(src, dest);

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, DI_VAR_SIZE(dest));
    cfValue = GET_FLAG_CF;

    // update AF, OF, CF
    DI_START_UPDATE_COA_FLAGS(dest);

    dest -= src + cfValue;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    // UNDOCUMENTED ***************
    if( !cfValue )
    {
        DI_END_UPDATE_COA_FLAGS_SUB(dest, src);
    }
    else
    {
        DI_END_UPDATE_COA_FLAGS_SUBCF(dest, src);
    }
    // UNDOCUMENTED ***************
    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}

int Diana_Call_stc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;

    SET_FLAG_CF;
    DI_PROC_END
}

int Diana_Call_std(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;

    SET_FLAG_DF;
    DI_PROC_END
}

int Diana_Call_sti(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;

    SET_FLAG_IF;
    DI_PROC_END
}

int Diana_Call_shld(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    int lastBit = 0;
    OPERAND_SIZE prevSign = 0;
    OPERAND_SIZE i = 0;
    OPERAND_SIZE signMask = 0;
    DI_DEF_LOCALS2(src1, src2, dest);
    oldDestValue;
    DI_MEM_GET_SRC2(src2);
    DI_MEM_GET_SRC(src1);
    DI_MEM_GET_DEST(dest);
    
    if( pDianaContext->iAMD64Mode == 1 && DI_REX_HAS_FLAG_W( pDianaContext->iRexPrefix ) )
    {
        src2 &= 0x3FULL;
    }
    else
    {
        src2 &= 0x1FULL;
    }
    if (!src2)
    {
        DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src1_size, &dest_size));
        DI_MEM_SET_DEST(dest);
        DI_PROC_END;
    }
    signMask = DianaProcessor_GetSignMask(dest_size);
    prevSign = signMask & dest;

    CLEAR_FLAG_OF;
    for(i = 0; i < src2; ++i)
    {
        // hack to act like x86 SHLD when count > 16
        if( dest_size == 2 && i > 0 && i % 16 == 0 )
        {
            // when count > 16 actually shifting op1:op2:op2 << count,
            // it is the same as shifting op2:op2 by count-16
            DI_MEM_GET_DEST(src1);
        }
        lastBit = 0;
        CLEAR_FLAG_CF;
        if (dest & signMask)
        {
            SET_FLAG_CF;
        }
        if (src1 & signMask)
        {
            lastBit = 1;
        }
        dest = (dest<<1) + lastBit;
        src1 = (src1<<1);
    }

    if (src2 == 1)
    {
        if ((signMask & dest) != prevSign)
        {
            SET_FLAG_OF;
        }
    }

    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src1_size, &dest_size));
    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}

int Diana_Call_shr(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //temp := COUNT;
    //WHILE (temp  0 )
    //DO
    //   tmpcf := low-order bit of (r/m);
    //   r/m := r/m / 2 + (tmpcf * 2^(width(r/m)));
    //   temp := temp - 1;
    //DO;
    //IF COUNT = 1
    //THEN
    //   IF (high-order bit of r/m)  (bit next to high-order bit of r/m)
    //   THEN OF := 1;
    //   ELSE OF := 0;
    //   FI;
    //ELSE OF := undefined; 
    OPERAND_SIZE i = 0;
    OPERAND_SIZE signMask = 0;
    OPERAND_SIZE original = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_SRC(src);
    DI_MEM_GET_DEST(dest);

    original = dest;
    if( pDianaContext->iAMD64Mode == 1 && DI_REX_HAS_FLAG_W( pDianaContext->iRexPrefix ) )
    {
        src &= 0x3FULL;
    }
    else
    {
        src &= 0x1FULL;
    }
    if (!src)
    {
        DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
        DI_MEM_SET_DEST(dest);
        DI_PROC_END;
    }

    CLEAR_FLAG_OF;
    for(i = 0; i < src; ++i)
    {
        CLEAR_FLAG_CF;
        if (dest&1)
        {
            SET_FLAG_CF;
        }
        dest = (dest>>1);
    }

    if (src == 1)
    {
        signMask = DianaProcessor_GetSignMask(dest_size);
        if (original & signMask)
        {
            SET_FLAG_OF;
        }
    }

    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}


int Diana_Call_shrd(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //temp := COUNT;
    //WHILE (temp  0 )
    //DO
    //   tmpcf := low-order bit of (r/m);
    //   r/m := r/m / 2 + (tmpcf * 2^(width(r/m)));
    //   temp := temp - 1;
    //DO;
    //IF COUNT = 1
    //THEN
    //   IF (high-order bit of r/m)  (bit next to high-order bit of r/m)
    //   THEN OF := 1;
    //   ELSE OF := 0;
    //   FI;
    //ELSE OF := undefined; 
    OPERAND_SIZE lastBit = 0;
    OPERAND_SIZE original = 0;
    OPERAND_SIZE i = 0;
    OPERAND_SIZE signMask = 0;
    DI_DEF_LOCALS2(src1, src2, dest);
    oldDestValue;
    DI_MEM_GET_SRC2(src2);
    DI_MEM_GET_SRC(src1);
    DI_MEM_GET_DEST(dest);

    if( pDianaContext->iAMD64Mode == 1 && DI_REX_HAS_FLAG_W( pDianaContext->iRexPrefix ) )
    {
        src2 &= 0x3FULL;
    }
    else
    {
        src2 &= 0x1FULL;
    }
    if (!src2)
    {
        DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src1_size, &dest_size));
        DI_MEM_SET_DEST(dest);
        DI_PROC_END;
    }

    original = dest;
    signMask = DianaProcessor_GetSignMask(dest_size);
    CLEAR_FLAG_OF;
    CLEAR_FLAG_CF;
    for(i = 0; i < src2; ++i)
    {
        lastBit = 0;
        if (dest & 1)
        {
            lastBit = signMask;
        }

        dest = (dest>>1);

        if (src1 & 1)
        {
            dest |= signMask;
        }
        src1 = (src1>>1)+lastBit;
    }

    if (lastBit)
    {
        SET_FLAG_CF;
    }

    if (src2 == 1)
    {
        if ((signMask & dest) != (signMask & original))
        {
            SET_FLAG_OF;
        }
    }

    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src1_size, &dest_size));
    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}

int Diana_Call_scas(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //SRC - DEST;
    DianaRmIndex destIndex;
    OPERAND_SIZE destRegAddress = 0;
    DI_DEF_LOCALS(src, dest);
    dest_size;
    
    src = GET_REG_RAX2(pCallContext->m_result.linkedOperands->usedSize);
    src_size = pCallContext->m_result.linkedOperands->usedSize;

    DI_CHECK(Di_PrepareSIDI_regs(pCallContext, 
                                 0, 
                                 &destIndex));
        
    DI_CHECK(Diana_ProcessorGetOperand_index_ex(pDianaContext,
                                                pCallContext,
                                                pCallContext->m_result.linkedOperands->usedSize,
                                                &destRegAddress,
                                                &dest,
                                                &destIndex));

    Di_UpdateSIDI(pCallContext,
                  0, 
                  &destRegAddress);

    // update AF, OF, CF
    DI_START_UPDATE_COA_FLAGS(src);

    src -= dest;
    
    DI_END_UPDATE_COA_FLAGS_SUB(src, dest);

    //--- update DI
    DianaProcessor_SetValue(pCallContext, 
                            destIndex.reg,
                            DianaProcessor_QueryReg(pCallContext,
                                                    destIndex.reg), 
                            destRegAddress);

    // update PSZ
    DianaProcessor_UpdatePSZ(pCallContext, 
                             src, 
                             pCallContext->m_result.linkedOperands->usedSize);

    DI_PROC_END
}


int Diana_Call_stos(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //SRC - DEST;
    DianaRmIndex destIndex;
    OPERAND_SIZE destRegAddress = 0, destSelectorValue = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    dest_size;
    
    DianaProcessor_CmdUsesNormalRep(pCallContext);

    src = GET_REG_RAX2(pCallContext->m_result.linkedOperands->usedSize);
    src_size = pCallContext->m_result.linkedOperands->usedSize;

    DI_CHECK(Di_PrepareSIDI_regs(pCallContext, 
                                 0, 
                                 &destIndex));
        
    DI_CHECK(Diana_ProcessorGetOperand_index_ex2(pDianaContext,
                                                pCallContext,
                                                pCallContext->m_result.linkedOperands->usedSize,
                                                &destRegAddress,
                                                &dest,
                                                &destIndex,
                                                &destSelectorValue));

    // set value
    DI_CHECK(DianaProcessor_SetMemValue(pCallContext, 
                                        destSelectorValue,
                                        destRegAddress, 
                                        pCallContext->m_result.linkedOperands->usedSize,
                                        &src,
                                        0,
                                        destIndex.seg_reg));

    //--- update DI
    Di_UpdateSIDI(pCallContext,
                  0, 
                  &destRegAddress);

    DianaProcessor_SetValue(pCallContext, 
                            destIndex.reg,
                            DianaProcessor_QueryReg(pCallContext,
                                                    destIndex.reg), 
                            destRegAddress);
    DI_PROC_END
}
