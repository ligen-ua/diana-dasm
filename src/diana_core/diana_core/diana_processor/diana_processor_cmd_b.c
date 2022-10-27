#include "diana_processor_cmd_b.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"


int Diana_Call_bsf(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //IF r/m = 0
    //THEN
    //   ZF := 1;
    //   register := UNDEFINED;
    //ELSE
    //   temp := 0;
    //   ZF := 0;
    //   WHILE BIT[r/m, temp = 0]
    //   DO
    //      temp := temp + 1;
    //      register := temp;
    //   OD;
    //FI; 

    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    CLEAR_FLAG_ZF;
    if (!src)
    {
        SET_FLAG_ZF;
    }
    else
    {
        int i = 0;
        OPERAND_SIZE mask = 1;
        
        for(i = 0; i<64 && !(mask & src); ++i, mask<<=1)
        {
        }
        dest = i;
        DI_MEM_SET_DEST(dest);
    }
    DI_PROC_END;
}

int Diana_Call_bsr(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //IF r/m = 0
    //THEN
    //   ZF := 1;
    //   register := UNDEFINED;
    //ELSE
    //   temp := OperandSize - 1;
    //   ZF := 0;
    //   WHILE BIT[r/m, temp] = 0
    //   DO
    //      temp := temp - 1;
    //      register := temp;
    //   OD;
    //FI; 
    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    CLEAR_FLAG_ZF;
    if (!src)
    {
        SET_FLAG_ZF;
    }
    else
    {
        int i = 0;
        OPERAND_SIZE mask = 0x8000000000000000ULL;
        
        for(i = 63; i>=0 && !(mask & src); --i, mask>>=1)
        {
        }
        dest = i;
        DI_MEM_SET_DEST(dest);
    }
    DI_PROC_END;
}

int Diana_Call_bt(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    // CF := BIT[LeftSRC, RightSRC]; 
    OPERAND_SIZE temp = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    CLEAR_FLAG_CF;

    switch( pDianaContext->iCurrentCmd_opsize ) {
    case 2:
        src %= 16;
        break;
    case 4:
        src %= 32;
        break;
    case 8:
        src %= 64;
        break;
    case 1:
    default:
        return DI_ERROR;
    }

    temp = DianaProcessor_GetSignMaskSpecifyBit(src);

    if (dest & temp)
    {
        SET_FLAG_CF;
    }
    DI_PROC_END;
}

int Diana_Call_btc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //CF := BIT[LeftSRC, RightSRC];
    //BIT[LeftSRC, RightSRC] := NOT BIT[LeftSRC, RightSRC]; 

    OPERAND_SIZE temp = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    CLEAR_FLAG_CF;

    switch( pDianaContext->iCurrentCmd_opsize ) 
    {
    case 2:
        src %= 16;
        break;
    case 4:
        src %= 32;
        break;
    case 8:
        src %= 64;
        break;
    case 1:
    default:
        return DI_ERROR;
    }

    temp = DianaProcessor_GetSignMaskSpecifyBit(src);

    if (dest & temp)
    {
        SET_FLAG_CF;
        dest = dest & ~temp;
    }
    else
    {
        dest |= temp;
    }
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
    DI_MEM_SET_DEST(dest);
    DI_PROC_END;
}

int Diana_Call_btr(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //CF := BIT[LeftSRC, RightSRC];
    //BIT[LeftSRC, RightSRC] := 0; 

    OPERAND_SIZE temp = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    CLEAR_FLAG_CF;

    switch( pDianaContext->iCurrentCmd_opsize ) {
    case 2:
        src %= 16;
        break;
    case 4:
        src %= 32;
        break;
    case 8:
        src %= 64;
        break;
    case 1:
    default:
        return DI_ERROR;
    }

    temp = DianaProcessor_GetSignMaskSpecifyBit(src);

    if (dest & temp)
    {
        SET_FLAG_CF;
        dest = dest & ~temp;
    }
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
    DI_MEM_SET_DEST(dest);
    DI_PROC_END;
}

int Diana_Call_bts(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //CF := BIT[LeftSRC, RightSRC];
    //BIT[LeftSRC, RightSRC] := 0; 

    OPERAND_SIZE temp = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    CLEAR_FLAG_CF;

    switch( pDianaContext->iCurrentCmd_opsize ) {
    case 2:
        src %= 16;
        break;
    case 4:
        src %= 32;
        break;
    case 8:
        src %= 64;
        break;
    case 1:
    default:
        return DI_ERROR;
    }

    temp = DianaProcessor_GetSignMaskSpecifyBit(src);

    if (dest & temp)
    {
        SET_FLAG_CF;
    }
    else
    {
        dest |= temp;
    }
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));
    DI_MEM_SET_DEST(dest);
    DI_PROC_END;
}

int Diana_Call_bswap(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    OPERAND_SIZE b0, b1, b2, b3, b4, b5, b6, b7;
    DI_DEF_LOCAL(dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);

    switch( dest_size )
    {
    // UNDOCUMENTED ***************
    case 2:
        dest = 0;
        break;
    // ****************************
    case 4:
        b0 = dest & 0xFF; dest>>= 8;
        b1 = dest & 0xFF; dest>>= 8;
        b2 = dest & 0xFF; dest>>= 8;
        dest |= (b0<<24) | (b1<<16) | (b2<<8);
        break;
    case 8:
        b0 = dest & 0xFF; dest>>= 8;
        b1 = dest & 0xFF; dest>>= 8;
        b2 = dest & 0xFF; dest>>= 8;
        b3 = dest & 0xFF; dest>>= 8;
        b4 = dest & 0xFF; dest>>= 8;
        b5 = dest & 0xFF; dest>>= 8;
        b6 = dest & 0xFF; dest>>= 8;
        b7 = dest;
        dest |= (b0<<56) | (b1<<48) | (b2<<40) | (b3<<32) | (b4<<24) | (b5<<16) | (b6<<8);
        break;
    default:
        return DI_ERROR;
    }
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, dest_size, &dest_size));
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}
