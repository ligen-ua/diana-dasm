#include "diana_processor_cmd_simd.h"
#include "diana_processor_cmd_fpu_internal.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

// SSE, SSE2, SSE3, SSE4.1, SSE4.2 instructions the processor didn't have: the packed compares, the horizontal and
// alternating arithmetic, the packed conversions, movmskps/pd, movhpd, the non-temporal moves, maskmovdqu/maskmovq,
// blends, dot products, insert/extract, pmovsx/zx, pmin/pmax, ptest, round, the string compares. The float work uses
// MXCSR (Diana_SSE_Status, Diana_SSE_Exceptions); an unmasked exception writes nothing. The operands are in the
// order of the decoder (the Intel order).

#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU

// ---------------------------------------------------------------------------------------------------------------
// operands

static int Di_IsMemoryOperand(DianaProcessor * pCallContext, int number)
{
    return pCallContext->m_result.linkedOperands[number].type == diana_index ||
           pCallContext->m_result.linkedOperands[number].type == diana_memory;
}

// an xmm (mm) register in full, memorySize bytes of a memory operand
static int Di_SimdGet(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                      DianaRegisterXMM_type * pValue, int memorySize)
{
    int size = Di_IsMemoryOperand(pCallContext, number) ? memorySize : 16;
    DIANA_MEMSET(pValue, 0, sizeof(*pValue));
    return DianaProcessor_XMM_SetGetOperandEx(pDianaContext, pCallContext, number, pValue, 0, &size, 0, 1);
}

// writes an xmm (mm) register in full, size bytes of memory
static int Di_SimdSet(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                      DianaRegisterXMM_type * pValue, int memorySize)
{
    int size = Di_IsMemoryOperand(pCallContext, number) ? memorySize : 16;
    return DianaProcessor_XMM_SetGetOperandEx(pDianaContext, pCallContext, number, pValue, 1, &size, 0, 1);
}

static int Di_SimdImm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                      OPERAND_SIZE * pImm)
{
    int size = 0;
    return DianaProcessor_SetGetOperand(pDianaContext, pCallContext, number, pImm, 0, &size, 0);
}

static OPERAND_SIZE Di_SizeMask(int size)
{
    return size >= 8 ? ~(OPERAND_SIZE)0 : (((OPERAND_SIZE)1 << (size * 8)) - 1);
}

// ---------------------------------------------------------------------------------------------------------------
// cmpps, cmpss, cmppd, cmpsd: the predicate imm8[2:0] (EQ LT LE UNORD NEQ NLT NLE ORD), all ones or zeros; LT, LE,
// NLT, NLE signal on a QNaN. The VEX forms: imm8[4:0], bits 3:0 the relation (EQ_UQ NGE NGT FALSE NEQ_OQ GE GT TRUE
// after the 8 of SSE), bit 4 swaps signaling and quiet.

static int Di_CmpPredicate(int relation, int predicate)
{
    int less = relation == float_relation_less, equal = relation == float_relation_equal;
    int greater = relation == float_relation_greater, unordered = relation == float_relation_unordered;
    switch (predicate & 15)
    {
    case 0: return equal;
    case 1: return less;
    case 2: return less || equal;
    case 3: return unordered;
    case 4: return !equal;
    case 5: return !less;
    case 6: return !(less || equal);
    case 7: return !unordered;
    case 8: return equal || unordered;
    case 9: return less || unordered;
    case 10: return !greater;
    case 11: return 0;
    case 12: return less || greater;
    case 13: return greater || equal;
    case 14: return greater;
    default: return 1;
    }
}

static int Di_CmpSignals(int predicate)
{
    static const char signals[16] = {0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0};
    return signals[predicate & 15] ^ ((predicate >> 4) & 1);
}

void Diana_SSE_Compare(void * pA, const void * pB, int elementSize, int count, int predicate,
                       float_status_t * pStatus)
{
    int i, relation;
    for (i = 0; i < count; ++i)
    {
        if (elementSize == 4)
        {
            DI_UINT32 * a = (DI_UINT32 *)pA + i;
            DI_UINT32 b = ((const DI_UINT32 *)pB)[i];
            relation = Di_CmpSignals(predicate) ? float32_compare(*a, b, pStatus)
                                                : float32_compare_quiet(*a, b, pStatus);
            *a = Di_CmpPredicate(relation, predicate) ? 0xffffffffUL : 0;
        }
        else
        {
            OPERAND_SIZE * a = (OPERAND_SIZE *)pA + i;
            OPERAND_SIZE b = ((const OPERAND_SIZE *)pB)[i];
            relation = Di_CmpSignals(predicate) ? float64_compare(*a, b, pStatus)
                                                : float64_compare_quiet(*a, b, pStatus);
            *a = Di_CmpPredicate(relation, predicate) ? ~(OPERAND_SIZE)0 : 0;
        }
    }
}

static int Di_SseCompare(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                         int count)
{
    DianaRegisterXMM_type dest, src;
    OPERAND_SIZE imm = 0;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 1, &src, count == 1 ? elementSize : 16));
    DI_CHECK(Di_SimdImm(pDianaContext, pCallContext, 2, &imm));
    Diana_SSE_Compare(&dest, &src, elementSize, count, (int)(imm & 7), &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_SimdSet(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

int Diana_Call_cmpps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseCompare(pDianaContext, pCallContext, 4, 4);
}
int Diana_Call_cmpss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseCompare(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_cmppd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseCompare(pDianaContext, pCallContext, 8, 2);
}
int Diana_Call_cmpsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    // the string instruction cmpsd is cmps
    return Di_SseCompare(pDianaContext, pCallContext, 8, 1);
}

// ---------------------------------------------------------------------------------------------------------------
// addsubps/pd: subtract in the even elements, add in the odd ones; haddps/pd, hsubps/pd: the pairs of dest, then of
// src

