#include "diana_processor_cmd_sse2.h"
#include "diana_processor_cmd_fpu_internal.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

/*=============================================================================
 * Helpers
 *===========================================================================*/

/* Clamp to [lo, hi] */
static DI_INT32 clamp32(DI_INT64 v, DI_INT32 lo, DI_INT32 hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return (DI_INT32)v;
}
static DI_UINT32 clamp32u(DI_INT64 v, DI_UINT32 hi)
{
    if (v < 0) return 0;
    if ((DI_UINT64)v > hi) return hi;
    return (DI_UINT32)(DI_UINT64)v;
}

/* Read the shift count from operand 1. For xmm shifts, operand 1 can be an
   xmm register (64-bit lo quadword), memory, or immediate. */
static OPERAND_SIZE GetShiftCount(struct _dianaContext * pDianaContext,
                                  DianaProcessor * pCallContext,
                                  int * pError)
{
    OPERAND_SIZE count = 0;
    int count_size = 0;
    *pError = 0;
    if (pCallContext->m_result.linkedOperands[1].type == diana_imm)
    {
        count = pCallContext->m_result.linkedOperands[1].value.imm;
    }
    else
    {
        /* xmm or memory: read the 64-bit lo quadword */
        DianaRegisterXMM_type xmm_src = {{0}};
        int xmm_size = 8;
        int err = DianaProcessor_XMM_SetGetOperand(pDianaContext, pCallContext, 1,
                                                   &xmm_src, 0, xmm_size, 0, 1);
        if (err != DI_SUCCESS) { *pError = err; return 0; }
        count = xmm_src.u64[0];
        (void)count_size;
    }
    return count;
}

/*=============================================================================
 * Packed integer add
 *===========================================================================*/
int Diana_Call_paddb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
        for (i = 0; i < 16; ++i) dest.u8[i] = (DI_CHAR)(dest.u8[i] + src.u8[i]);
    else
        for (i = 0; i < 8; ++i)  dest.u8[i] = (DI_CHAR)(dest.u8[i] + src.u8[i]);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_paddw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
        for (i = 0; i < 8; ++i) dest.u16[i] = (DI_UINT16)(dest.u16[i] + src.u16[i]);
    else
        for (i = 0; i < 4; ++i) dest.u16[i] = (DI_UINT16)(dest.u16[i] + src.u16[i]);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_paddd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
        for (i = 0; i < 4; ++i) dest.u32[i] = dest.u32[i] + src.u32[i];
    else
        for (i = 0; i < 2; ++i) dest.u32[i] = dest.u32[i] + src.u32[i];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_paddq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
        for (i = 0; i < 2; ++i) dest.u64[i] = dest.u64[i] + src.u64[i];
    else
        dest.u64[0] = dest.u64[0] + src.u64[0];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_paddsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i)
        dest.s8[i] = (DI_SIGNED_CHAR)clamp32((DI_INT64)dest.s8[i] + (DI_INT64)src.s8[i], -128, 127);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_paddsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
        dest.s16[i] = (DI_INT16)clamp32((DI_INT64)dest.s16[i] + (DI_INT64)src.s16[i], -32768, 32767);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_paddusb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i)
        dest.u8[i] = (DI_CHAR)clamp32u((DI_INT64)(DI_UINT32)dest.u8[i] + (DI_INT64)(DI_UINT32)src.u8[i], 0xFF);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_paddusw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
        dest.u16[i] = (DI_UINT16)clamp32u((DI_INT64)(DI_UINT32)dest.u16[i] + (DI_INT64)(DI_UINT32)src.u16[i], 0xFFFF);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

/*=============================================================================
 * Packed integer subtract
 *===========================================================================*/
int Diana_Call_psubb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i) dest.u8[i] = (DI_CHAR)(dest.u8[i] - src.u8[i]);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_psubw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i) dest.u16[i] = (DI_UINT16)(dest.u16[i] - src.u16[i]);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_psubd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 4 : 2;
    for (i = 0; i < n; ++i) dest.u32[i] = dest.u32[i] - src.u32[i];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_psubq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 2 : 1;
    for (i = 0; i < n; ++i) dest.u64[i] = dest.u64[i] - src.u64[i];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_psubsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i)
        dest.s8[i] = (DI_SIGNED_CHAR)clamp32((DI_INT64)dest.s8[i] - (DI_INT64)src.s8[i], -128, 127);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_psubsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
        dest.s16[i] = (DI_INT16)clamp32((DI_INT64)dest.s16[i] - (DI_INT64)src.s16[i], -32768, 32767);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_psubusb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i)
    {
        DI_INT32 v = (DI_INT32)(DI_UINT32)dest.u8[i] - (DI_INT32)(DI_UINT32)src.u8[i];
        dest.u8[i] = (DI_CHAR)(v < 0 ? 0 : v);
    }
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_psubusw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
    {
        DI_INT32 v = (DI_INT32)(DI_UINT32)dest.u16[i] - (DI_INT32)(DI_UINT32)src.u16[i];
        dest.u16[i] = (DI_UINT16)(v < 0 ? 0 : v);
    }
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

/*=============================================================================
 * Packed compare
 *===========================================================================*/
int Diana_Call_pcmpeqb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i) dest.u8[i] = (dest.u8[i] == src.u8[i]) ? 0xFF : 0x00;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pcmpeqw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i) dest.u16[i] = (dest.u16[i] == src.u16[i]) ? 0xFFFF : 0x0000;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pcmpeqd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 4 : 2;
    for (i = 0; i < n; ++i) dest.u32[i] = (dest.u32[i] == src.u32[i]) ? 0xFFFFFFFFU : 0;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pcmpgtb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i) dest.u8[i] = (dest.s8[i] > src.s8[i]) ? 0xFF : 0x00;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pcmpgtw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i) dest.u16[i] = (dest.s16[i] > src.s16[i]) ? 0xFFFF : 0x0000;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pcmpgtd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 4 : 2;
    for (i = 0; i < n; ++i) dest.u32[i] = (dest.s32[i] > src.s32[i]) ? 0xFFFFFFFFU : 0;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

