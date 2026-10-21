#include "diana_processor_cmd_avx2.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

// The integer instructions of AVX and AVX2 in their VEX.128 and VEX.256 forms: element cores (values in, values
// out: the vector length in bytes, the element size) and the handlers that read and write the operands. Most of them
// work per 128-bit lane; the cores take the whole vector and do the lanes. The legacy (non-VEX) forms have their own
// handlers (diana_processor_cmd_sse2.c, _simd.c); EVEX is meant to reuse these cores with a mask.

// ---------------------------------------------------------------------------------------------------------------
// elements

static OPERAND_SIZE Di_IntGetU(const DI_CHAR * p, int size)
{
    OPERAND_SIZE value = 0;
    DIANA_MEMCPY(&value, p, size);
    return value;
}

static OPERAND_SIZE_SIGNED Di_IntGetS(const DI_CHAR * p, int size)
{
    OPERAND_SIZE value = Di_IntGetU(p, size);
    if (size < 8 && (value >> (size * 8 - 1)) & 1)
        value |= ~(OPERAND_SIZE)0 << (size * 8);
    return (OPERAND_SIZE_SIGNED)value;
}

static void Di_IntSet(DI_CHAR * p, int size, OPERAND_SIZE value)
{
    DIANA_MEMCPY(p, &value, size);
}

static OPERAND_SIZE_SIGNED Di_IntSaturateS(OPERAND_SIZE_SIGNED value, int size)
{
    OPERAND_SIZE_SIGNED high = ((OPERAND_SIZE_SIGNED)1 << (size * 8 - 1)) - 1, low = -high - 1;
    return value > high ? high : value < low ? low : value;
}

static OPERAND_SIZE_SIGNED Di_IntSaturateU(OPERAND_SIZE_SIGNED value, int size)
{
    OPERAND_SIZE_SIGNED high = ((OPERAND_SIZE_SIGNED)1 << (size * 8)) - 1;
    return value > high ? high : value < 0 ? 0 : value;
}

// r = a op b, element by element (sizes 1, 2, 4, 8; the saturating and the multiplying ones up to 4)
void Diana_Int_Elementwise(void * r, const void * a, const void * b, int bytes, int size, DiIntOperation operation)
{
    DI_CHAR * pr = (DI_CHAR *)r;
    const DI_CHAR * pa = (const DI_CHAR *)a, * pb = (const DI_CHAR *)b;
    int i;
    for (i = 0; i < bytes; i += size)
    {
        OPERAND_SIZE ua = Di_IntGetU(pa + i, size), ub = Di_IntGetU(pb + i, size), result = 0;
        OPERAND_SIZE_SIGNED sa = Di_IntGetS(pa + i, size), sb = Di_IntGetS(pb + i, size);
        switch (operation)
        {
        case DI_INT_ADD: result = ua + ub; break;
        case DI_INT_SUB: result = ua - ub; break;
        case DI_INT_ADDS: result = (OPERAND_SIZE)Di_IntSaturateS(sa + sb, size); break;
        case DI_INT_SUBS: result = (OPERAND_SIZE)Di_IntSaturateS(sa - sb, size); break;
        case DI_INT_ADDUS: result = (OPERAND_SIZE)Di_IntSaturateU((OPERAND_SIZE_SIGNED)(ua + ub), size); break;
        case DI_INT_SUBUS: result = ua > ub ? ua - ub : 0; break;
        case DI_INT_AVG: result = (ua + ub + 1) >> 1; break;
        case DI_INT_CMPEQ: result = ua == ub ? ~(OPERAND_SIZE)0 : 0; break;
        case DI_INT_CMPGT: result = sa > sb ? ~(OPERAND_SIZE)0 : 0; break;
        case DI_INT_MIN: result = (OPERAND_SIZE)(sa < sb ? sa : sb); break;
        case DI_INT_MAX: result = (OPERAND_SIZE)(sa > sb ? sa : sb); break;
        case DI_INT_MINU: result = ua < ub ? ua : ub; break;
        case DI_INT_MAXU: result = ua > ub ? ua : ub; break;
        case DI_INT_MULLO: result = (OPERAND_SIZE)(sa * sb); break;
        case DI_INT_MULHI: result = (OPERAND_SIZE)((sa * sb) >> (size * 8)); break;
        case DI_INT_MULHU: result = (ua * ub) >> (size * 8); break;
        case DI_INT_MULHRS: result = (OPERAND_SIZE)((((sa * sb) >> 14) + 1) >> 1); break;
        case DI_INT_SIGN: result = sb < 0 ? 0 - ua : sb ? ua : 0; break;
        case DI_INT_AND: result = ua & ub; break;
        case DI_INT_ANDN: result = ~ua & ub; break;
        case DI_INT_OR: result = ua | ub; break;
        default: result = ua ^ ub; break;
        }
        Di_IntSet(pr + i, size, result);
    }
}