void Diana_SSE_Horizontal(DianaRegisterXMM_type * pResult, const DianaRegisterXMM_type * pA,
                          const DianaRegisterXMM_type * pB, int elementSize, DiHorizontal operation,
                          float_status_t * pStatus)
{
    int i;
    DIANA_MEMSET(pResult, 0, sizeof(*pResult));
    if (elementSize == 4)
    {
        if (operation == DI_ADDSUB)
        {
            for (i = 0; i < 4; ++i)
                pResult->u32[i] = i & 1 ? float32_add(pA->u32[i], pB->u32[i], pStatus)
                                        : float32_sub(pA->u32[i], pB->u32[i], pStatus);
        }
        else
        {
            for (i = 0; i < 4; ++i)
            {
                const DianaRegisterXMM_type * pPair = i < 2 ? pA : pB;
                float32 a = pPair->u32[(i & 1) * 2], b = pPair->u32[(i & 1) * 2 + 1];
                pResult->u32[i] = operation == DI_HADD ? float32_add(a, b, pStatus) : float32_sub(a, b, pStatus);
            }
        }
    }
    else
    {
        if (operation == DI_ADDSUB)
        {
            pResult->u64[0] = float64_sub(pA->u64[0], pB->u64[0], pStatus);
            pResult->u64[1] = float64_add(pA->u64[1], pB->u64[1], pStatus);
        }
        else
        {
            pResult->u64[0] = operation == DI_HADD ? float64_add(pA->u64[0], pA->u64[1], pStatus)
                                                   : float64_sub(pA->u64[0], pA->u64[1], pStatus);
            pResult->u64[1] = operation == DI_HADD ? float64_add(pB->u64[0], pB->u64[1], pStatus)
                                                   : float64_sub(pB->u64[0], pB->u64[1], pStatus);
        }
    }
}

static int Di_SseHorizontal(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                            DiHorizontal operation)
{
    DianaRegisterXMM_type dest, src, result;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 1, &src, 16));
    Diana_SSE_Horizontal(&result, &dest, &src, elementSize, operation, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_SimdSet(pDianaContext, pCallContext, 0, &result, 16));
    DI_PROC_END
}

int Diana_Call_addsubps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseHorizontal(pDianaContext, pCallContext, 4, DI_ADDSUB);
}
int Diana_Call_addsubpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseHorizontal(pDianaContext, pCallContext, 8, DI_ADDSUB);
}
int Diana_Call_haddps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseHorizontal(pDianaContext, pCallContext, 4, DI_HADD);
}
int Diana_Call_haddpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseHorizontal(pDianaContext, pCallContext, 8, DI_HADD);
}
int Diana_Call_hsubps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseHorizontal(pDianaContext, pCallContext, 4, DI_HSUB);
}
int Diana_Call_hsubpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseHorizontal(pDianaContext, pCallContext, 8, DI_HSUB);
}

// ---------------------------------------------------------------------------------------------------------------
// the packed conversions: the element types of the source and the destination, the number of elements; the rest of
// an xmm destination is zeroed (cvtpd2dq, cvtpd2ps) or kept (cvtpi2ps); an mm destination is 64 bits

static int Di_ElementSize(DiElement element)
{
    return element == DI_F64 ? 8 : 4;
}

void Diana_SSE_Convert(void * pResult, const void * pSrc, DiElement from, DiElement to, int count, int truncate,
                       float_status_t * pStatus)
{
    DI_UINT32 * r32 = (DI_UINT32 *)pResult;
    OPERAND_SIZE * r64 = (OPERAND_SIZE *)pResult;
    const DI_UINT32 * s32 = (const DI_UINT32 *)pSrc;
    const OPERAND_SIZE * s64 = (const OPERAND_SIZE *)pSrc;
    int i;
    for (i = 0; i < count; ++i)
    {
        if (from == DI_I32 && to == DI_F32)
            r32[i] = int32_to_float32((DI_INT32)s32[i], pStatus);
        else if (from == DI_I32 && to == DI_F64)
            r64[i] = int32_to_float64((DI_INT32)s32[i]);
        else if (from == DI_F32 && to == DI_I32)
            r32[i] = (DI_UINT32)(truncate ? float32_to_int32_round_to_zero(s32[i], pStatus)
                                          : float32_to_int32(s32[i], pStatus));
        else if (from == DI_F64 && to == DI_I32)
            r32[i] = (DI_UINT32)(truncate ? float64_to_int32_round_to_zero(s64[i], pStatus)
                                          : float64_to_int32(s64[i], pStatus));
        else if (from == DI_F32 && to == DI_F64)
            r64[i] = float32_to_float64(s32[i], pStatus);
        else
            r32[i] = float64_to_float32(s64[i], pStatus);
    }
}

static int Di_SseConvert(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiElement from,
                         DiElement to, int count, int truncate, int keepRest)
{
    DianaRegisterXMM_type dest, src, result;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 1, &src, Di_ElementSize(from) * count));
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 0, &dest, 16));
    result = keepRest ? dest : src;
    if (!keepRest)
        DIANA_MEMSET(&result, 0, sizeof(result));
    Diana_SSE_Convert(&result, &src, from, to, count, truncate, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_SimdSet(pDianaContext, pCallContext, 0, &result, 16));
    DI_PROC_END
}

