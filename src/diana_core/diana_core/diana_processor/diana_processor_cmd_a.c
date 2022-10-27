#include "diana_processor_cmd_a.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"
#include "diana_processor_core_impl.h"

int Diana_Call_aaa(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;

    if (((GET_REG_AL & 0x0F) > 9) || GET_FLAG_AF) 
    {
        SET_REG_AX(GET_REG_AX + 6);
        SET_REG_AH(GET_REG_AH + 1);
        SET_FLAG_AF;
        SET_FLAG_CF;
    } 
    else 
    {
        CLEAR_FLAG_AF;
        CLEAR_FLAG_CF;
    }
    SET_REG_AL(GET_REG_AL & 0x0F);
    // UNDOCUMENTED ***************
    DianaProcessor_UpdatePSZ( pCallContext, GET_REG_AX, 2 );
    CLEAR_FLAG_OF;
    CLEAR_FLAG_SF;
    // ****************************
    DI_PROC_END;
}

int Diana_Call_aad(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    DianaRegisterValue16_type ax;
    DI_DEF_LOCAL(src);
    oldDestValue;
    DI_MEM_GET_DEST(src);

    //AL := AH * 10 + AL;
    //AH := 0;
    ax.value = ( DI_UINT16 )GET_REG_AX;

    ax.impl.l = ax.impl.h * ( DI_CHAR )src + ax.impl.l;
    ax.impl.h = 0;

    DI_UPDATE_FLAGS_PSZ(SET_REG_AX(ax.value));
    DI_PROC_END;
}

int Diana_Call_aam(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    DianaRegisterValue16_type ax;
    DI_DEF_LOCAL(src);
    oldDestValue;
    DI_MEM_GET_DEST(src);

    //AH := AL / 10;
    //AL := AL MOD 10;
    ax.value = ( DI_UINT16 )GET_REG_AX;

    if( src == 0 )
        return DI_DIVISION_BY_ZERO;
    ax.impl.h = ax.impl.l / ( DI_CHAR )src;
    ax.impl.l = ax.impl.l % ( DI_CHAR )src;

    DI_UPDATE_FLAGS_PSZ(SET_REG_AX(ax.value));
    DI_PROC_END;
}

int Diana_Call_aas(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;

   if ((GET_REG_AL & 0xF) > 9 || GET_FLAG_AF) 
   {
        SET_REG_AX(GET_REG_AX - 6);
        SET_REG_AH(GET_REG_AH - 1);
  
        SET_FLAG_AF;
        SET_FLAG_CF;
   } 
   else 
   {
        CLEAR_FLAG_AF;
        CLEAR_FLAG_CF;
   }
   SET_REG_AL(GET_REG_AL & 0x0F);
   // UNDOCUMENTED ***************
   DianaProcessor_UpdatePSZ( pCallContext, GET_REG_AX, 2 );
   CLEAR_FLAG_OF;
   CLEAR_FLAG_SF;
   // ****************************
   DI_PROC_END;
}

    
int Diana_Call_adc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{    
    //DEST := DEST + SRC + CF;
    int cfValue = 0;
    DI_DEF_LOCALS(src, dest);

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, DI_VAR_SIZE(dest));

    cfValue = GET_FLAG_CF;
    DI_START_UPDATE_COA_FLAGS(dest);

    dest += src + cfValue;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    // UNDOCUMENTED ***************
    if( !cfValue )
    {
        DI_END_UPDATE_COA_FLAGS_ADD(dest, src);
    }
    else
    {
        DI_END_UPDATE_COA_FLAGS_ADDCF(dest, src);
    }
    // ****************************

    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}

int Diana_Call_add(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //DEST := DEST + SRC;
    DI_DEF_LOCALS(src, dest);
        
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, DI_VAR_SIZE(dest));
    // update AF, OF, CF
    DI_START_UPDATE_COA_FLAGS(dest);

    dest += src;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    DI_END_UPDATE_COA_FLAGS_ADD(dest, src);

    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}

int Diana_Call_and(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //DEST := DEST AND SRC;
    //CF := 0;
    //OF := 0;

    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, DI_VAR_SIZE(dest));

    dest = dest & src;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    CLEAR_FLAG_CF;
    CLEAR_FLAG_OF;

    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    // UNDOCUMENTED ***************
    CLEAR_FLAG_AF;
    // ****************************
    DI_PROC_END
}