// pabs*: r = |a| (-2^(n-1) stays)
void Diana_Int_Abs(void * r, const void * a, int bytes, int size)
{
    int i;
    for (i = 0; i < bytes; i += size)
    {
        OPERAND_SIZE_SIGNED value = Di_IntGetS((const DI_CHAR *)a + i, size);
        Di_IntSet((DI_CHAR *)r + i, size, (OPERAND_SIZE)(value < 0 ? -value : value));
    }
}

// the shifts of every element by one count (psll, psrl, psra by xmm or imm8): a count above the element bits gives
// 0 (logical) or the sign (arithmetic)
void Diana_Int_Shift(void * r, const void * a, int bytes, int size, DiIntShift shift, OPERAND_SIZE count)
{
    int i, bits = size * 8;
    for (i = 0; i < bytes; i += size)
    {
        OPERAND_SIZE ua = Di_IntGetU((const DI_CHAR *)a + i, size), result;
        OPERAND_SIZE_SIGNED sa = Di_IntGetS((const DI_CHAR *)a + i, size);
        if (shift == DI_SHIFT_LEFT)
            result = count >= (OPERAND_SIZE)bits ? 0 : ua << count;
        else if (shift == DI_SHIFT_RIGHT)
            result = count >= (OPERAND_SIZE)bits ? 0 : ua >> count;
        else
            result = (OPERAND_SIZE)(sa >> (count >= (OPERAND_SIZE)bits ? bits - 1 : (int)count));
        Di_IntSet((DI_CHAR *)r + i, size, result);
    }
}

// vpsllv*, vpsrlv*, vpsrav*: the count of every element in b
void Diana_Int_ShiftVariable(void * r, const void * a, const void * b, int bytes, int size, DiIntShift shift)
{
    int i;
    for (i = 0; i < bytes; i += size)
        Diana_Int_Shift((DI_CHAR *)r + i, (const DI_CHAR *)a + i, size, size, shift,
                        Di_IntGetU((const DI_CHAR *)b + i, size));
}

// pack*: per lane, the elements of a then of b, saturated to half the size (signed or unsigned)
void Diana_Int_Pack(void * r, const void * a, const void * b, int bytes, int fromSize, int toUnsigned)
{
    DI_CHAR result[64];
    int lane, k, i, half = 16 / fromSize;
    for (lane = 0; lane < bytes; lane += 16)
        for (k = 0; k < 2; ++k)
            for (i = 0; i < half; ++i)
            {
                const DI_CHAR * p = (const DI_CHAR *)(k ? b : a) + lane + i * fromSize;
                OPERAND_SIZE_SIGNED v = Di_IntGetS(p, fromSize);
                v = toUnsigned ? Di_IntSaturateU(v, fromSize / 2) : Di_IntSaturateS(v, fromSize / 2);
                Di_IntSet(result + lane + (k * half + i) * (fromSize / 2), fromSize / 2, (OPERAND_SIZE)v);
            }
    DIANA_MEMCPY(r, result, bytes);
}

// punpckl*, punpckh*: per lane, the low (high) elements of a and b interleaved
void Diana_Int_Unpack(void * r, const void * a, const void * b, int bytes, int size, int high)
{
    DI_CHAR result[64];
    int lane, i, per = 16 / size;
    for (lane = 0; lane < bytes; lane += 16)
        for (i = 0; i < per / 2; ++i)
        {
            int from = lane + (i + (high ? per / 2 : 0)) * size;
            DIANA_MEMCPY(result + lane + 2 * i * size, (const DI_CHAR *)a + from, size);
            DIANA_MEMCPY(result + lane + (2 * i + 1) * size, (const DI_CHAR *)b + from, size);
        }
    DIANA_MEMCPY(r, result, bytes);
}

// phadd*, phsub*: per lane, the pairs of a then of b (sw: saturated words)
void Diana_Int_Horizontal(void * r, const void * a, const void * b, int bytes, int size, int subtract, int saturate)
{
    DI_CHAR result[64];
    int lane, k, i, pairs = 16 / size / 2;
    for (lane = 0; lane < bytes; lane += 16)
        for (k = 0; k < 2; ++k)
            for (i = 0; i < pairs; ++i)
            {
                const DI_CHAR * p = (const DI_CHAR *)(k ? b : a) + lane + 2 * i * size;
                OPERAND_SIZE_SIGNED x = Di_IntGetS(p, size), y = Di_IntGetS(p + size, size);
                OPERAND_SIZE_SIGNED v = subtract ? x - y : x + y;
                if (saturate)
                    v = Di_IntSaturateS(v, size);
                Di_IntSet(result + lane + (k * pairs + i) * size, size, (OPERAND_SIZE)v);
            }
    DIANA_MEMCPY(r, result, bytes);
}