int Diana_Call_cvtdq2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_I32, DI_F32, 4, 0, 0);
}
int Diana_Call_cvtdq2pd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_I32, DI_F64, 2, 0, 0);
}
int Diana_Call_cvtps2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F32, DI_I32, 4, 0, 0);
}
int Diana_Call_cvttps2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F32, DI_I32, 4, 1, 0);
}
int Diana_Call_cvtpd2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F64, DI_I32, 2, 0, 0);
}
int Diana_Call_cvttpd2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F64, DI_I32, 2, 1, 0);
}
int Diana_Call_cvtps2pd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F32, DI_F64, 2, 0, 0);
}
int Diana_Call_cvtpd2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F64, DI_F32, 2, 0, 0);
}
// mm destinations: 64 bits (the operand layer)
int Diana_Call_cvtps2pi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F32, DI_I32, 2, 0, 0);
}
int Diana_Call_cvttps2pi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F32, DI_I32, 2, 1, 0);
}
int Diana_Call_cvtpd2pi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F64, DI_I32, 2, 0, 0);
}
int Diana_Call_cvttpd2pi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_F64, DI_I32, 2, 1, 0);
}
// mm/m64 sources: cvtpi2ps keeps the high half of xmm, cvtpi2pd writes all of it
int Diana_Call_cvtpi2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_I32, DI_F32, 2, 0, 1);
}
int Diana_Call_cvtpi2pd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseConvert(pDianaContext, pCallContext, DI_I32, DI_F64, 2, 0, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// round*: imm8[1:0] the rounding mode, imm8[2] MXCSR's instead, imm8[3] no PE

void Diana_SSE_Round(void * pResult, const void * pSrc, int elementSize, int count, int imm,
                     float_status_t * pStatus)
{
    int i;
    if (!(imm & 4))
        pStatus->float_rounding_mode = imm & 3;
    for (i = 0; i < count; ++i)
    {
        if (elementSize == 4)
            ((DI_UINT32 *)pResult)[i] = float32_round_to_int(((const DI_UINT32 *)pSrc)[i], pStatus);
        else
            ((OPERAND_SIZE *)pResult)[i] = float64_round_to_int(((const OPERAND_SIZE *)pSrc)[i], pStatus);
    }
    if (imm & 8)
        pStatus->float_exception_flags &= ~float_flag_inexact;
}

static int Di_SseRound(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                       int count)
{
    DianaRegisterXMM_type dest, src;
    OPERAND_SIZE imm = 0;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 1, &src, count == 1 ? elementSize : 16));
    DI_CHECK(Di_SimdImm(pDianaContext, pCallContext, 2, &imm));
    if (count > 1)
        dest = src;
    Diana_SSE_Round(&dest, &src, elementSize, count, (int)imm, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_SimdSet(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

int Diana_Call_roundps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseRound(pDianaContext, pCallContext, 4, 4);
}
int Diana_Call_roundpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseRound(pDianaContext, pCallContext, 8, 2);
}
int Diana_Call_roundss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseRound(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_roundsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseRound(pDianaContext, pCallContext, 8, 1);
}

// ---------------------------------------------------------------------------------------------------------------
// dpps, dppd: the products (dest * src) of the elements imm8[7:4] selects (+0 for the others), summed in pairs, the
// sum to the elements imm8[3:0] selects (+0 to the others). Every lane adds in its own order, the CPU's (it shows in
// the NaN a lane returns): dpps lane L (p[X] + p[X^1]) + (p[X^2] + p[X^3]) with X = L^1, dppd lane L p[L] + p[L^1].

void Diana_SSE_DotProduct(DianaRegisterXMM_type * pA, const DianaRegisterXMM_type * pB, int elementSize, int imm,
                          float_status_t * pStatus)
{
    int i;
    if (elementSize == 4)
    {
        float32 products[4], sum;
        for (i = 0; i < 4; ++i)
            products[i] = (imm >> (4 + i)) & 1 ? float32_mul(pA->u32[i], pB->u32[i], pStatus) : 0;
        for (i = 0; i < 4; ++i)
        {
            int x = i ^ 1;
            sum = float32_add(float32_add(products[x], products[x ^ 1], pStatus),
                              float32_add(products[x ^ 2], products[x ^ 3], pStatus), pStatus);
            pA->u32[i] = (imm >> i) & 1 ? sum : 0;
        }
    }
    else
    {
        float64 products[2], sum;
        for (i = 0; i < 2; ++i)
            products[i] = (imm >> (4 + i)) & 1 ? float64_mul(pA->u64[i], pB->u64[i], pStatus) : 0;
        for (i = 0; i < 2; ++i)
        {
            sum = float64_add(products[i], products[i ^ 1], pStatus);
            pA->u64[i] = (imm >> i) & 1 ? sum : 0;
        }
    }
}

static int Di_SseDotProduct(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterXMM_type dest, src;
    OPERAND_SIZE imm = 0;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 1, &src, 16));
    DI_CHECK(Di_SimdImm(pDianaContext, pCallContext, 2, &imm));
    Diana_SSE_DotProduct(&dest, &src, elementSize, (int)imm, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_SimdSet(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

int Diana_Call_dpps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseDotProduct(pDianaContext, pCallContext, 4);
}

int Diana_Call_dppd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_SseDotProduct(pDianaContext, pCallContext, 8);
}

// ---------------------------------------------------------------------------------------------------------------
// movmskps, movmskpd: the sign bits to a general register (zero-extended)

static int Di_MoveMask(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterXMM_type src;
    int i;
    DI_DEF_LOCAL(dest);
    oldDestValue;
    DI_MEM_GET_DEST_SIZE(dest);
    DI_CHECK(Di_SimdGet(pDianaContext, pCallContext, 1, &src, 16));
    for (i = 0; i < 16 / elementSize; ++i)
        if (src.u8[(i + 1) * elementSize - 1] & 0x80)
            dest |= (OPERAND_SIZE)1 << i;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_movmskps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_MoveMask(pDianaContext, pCallContext, 4);
}
int Diana_Call_movmskpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_MoveMask(pDianaContext, pCallContext, 8);
}

#else /* !DIANA_PROCESSOR_USE_SOFTFLOAT_FPU */

#define DIANA_SIMD_FLOAT_STUB(name) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ pDianaContext; pCallContext; return DI_UNSUPPORTED_COMMAND; }