/*=============================================================================
 * Packed multiply
 *===========================================================================*/
int Diana_Call_pmullw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
        dest.u16[i] = (DI_UINT16)((DI_INT32)dest.s16[i] * (DI_INT32)src.s16[i]);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pmulhw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
        dest.u16[i] = (DI_UINT16)(((DI_INT32)dest.s16[i] * (DI_INT32)src.s16[i]) >> 16);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pmulhuw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
        dest.u16[i] = (DI_UINT16)(((DI_UINT32)dest.u16[i] * (DI_UINT32)src.u16[i]) >> 16);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pmuludq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 2 : 1;
    for (i = 0; i < n; ++i)
        dest.u64[i] = (DI_UINT64)(DI_UINT32)dest.u32[i*2] * (DI_UINT64)(DI_UINT32)src.u32[i*2];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pmaddwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DianaRegisterXMM_type result = {{0}};
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 4 : 2;
    for (i = 0; i < n; ++i)
        result.s32[i] = (DI_INT32)dest.s16[i*2] * (DI_INT32)src.s16[i*2]
                      + (DI_INT32)dest.s16[i*2+1] * (DI_INT32)src.s16[i*2+1];
    dest = result;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

/*=============================================================================
 * Average / min / max
 *===========================================================================*/
int Diana_Call_pavgb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i)
        dest.u8[i] = (DI_CHAR)(((DI_UINT32)dest.u8[i] + (DI_UINT32)src.u8[i] + 1) >> 1);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pavgw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
        dest.u16[i] = (DI_UINT16)(((DI_UINT32)dest.u16[i] + (DI_UINT32)src.u16[i] + 1) >> 1);
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pmaxsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
        dest.s16[i] = (dest.s16[i] > src.s16[i]) ? dest.s16[i] : src.s16[i];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pmaxub(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i)
        dest.u8[i] = (dest.u8[i] > src.u8[i]) ? dest.u8[i] : src.u8[i];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pminsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i)
        dest.s16[i] = (dest.s16[i] < src.s16[i]) ? dest.s16[i] : src.s16[i];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pminub(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    n = (dest_size == 0x10) ? 16 : 8;
    for (i = 0; i < n; ++i)
        dest.u8[i] = (dest.u8[i] < src.u8[i]) ? dest.u8[i] : src.u8[i];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_psadbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
    {
        DI_UINT32 sum0 = 0, sum1 = 0;
        for (i = 0; i < 8; ++i)
        {
            int d = (int)(DI_UINT32)dest.u8[i] - (int)(DI_UINT32)src.u8[i];
            sum0 += (DI_UINT32)(d < 0 ? -d : d);
        }
        for (i = 8; i < 16; ++i)
        {
            int d = (int)(DI_UINT32)dest.u8[i] - (int)(DI_UINT32)src.u8[i];
            sum1 += (DI_UINT32)(d < 0 ? -d : d);
        }
        dest.u64[0] = sum0;
        dest.u64[1] = sum1;
    }
    else
    {
        DI_UINT32 sum = 0;
        for (i = 0; i < 8; ++i)
        {
            int d = (int)(DI_UINT32)dest.u8[i] - (int)(DI_UINT32)src.u8[i];
            sum += (DI_UINT32)(d < 0 ? -d : d);
        }
        dest.u64[0] = sum;
    }
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

/*=============================================================================
 * pmovmskb
 *===========================================================================*/
int Diana_Call_pmovmskb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n;
    DI_DEF_LOCAL(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    oldDestValue;
    n = (src_size == 0x10) ? 16 : 8;
    dest = 0;
    for (i = 0; i < n; ++i)
        if (src.u8[i] & 0x80)
            dest |= (OPERAND_SIZE)(1 << i);
    DI_MEM_SET_DEST(dest)
    DI_PROC_END
}

/*=============================================================================
 * pinsrw / pextrw
 *===========================================================================*/
int Diana_Call_pinsrw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    OPERAND_SIZE count = 0, val = 0;
    int val_size = 0, count_size = 0;
    DI_DEF_LOCAL_XMM(dest)
    DI_MEM_GET_DEST_ANY(dest)
    DI_MEM_GET(val, 1)
    DI_MEM_GET(count, 2)
    count &= dest_size == 8 ? 0x3 : 0x7;     // mm: 4 words
    dest.u16[count] = (DI_UINT16)val;
    (void)val_size; (void)count_size;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_pextrw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    OPERAND_SIZE count = 0;
    int count_size = 0;
    DI_DEF_LOCAL(dest)
    DI_DEF_LOCAL_XMM(src)
    oldDestValue;
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET(count, 2)
    count &= src_size == 8 ? 0x3 : 0x7;
    dest = src.u16[count];
    (void)count_size;
    DI_MEM_SET_DEST(dest)
    DI_PROC_END
}

/*=============================================================================
 * Pack
 *===========================================================================*/