// pshufb: per lane, the byte of a that b selects (bit 7: zero)
void Diana_Int_ShuffleBytes(void * r, const void * a, const void * b, int bytes)
{
    DI_CHAR result[64];
    const DI_CHAR * pa = (const DI_CHAR *)a, * pb = (const DI_CHAR *)b;
    int i;
    for (i = 0; i < bytes; ++i)
        result[i] = (DI_CHAR)(pb[i] & 0x80 ? 0 : pa[(i & ~15) + (pb[i] & 15)]);
    DIANA_MEMCPY(r, result, bytes);
}

// pshufd (dwords), pshufhw, pshuflw (the high/low 4 words): per lane, the elements imm8 selects
void Diana_Int_ShuffleImm(void * r, const void * a, int bytes, DiIntShuffle shuffle, int imm)
{
    DI_CHAR result[64];
    const DI_CHAR * pa = (const DI_CHAR *)a;
    int lane, i;
    DIANA_MEMCPY(result, pa, bytes);
    for (lane = 0; lane < bytes; lane += 16)
        for (i = 0; i < 4; ++i)
        {
            int select = (imm >> (2 * i)) & 3;
            if (shuffle == DI_SHUFFLE_D)
                DIANA_MEMCPY(result + lane + 4 * i, pa + lane + 4 * select, 4);
            else
            {
                int base = lane + (shuffle == DI_SHUFFLE_HW ? 8 : 0);
                DIANA_MEMCPY(result + base + 2 * i, pa + base + 2 * select, 2);
            }
        }
    DIANA_MEMCPY(r, result, bytes);
}

// palignr: per lane, a:b (a the high part) shifted right by imm8 bytes
void Diana_Int_Align(void * r, const void * a, const void * b, int bytes, int imm)
{
    DI_CHAR result[64], temp[32];
    int lane, i;
    for (lane = 0; lane < bytes; lane += 16)
    {
        DIANA_MEMCPY(temp, (const DI_CHAR *)b + lane, 16);
        DIANA_MEMCPY(temp + 16, (const DI_CHAR *)a + lane, 16);
        for (i = 0; i < 16; ++i)
            result[lane + i] = (DI_CHAR)(imm + i < 32 ? temp[imm + i] : 0);
    }
    DIANA_MEMCPY(r, result, bytes);
}

// pslldq, psrldq: per lane, the bytes shifted by imm8 (above 15: zero)
void Diana_Int_ShiftBytes(void * r, const void * a, int bytes, int left, int imm)
{
    DI_CHAR result[64];
    const DI_CHAR * pa = (const DI_CHAR *)a;
    int lane, i;
    for (lane = 0; lane < bytes; lane += 16)
        for (i = 0; i < 16; ++i)
        {
            int from = left ? i - imm : i + imm;
            result[lane + i] = (DI_CHAR)(from >= 0 && from < 16 ? pa[lane + from] : 0);
        }
    DIANA_MEMCPY(r, result, bytes);
}

// pmaddwd (signed words, the pairs summed to dwords, 0x8000 * 0x8000 * 2 wraps), pmaddubsw (the unsigned bytes of a
// times the signed bytes of b, the pairs summed with signed saturation to words)
void Diana_Int_MultiplyAdd(void * r, const void * a, const void * b, int bytes, int bytesAsUnsigned)
{
    DI_CHAR result[64];
    const DI_CHAR * pa = (const DI_CHAR *)a, * pb = (const DI_CHAR *)b;
    int i;
    if (bytesAsUnsigned)
    {
        for (i = 0; i < bytes; i += 2)
        {
            OPERAND_SIZE_SIGNED v = (OPERAND_SIZE_SIGNED)pa[i] * Di_IntGetS(pb + i, 1) +
                                    (OPERAND_SIZE_SIGNED)pa[i + 1] * Di_IntGetS(pb + i + 1, 1);
            Di_IntSet(result + i, 2, (OPERAND_SIZE)Di_IntSaturateS(v, 2));
        }
    }
    else
    {
        for (i = 0; i < bytes; i += 4)
        {
            OPERAND_SIZE_SIGNED v = Di_IntGetS(pa + i, 2) * Di_IntGetS(pb + i, 2) +
                                    Di_IntGetS(pa + i + 2, 2) * Di_IntGetS(pb + i + 2, 2);
            Di_IntSet(result + i, 4, (OPERAND_SIZE)v);
        }
    }
    DIANA_MEMCPY(r, result, bytes);
}

// pmuludq, pmuldq: the even dwords multiplied to qwords
void Diana_Int_MultiplyEven(void * r, const void * a, const void * b, int bytes, int isSigned)
{
    DI_CHAR result[64];
    int i;
    for (i = 0; i < bytes; i += 8)
    {
        OPERAND_SIZE v = isSigned ? (OPERAND_SIZE)(Di_IntGetS((const DI_CHAR *)a + i, 4) *
                                                   Di_IntGetS((const DI_CHAR *)b + i, 4))
                                  : Di_IntGetU((const DI_CHAR *)a + i, 4) * Di_IntGetU((const DI_CHAR *)b + i, 4);
        Di_IntSet(result + i, 8, v);
    }
    DIANA_MEMCPY(r, result, bytes);
}