DIANA_SIMD_FLOAT_STUB(cmpps) DIANA_SIMD_FLOAT_STUB(cmpss) DIANA_SIMD_FLOAT_STUB(cmppd) DIANA_SIMD_FLOAT_STUB(cmpsd)
DIANA_SIMD_FLOAT_STUB(addsubps) DIANA_SIMD_FLOAT_STUB(addsubpd) DIANA_SIMD_FLOAT_STUB(haddps) DIANA_SIMD_FLOAT_STUB(haddpd)
DIANA_SIMD_FLOAT_STUB(hsubps) DIANA_SIMD_FLOAT_STUB(hsubpd) DIANA_SIMD_FLOAT_STUB(cvtdq2ps) DIANA_SIMD_FLOAT_STUB(cvtdq2pd)
DIANA_SIMD_FLOAT_STUB(cvtps2dq) DIANA_SIMD_FLOAT_STUB(cvttps2dq) DIANA_SIMD_FLOAT_STUB(cvtpd2dq) DIANA_SIMD_FLOAT_STUB(cvttpd2dq)
DIANA_SIMD_FLOAT_STUB(cvtps2pd) DIANA_SIMD_FLOAT_STUB(cvtpd2ps) DIANA_SIMD_FLOAT_STUB(cvtps2pi) DIANA_SIMD_FLOAT_STUB(cvttps2pi)
DIANA_SIMD_FLOAT_STUB(cvtpd2pi) DIANA_SIMD_FLOAT_STUB(cvttpd2pi) DIANA_SIMD_FLOAT_STUB(cvtpi2ps) DIANA_SIMD_FLOAT_STUB(cvtpi2pd)
DIANA_SIMD_FLOAT_STUB(roundps) DIANA_SIMD_FLOAT_STUB(roundpd) DIANA_SIMD_FLOAT_STUB(roundss) DIANA_SIMD_FLOAT_STUB(roundsd)
DIANA_SIMD_FLOAT_STUB(dpps) DIANA_SIMD_FLOAT_STUB(dppd) DIANA_SIMD_FLOAT_STUB(movmskps) DIANA_SIMD_FLOAT_STUB(movmskpd)

#endif /* DIANA_PROCESSOR_USE_SOFTFLOAT_FPU */

// ---------------------------------------------------------------------------------------------------------------
// moves

static int Di_SimdGetAny(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                         DianaRegisterXMM_type * pValue, int memorySize)
{
    int size = (pCallContext->m_result.linkedOperands[number].type == diana_index ||
                pCallContext->m_result.linkedOperands[number].type == diana_memory) ? memorySize : 16;
    DIANA_MEMSET(pValue, 0, sizeof(*pValue));
    return DianaProcessor_XMM_SetGetOperandEx(pDianaContext, pCallContext, number, pValue, 0, &size, 0, 1);
}

static int Di_SimdSetAny(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                         DianaRegisterXMM_type * pValue, int memorySize)
{
    int size = (pCallContext->m_result.linkedOperands[number].type == diana_index ||
                pCallContext->m_result.linkedOperands[number].type == diana_memory) ? memorySize : 16;
    return DianaProcessor_XMM_SetGetOperandEx(pDianaContext, pCallContext, number, pValue, 1, &size, 0, 1);
}

static int Di_SimdImmAny(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                         OPERAND_SIZE * pImm)
{
    int size = 0;
    return DianaProcessor_SetGetOperand(pDianaContext, pCallContext, number, pImm, 0, &size, 0);
}

// movhpd: the high double of xmm from m64 or to m64
int Diana_Call_movhpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src;
    if (pCallContext->m_result.linkedOperands[0].type != diana_register)
    {
        DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
        dest.u64[0] = src.u64[1];
        dest.u64[1] = 0;
        DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 8));
        DI_PROC_END
    }
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 8));
    dest.u64[1] = src.u64[0];
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

// movntq m64, mm; movntdqa xmm, m128
int Diana_Call_movntq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type value;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &value, 8));
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &value, 8));
    DI_PROC_END
}

int Diana_Call_movntdqa(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type value;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &value, 16));
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &value, 16));
    DI_PROC_END
}

// maskmovdqu xmm1, xmm2; maskmovq mm1, mm2: the bytes of the first register whose byte in the second has bit 7 set
// to DS:[rDI] (a segment prefix replaces DS, the address size picks rDI, eDI)
static int Di_MaskMove(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int size)
{
    DianaRegisterXMM_type data, mask;
    DianaUnifiedRegister segReg = reg_DS, addressReg = reg_RDI;
    OPERAND_SIZE address = 0, selector = 0;
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &data, size));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &mask, size));
    if (pDianaContext->currentCmd_sreg != reg_none)
        segReg = pDianaContext->currentCmd_sreg;
    switch (pDianaContext->iCurrentCmd_addressSize)
    {
    case DIANA_MODE64:
        addressReg = reg_RDI;
        break;
    case DIANA_MODE32:
        addressReg = reg_EDI;
        break;
    default:
        addressReg = reg_DI;
        break;
    }
    address = DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, addressReg));
    selector = DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, segReg));
    for (i = 0; i < size; ++i)
    {
        OPERAND_SIZE byte = data.u8[i];
        if (!(mask.u8[i] & 0x80))
            continue;
        DI_CHECK(DianaProcessor_SetMemValue(pCallContext, selector,
                                            (address + i) & Di_SizeMask(pDianaContext->iCurrentCmd_addressSize),
                                            1, &byte, 0, segReg));
    }
    DI_PROC_END
}

int Diana_Call_maskmovdqu(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_MaskMove(pDianaContext, pCallContext, 16);
}
int Diana_Call_maskmovq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_MaskMove(pDianaContext, pCallContext, 8);
}

// ---------------------------------------------------------------------------------------------------------------
// SSE4.1 blends, insert, extract

// blendps, blendpd, pblendw: the elements imm8 selects from src
static int Di_Blend(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterXMM_type dest, src;
    OPERAND_SIZE imm = 0;
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    DI_CHECK(Di_SimdImmAny(pDianaContext, pCallContext, 2, &imm));
    for (i = 0; i < 16 / elementSize; ++i)
        if ((imm >> i) & 1)
            DIANA_MEMCPY(dest.u8 + i * elementSize, src.u8 + i * elementSize, elementSize);
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

int Diana_Call_blendps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Blend(pDianaContext, pCallContext, 4);
}
int Diana_Call_blendpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Blend(pDianaContext, pCallContext, 8);
}
int Diana_Call_pblendw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Blend(pDianaContext, pCallContext, 2);
}