int Diana_Call_packssdw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DianaRegisterXMM_type result = {{0}};
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
    {
        for (i = 0; i < 4; ++i)
            result.s16[i]   = (DI_INT16)clamp32((DI_INT64)dest.s32[i], -32768, 32767);
        for (i = 0; i < 4; ++i)
            result.s16[i+4] = (DI_INT16)clamp32((DI_INT64)src.s32[i], -32768, 32767);
    }
    else
    {
        for (i = 0; i < 2; ++i)
            result.s16[i]   = (DI_INT16)clamp32((DI_INT64)dest.s32[i], -32768, 32767);
        for (i = 0; i < 2; ++i)
            result.s16[i+2] = (DI_INT16)clamp32((DI_INT64)src.s32[i], -32768, 32767);
    }
    dest = result;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_packsswb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DianaRegisterXMM_type result = {{0}};
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
    {
        for (i = 0; i < 8; ++i)
            result.s8[i]   = (DI_SIGNED_CHAR)clamp32((DI_INT64)dest.s16[i], -128, 127);
        for (i = 0; i < 8; ++i)
            result.s8[i+8] = (DI_SIGNED_CHAR)clamp32((DI_INT64)src.s16[i], -128, 127);
    }
    else
    {
        for (i = 0; i < 4; ++i)
            result.s8[i]   = (DI_SIGNED_CHAR)clamp32((DI_INT64)dest.s16[i], -128, 127);
        for (i = 0; i < 4; ++i)
            result.s8[i+4] = (DI_SIGNED_CHAR)clamp32((DI_INT64)src.s16[i], -128, 127);
    }
    dest = result;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_packuswb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DianaRegisterXMM_type result = {{0}};
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
    {
        for (i = 0; i < 8; ++i)
            result.u8[i]   = (DI_CHAR)clamp32u((DI_INT64)dest.s16[i], 0xFF);
        for (i = 0; i < 8; ++i)
            result.u8[i+8] = (DI_CHAR)clamp32u((DI_INT64)src.s16[i], 0xFF);
    }
    else
    {
        for (i = 0; i < 4; ++i)
            result.u8[i]   = (DI_CHAR)clamp32u((DI_INT64)dest.s16[i], 0xFF);
        for (i = 0; i < 4; ++i)
            result.u8[i+4] = (DI_CHAR)clamp32u((DI_INT64)src.s16[i], 0xFF);
    }
    dest = result;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

/*=============================================================================
 * Unpack high
 *===========================================================================*/
int Diana_Call_punpckhbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
    {
        result.u8[0]  = dest.u8[8];  result.u8[1]  = src.u8[8];
        result.u8[2]  = dest.u8[9];  result.u8[3]  = src.u8[9];
        result.u8[4]  = dest.u8[10]; result.u8[5]  = src.u8[10];
        result.u8[6]  = dest.u8[11]; result.u8[7]  = src.u8[11];
        result.u8[8]  = dest.u8[12]; result.u8[9]  = src.u8[12];
        result.u8[10] = dest.u8[13]; result.u8[11] = src.u8[13];
        result.u8[12] = dest.u8[14]; result.u8[13] = src.u8[14];
        result.u8[14] = dest.u8[15]; result.u8[15] = src.u8[15];
    }
    else
    {
        result.u8[0] = dest.u8[4]; result.u8[1] = src.u8[4];
        result.u8[2] = dest.u8[5]; result.u8[3] = src.u8[5];
        result.u8[4] = dest.u8[6]; result.u8[5] = src.u8[6];
        result.u8[6] = dest.u8[7]; result.u8[7] = src.u8[7];
    }
    dest = result;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_punpckhwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
    {
        result.u16[0] = dest.u16[4]; result.u16[1] = src.u16[4];
        result.u16[2] = dest.u16[5]; result.u16[3] = src.u16[5];
        result.u16[4] = dest.u16[6]; result.u16[5] = src.u16[6];
        result.u16[6] = dest.u16[7]; result.u16[7] = src.u16[7];
    }
    else
    {
        result.u16[0] = dest.u16[2]; result.u16[1] = src.u16[2];
        result.u16[2] = dest.u16[3]; result.u16[3] = src.u16[3];
    }
    dest = result;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_punpckhdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (dest_size == 0x10)
    {
        result.u32[0] = dest.u32[2]; result.u32[1] = src.u32[2];
        result.u32[2] = dest.u32[3]; result.u32[3] = src.u32[3];
    }
    else
    {
        result.u32[0] = dest.u32[1]; result.u32[1] = src.u32[1];
    }
    dest = result;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_punpckhqdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    result.u64[0] = dest.u64[1];
    result.u64[1] = src.u64[1];
    dest = result;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

/*=============================================================================
 * Shifts
 *===========================================================================*/