// psadbw: per qword, the sum of the absolute differences of the bytes, in the low word
void Diana_Int_SumAbsoluteDifferences(void * r, const void * a, const void * b, int bytes)
{
    DI_CHAR result[64];
    const DI_CHAR * pa = (const DI_CHAR *)a, * pb = (const DI_CHAR *)b;
    int i, j;
    for (i = 0; i < bytes; i += 8)
    {
        OPERAND_SIZE sum = 0;
        for (j = 0; j < 8; ++j)
            sum += pa[i + j] > pb[i + j] ? pa[i + j] - pb[i + j] : pb[i + j] - pa[i + j];
        Di_IntSet(result + i, 8, sum);
    }
    DIANA_MEMCPY(r, result, bytes);
}

// mpsadbw: per lane (lane 0 imm8[2:0], lane 1 imm8[5:3]), 8 sums of absolute differences of 4 bytes: the block of a
// at bit 2 * 4 + i, of b at bits 1:0 * 4
void Diana_Int_MultipleSad(void * r, const void * a, const void * b, int bytes, int imm)
{
    DI_CHAR result[64];
    const DI_CHAR * pa = (const DI_CHAR *)a, * pb = (const DI_CHAR *)b;
    int lane, i, j;
    for (lane = 0; lane < bytes; lane += 16)
    {
        int control = (imm >> (lane / 16 * 3)) & 7;
        int aOffset = lane + ((control >> 2) & 1) * 4, bOffset = lane + (control & 3) * 4;
        for (i = 0; i < 8; ++i)
        {
            unsigned sum = 0;
            for (j = 0; j < 4; ++j)
            {
                int x = pa[aOffset + i + j], y = pb[bOffset + j];
                sum += (unsigned)(x > y ? x - y : y - x);
            }
            Di_IntSet(result + lane + 2 * i, 2, sum);
        }
    }
    DIANA_MEMCPY(r, result, bytes);
}

// pblendw (imm8 per lane), vpblendd (imm8 a bit a dword), pblendvb (the sign bits of mask): the elements of b
void Diana_Int_Blend(void * r, const void * a, const void * b, const void * mask, int bytes, int size, int imm)
{
    DI_CHAR result[64];
    int i;
    DIANA_MEMCPY(result, a, bytes);
    for (i = 0; i < bytes; i += size)
    {
        int take = mask ? (((const DI_CHAR *)mask)[i + size - 1] & 0x80) != 0
                        : (imm >> ((i / size) % (size == 2 ? 8 : 32))) & 1;
        if (take)
            DIANA_MEMCPY(result + i, (const DI_CHAR *)b + i, size);
    }
    DIANA_MEMCPY(r, result, bytes);
}

// pmovsx*, pmovzx*: the low elements of a, extended
void Diana_Int_Extend(void * r, const void * a, int bytes, int fromSize, int toSize, int isSigned)
{
    DI_CHAR result[64];
    int i;
    for (i = 0; i < bytes / toSize; ++i)
    {
        const DI_CHAR * p = (const DI_CHAR *)a + i * fromSize;
        Di_IntSet(result + i * toSize, toSize, isSigned ? (OPERAND_SIZE)Di_IntGetS(p, fromSize) : Di_IntGetU(p, fromSize));
    }
    DIANA_MEMCPY(r, result, bytes);
}

// ---------------------------------------------------------------------------------------------------------------
// the operands

static int Di_Avx2IsMemory(DianaProcessor * pCallContext, int number)
{
    return pCallContext->m_result.linkedOperands[number].type == diana_index ||
           pCallContext->m_result.linkedOperands[number].type == diana_memory;
}

static int Di_Avx2Bytes(DianaProcessor * pCallContext)
{
    return (pCallContext->m_result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1) ? 32 : 16;
}

// a register of the vector length (an xmm one: 16 bytes), memorySize bytes of memory
static int Di_Avx2Get(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                      DianaRegisterYMM_type * pValue, int memorySize)
{
    int size = Di_Avx2IsMemory(pCallContext, number) ? memorySize : Di_Avx2Bytes(pCallContext);
    return DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, number, pValue, 0, size, 0);
}

static int Di_Avx2Set(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                      DianaRegisterYMM_type * pValue)
{
    return DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, number, pValue, 1,
                                            Di_Avx2Bytes(pCallContext), 0);
}

static int Di_Avx2Imm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number, int * pImm)
{
    OPERAND_SIZE imm = 0;
    int size = 0;
    DI_CHECK(DianaProcessor_SetGetOperand(pDianaContext, pCallContext, number, &imm, 0, &size, 0));
    *pImm = (int)(imm & 0xff);
    return DI_SUCCESS;
}

// ---------------------------------------------------------------------------------------------------------------
// the handlers

typedef enum
{
    DI_AVX2_ELEMENTWISE, DI_AVX2_PACK, DI_AVX2_UNPACK, DI_AVX2_HORIZONTAL, DI_AVX2_SHUFFLE_BYTES,
    DI_AVX2_MULTIPLY_ADD, DI_AVX2_MULTIPLY_EVEN, DI_AVX2_SAD, DI_AVX2_SHIFT_VARIABLE
} DiAvx2Kind;