// extractps r/m32, xmm, imm8: the dword imm8[1:0]
int Diana_Call_extractps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type src;
    OPERAND_SIZE imm = 0;
    DI_DEF_LOCAL(dest);
    oldDestValue;
    DI_MEM_GET_DEST_SIZE(dest);
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    DI_CHECK(Di_SimdImmAny(pDianaContext, pCallContext, 2, &imm));
    dest = src.u32[imm & 3];
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// pextrb r32/m8, pextrd r/m32, pextrq r/m64 (REX.W), xmm, imm8: the element zero-extended to a register
int Diana_Call_pextrb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type src;
    OPERAND_SIZE imm = 0;
    DI_DEF_LOCAL(dest);
    oldDestValue;
    DI_MEM_GET_DEST_SIZE(dest);
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    DI_CHECK(Di_SimdImmAny(pDianaContext, pCallContext, 2, &imm));
    dest = src.u8[imm & 15];
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_pextrd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type src;
    OPERAND_SIZE imm = 0;
    DI_DEF_LOCAL(dest);
    oldDestValue;
    DI_MEM_GET_DEST_SIZE(dest);
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    DI_CHECK(Di_SimdImmAny(pDianaContext, pCallContext, 2, &imm));
    dest = dest_size == 8 ? src.u64[imm & 1] : src.u32[imm & 3];
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// pinsrb xmm, r32/m8, imm8; pinsrd xmm, r/m32, imm8; pinsrq (REX.W)
static int Di_Insert(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterXMM_type dest;
    OPERAND_SIZE imm = 0;
    DI_DEF_LOCAL(src);
    oldDestValue;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_MEM_GET_SRC(src);
    DI_CHECK(Di_SimdImmAny(pDianaContext, pCallContext, 2, &imm));
    if (elementSize == 4 && src_size == 8)
        elementSize = 8;
    imm &= (16 / elementSize) - 1;
    DIANA_MEMCPY(dest.u8 + imm * elementSize, &src, elementSize);
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

int Diana_Call_pinsrb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Insert(pDianaContext, pCallContext, 1);
}
int Diana_Call_pinsrd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Insert(pDianaContext, pCallContext, 4);
}

// insertps xmm1, xmm2/m32, imm8: the dword imm8[7:6] of xmm2 (or m32) to the dword imm8[5:4], the dwords of
// imm8[3:0] zeroed
int Diana_Call_insertps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src;
    OPERAND_SIZE imm = 0;
    DI_UINT32 value = 0;
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 4));
    DI_CHECK(Di_SimdImmAny(pDianaContext, pCallContext, 2, &imm));
    value = pCallContext->m_result.linkedOperands[1].type == diana_register ? src.u32[(imm >> 6) & 3] : src.u32[0];
    dest.u32[(imm >> 4) & 3] = value;
    for (i = 0; i < 4; ++i)
        if ((imm >> i) & 1)
            dest.u32[i] = 0;
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

// ---------------------------------------------------------------------------------------------------------------
// SSE4.1 integer

// pmovsx*, pmovzx*: the low elements of the source sign- or zero-extended
static int Di_MoveExtend(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int fromSize,
                         int toSize, int isSigned)
{
    DianaRegisterXMM_type dest, src;
    int i, count = 16 / toSize;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, count * fromSize));
    DIANA_MEMSET(&dest, 0, sizeof(dest));
    for (i = 0; i < count; ++i)
    {
        OPERAND_SIZE value = 0;
        DIANA_MEMCPY(&value, src.u8 + i * fromSize, fromSize);
        if (isSigned && (value >> (fromSize * 8 - 1)) & 1)
            value |= ~Di_SizeMask(fromSize);
        DIANA_MEMCPY(dest.u8 + i * toSize, &value, toSize);
    }
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

#define DI_MOVE_EXTEND(name, from, to, isSigned) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_MoveExtend(pDianaContext, pCallContext, from, to, isSigned); \
}

DI_MOVE_EXTEND(pmovsxbw, 1, 2, 1)
DI_MOVE_EXTEND(pmovsxbd, 1, 4, 1)
DI_MOVE_EXTEND(pmovsxwd, 2, 4, 1)
DI_MOVE_EXTEND(pmovsxwq, 2, 8, 1)
DI_MOVE_EXTEND(pmovsxdq, 4, 8, 1)
DI_MOVE_EXTEND(pmovzxbw, 1, 2, 0)
DI_MOVE_EXTEND(pmovzxbd, 1, 4, 0)
DI_MOVE_EXTEND(pmovzxwd, 2, 4, 0)
DI_MOVE_EXTEND(pmovzxwq, 2, 8, 0)
DI_MOVE_EXTEND(pmovzxdq, 4, 8, 0)

// pminsb ... pmaxud: elementwise minimum or maximum
static int Di_MinMax(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                     int isSigned, int maximum)
{
    DianaRegisterXMM_type dest, src;
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    for (i = 0; i < 16 / elementSize; ++i)
    {
        OPERAND_SIZE a = 0, b = 0;
        int less;
        DIANA_MEMCPY(&a, dest.u8 + i * elementSize, elementSize);
        DIANA_MEMCPY(&b, src.u8 + i * elementSize, elementSize);
        if (isSigned)
        {
            OPERAND_SIZE sign = (OPERAND_SIZE)1 << (elementSize * 8 - 1);
            less = (OPERAND_SIZE_SIGNED)((a ^ sign) - sign) < (OPERAND_SIZE_SIGNED)((b ^ sign) - sign);
        }
        else
        {
            less = a < b;
        }
        if (maximum ? less : !less && a != b)
            DIANA_MEMCPY(dest.u8 + i * elementSize, &b, elementSize);
    }
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

#define DI_MIN_MAX(name, size, isSigned, maximum) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_MinMax(pDianaContext, pCallContext, size, isSigned, maximum); \
}

DI_MIN_MAX(pminsb, 1, 1, 0)
DI_MIN_MAX(pminsd, 4, 1, 0)
DI_MIN_MAX(pminuw, 2, 0, 0)
DI_MIN_MAX(pminud, 4, 0, 0)
DI_MIN_MAX(pmaxsb, 1, 1, 1)
DI_MIN_MAX(pmaxsd, 4, 1, 1)
DI_MIN_MAX(pmaxuw, 2, 0, 1)
DI_MIN_MAX(pmaxud, 4, 0, 1)

// pcmpeqq, pcmpgtq (signed): all ones or zeros
static int Di_CompareQ(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int greater)
{
    DianaRegisterXMM_type dest, src;
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    for (i = 0; i < 2; ++i)
    {
        int result = greater ? dest.s64[i] > src.s64[i] : dest.u64[i] == src.u64[i];
        dest.u64[i] = result ? ~(OPERAND_SIZE)0 : 0;
    }
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

int Diana_Call_pcmpeqq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_CompareQ(pDianaContext, pCallContext, 0);
}
int Diana_Call_pcmpgtq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_CompareQ(pDianaContext, pCallContext, 1);
}

