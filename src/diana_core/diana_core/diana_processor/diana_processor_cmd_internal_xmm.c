#include "diana_processor_cmd_internal_xmm.h"


int diana_mov_xmm(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    // DEST := SRC
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET_SRC_XMM(src)

    DIANA_MEMCPY(dest.u8, src.u8, dest_size);

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END;
}

int diana_mov_xmm8(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    // DEST := SRC
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    dest_size = 8;
    DI_MEM_GET_DEST_XMM(dest)
    src_size = 8;
    DI_MEM_GET_SRC_XMM(src)
    
    DIANA_MEMCPY(dest.u8, src.u8, dest_size);

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END;
}