// dest = src1 op src2 (the NDS forms): kind, the element size and the parameters of the kind
static int Di_Avx2Binary(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiAvx2Kind kind,
                         int size, int parameter, int parameter2)
{
    DianaRegisterYMM_type a, b, r;
    int bytes = Di_Avx2Bytes(pCallContext);
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 2, &b, bytes));
    DIANA_MEMSET(&r, 0, sizeof(r));
    switch (kind)
    {
    case DI_AVX2_ELEMENTWISE: Diana_Int_Elementwise(&r, &a, &b, bytes, size, (DiIntOperation)parameter); break;
    case DI_AVX2_PACK: Diana_Int_Pack(&r, &a, &b, bytes, size, parameter); break;
    case DI_AVX2_UNPACK: Diana_Int_Unpack(&r, &a, &b, bytes, size, parameter); break;
    case DI_AVX2_HORIZONTAL: Diana_Int_Horizontal(&r, &a, &b, bytes, size, parameter, parameter2); break;
    case DI_AVX2_SHUFFLE_BYTES: Diana_Int_ShuffleBytes(&r, &a, &b, bytes); break;
    case DI_AVX2_MULTIPLY_ADD: Diana_Int_MultiplyAdd(&r, &a, &b, bytes, parameter); break;
    case DI_AVX2_MULTIPLY_EVEN: Diana_Int_MultiplyEven(&r, &a, &b, bytes, parameter); break;
    case DI_AVX2_SAD: Diana_Int_SumAbsoluteDifferences(&r, &a, &b, bytes); break;
    default: Diana_Int_ShiftVariable(&r, &a, &b, bytes, size, (DiIntShift)parameter); break;
    }
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

#define DI_AVX2_BINARY(name, kind, size, parameter, parameter2) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_Avx2Binary(pDianaContext, pCallContext, kind, size, parameter, parameter2); \
}

#define DI_AVX2_EW(name, size, operation) DI_AVX2_BINARY(name, DI_AVX2_ELEMENTWISE, size, operation, 0)