// pmuldq: the signed products of dwords 0 and 2 as qwords; pmulld: the low dwords of the signed products
int Diana_Call_pmuldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    dest.s64[0] = (OPERAND_SIZE_SIGNED)dest.s32[0] * (OPERAND_SIZE_SIGNED)src.s32[0];
    dest.s64[1] = (OPERAND_SIZE_SIGNED)dest.s32[2] * (OPERAND_SIZE_SIGNED)src.s32[2];
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

int Diana_Call_pmulld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src;
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    for (i = 0; i < 4; ++i)
        dest.u32[i] = (DI_UINT32)((OPERAND_SIZE)dest.u32[i] * (OPERAND_SIZE)src.u32[i]);
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

// packusdw: signed dwords to unsigned words with saturation, dest then src
int Diana_Call_packusdw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src, result;
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    for (i = 0; i < 8; ++i)
    {
        DI_INT32 value = i < 4 ? dest.s32[i] : src.s32[i - 4];
        result.u16[i] = (DI_UINT16)(value < 0 ? 0 : value > 0xffff ? 0xffff : value);
    }
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &result, 16));
    DI_PROC_END
}

// phminposuw: the minimum unsigned word in word 0, its lowest index in bits 18:16, the rest 0
int Diana_Call_phminposuw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src;
    int i, index = 0;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    for (i = 1; i < 8; ++i)
        if (src.u16[i] < src.u16[index])
            index = i;
    DIANA_MEMSET(&dest, 0, sizeof(dest));
    dest.u16[0] = src.u16[index];
    dest.u16[1] = (DI_UINT16)index;
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

// mpsadbw: eight sums of absolute differences of 4 bytes, the dest bytes from imm8[2] * 4 sliding, the src block
// imm8[1:0] * 4
int Diana_Call_mpsadbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src, result;
    OPERAND_SIZE imm = 0;
    int i, k, destOffset, srcOffset;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    DI_CHECK(Di_SimdImmAny(pDianaContext, pCallContext, 2, &imm));
    destOffset = (int)((imm >> 2) & 1) * 4;
    srcOffset = (int)(imm & 3) * 4;
    for (i = 0; i < 8; ++i)
    {
        int sum = 0;
        for (k = 0; k < 4; ++k)
        {
            int difference = (int)dest.u8[destOffset + i + k] - (int)src.u8[srcOffset + k];
            sum += difference < 0 ? -difference : difference;
        }
        result.u16[i] = (DI_UINT16)sum;
    }
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &result, 16));
    DI_PROC_END
}

// ptest: ZF for dest AND src all 0, CF for src AND NOT dest all 0, AF OF PF SF cleared
int Diana_Call_ptest(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    CLEAR_FLAG_AF; CLEAR_FLAG_OF; CLEAR_FLAG_PF; CLEAR_FLAG_SF; CLEAR_FLAG_ZF; CLEAR_FLAG_CF;
    if (!((dest.u64[0] & src.u64[0]) | (dest.u64[1] & src.u64[1])))
        SET_FLAG_ZF;
    if (!((~dest.u64[0] & src.u64[0]) | (~dest.u64[1] & src.u64[1])))
        SET_FLAG_CF;
    DI_PROC_END
}

// ---------------------------------------------------------------------------------------------------------------
// SSE4.2 string compares: pcmpestri, pcmpestrm (the lengths in eAX, eDX; rAX, rDX with REX.W), pcmpistri,
// pcmpistrm (the lengths to the first 0 element). imm8[1:0] the element type (unsigned or signed bytes or words),
// imm8[3:2] the aggregation (equal any, ranges, equal each, equal ordered), imm8[5:4] the polarity, imm8[6] the
// most significant index (I) or the byte mask (M). ECX (the index) or xmm0 (the mask); CF: a result bit set, ZF:
// the second string ends in it, SF: the first one does, OF: result bit 0, AF PF cleared.

static int Di_StringElement(const DianaRegisterXMM_type * pValue, int i, int words, int isSigned)
{
    if (words)
        return isSigned ? (int)pValue->s16[i] : (int)pValue->u16[i];
    return isSigned ? (int)pValue->s8[i] : (int)pValue->u8[i];
}

static int Di_ImplicitLength(const DianaRegisterXMM_type * pValue, int count, int words)
{
    int i;
    for (i = 0; i < count; ++i)
        if (!(words ? pValue->u16[i] : pValue->u8[i]))
            return i;
    return count;
}

static int Di_ExplicitLength(OPERAND_SIZE value, int size, int count)
{
    OPERAND_SIZE_SIGNED length = size == 8 ? (OPERAND_SIZE_SIGNED)value : (OPERAND_SIZE_SIGNED)(DI_INT32)value;
    // the bounds before the negation: -INT64_MIN is undefined (the optimizer dropped a check after it)
    if (length > count || length < -count)
        return count;
    return (int)(length < 0 ? -length : length);
}

