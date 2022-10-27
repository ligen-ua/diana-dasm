#include "diana_processor_cmd_m_xmm.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_processor_cmd_internal_xmm.h"


int Diana_Call_pandn(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_ANY(dest)
    DI_MEM_GET_SRC_ANY(src)

    {
        int i;
        for(i = 0; i < dest_size; ++i)
        {
            dest.u8[i] &= ~(src.u8[i]);
        }
    }

    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pand(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_ANY(dest)
    DI_MEM_GET_SRC_ANY(src)

    {
        int i;
        for(i = 0; i < dest_size; ++i)
        {
            dest.u8[i] &= src.u8[i];
        }
    }

    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_por(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_ANY(dest)
    DI_MEM_GET_SRC_ANY(src)

    {
        int i;
        for(i = 0; i < dest_size; ++i)
        {
            dest.u8[i] |= src.u8[i];
        }
    }

    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pxor(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_ANY(dest)
    DI_MEM_GET_SRC_ANY(src)

    {
        int i;
        for(i = 0; i < dest_size; ++i)
        {
            dest.u8[i] ^= src.u8[i];
        }
    }

    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

int Diana_Call_movups(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}

int Diana_Call_movupd(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}

int Diana_Call_movdqu(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}

int Diana_Call_movaps(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}

int Diana_Call_movapd(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}

int Diana_Call_movdqa(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}

int Diana_Call_movntps(struct _dianaContext * pDianaContext,
                       DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}

int Diana_Call_movntpd(struct _dianaContext * pDianaContext,
                       DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}

int Diana_Call_movntdq(struct _dianaContext * pDianaContext,
                       DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm(pDianaContext,
                           pCallContext));
    DI_PROC_END
}

int Diana_Call_movss(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET_SRC_XMM(src)

    dest.u32[0] = src.u32[0];
    if (diana_register == pCallContext->m_result.linkedOperands[0].type &&
        diana_register == pCallContext->m_result.linkedOperands[1].type)
    {
        // when the source and destination operands are both XMM registers
        // DEST[31:0] <- SRC[31:0]
    }
    else
    if (diana_index == pCallContext->m_result.linkedOperands[0].type &&
        diana_register == pCallContext->m_result.linkedOperands[1].type)
    {
        // when the source operand is an XMM register and the destination is memory
        // DEST[31:0] <- SRC[31:0]
        dest_size = 4;
    }
    else
    if (diana_register == pCallContext->m_result.linkedOperands[0].type &&
        diana_index == pCallContext->m_result.linkedOperands[1].type)
    {
        // when the source operand is memory and the destination is an XMM register
        // DEST[31:0] <- SRC[31:0]
        // DEST[127:32] <- 0
        dest.u32[1] = 0;
        dest.u64[1] = 0;
    }
    else
        Diana_FatalBreak();

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

int Diana_Call_movlpd(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm8(pDianaContext,
                            pCallContext));
    DI_PROC_END
}

int Diana_Call_movlps(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_CHECK(diana_mov_xmm8(pDianaContext,
                            pCallContext));
    DI_PROC_END
}

int Diana_Call_movhps(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    // DEST := SRC
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    dest_size = 16;
    DI_MEM_GET_DEST_XMM(dest)

    src_size = 8;
    DI_MEM_GET_SRC_XMM(src)

    dest.u64[1] = src.u64[0];

    DI_MEM_SET_DEST_XMM(dest)

    DI_PROC_END
}




int Diana_Call_movsd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM_FPU(dest)
    DI_DEF_LOCAL_XMM_FPU(src)

    DI_MEM_GET_SRC_XMM(src)

    dest = src;

    if (diana_register == pCallContext->m_result.linkedOperands[0].type &&
        diana_index == pCallContext->m_result.linkedOperands[1].type)
    {
        dest_size = 16;
        dest.u64[1] = 0;
    }

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

int Diana_Call_movddup(struct _dianaContext * pDianaContext,
                       DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    DI_MEM_GET_DEST_XMM(dest)
    src_size = 8;
    DI_MEM_GET_SRC_XMM(src)

    dest.u64[0] = src.u64[0];
    dest.u64[1] = src.u64[0];

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}


int Diana_Call_movd(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    DI_MEM_GET_DEST_SIZE(dest)
    DI_MEM_GET_SRC_ANY(src)

    dest = src;

    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

// streams
int Diana_Call_andpd(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET_SRC_XMM(src)

    dest.u64[0] &= src.u64[0];
    dest.u64[1] &= src.u64[1];

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

int Diana_Call_andps(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_andpd(pDianaContext, pCallContext);
}

int Diana_Call_andnpd(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET_SRC_XMM(src)

    dest.u64[0] &= ~src.u64[0];
    dest.u64[1] &= ~src.u64[1];

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

int Diana_Call_andnps(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_andnpd(pDianaContext, pCallContext);
}

int Diana_Call_orpd(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET_SRC_XMM(src)

    dest.u64[0] |= src.u64[0];
    dest.u64[1] |= src.u64[1];

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

int Diana_Call_orps(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_orpd(pDianaContext, pCallContext);
}

int Diana_Call_xorpd(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET_SRC_XMM(src)

    dest.u64[0] ^= src.u64[0];
    dest.u64[1] ^= src.u64[1];

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

int Diana_Call_xorps(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_xorpd(pDianaContext, pCallContext);
}