DI_AVX2_EW(vpaddsb, 1, DI_INT_ADDS)
DI_AVX2_EW(vpaddsw, 2, DI_INT_ADDS)
DI_AVX2_EW(vpaddusb, 1, DI_INT_ADDUS)
DI_AVX2_EW(vpaddusw, 2, DI_INT_ADDUS)
DI_AVX2_EW(vpsubsb, 1, DI_INT_SUBS)
DI_AVX2_EW(vpsubsw, 2, DI_INT_SUBS)
DI_AVX2_EW(vpsubusb, 1, DI_INT_SUBUS)
DI_AVX2_EW(vpsubusw, 2, DI_INT_SUBUS)
DI_AVX2_EW(vpavgb, 1, DI_INT_AVG)
DI_AVX2_EW(vpavgw, 2, DI_INT_AVG)
DI_AVX2_EW(vpcmpeqb, 1, DI_INT_CMPEQ)
DI_AVX2_EW(vpcmpeqw, 2, DI_INT_CMPEQ)
DI_AVX2_EW(vpcmpeqd, 4, DI_INT_CMPEQ)
DI_AVX2_EW(vpcmpeqq, 8, DI_INT_CMPEQ)
DI_AVX2_EW(vpcmpgtb, 1, DI_INT_CMPGT)
DI_AVX2_EW(vpcmpgtw, 2, DI_INT_CMPGT)
DI_AVX2_EW(vpcmpgtd, 4, DI_INT_CMPGT)
DI_AVX2_EW(vpcmpgtq, 8, DI_INT_CMPGT)
DI_AVX2_EW(vpmaxsb, 1, DI_INT_MAX)
DI_AVX2_EW(vpmaxsw, 2, DI_INT_MAX)
DI_AVX2_EW(vpmaxsd, 4, DI_INT_MAX)
DI_AVX2_EW(vpmaxub, 1, DI_INT_MAXU)
DI_AVX2_EW(vpmaxuw, 2, DI_INT_MAXU)
DI_AVX2_EW(vpmaxud, 4, DI_INT_MAXU)
DI_AVX2_EW(vpminsb, 1, DI_INT_MIN)
DI_AVX2_EW(vpminsw, 2, DI_INT_MIN)
DI_AVX2_EW(vpminsd, 4, DI_INT_MIN)
DI_AVX2_EW(vpminub, 1, DI_INT_MINU)
DI_AVX2_EW(vpminuw, 2, DI_INT_MINU)
DI_AVX2_EW(vpminud, 4, DI_INT_MINU)
DI_AVX2_EW(vpmullw, 2, DI_INT_MULLO)
DI_AVX2_EW(vpmulld, 4, DI_INT_MULLO)
DI_AVX2_EW(vpmulhw, 2, DI_INT_MULHI)
DI_AVX2_EW(vpmulhuw, 2, DI_INT_MULHU)
DI_AVX2_EW(vpmulhrsw, 2, DI_INT_MULHRS)
DI_AVX2_EW(vpsignb, 1, DI_INT_SIGN)
DI_AVX2_EW(vpsignw, 2, DI_INT_SIGN)
DI_AVX2_EW(vpsignd, 4, DI_INT_SIGN)
DI_AVX2_BINARY(vpacksswb, DI_AVX2_PACK, 2, 0, 0)
DI_AVX2_BINARY(vpackssdw, DI_AVX2_PACK, 4, 0, 0)
DI_AVX2_BINARY(vpackuswb, DI_AVX2_PACK, 2, 1, 0)
DI_AVX2_BINARY(vpackusdw, DI_AVX2_PACK, 4, 1, 0)
DI_AVX2_BINARY(vpunpcklbw, DI_AVX2_UNPACK, 1, 0, 0)
DI_AVX2_BINARY(vpunpcklwd, DI_AVX2_UNPACK, 2, 0, 0)
DI_AVX2_BINARY(vpunpckldq, DI_AVX2_UNPACK, 4, 0, 0)
DI_AVX2_BINARY(vpunpcklqdq, DI_AVX2_UNPACK, 8, 0, 0)
DI_AVX2_BINARY(vpunpckhbw, DI_AVX2_UNPACK, 1, 1, 0)
DI_AVX2_BINARY(vpunpckhwd, DI_AVX2_UNPACK, 2, 1, 0)
DI_AVX2_BINARY(vpunpckhdq, DI_AVX2_UNPACK, 4, 1, 0)
DI_AVX2_BINARY(vpunpckhqdq, DI_AVX2_UNPACK, 8, 1, 0)
DI_AVX2_BINARY(vphaddw, DI_AVX2_HORIZONTAL, 2, 0, 0)
DI_AVX2_BINARY(vphaddd, DI_AVX2_HORIZONTAL, 4, 0, 0)
DI_AVX2_BINARY(vphaddsw, DI_AVX2_HORIZONTAL, 2, 0, 1)
DI_AVX2_BINARY(vphsubw, DI_AVX2_HORIZONTAL, 2, 1, 0)
DI_AVX2_BINARY(vphsubd, DI_AVX2_HORIZONTAL, 4, 1, 0)
DI_AVX2_BINARY(vphsubsw, DI_AVX2_HORIZONTAL, 2, 1, 1)
DI_AVX2_BINARY(vpshufb, DI_AVX2_SHUFFLE_BYTES, 1, 0, 0)
DI_AVX2_BINARY(vpmaddwd, DI_AVX2_MULTIPLY_ADD, 2, 0, 0)
DI_AVX2_BINARY(vpmaddubsw, DI_AVX2_MULTIPLY_ADD, 1, 1, 0)
DI_AVX2_BINARY(vpmuludq, DI_AVX2_MULTIPLY_EVEN, 4, 0, 0)
DI_AVX2_BINARY(vpmuldq, DI_AVX2_MULTIPLY_EVEN, 4, 1, 0)
DI_AVX2_BINARY(vpsadbw, DI_AVX2_SAD, 1, 0, 0)
DI_AVX2_BINARY(vpsllvd, DI_AVX2_SHIFT_VARIABLE, 4, DI_SHIFT_LEFT, 0)
DI_AVX2_BINARY(vpsllvq, DI_AVX2_SHIFT_VARIABLE, 8, DI_SHIFT_LEFT, 0)
DI_AVX2_BINARY(vpsrlvd, DI_AVX2_SHIFT_VARIABLE, 4, DI_SHIFT_RIGHT, 0)
DI_AVX2_BINARY(vpsrlvq, DI_AVX2_SHIFT_VARIABLE, 8, DI_SHIFT_RIGHT, 0)
DI_AVX2_BINARY(vpsravd, DI_AVX2_SHIFT_VARIABLE, 4, DI_SHIFT_ARITHMETIC, 0)

// the shifts by xmm/m128 (src1, the count of src2's low qword) or by imm8 (NDD: src, imm8)
static int Di_Avx2Shift(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int size, DiIntShift shift)
{
    DianaRegisterYMM_type a, count, r;
    int bytes = Di_Avx2Bytes(pCallContext);
    OPERAND_SIZE value = 0;
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &a, bytes));
    if (pCallContext->m_result.linkedOperands[2].type == diana_imm)
    {
        int imm = 0;
        DI_CHECK(Di_Avx2Imm(pDianaContext, pCallContext, 2, &imm));
        value = (OPERAND_SIZE)imm;
    }
    else
    {
        // xmm3/m128: 16 bytes of memory, the count in the low 64 bits
        DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 2, &count, 0, 16, 0));
        value = count.u64[0];
    }
    Diana_Int_Shift(&r, &a, bytes, size, shift, value);
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