static int Di_StringCompare(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int explicitLength,
                            int index)
{
    DianaRegisterXMM_type a, b;
    OPERAND_SIZE imm = 0;
    int words, isSigned, aggregation, polarity, count, lengthA, lengthB, i, j, k;
    unsigned result = 0, valid = 0;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &a, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &b, 16));
    DI_CHECK(Di_SimdImmAny(pDianaContext, pCallContext, 2, &imm));
    words = (int)(imm & 1);
    isSigned = (int)((imm >> 1) & 1);
    aggregation = (int)((imm >> 2) & 3);
    polarity = (int)((imm >> 4) & 3);
    count = words ? 8 : 16;
    if (explicitLength)
    {
        int size = pCallContext->m_result.iOperandSize == 8 ? 8 : 4;
        lengthA = Di_ExplicitLength(GET_REG_RAX, size, count);
        lengthB = Di_ExplicitLength(GET_REG_RDX, size, count);
    }
    else
    {
        lengthA = Di_ImplicitLength(&a, count, words);
        lengthB = Di_ImplicitLength(&b, count, words);
    }
    for (j = 0; j < count; ++j)
    {
        int bit = 0;
        int bValid = j < lengthB;
        switch (aggregation)
        {
        case 0:
            // equal any: b[j] is one of a[0 .. lengthA)
            for (i = 0; i < lengthA && bValid; ++i)
                if (Di_StringElement(&a, i, words, isSigned) == Di_StringElement(&b, j, words, isSigned))
                    bit = 1;
            break;
        case 1:
            // ranges: a[i] <= b[j] <= a[i + 1] for an even i
            for (i = 0; i + 1 < count && bValid; i += 2)
            {
                if (i >= lengthA || i + 1 >= lengthA)
                    break;
                if (Di_StringElement(&a, i, words, isSigned) <= Di_StringElement(&b, j, words, isSigned) &&
                    Di_StringElement(&b, j, words, isSigned) <= Di_StringElement(&a, i + 1, words, isSigned))
                    bit = 1;
            }
            break;
        case 2:
            // equal each: both invalid true, one invalid false
            if (j >= lengthA && !bValid)
                bit = 1;
            else if (j < lengthA && bValid)
                bit = Di_StringElement(&a, j, words, isSigned) == Di_StringElement(&b, j, words, isSigned);
            break;
        default:
            // equal ordered: a from b[j] on; an invalid a element true, an invalid b element with a valid a false
            bit = 1;
            for (i = 0, k = j; i < count - j && k < count; ++i, ++k)
            {
                if (i >= lengthA)
                    break;
                if (k >= lengthB)
                {
                    bit = 0;
                    break;
                }
                if (Di_StringElement(&a, i, words, isSigned) != Di_StringElement(&b, k, words, isSigned))
                {
                    bit = 0;
                    break;
                }
            }
            break;
        }
        result |= (unsigned)bit << j;
    }
    valid = (1u << lengthB) - 1;
    switch (polarity)
    {
    case 1:
        result = ~result;
        break;
    case 3:
        result ^= valid;
        break;
    default:
        break;
    }
    result &= (count == 16 ? 0xffffu : 0xffu);

    CLEAR_FLAG_AF; CLEAR_FLAG_PF; CLEAR_FLAG_CF; CLEAR_FLAG_ZF; CLEAR_FLAG_SF; CLEAR_FLAG_OF;
    if (result)
        SET_FLAG_CF;
    if (lengthB < count)
        SET_FLAG_ZF;
    if (lengthA < count)
        SET_FLAG_SF;
    if (result & 1)
        SET_FLAG_OF;

    if (index)
    {
        // ECX: the least or the most significant set bit, count when none
        int position = count;
        if (result)
        {
            if (imm & 0x40)
                for (position = count - 1; !((result >> position) & 1); --position)
                {
                }
            else
                for (position = 0; !((result >> position) & 1); ++position)
                {
                }
        }
        SET_REG_ECX(position);
    }
    else
    {
        // xmm0: the bits, or each element all ones or zeros
        DianaRegisterXMM_type mask;
        DIANA_MEMSET(&mask, 0, sizeof(mask));
        if (imm & 0x40)
        {
            for (i = 0; i < count; ++i)
                if ((result >> i) & 1)
                    DIANA_MEMSET(mask.u8 + i * (words ? 2 : 1), 0xff, words ? 2 : 1);
        }
        else
        {
            mask.u16[0] = (DI_UINT16)result;
        }
        DianaProcessor_XMM_SetValue(pCallContext, reg_XMM0, DianaProcessor_QueryReg(pCallContext, reg_XMM0), &mask);
    }
    DI_PROC_END
}