int Diana_Call_psllw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n; int err;
    OPERAND_SIZE count = GetShiftCount(pDianaContext, pCallContext, &err);
    DI_DEF_LOCAL_XMM(dest)
    if (err) return err;
    DI_MEM_GET_DEST_XMM(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    if (count >= 16) { int j; for (j=0;j<8;++j) dest.u16[j]=0; }
    else for (i = 0; i < n; ++i) dest.u16[i] = (DI_UINT16)(dest.u16[i] << count);
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_pslld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n; int err;
    OPERAND_SIZE count = GetShiftCount(pDianaContext, pCallContext, &err);
    DI_DEF_LOCAL_XMM(dest)
    if (err) return err;
    DI_MEM_GET_DEST_XMM(dest)
    n = (dest_size == 0x10) ? 4 : 2;
    if (count >= 32) { int j; for (j=0;j<4;++j) dest.u32[j]=0; }
    else for (i = 0; i < n; ++i) dest.u32[i] = dest.u32[i] << (int)count;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_psllq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n; int err;
    OPERAND_SIZE count = GetShiftCount(pDianaContext, pCallContext, &err);
    DI_DEF_LOCAL_XMM(dest)
    if (err) return err;
    DI_MEM_GET_DEST_XMM(dest)
    n = (dest_size == 0x10) ? 2 : 1;
    if (count >= 64) { int j; for (j=0;j<2;++j) dest.u64[j]=0; }
    else for (i = 0; i < n; ++i) dest.u64[i] = dest.u64[i] << (int)count;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_pslldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    OPERAND_SIZE count = 0;
    int count_size = 0;
    DI_DEF_LOCAL_XMM(dest)
    DI_MEM_GET(count, 1)
    DI_MEM_GET_DEST_XMM(dest)
    if (count >= 16) { int j; for(j=0;j<16;++j) dest.u8[j]=0; }
    else
    {
        for (i = 15; i >= (int)count; --i) dest.u8[i] = dest.u8[i - (int)count];
        for (i = 0; i < (int)count; ++i) dest.u8[i] = 0;
    }
    (void)count_size;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_psrlw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n; int err;
    OPERAND_SIZE count = GetShiftCount(pDianaContext, pCallContext, &err);
    DI_DEF_LOCAL_XMM(dest)
    if (err) return err;
    DI_MEM_GET_DEST_XMM(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    if (count >= 16) { int j; for(j=0;j<8;++j) dest.u16[j]=0; }
    else for (i = 0; i < n; ++i) dest.u16[i] = (DI_UINT16)(dest.u16[i] >> count);
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_psrld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n; int err;
    OPERAND_SIZE count = GetShiftCount(pDianaContext, pCallContext, &err);
    DI_DEF_LOCAL_XMM(dest)
    if (err) return err;
    DI_MEM_GET_DEST_XMM(dest)
    n = (dest_size == 0x10) ? 4 : 2;
    if (count >= 32) { int j; for(j=0;j<4;++j) dest.u32[j]=0; }
    else for (i = 0; i < n; ++i) dest.u32[i] = dest.u32[i] >> (int)count;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_psrlq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n; int err;
    OPERAND_SIZE count = GetShiftCount(pDianaContext, pCallContext, &err);
    DI_DEF_LOCAL_XMM(dest)
    if (err) return err;
    DI_MEM_GET_DEST_XMM(dest)
    n = (dest_size == 0x10) ? 2 : 1;
    if (count >= 64) { int j; for(j=0;j<2;++j) dest.u64[j]=0; }
    else for (i = 0; i < n; ++i) dest.u64[i] = dest.u64[i] >> (int)count;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_psrldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    OPERAND_SIZE count = 0;
    int count_size = 0;
    DI_DEF_LOCAL_XMM(dest)
    DI_MEM_GET(count, 1)
    DI_MEM_GET_DEST_XMM(dest)
    if (count >= 16) { int j; for(j=0;j<16;++j) dest.u8[j]=0; }
    else
    {
        for (i = 0; i < 16 - (int)count; ++i) dest.u8[i] = dest.u8[i + (int)count];
        for (i = 16 - (int)count; i < 16; ++i) dest.u8[i] = 0;
    }
    (void)count_size;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_psraw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n; int err;
    OPERAND_SIZE count = GetShiftCount(pDianaContext, pCallContext, &err);
    DI_DEF_LOCAL_XMM(dest)
    if (err) return err;
    if (count > 15) count = 15;
    DI_MEM_GET_DEST_XMM(dest)
    n = (dest_size == 0x10) ? 8 : 4;
    for (i = 0; i < n; ++i) dest.s16[i] = (DI_INT16)(dest.s16[i] >> (int)count);
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_psrad(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i; int n; int err;
    OPERAND_SIZE count = GetShiftCount(pDianaContext, pCallContext, &err);
    DI_DEF_LOCAL_XMM(dest)
    if (err) return err;
    if (count > 31) count = 31;
    DI_MEM_GET_DEST_XMM(dest)
    n = (dest_size == 0x10) ? 4 : 2;
    for (i = 0; i < n; ++i) dest.s32[i] = dest.s32[i] >> (int)count;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

/*=============================================================================
 * Shuffle words
 *===========================================================================*/
int Diana_Call_pshufhw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    OPERAND_SIZE order = 0;
    int order_size = 0;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET(order, 2)
    dest.u16[0] = src.u16[0];
    dest.u16[1] = src.u16[1];
    dest.u16[2] = src.u16[2];
    dest.u16[3] = src.u16[3];
    dest.u16[4] = src.u16[4 + (int)((order >> 0) & 3)];
    dest.u16[5] = src.u16[4 + (int)((order >> 2) & 3)];
    dest.u16[6] = src.u16[4 + (int)((order >> 4) & 3)];
    dest.u16[7] = src.u16[4 + (int)((order >> 6) & 3)];
    (void)order_size;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_pshuflw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    OPERAND_SIZE order = 0;
    int order_size = 0;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET(order, 2)
    dest.u16[0] = src.u16[(order >> 0) & 3];
    dest.u16[1] = src.u16[(order >> 2) & 3];
    dest.u16[2] = src.u16[(order >> 4) & 3];
    dest.u16[3] = src.u16[(order >> 6) & 3];
    dest.u16[4] = src.u16[4];
    dest.u16[5] = src.u16[5];
    dest.u16[6] = src.u16[6];
    dest.u16[7] = src.u16[7];
    (void)order_size;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_pshufw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    /* pshufw mm, mm/m64, imm8 */
    OPERAND_SIZE order = 0;
    int order_size = 0;
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    src_size = 8;
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET(order, 2)
    result.u16[0] = src.u16[(order >> 0) & 3];
    result.u16[1] = src.u16[(order >> 2) & 3];
    result.u16[2] = src.u16[(order >> 4) & 3];
    result.u16[3] = src.u16[(order >> 6) & 3];
    (void)order_size;
    dest = result;
    dest_size = 8;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

/*=============================================================================
 * Move quadword
 *===========================================================================*/
int Diana_Call_movq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    /* movq xmm, xmm/m64: zero-extends high quadword
       movq xmm/m64, xmm: stores low 64 bits
       movq mm, mm/m64 and mm/m64, mm: 64 bits */
    const DianaLinkedOperand * pDest = pCallContext->m_result.linkedOperands;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM_FPU(src)
    DI_MEM_GET_SRC_XMM(src)
    dest.u64[0] = src.u64[0];
    dest.u64[1] = 0;
    if (pDest->type != diana_register ||
        (pDest->value.recognizedRegister >= reg_MM0 && pDest->value.recognizedRegister <= reg_MM7))
    {
        dest_size = 8;
    }
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_movq2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM_FPU(src)
    DI_DEF_LOCAL_XMM(dest)
    dest.u64[0] = 0; dest.u64[1] = 0;
    DI_MEM_GET_SRC_ANY(src)
    dest.u64[0] = src.u64[0];
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_movdq2q(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(src)
    DI_DEF_LOCAL_XMM_FPU(dest)
    DI_MEM_GET_SRC_XMM(src)
    dest.u64[0] = src.u64[0];
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

/*=============================================================================
 * MMX state
 *===========================================================================*/
int Diana_Call_emms(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    // every x87 register empty, TOP kept
    int i;
    pDianaContext;
    for (i = 0; i < 8; ++i)
        pCallContext->m_fpu.registerFlags[i] &= ~DI_PROCESSOR_FPU_REGISTER_BUSY;
    DI_PROC_END
}
int Diana_Call_femms(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Diana_Call_emms(pDianaContext, pCallContext);
}

/*=============================================================================
 * SSE floating point: MXCSR gives the rounding, DAZ, FTZ and takes the exception flags (Diana_SSE_Status,
 * Diana_SSE_Exceptions), the x87 state is not used. A scalar form changes element 0 and keeps the rest of the
 * destination, its memory source is one element. An unmasked exception (#XM) writes nothing.
 *===========================================================================*/
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU

float32 Diana_SSE_Float32(DiSseOperation operation, float32 a, float32 b, float_status_t * pStatus)
{
    switch (operation)
    {
    case DI_SSE_ADD: return float32_add(a, b, pStatus);
    case DI_SSE_SUB: return float32_sub(a, b, pStatus);
    case DI_SSE_MUL: return float32_mul(a, b, pStatus);
    case DI_SSE_DIV: return float32_div(a, b, pStatus);
    case DI_SSE_SQRT: return float32_sqrt(b, pStatus);
    case DI_SSE_MIN: return float32_min(a, b, pStatus);
    default: return float32_max(a, b, pStatus);
    }
}

float64 Diana_SSE_Float64(DiSseOperation operation, float64 a, float64 b, float_status_t * pStatus)
{
    switch (operation)
    {
    case DI_SSE_ADD: return float64_add(a, b, pStatus);
    case DI_SSE_SUB: return float64_sub(a, b, pStatus);
    case DI_SSE_MUL: return float64_mul(a, b, pStatus);
    case DI_SSE_DIV: return float64_div(a, b, pStatus);
    case DI_SSE_SQRT: return float64_sqrt(b, pStatus);
    case DI_SSE_MIN: return float64_min(a, b, pStatus);
    default: return float64_max(a, b, pStatus);
    }
}

// elementSize 4 (float) or 8 (double); count 1 (scalar) or 16 / elementSize (packed)
static int Di_SseArithmetic(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext,
                            int elementSize, int count, DiSseOperation operation)
{
    DianaRegisterXMM_type dest = {{0}}, src = {{0}};
    int dest_size = 16, src_size = count == 1 ? elementSize : 16;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int i;
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    for (i = 0; i < count; ++i)
    {
        if (elementSize == 4)
            dest.u32[i] = Diana_SSE_Float32(operation, dest.u32[i], src.u32[i], &status);
        else
            dest.u64[i] = Diana_SSE_Float64(operation, dest.u64[i], src.u64[i], &status);
    }
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

#define DI_SSE_ARITHMETIC(name, elementSize, count, operation) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_SseArithmetic(pDianaContext, pCallContext, elementSize, count, operation); \
}

DI_SSE_ARITHMETIC(addss, 4, 1, DI_SSE_ADD)
DI_SSE_ARITHMETIC(subss, 4, 1, DI_SSE_SUB)
DI_SSE_ARITHMETIC(mulss, 4, 1, DI_SSE_MUL)
DI_SSE_ARITHMETIC(divss, 4, 1, DI_SSE_DIV)
DI_SSE_ARITHMETIC(sqrtss, 4, 1, DI_SSE_SQRT)
DI_SSE_ARITHMETIC(minss, 4, 1, DI_SSE_MIN)
DI_SSE_ARITHMETIC(maxss, 4, 1, DI_SSE_MAX)
DI_SSE_ARITHMETIC(addps, 4, 4, DI_SSE_ADD)
DI_SSE_ARITHMETIC(subps, 4, 4, DI_SSE_SUB)
DI_SSE_ARITHMETIC(mulps, 4, 4, DI_SSE_MUL)
DI_SSE_ARITHMETIC(divps, 4, 4, DI_SSE_DIV)
DI_SSE_ARITHMETIC(sqrtps, 4, 4, DI_SSE_SQRT)
DI_SSE_ARITHMETIC(minps, 4, 4, DI_SSE_MIN)
DI_SSE_ARITHMETIC(maxps, 4, 4, DI_SSE_MAX)
DI_SSE_ARITHMETIC(addsd, 8, 1, DI_SSE_ADD)
DI_SSE_ARITHMETIC(subsd, 8, 1, DI_SSE_SUB)
DI_SSE_ARITHMETIC(mulsd, 8, 1, DI_SSE_MUL)
DI_SSE_ARITHMETIC(divsd, 8, 1, DI_SSE_DIV)
DI_SSE_ARITHMETIC(sqrtsd, 8, 1, DI_SSE_SQRT)
DI_SSE_ARITHMETIC(minsd, 8, 1, DI_SSE_MIN)
DI_SSE_ARITHMETIC(maxsd, 8, 1, DI_SSE_MAX)
DI_SSE_ARITHMETIC(addpd, 8, 2, DI_SSE_ADD)
DI_SSE_ARITHMETIC(subpd, 8, 2, DI_SSE_SUB)
DI_SSE_ARITHMETIC(mulpd, 8, 2, DI_SSE_MUL)
DI_SSE_ARITHMETIC(divpd, 8, 2, DI_SSE_DIV)
DI_SSE_ARITHMETIC(sqrtpd, 8, 2, DI_SSE_SQRT)
DI_SSE_ARITHMETIC(minpd, 8, 2, DI_SSE_MIN)
DI_SSE_ARITHMETIC(maxpd, 8, 2, DI_SSE_MAX)

// comiss, ucomiss, comisd, ucomisd: ZF PF CF, OF SF AF cleared; comis* signal invalid for a QNaN too
int Diana_SSE_CompareScalar(const DianaRegisterXMM_type * pA, const DianaRegisterXMM_type * pB, int elementSize,
                            int quiet, float_status_t * pStatus)
{
    if (elementSize == 4)
        return quiet ? float32_compare_quiet(pA->u32[0], pB->u32[0], pStatus)
                     : float32_compare(pA->u32[0], pB->u32[0], pStatus);
    return quiet ? float64_compare_quiet(pA->u64[0], pB->u64[0], pStatus)
                 : float64_compare(pA->u64[0], pB->u64[0], pStatus);
}

void Diana_SSE_SetCompareFlags(DianaProcessor * pCallContext, int relation)
{
    CLEAR_FLAG_OF; CLEAR_FLAG_SF; CLEAR_FLAG_AF;
    switch (relation)
    {
    case float_relation_less:    CLEAR_FLAG_ZF; CLEAR_FLAG_PF; SET_FLAG_CF; break;
    case float_relation_equal:   SET_FLAG_ZF;   CLEAR_FLAG_PF; CLEAR_FLAG_CF; break;
    case float_relation_greater: CLEAR_FLAG_ZF; CLEAR_FLAG_PF; CLEAR_FLAG_CF; break;
    default:                     SET_FLAG_ZF;   SET_FLAG_PF;   SET_FLAG_CF; break;
    }
}

static int Di_SseCompareFlags(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext,
                              int elementSize, int quiet)
{
    DianaRegisterXMM_type dest = {{0}}, src = {{0}};
    int dest_size = elementSize, src_size = elementSize;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int result;
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    result = Diana_SSE_CompareScalar(&dest, &src, elementSize, quiet, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    Diana_SSE_SetCompareFlags(pCallContext, result);
    DI_PROC_END
}

int Diana_Call_comiss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseCompareFlags(pDianaContext, pCallContext, 4, 0);
}
int Diana_Call_ucomiss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseCompareFlags(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_comisd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseCompareFlags(pDianaContext, pCallContext, 8, 0);
}
int Diana_Call_ucomisd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseCompareFlags(pDianaContext, pCallContext, 8, 1);
}

// cvtsi2ss, cvtsi2sd: a 32- or 64-bit integer (REX.W) to element 0
void Diana_SSE_FromInteger(DianaRegisterXMM_type * pDest, int elementSize, OPERAND_SIZE value, int size,
                           float_status_t * pStatus)
{
    if (elementSize == 4)
        pDest->u32[0] = size == 8 ? int64_to_float32((DI_INT64)value, pStatus)
                                  : int32_to_float32((DI_INT32)value, pStatus);
    else
        pDest->u64[0] = size == 8 ? int64_to_float64((DI_INT64)value, pStatus) : int32_to_float64((DI_INT32)value);
}

static int Di_SseFromInteger(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterXMM_type dest = {{0}};
    int dest_size = 16;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_DEF_LOCAL(src)
    &oldDestValue;
    DI_MEM_GET_SRC(src)
    DI_MEM_GET_DEST_XMM(dest)
    Diana_SSE_FromInteger(&dest, elementSize, src, src_size, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

int Diana_Call_cvtsi2ss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseFromInteger(pDianaContext, pCallContext, 4);
}
int Diana_Call_cvtsi2sd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseFromInteger(pDianaContext, pCallContext, 8);
}

// cvtss2si, cvttss2si, cvtsd2si, cvttsd2si: element 0 to a 32- or 64-bit integer (REX.W), rounded by MXCSR or
// truncated; NaN and out of range give the integer indefinite (0x80000000, 0x8000000000000000)
OPERAND_SIZE Diana_SSE_ToInteger(const DianaRegisterXMM_type * pSrc, int elementSize, int size, int truncate,
                                 float_status_t * pStatus)
{
    if (elementSize == 4)
    {
        if (size == 8)
            return (OPERAND_SIZE)(truncate ? float32_to_int64_round_to_zero(pSrc->u32[0], pStatus)
                                           : float32_to_int64(pSrc->u32[0], pStatus));
        return (DI_UINT32)(truncate ? float32_to_int32_round_to_zero(pSrc->u32[0], pStatus)
                                    : float32_to_int32(pSrc->u32[0], pStatus));
    }
    if (size == 8)
        return (OPERAND_SIZE)(truncate ? float64_to_int64_round_to_zero(pSrc->u64[0], pStatus)
                                       : float64_to_int64(pSrc->u64[0], pStatus));
    return (DI_UINT32)(truncate ? float64_to_int32_round_to_zero(pSrc->u64[0], pStatus)
                                : float64_to_int32(pSrc->u64[0], pStatus));
}

static int Di_SseToInteger(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext,
                           int elementSize, int truncate)
{
    DianaRegisterXMM_type src = {{0}};
    int src_size = elementSize;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_DEF_LOCAL(dest)
    &oldDestValue;
    DI_MEM_GET_DEST_SIZE(dest)
    DI_MEM_GET_SRC_XMM(src)
    dest = Diana_SSE_ToInteger(&src, elementSize, dest_size, truncate, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_MEM_SET_DEST(dest)
    DI_PROC_END
}

int Diana_Call_cvtss2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseToInteger(pDianaContext, pCallContext, 4, 0);
}
int Diana_Call_cvttss2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseToInteger(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_cvtsd2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseToInteger(pDianaContext, pCallContext, 8, 0);
}
int Diana_Call_cvttsd2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseToInteger(pDianaContext, pCallContext, 8, 1);
}

// cvtss2sd, cvtsd2ss: element 0, the rest of the destination kept
int Diana_Call_cvtss2sd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest = {{0}}, src = {{0}};
    int dest_size = 16, src_size = 4;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    dest.u64[0] = float32_to_float64(src.u32[0], &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_cvtsd2ss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest = {{0}}, src = {{0}};
    int dest_size = 16, src_size = 8;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    dest.u32[0] = float64_to_float32(src.u64[0], &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

#else /* !DIANA_PROCESSOR_USE_SOFTFLOAT_FPU */

#define DIANA_SSE_FLOAT_STUB(name) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ pDianaContext; pCallContext; return DI_UNSUPPORTED_COMMAND; }

DIANA_SSE_FLOAT_STUB(addss)    DIANA_SSE_FLOAT_STUB(subss)    DIANA_SSE_FLOAT_STUB(mulss)
DIANA_SSE_FLOAT_STUB(divss)    DIANA_SSE_FLOAT_STUB(sqrtss)   DIANA_SSE_FLOAT_STUB(comiss)
DIANA_SSE_FLOAT_STUB(ucomiss)  DIANA_SSE_FLOAT_STUB(cvtsi2ss) DIANA_SSE_FLOAT_STUB(cvtss2sd)
DIANA_SSE_FLOAT_STUB(cvtss2si) DIANA_SSE_FLOAT_STUB(cvttss2si) DIANA_SSE_FLOAT_STUB(maxss)
DIANA_SSE_FLOAT_STUB(minss)
DIANA_SSE_FLOAT_STUB(addps) DIANA_SSE_FLOAT_STUB(subps) DIANA_SSE_FLOAT_STUB(mulps)
DIANA_SSE_FLOAT_STUB(divps) DIANA_SSE_FLOAT_STUB(sqrtps) DIANA_SSE_FLOAT_STUB(maxps)
DIANA_SSE_FLOAT_STUB(minps)
DIANA_SSE_FLOAT_STUB(addpd) DIANA_SSE_FLOAT_STUB(subpd) DIANA_SSE_FLOAT_STUB(mulpd)
DIANA_SSE_FLOAT_STUB(divpd) DIANA_SSE_FLOAT_STUB(sqrtpd) DIANA_SSE_FLOAT_STUB(maxpd)
DIANA_SSE_FLOAT_STUB(minpd)
DIANA_SSE_FLOAT_STUB(addsd) DIANA_SSE_FLOAT_STUB(subsd) DIANA_SSE_FLOAT_STUB(mulsd)
DIANA_SSE_FLOAT_STUB(divsd) DIANA_SSE_FLOAT_STUB(comisd) DIANA_SSE_FLOAT_STUB(cvttsd2si)
DIANA_SSE_FLOAT_STUB(sqrtsd) DIANA_SSE_FLOAT_STUB(ucomisd) DIANA_SSE_FLOAT_STUB(maxsd)
DIANA_SSE_FLOAT_STUB(minsd) DIANA_SSE_FLOAT_STUB(cvtsi2sd) DIANA_SSE_FLOAT_STUB(cvtsd2ss)
DIANA_SSE_FLOAT_STUB(cvtsd2si)

#endif /* DIANA_PROCESSOR_USE_SOFTFLOAT_FPU */

/*=============================================================================
 * MXCSR
 *===========================================================================*/
int Diana_Call_ldmxcsr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_UINT32 mxcsr = 0;
    OPERAND_SIZE doneBytes = 0;
    DI_CHECK(Diana_ReadRawBufferFromArgMem(pDianaContext, pCallContext, 0, &mxcsr, sizeof(mxcsr), &doneBytes, 0));
    // a reserved bit is #GP
    if (mxcsr & DI_MXCSR_RESERVED)
        return DI_ERROR;
    pCallContext->m_mxcsr = mxcsr;
    DI_PROC_END
}

int Diana_Call_stmxcsr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_UINT32 mxcsr = pCallContext->m_mxcsr;
    OPERAND_SIZE doneBytes = 0;
    DI_CHECK(Diana_WriteRawBufferToArgMem(pDianaContext, pCallContext, 0, &mxcsr, sizeof(mxcsr), &doneBytes, 0));
    DI_PROC_END
}

/*=============================================================================
 * Move with shuffle/duplication
 *===========================================================================*/
int Diana_Call_movshdup(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    dest.u32[0] = src.u32[1];
    dest.u32[1] = src.u32[1];
    dest.u32[2] = src.u32[3];
    dest.u32[3] = src.u32[3];
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_movsldup(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    dest.u32[0] = src.u32[0];
    dest.u32[1] = src.u32[0];
    dest.u32[2] = src.u32[2];
    dest.u32[3] = src.u32[2];
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

/*=============================================================================
 * Unpack
 *===========================================================================*/
int Diana_Call_unpcklps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    result.u32[0] = dest.u32[0]; result.u32[1] = src.u32[0];
    result.u32[2] = dest.u32[1]; result.u32[3] = src.u32[1];
    dest = result;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_unpckhps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    result.u32[0] = dest.u32[2]; result.u32[1] = src.u32[2];
    result.u32[2] = dest.u32[3]; result.u32[3] = src.u32[3];
    dest = result;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_unpcklpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    dest.u64[1] = src.u64[0];
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_unpckhpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    result.u64[0] = dest.u64[1];
    result.u64[1] = src.u64[1];
    dest = result;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

/*=============================================================================
 * Shuffle
 *===========================================================================*/
int Diana_Call_shufps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    OPERAND_SIZE imm = 0;
    int imm_size = 0;
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET(imm, 2)
    result.u32[0] = dest.u32[(imm >> 0) & 3];
    result.u32[1] = dest.u32[(imm >> 2) & 3];
    result.u32[2] = src.u32[(imm >> 4) & 3];
    result.u32[3] = src.u32[(imm >> 6) & 3];
    dest = result;
    (void)imm_size;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_shufpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    OPERAND_SIZE imm = 0;
    int imm_size = 0;
    DianaRegisterXMM_type result = {{0}};
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET(imm, 2)
    result.u64[0] = dest.u64[(imm >> 0) & 1];
    result.u64[1] = src.u64[(imm >> 1) & 1];
    dest = result;
    (void)imm_size;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

/*=============================================================================
 * System/memory no-ops
 *===========================================================================*/
int Diana_Call_rdtsc(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext;
    SET_REG_EAX(0);
    SET_REG_EDX(0);
    DI_PROC_END
}
int Diana_Call_rdtscp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext;
    SET_REG_EAX(0);
    SET_REG_EDX(0);
    SET_REG_ECX(0);
    DI_PROC_END
}
int Diana_Call_mfence(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext; DI_PROC_END
}
int Diana_Call_lfence(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext; DI_PROC_END
}
int Diana_Call_sfence(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext; DI_PROC_END
}
int Diana_Call_clflush(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext; DI_PROC_END
}
int Diana_Call_ud2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    return DI_UNSUPPORTED_COMMAND;
}

/*=============================================================================
 * The smaller gaps: movhlps, movlhps; CET shadow stack (not enabled: rdssp is a nop, the others #UD); TSX (RTM
 * present, a transaction aborts at once); PKU (the PKRU register, no keys on memory)
 *===========================================================================*/
int Diana_Call_movhlps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET_SRC_XMM(src)
    dest.u64[0] = src.u64[1];
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}
int Diana_Call_movlhps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET_SRC_XMM(src)
    dest.u64[1] = src.u64[0];
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

// eax: rax with the upper half 0 in 64-bit mode
static void Di_SetEax(DianaProcessor * pCallContext, DI_UINT32 value)
{
    if (pCallContext->m_context.iAMD64Mode)
        SET_REG_RAX(value);
    else
        SET_REG_EAX(value);
}
static void Di_SetEdx(DianaProcessor * pCallContext, DI_UINT32 value)
{
    if (pCallContext->m_context.iAMD64Mode)
        SET_REG_RDX(value);
    else
        SET_REG_EDX(value);
}

int Diana_Call_rdsspd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    // shadow stacks not enabled: a nop, the destination is unchanged
    pDianaContext; pCallContext; DI_PROC_END
}
int Diana_Call_cet_ud(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    // incssp, saveprevssp, rstorssp, setssbsy, clrssbsy, wrss, wruss: #UD while shadow stacks are not enabled
    pDianaContext; pCallContext;
    return DI_INVALID_OPCODE;
}