#define DI_AVX2_SHIFT(name, size, shift) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_Avx2Shift(pDianaContext, pCallContext, size, shift); \
}

DI_AVX2_SHIFT(vpsllw, 2, DI_SHIFT_LEFT)
DI_AVX2_SHIFT(vpslld, 4, DI_SHIFT_LEFT)
DI_AVX2_SHIFT(vpsllq, 8, DI_SHIFT_LEFT)
DI_AVX2_SHIFT(vpsrlw, 2, DI_SHIFT_RIGHT)
DI_AVX2_SHIFT(vpsrld, 4, DI_SHIFT_RIGHT)
DI_AVX2_SHIFT(vpsrlq, 8, DI_SHIFT_RIGHT)
DI_AVX2_SHIFT(vpsraw, 2, DI_SHIFT_ARITHMETIC)
DI_AVX2_SHIFT(vpsrad, 4, DI_SHIFT_ARITHMETIC)

// vpslldq, vpsrldq (NDD: dest, src, imm8)
static int Di_Avx2ShiftBytes(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int left)
{
    DianaRegisterYMM_type a, r;
    int bytes = Di_Avx2Bytes(pCallContext), imm = 0;
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_Avx2Imm(pDianaContext, pCallContext, 2, &imm));
    DIANA_MEMSET(&r, 0, sizeof(r));
    Diana_Int_ShiftBytes(&r, &a, bytes, left, imm);
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

int Diana_Call_vpslldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2ShiftBytes(pDianaContext, pCallContext, 1);
}
int Diana_Call_vpsrldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2ShiftBytes(pDianaContext, pCallContext, 0);
}

// the two-operand forms: vpabs* (dest, src), vpshufd/hw/lw (dest, src, imm8), vpmovsx*/zx* (dest, xmm/mem)
static int Di_Avx2Abs(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int size)
{
    DianaRegisterYMM_type a, r;
    int bytes = Di_Avx2Bytes(pCallContext);
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &a, bytes));
    DIANA_MEMSET(&r, 0, sizeof(r));
    Diana_Int_Abs(&r, &a, bytes, size);
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

int Diana_Call_vpabsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2Abs(pDianaContext, pCallContext, 1);
}
int Diana_Call_vpabsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2Abs(pDianaContext, pCallContext, 2);
}
int Diana_Call_vpabsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2Abs(pDianaContext, pCallContext, 4);
}

static int Di_Avx2ShuffleImm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext,
                             DiIntShuffle shuffle)
{
    DianaRegisterYMM_type a, r;
    int bytes = Di_Avx2Bytes(pCallContext), imm = 0;
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_Avx2Imm(pDianaContext, pCallContext, 2, &imm));
    DIANA_MEMSET(&r, 0, sizeof(r));
    Diana_Int_ShuffleImm(&r, &a, bytes, shuffle, imm);
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

int Diana_Call_vpshufd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2ShuffleImm(pDianaContext, pCallContext, DI_SHUFFLE_D);
}
int Diana_Call_vpshufhw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2ShuffleImm(pDianaContext, pCallContext, DI_SHUFFLE_HW);
}
int Diana_Call_vpshuflw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2ShuffleImm(pDianaContext, pCallContext, DI_SHUFFLE_LW);
}

static int Di_Avx2Extend(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int fromSize,
                         int toSize, int isSigned)
{
    DianaRegisterYMM_type a, r;
    int bytes = Di_Avx2Bytes(pCallContext);
    // the source: the low elements of an xmm register or memory of their size
    DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 1, &a, 0,
                                              Di_Avx2IsMemory(pCallContext, 1) ? bytes / toSize * fromSize : 16, 0));
    DIANA_MEMSET(&r, 0, sizeof(r));
    Diana_Int_Extend(&r, &a, bytes, fromSize, toSize, isSigned);
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

#define DI_AVX2_EXTEND(name, from, to, isSigned) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_Avx2Extend(pDianaContext, pCallContext, from, to, isSigned); \
}

DI_AVX2_EXTEND(vpmovsxbw, 1, 2, 1)
DI_AVX2_EXTEND(vpmovsxbd, 1, 4, 1)
DI_AVX2_EXTEND(vpmovsxbq, 1, 8, 1)
DI_AVX2_EXTEND(vpmovsxwd, 2, 4, 1)
DI_AVX2_EXTEND(vpmovsxwq, 2, 8, 1)
DI_AVX2_EXTEND(vpmovsxdq, 4, 8, 1)
DI_AVX2_EXTEND(vpmovzxbw, 1, 2, 0)
DI_AVX2_EXTEND(vpmovzxbd, 1, 4, 0)
DI_AVX2_EXTEND(vpmovzxbq, 1, 8, 0)
DI_AVX2_EXTEND(vpmovzxwd, 2, 4, 0)
DI_AVX2_EXTEND(vpmovzxwq, 2, 8, 0)
DI_AVX2_EXTEND(vpmovzxdq, 4, 8, 0)