int Diana_Call_pcmpestri(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_StringCompare(pDianaContext, pCallContext, 1, 1);
}
int Diana_Call_pcmpestrm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_StringCompare(pDianaContext, pCallContext, 1, 0);
}
int Diana_Call_pcmpistri(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_StringCompare(pDianaContext, pCallContext, 0, 1);
}
int Diana_Call_pcmpistrm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_StringCompare(pDianaContext, pCallContext, 0, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// SSSE3 (mm and xmm forms), the SSE4.1 rest: pblendvb, blendvps, blendvpd (the mask: operand 2, the implicit xmm0),
// pmovsxbq, pmovzxbq

// the bytes of an SSSE3 operation: 8 with an mm destination, 16 with xmm
static int Di_SimdBytes(DianaProcessor * pCallContext)
{
    const DianaLinkedOperand * pDest = pCallContext->m_result.linkedOperands;
    return pDest->type == diana_register && pDest->value.recognizedRegister >= reg_MM0 &&
           pDest->value.recognizedRegister <= reg_MM7 ? 8 : 16;
}

static DI_INT16 Di_SaturateWord(DI_INT32 value)
{
    return (DI_INT16)(value > 32767 ? 32767 : value < -32768 ? -32768 : value);
}

typedef enum
{
    DI_PSHUFB, DI_PHADDW, DI_PHADDD, DI_PHADDSW, DI_PMADDUBSW, DI_PHSUBW, DI_PHSUBD, DI_PHSUBSW,
    DI_PSIGNB, DI_PSIGNW, DI_PSIGND, DI_PMULHRSW, DI_PABSB, DI_PABSW, DI_PABSD
} DiSsse3;

// dest op src; the horizontal forms: the pairs of dest, then of src
static int Di_Ssse3(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiSsse3 operation)
{
    DianaRegisterXMM_type a, b, r;
    int n = Di_SimdBytes(pCallContext);
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &a, n));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &b, n));
    DIANA_MEMSET(&r, 0, sizeof(r));
    switch (operation)
    {
    case DI_PSHUFB:
        for (i = 0; i < n; ++i)
            r.u8[i] = (DI_CHAR)(b.u8[i] & 0x80 ? 0 : a.u8[b.u8[i] & (n - 1)]);
        break;
    case DI_PHADDW: case DI_PHSUBW: case DI_PHADDSW: case DI_PHSUBSW:
    {
        int half = n / 4;
        for (i = 0; i < half; ++i)
        {
            int k;
            for (k = 0; k < 2; ++k)
            {
                const DianaRegisterXMM_type * p = k ? &b : &a;
                DI_INT32 x = p->s16[2 * i], y = p->s16[2 * i + 1];
                DI_INT32 v = operation == DI_PHADDW || operation == DI_PHADDSW ? x + y : x - y;
                r.s16[k * half + i] = operation == DI_PHADDSW || operation == DI_PHSUBSW ? Di_SaturateWord(v)
                                                                                         : (DI_INT16)v;
            }
        }
        break;
    }
    case DI_PHADDD: case DI_PHSUBD:
    {
        int half = n / 8;
        for (i = 0; i < half; ++i)
        {
            r.u32[i] = operation == DI_PHADDD ? a.u32[2 * i] + a.u32[2 * i + 1] : a.u32[2 * i] - a.u32[2 * i + 1];
            r.u32[half + i] = operation == DI_PHADDD ? b.u32[2 * i] + b.u32[2 * i + 1] : b.u32[2 * i] - b.u32[2 * i + 1];
        }
        break;
    }
    case DI_PMADDUBSW:
        // the unsigned bytes of dest times the signed bytes of src, the pairs added with signed saturation
        for (i = 0; i < n / 2; ++i)
            r.s16[i] = Di_SaturateWord((DI_INT32)a.u8[2 * i] * b.s8[2 * i] +
                                       (DI_INT32)a.u8[2 * i + 1] * b.s8[2 * i + 1]);
        break;
    case DI_PMULHRSW:
        for (i = 0; i < n / 2; ++i)
            r.s16[i] = (DI_INT16)(((((DI_INT32)a.s16[i] * b.s16[i]) >> 14) + 1) >> 1);
        break;
    case DI_PSIGNB:
        for (i = 0; i < n; ++i)
            r.u8[i] = (DI_CHAR)(b.s8[i] < 0 ? 0 - a.u8[i] : b.s8[i] ? a.u8[i] : 0);
        break;
    case DI_PSIGNW:
        for (i = 0; i < n / 2; ++i)
            r.u16[i] = (DI_UINT16)(b.s16[i] < 0 ? 0 - a.u16[i] : b.s16[i] ? a.u16[i] : 0);
        break;
    case DI_PSIGND:
        for (i = 0; i < n / 4; ++i)
            r.u32[i] = b.s32[i] < 0 ? 0 - a.u32[i] : b.s32[i] ? a.u32[i] : 0;
        break;
    // pabs*: of src (-128 stays 0x80)
    case DI_PABSB:
        for (i = 0; i < n; ++i)
            r.u8[i] = (DI_CHAR)(b.s8[i] < 0 ? 0 - b.u8[i] : b.u8[i]);
        break;
    case DI_PABSW:
        for (i = 0; i < n / 2; ++i)
            r.u16[i] = (DI_UINT16)(b.s16[i] < 0 ? 0 - b.u16[i] : b.u16[i]);
        break;
    default:
        for (i = 0; i < n / 4; ++i)
            r.u32[i] = b.s32[i] < 0 ? 0 - b.u32[i] : b.u32[i];
        break;
    }
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &r, n));
    DI_PROC_END
}

#define DI_SSSE3(name, operation) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_Ssse3(pDianaContext, pCallContext, operation); \
}

DI_SSSE3(pshufb, DI_PSHUFB)
DI_SSSE3(phaddw, DI_PHADDW)
DI_SSSE3(phaddd, DI_PHADDD)
DI_SSSE3(phaddsw, DI_PHADDSW)
DI_SSSE3(pmaddubsw, DI_PMADDUBSW)
DI_SSSE3(phsubw, DI_PHSUBW)
DI_SSSE3(phsubd, DI_PHSUBD)
DI_SSSE3(phsubsw, DI_PHSUBSW)
DI_SSSE3(psignb, DI_PSIGNB)
DI_SSSE3(psignw, DI_PSIGNW)
DI_SSSE3(psignd, DI_PSIGND)
DI_SSSE3(pmulhrsw, DI_PMULHRSW)
DI_SSSE3(pabsb, DI_PABSB)
DI_SSSE3(pabsw, DI_PABSW)
DI_SSSE3(pabsd, DI_PABSD)

// palignr: dest:src (dest the high part) shifted right by imm8 bytes, the low part kept
int Diana_Call_palignr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type a, b, r;
    DI_CHAR temp[32];
    OPERAND_SIZE imm = 0;
    int n = Di_SimdBytes(pCallContext);
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &a, n));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &b, n));
    DI_CHECK(Di_SimdImmAny(pDianaContext, pCallContext, 2, &imm));
    DIANA_MEMCPY(temp, b.u8, n);
    DIANA_MEMCPY(temp + n, a.u8, n);
    DIANA_MEMSET(&r, 0, sizeof(r));
    for (i = 0; i < n; ++i)
        r.u8[i] = (DI_CHAR)((imm & 0xff) + i < (OPERAND_SIZE)(2 * n) ? temp[(imm & 0xff) + i] : 0);
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &r, n));
    DI_PROC_END
}

// pblendvb, blendvps, blendvpd: the elements of src whose mask element (operand 2: xmm0) has the sign bit set
static int Di_BlendVariable(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterXMM_type dest, src, mask;
    int i;
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 1, &src, 16));
    DI_CHECK(Di_SimdGetAny(pDianaContext, pCallContext, 2, &mask, 16));
    for (i = 0; i < 16 / elementSize; ++i)
        if (mask.u8[i * elementSize + elementSize - 1] & 0x80)
            DIANA_MEMCPY(dest.u8 + i * elementSize, src.u8 + i * elementSize, elementSize);
    DI_CHECK(Di_SimdSetAny(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

int Diana_Call_pblendvb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_BlendVariable(pDianaContext, pCallContext, 1);
}
int Diana_Call_blendvps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_BlendVariable(pDianaContext, pCallContext, 4);
}
int Diana_Call_blendvpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_BlendVariable(pDianaContext, pCallContext, 8);
}

DI_MOVE_EXTEND(pmovsxbq, 1, 8, 1)
DI_MOVE_EXTEND(pmovzxbq, 1, 8, 0)
