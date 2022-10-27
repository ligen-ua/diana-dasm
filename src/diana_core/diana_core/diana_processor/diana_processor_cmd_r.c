#include "diana_processor_cmd_r.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"


int Diana_Call_rcl(struct _dianaContext * pDianaContext,
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

    int lastBit = 0;
    OPERAND_SIZE i = 0;
    OPERAND_SIZE signMask = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_SRC(src);
    DI_MEM_GET_DEST(dest);

    switch( dest_size )
    {
    case 1:
        src =  (src & 0x1FULL) % 9;
        break;
    case 2:
        src =  (src & 0x1FULL) % 17;
        break;
    case 4:
        src &= 0x1FULL;
        break;
    case 8:
        src &= 0x3FULL;
        break;
    default:
        return DI_ERROR;
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
        lastBit = 0;
        if (GET_FLAG_CF)
        {
            lastBit = 1;
        }

        if (signMask & dest)
        {
            SET_FLAG_CF;
        }
        else
        {
            CLEAR_FLAG_CF;
        }

        dest = (dest<<1) + lastBit;
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
        
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}


int Diana_Call_rcr(struct _dianaContext * pDianaContext,
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
    OPERAND_SIZE i = 0;
    OPERAND_SIZE signMask = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_SRC(src);
    DI_MEM_GET_DEST(dest);

    switch( dest_size )
    {
    case 1:
        src =  (src & 0x1FULL) % 9;
        break;
    case 2:
        src =  (src & 0x1FULL) % 17;
        break;
    case 4:
        src &= 0x1FULL;
        break;
    case 8:
        src &= 0x3FULL;
        break;
    default:
        return DI_ERROR;
    }
    if (!src)
    {
        DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
        DI_MEM_SET_DEST(dest);
        DI_PROC_END;
    }
    signMask = DianaProcessor_GetSignMask(dest_size);

    CLEAR_FLAG_OF;
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
    for(i = 0; i < src; ++i)
    {
        lastBit = 0;
        if (GET_FLAG_CF)
            lastBit = signMask;


        if (dest & 1)
        {
            SET_FLAG_CF;
        }
        else
        {
            CLEAR_FLAG_CF;
        }

        dest = (dest>>1) | lastBit;
    }
        
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}


int Diana_Call_rol(struct _dianaContext * pDianaContext,
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

    int lastBit = 0;
    OPERAND_SIZE i = 0;
    OPERAND_SIZE signMask = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_SRC(src);
    DI_MEM_GET_DEST(dest);

    if( pDianaContext->iAMD64Mode == 1 && DI_REX_HAS_FLAG_W( pDianaContext->iRexPrefix ) ) {
        src &= 0x3FULL;
    } else {
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
        lastBit = 0;
        if (signMask & dest)
        {
            lastBit = 1;
        }

        dest = (dest<<1) + lastBit;
    }

    CLEAR_FLAG_CF;
    if (dest&1)
    {
        SET_FLAG_CF;
    }

    if (src == 1)
    {
        int tmp = 0;
        if (signMask & dest)
        {
            tmp = 1;
        }
        if ((dest&1) ^ tmp)
        {
            SET_FLAG_OF;
        }
    }
        
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}


int Diana_Call_ror(struct _dianaContext * pDianaContext,
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
    OPERAND_SIZE i = 0;
    OPERAND_SIZE signMask = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_SRC(src);
    DI_MEM_GET_DEST(dest);

    if( pDianaContext->iAMD64Mode == 1 && DI_REX_HAS_FLAG_W( pDianaContext->iRexPrefix ) ) {
        src &= 0x3FULL;
    } else {
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
        lastBit = 0;
        if (dest & 1)
        {
            lastBit = signMask;
        }

        dest = (dest>>1) | lastBit;
    }

    CLEAR_FLAG_CF;
    if (dest&signMask)
    {
        SET_FLAG_CF;
    }

    if (src == 1)
    {
        int tmp = 0, tmp2 = 0;
        if (dest & signMask)
        {
            tmp = 1;
        }
        if (dest & (signMask >> 1))
        {
            tmp2 = 1;
        }
        if (tmp ^ tmp2)
        {
            SET_FLAG_OF;
        }
    }       
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
    DI_MEM_SET_DEST(dest);

    DI_PROC_END
}


int Diana_Call_rdrand(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL(dest);
    &oldDestValue;
    dest = DianaProcessRandom_Generate(&pCallContext->m_random);
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}
int Diana_Call_rdseed(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL(dest);
    &oldDestValue;
    dest = DianaProcessRandom_Generate(&pCallContext->m_random);
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}