// with imm8: vpalignr, vpblendw, vpblendd, vmpsadbw (dest, src1, src2, imm8)
typedef enum { DI_AVX2_ALIGN, DI_AVX2_BLENDW, DI_AVX2_BLENDD, DI_AVX2_MPSADBW } DiAvx2Imm;

static int Di_Avx2BinaryImm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiAvx2Imm kind)
{
    DianaRegisterYMM_type a, b, r;
    int bytes = Di_Avx2Bytes(pCallContext), imm = 0;
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 2, &b, bytes));
    DI_CHECK(Di_Avx2Imm(pDianaContext, pCallContext, 3, &imm));
    DIANA_MEMSET(&r, 0, sizeof(r));
    switch (kind)
    {
    case DI_AVX2_ALIGN: Diana_Int_Align(&r, &a, &b, bytes, imm); break;
    case DI_AVX2_BLENDW: Diana_Int_Blend(&r, &a, &b, 0, bytes, 2, imm); break;
    case DI_AVX2_BLENDD: Diana_Int_Blend(&r, &a, &b, 0, bytes, 4, imm); break;
    default: Diana_Int_MultipleSad(&r, &a, &b, bytes, imm); break;
    }
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

int Diana_Call_vpalignr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2BinaryImm(pDianaContext, pCallContext, DI_AVX2_ALIGN);
}
int Diana_Call_vpblendw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2BinaryImm(pDianaContext, pCallContext, DI_AVX2_BLENDW);
}
int Diana_Call_vpblendd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2BinaryImm(pDianaContext, pCallContext, DI_AVX2_BLENDD);
}
int Diana_Call_vmpsadbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2BinaryImm(pDianaContext, pCallContext, DI_AVX2_MPSADBW);
}

// vpmovmskb: the sign bits of the bytes to a general register
int Diana_Call_vpmovmskb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type a, mask;
    int bytes = Di_Avx2Bytes(pCallContext), i;
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &a, bytes));
    DIANA_MEMSET(&mask, 0, sizeof(mask));
    for (i = 0; i < bytes; ++i)
        if (a.u8[i] & 0x80)
            mask.u32[0] |= 1u << i;
    DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 0, &mask, 1, 8, 0));
    DI_PROC_END
}

// vpbroadcastb/w/d/q: element 0 of an xmm register or memory in every element
static int Di_Avx2Broadcast(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int size)
{
    DianaRegisterYMM_type a, r;
    int bytes = Di_Avx2Bytes(pCallContext), i;
    DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 1, &a, 0,
                                              Di_Avx2IsMemory(pCallContext, 1) ? size : 16, 0));
    for (i = 0; i < bytes; i += size)
        DIANA_MEMCPY(r.u8 + i, a.u8, size);
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

int Diana_Call_vpbroadcastb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2Broadcast(pDianaContext, pCallContext, 1);
}
int Diana_Call_vpbroadcastw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2Broadcast(pDianaContext, pCallContext, 2);
}
int Diana_Call_vpbroadcastd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2Broadcast(pDianaContext, pCallContext, 4);
}
int Diana_Call_vpbroadcastq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2Broadcast(pDianaContext, pCallContext, 8);
}

// vpermd, vpermps: dest = the dwords of src2 that the dwords of src1 select (cross lane)
static int Di_Avx2PermuteVariable(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type index, data, r;
    int i;
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &index, 32));
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 2, &data, 32));
    for (i = 0; i < 8; ++i)
        r.u32[i] = data.u32[index.u32[i] & 7];
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

int Diana_Call_vpermd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2PermuteVariable(pDianaContext, pCallContext);
}
int Diana_Call_vpermps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2PermuteVariable(pDianaContext, pCallContext);
}

// vpermq, vpermpd: the qwords of src that imm8 selects (2 bits each, cross lane)
static int Di_Avx2PermuteImm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type a, r;
    int imm = 0, i;
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &a, 32));
    DI_CHECK(Di_Avx2Imm(pDianaContext, pCallContext, 2, &imm));
    for (i = 0; i < 4; ++i)
        r.u64[i] = a.u64[(imm >> (2 * i)) & 3];
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}

int Diana_Call_vpermq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2PermuteImm(pDianaContext, pCallContext);
}
int Diana_Call_vpermpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Avx2PermuteImm(pDianaContext, pCallContext);
}

// vpblendvb: dest = the bytes of src2 where the mask (the is4 register) has the sign bit set, else of src1
int Diana_Call_vpblendvb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type a, b, mask, r;
    int bytes = Di_Avx2Bytes(pCallContext);
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 2, &b, bytes));
    DI_CHECK(Di_Avx2Get(pDianaContext, pCallContext, 3, &mask, bytes));
    DIANA_MEMSET(&r, 0, sizeof(r));
    Diana_Int_Blend(&r, &a, &b, &mask, bytes, 1, 0);
    DI_CHECK(Di_Avx2Set(pDianaContext, pCallContext, 0, &r));
    DI_PROC_END
}