int Diana_Call_xbegin(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    // the transaction aborts at once: eax = 0 (no status bit: not a retry, not xabort), rip = the fallback
    OPERAND_SIZE fallback = GET_REG_RIP2(pCallContext->m_context.iCurrentCmd_opsize) +
                            pCallContext->m_result.linkedOperands[0].value.rel.m_value +
                            pCallContext->m_result.iFullCmdSize;
    pDianaContext;
    if (pCallContext->m_result.iLinkedOpCount != 1 || pCallContext->m_result.linkedOperands[0].type != diana_rel)
        return DI_ERROR;
    Di_SetEax(pCallContext, 0);
    SET_REG_RIP(fallback);
    DI_PROC_END
}
int Diana_Call_xabort(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    // outside a transaction: a nop
    pDianaContext; pCallContext; DI_PROC_END
}
int Diana_Call_xend(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    // outside a transaction: #GP(0)
    pDianaContext; pCallContext;
    return DI_GP;
}
int Diana_Call_xtest(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    // not in a transaction: ZF = 1, CF OF SF PF AF = 0
    pDianaContext;
    SET_FLAG_ZF;
    CLEAR_FLAG_CF;
    CLEAR_FLAG_OF;
    CLEAR_FLAG_SF;
    CLEAR_FLAG_PF;
    CLEAR_FLAG_AF;
    DI_PROC_END
}

int Diana_Call_rdpkru(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext;
    if ((DI_UINT32)GET_REG_ECX)
        return DI_GP;
    Di_SetEax(pCallContext, pCallContext->m_pkru);
    Di_SetEdx(pCallContext, 0);
    DI_PROC_END
}
int Diana_Call_wrpkru(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext;
    if ((DI_UINT32)GET_REG_ECX || (DI_UINT32)GET_REG_EDX)
        return DI_GP;
    pCallContext->m_pkru = (DI_UINT32)GET_REG_EAX;
    DI_PROC_END
}
