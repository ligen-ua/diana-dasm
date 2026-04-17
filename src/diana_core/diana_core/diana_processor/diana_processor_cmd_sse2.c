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
    count &= 0x7;
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
    count &= 0x7;
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

/*=============================================================================
 * Move quadword
 *===========================================================================*/
int Diana_Call_movq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    /* movq xmm, xmm/m64: zero-extends high quadword
       movq xmm/m64, xmm: stores low 64 bits */
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM_FPU(src)
    DI_MEM_GET_SRC_XMM(src)
    dest.u64[0] = src.u64[0];
    dest.u64[1] = 0;
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
 * MMX state (no-ops)
 *===========================================================================*/
int Diana_Call_emms(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}
int Diana_Call_femms(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}

/*=============================================================================
 * Scalar float32 operations
 *===========================================================================*/
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU

#define DI_DEF_LOCAL_XMM_F32(X) \
    DianaRegisterXMM_type X = {{0}}; \
    int X##_size = 4;

int Diana_Call_addss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u32[0] = float32_add(dest.u32[0], src.u32[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_subss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u32[0] = float32_sub(dest.u32[0], src.u32[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_mulss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u32[0] = float32_mul(dest.u32[0], src.u32[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_divss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u32[0] = float32_div(dest.u32[0], src.u32[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_sqrtss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u32[0] = float32_sqrt(src.u32[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_comiss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int result;
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        result = float32_compare(dest.u32[0], src.u32[0], &status);
        CLEAR_FLAG_OF; CLEAR_FLAG_SF; CLEAR_FLAG_AF;
        switch (result)
        {
        case float_relation_less:    CLEAR_FLAG_ZF; CLEAR_FLAG_PF; SET_FLAG_CF; break;
        case float_relation_equal:   SET_FLAG_ZF;   CLEAR_FLAG_PF; CLEAR_FLAG_CF; break;
        case float_relation_greater: CLEAR_FLAG_ZF; CLEAR_FLAG_PF; CLEAR_FLAG_CF; break;
        default: SET_FLAG_ZF; SET_FLAG_PF; SET_FLAG_CF; break;
        }
    }
    DI_PROC_END
}
int Diana_Call_ucomiss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int result;
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        result = float32_compare_quiet(dest.u32[0], src.u32[0], &status);
        CLEAR_FLAG_OF; CLEAR_FLAG_SF; CLEAR_FLAG_AF;
        switch (result)
        {
        case float_relation_less:    CLEAR_FLAG_ZF; CLEAR_FLAG_PF; SET_FLAG_CF; break;
        case float_relation_equal:   SET_FLAG_ZF;   CLEAR_FLAG_PF; CLEAR_FLAG_CF; break;
        case float_relation_greater: CLEAR_FLAG_ZF; CLEAR_FLAG_PF; CLEAR_FLAG_CF; break;
        default: SET_FLAG_ZF; SET_FLAG_PF; SET_FLAG_CF; break;
        }
    }
    DI_PROC_END
}
int Diana_Call_cvtsi2ss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL(src)
        DI_DEF_LOCAL_XMM_F32(dest)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        &oldDestValue;
        DI_MEM_GET_SRC(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u32[0] = int32_to_float32((DI_INT32)src, &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_cvtss2sd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(src)
        DI_DEF_LOCAL_XMM_FPU(dest)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u64[0] = float32_to_float64(src.u32[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_cvtss2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        &oldDestValue;
        DI_MEM_GET_SRC_XMM(src)
        dest = (OPERAND_SIZE)(DI_INT32)float32_to_int32(src.u32[0], &status);
        DI_MEM_SET_DEST(dest)
    }
    DI_PROC_END
}
int Diana_Call_cvttss2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        &oldDestValue;
        DI_MEM_GET_SRC_XMM(src)
        dest = (OPERAND_SIZE)(DI_INT32)float32_to_int32_round_to_zero(src.u32[0], &status);
        DI_MEM_SET_DEST(dest)
    }
    DI_PROC_END
}
int Diana_Call_maxss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u32[0] = float32_max(dest.u32[0], src.u32[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_minss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_F32(dest)
        DI_DEF_LOCAL_XMM_F32(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u32[0] = float32_min(dest.u32[0], src.u32[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}

/*=============================================================================
 * Packed float32 operations
 *===========================================================================*/
int Diana_Call_addps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 4; ++i)
            dest.u32[i] = float32_add(dest.u32[i], src.u32[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_subps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 4; ++i)
            dest.u32[i] = float32_sub(dest.u32[i], src.u32[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_mulps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 4; ++i)
            dest.u32[i] = float32_mul(dest.u32[i], src.u32[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_divps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 4; ++i)
            dest.u32[i] = float32_div(dest.u32[i], src.u32[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_sqrtps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 4; ++i)
            dest.u32[i] = float32_sqrt(src.u32[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_maxps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 4; ++i)
            dest.u32[i] = float32_max(dest.u32[i], src.u32[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_minps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 4; ++i)
            dest.u32[i] = float32_min(dest.u32[i], src.u32[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}

/*=============================================================================
 * Packed float64 operations
 *===========================================================================*/
int Diana_Call_addpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 2; ++i)
            dest.u64[i] = float64_add(dest.u64[i], src.u64[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_subpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 2; ++i)
            dest.u64[i] = float64_sub(dest.u64[i], src.u64[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_mulpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 2; ++i)
            dest.u64[i] = float64_mul(dest.u64[i], src.u64[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_divpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 2; ++i)
            dest.u64[i] = float64_div(dest.u64[i], src.u64[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_sqrtpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 2; ++i)
            dest.u64[i] = float64_sqrt(src.u64[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_maxpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 2; ++i)
            dest.u64[i] = float64_max(dest.u64[i], src.u64[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_minpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i;
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM(dest)
        DI_DEF_LOCAL_XMM(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        for (i = 0; i < 2; ++i)
            dest.u64[i] = float64_min(dest.u64[i], src.u64[i], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}

/*=============================================================================
 * Scalar float64 extras
 *===========================================================================*/
int Diana_Call_sqrtsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(dest)
        DI_DEF_LOCAL_XMM_FPU(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u64[0] = float64_sqrt(src.u64[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_ucomisd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(dest)
        DI_DEF_LOCAL_XMM_FPU(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int result;
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        result = float64_compare_quiet(dest.u64[0], src.u64[0], &status);
        CLEAR_FLAG_OF; CLEAR_FLAG_SF; CLEAR_FLAG_AF;
        switch (result)
        {
        case float_relation_less:    CLEAR_FLAG_ZF; CLEAR_FLAG_PF; SET_FLAG_CF; break;
        case float_relation_equal:   SET_FLAG_ZF;   CLEAR_FLAG_PF; CLEAR_FLAG_CF; break;
        case float_relation_greater: CLEAR_FLAG_ZF; CLEAR_FLAG_PF; CLEAR_FLAG_CF; break;
        default: SET_FLAG_ZF; SET_FLAG_PF; SET_FLAG_CF; break;
        }
    }
    DI_PROC_END
}
int Diana_Call_maxsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(dest)
        DI_DEF_LOCAL_XMM_FPU(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u64[0] = float64_max(dest.u64[0], src.u64[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_minsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(dest)
        DI_DEF_LOCAL_XMM_FPU(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u64[0] = float64_min(dest.u64[0], src.u64[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_cvtsi2sd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL(src)
        DI_DEF_LOCAL_XMM_FPU(dest)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        &oldDestValue;
        DI_MEM_GET_SRC(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u64[0] = int32_to_float64((DI_INT32)src);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
    (void)pDianaContext;
}
int Diana_Call_cvtsd2ss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(src)
        DI_DEF_LOCAL_XMM_F32(dest)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)
        dest.u32[0] = float64_to_float32(src.u64[0], &status);
        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
}
int Diana_Call_cvtsd2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DI_DEF_LOCAL(dest)
        DI_DEF_LOCAL_XMM_FPU(src)
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        &oldDestValue;
        DI_MEM_GET_SRC_XMM(src)
        dest = (OPERAND_SIZE)(DI_INT32)float64_to_int32(src.u64[0], &status);
        DI_MEM_SET_DEST(dest)
    }
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
DIANA_SSE_FLOAT_STUB(sqrtsd) DIANA_SSE_FLOAT_STUB(ucomisd) DIANA_SSE_FLOAT_STUB(maxsd)
DIANA_SSE_FLOAT_STUB(minsd) DIANA_SSE_FLOAT_STUB(cvtsi2sd) DIANA_SSE_FLOAT_STUB(cvtsd2ss)
DIANA_SSE_FLOAT_STUB(cvtsd2si)

#endif /* DIANA_PROCESSOR_USE_SOFTFLOAT_FPU */

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
 * ldmxcsr / stmxcsr (stmxcsr already exists; ldmxcsr = load MXCSR from mem) */
/*=============================================================================*/
int Diana_Call_ldmxcsr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    /* We store the rounding/exception bits in the FPU control word */
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        DI_UINT32 mxcsr = 0;
        OPERAND_SIZE doneBytes = 0;
        DI_CHECK(Diana_ReadRawBufferFromArgMem(pDianaContext, pCallContext, 0,
                                              &mxcsr, sizeof(mxcsr), &doneBytes, 0));
        /* Map relevant bits into the FPU control word */
        pCallContext->m_fpu.controlWord = (pCallContext->m_fpu.controlWord & ~0x0F3F)
                                        | (DI_UINT16)(mxcsr & 0x0F3F);
    }
    DI_PROC_END
#else
    pDianaContext; pCallContext;
    DI_PROC_END
#endif
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
