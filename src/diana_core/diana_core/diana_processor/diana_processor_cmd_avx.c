#include "diana_processor_cmd_avx.h"
#include "diana_processor_cmd_fpu_internal.h"
#include "diana_processor_cmd_crypto.h"
#include "diana_processor_cmd_sse2.h"
#include "diana_processor_cmd_fpu_sse.h"
#include "diana_processor_cmd_simd.h"
#include "diana_processor_cmd_avx2.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

// The VEX instructions: dest = src1 op src2 (src1 is VEX.vvvv), a scalar form takes the elements above 0 from src1.
// VEX.L of the command is the vector length: 16 or 32 bytes. A write of an xmm register clears bits 255:128 of its
// ymm register (DianaProcessor_YMM_SetGetOperand), a legacy SSE write keeps them.

static int Di_AvxIsMemory(DianaProcessor * pCallContext, int number)
{
    return pCallContext->m_result.linkedOperands[number].type == diana_index ||
           pCallContext->m_result.linkedOperands[number].type == diana_memory;
}

// 1: VEX.256
static int Di_AvxIs256(DianaProcessor * pCallContext)
{
    return (pCallContext->m_result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1) != 0;
}

// the vector length in bytes
static int Di_AvxBytes(DianaProcessor * pCallContext)
{
    return Di_AvxIs256(pCallContext) ? 32 : 16;
}

// an operand: a register of the vector length, memorySize bytes of memory
static int Di_AvxGetEx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                       DianaRegisterYMM_type * pValue, int memorySize, int alignment)
{
    int size = Di_AvxIsMemory(pCallContext, number) ? memorySize : Di_AvxBytes(pCallContext);
    return DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, number, pValue, 0, size, alignment);
}

static int Di_AvxSetEx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                       DianaRegisterYMM_type * pValue, int memorySize, int alignment)
{
    int size = Di_AvxIsMemory(pCallContext, number) ? memorySize : Di_AvxBytes(pCallContext);
    return DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, number, pValue, 1, size, alignment);
}

static int Di_AvxGet(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                     DianaRegisterYMM_type * pValue, int memorySize)
{
    return Di_AvxGetEx(pDianaContext, pCallContext, number, pValue, memorySize, 0);
}

static int Di_AvxSet(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                     DianaRegisterYMM_type * pValue, int memorySize)
{
    return Di_AvxSetEx(pDianaContext, pCallContext, number, pValue, memorySize, 0);
}

static int Di_AvxImm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                     OPERAND_SIZE * pImm)
{
    int size = 0;
    return DianaProcessor_SetGetOperand(pDianaContext, pCallContext, number, pImm, 0, &size, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// floating point

#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU

// count: the elements of a 128-bit lane (1: scalar, VEX.L ignored)
static int Di_AvxFloat(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                       int count, DiSseOperation operation)
{
    DianaRegisterYMM_type a, b;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = count == 1 ? 16 : Di_AvxBytes(pCallContext);
    int i;
    if (count != 1)
        count = bytes / elementSize;
    DI_CHECK(Di_AvxGetEx(pDianaContext, pCallContext, 1, &a, bytes, 0));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, count == 1 ? elementSize : bytes));
    for (i = 0; i < count; ++i)
    {
        if (elementSize == 4)
            a.u32[i] = Diana_SSE_Float32(operation, a.u32[i], b.u32[i], &status);
        else
            a.u64[i] = Diana_SSE_Float64(operation, a.u64[i], b.u64[i], &status);
    }
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSetEx(pDianaContext, pCallContext, 0, &a, bytes, 0));
    DI_PROC_END
}

#define DI_AVX_FLOAT(name, elementSize, count, operation) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_AvxFloat(pDianaContext, pCallContext, elementSize, count, operation); \
}

#else

#define DI_AVX_FLOAT(name, elementSize, count, operation) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ pDianaContext; pCallContext; return DI_UNSUPPORTED_COMMAND; }

#endif

DI_AVX_FLOAT(vaddps, 4, 4, DI_SSE_ADD)
DI_AVX_FLOAT(vaddpd, 8, 2, DI_SSE_ADD)
DI_AVX_FLOAT(vaddss, 4, 1, DI_SSE_ADD)
DI_AVX_FLOAT(vaddsd, 8, 1, DI_SSE_ADD)
DI_AVX_FLOAT(vsubps, 4, 4, DI_SSE_SUB)
DI_AVX_FLOAT(vsubpd, 8, 2, DI_SSE_SUB)
DI_AVX_FLOAT(vsubss, 4, 1, DI_SSE_SUB)
DI_AVX_FLOAT(vsubsd, 8, 1, DI_SSE_SUB)
DI_AVX_FLOAT(vmulps, 4, 4, DI_SSE_MUL)
DI_AVX_FLOAT(vmulpd, 8, 2, DI_SSE_MUL)
DI_AVX_FLOAT(vmulss, 4, 1, DI_SSE_MUL)
DI_AVX_FLOAT(vmulsd, 8, 1, DI_SSE_MUL)
DI_AVX_FLOAT(vdivps, 4, 4, DI_SSE_DIV)
DI_AVX_FLOAT(vdivpd, 8, 2, DI_SSE_DIV)
DI_AVX_FLOAT(vdivss, 4, 1, DI_SSE_DIV)
DI_AVX_FLOAT(vdivsd, 8, 1, DI_SSE_DIV)
DI_AVX_FLOAT(vminps, 4, 4, DI_SSE_MIN)
DI_AVX_FLOAT(vminpd, 8, 2, DI_SSE_MIN)
DI_AVX_FLOAT(vminss, 4, 1, DI_SSE_MIN)
DI_AVX_FLOAT(vminsd, 8, 1, DI_SSE_MIN)
DI_AVX_FLOAT(vmaxps, 4, 4, DI_SSE_MAX)
DI_AVX_FLOAT(vmaxpd, 8, 2, DI_SSE_MAX)
DI_AVX_FLOAT(vmaxss, 4, 1, DI_SSE_MAX)
DI_AVX_FLOAT(vmaxsd, 8, 1, DI_SSE_MAX)

// ---------------------------------------------------------------------------------------------------------------
// bitwise and integer (VEX.256 of the integer forms: AVX2)

typedef enum
{
    DI_AVX_AND, DI_AVX_ANDN, DI_AVX_OR, DI_AVX_XOR, DI_AVX_PADD, DI_AVX_PSUB
} DiAvxIntegerOperation;

static int Di_AvxInteger(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                         DiAvxIntegerOperation operation)
{
    DianaRegisterYMM_type a, b;
    int bytes = Di_AvxBytes(pCallContext);
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, bytes));
    for (i = 0; i < bytes / elementSize; ++i)
    {
        OPERAND_SIZE x = 0, y = 0, result = 0;
        DIANA_MEMCPY(&x, a.u8 + i * elementSize, elementSize);
        DIANA_MEMCPY(&y, b.u8 + i * elementSize, elementSize);
        switch (operation)
        {
        case DI_AVX_AND: result = x & y; break;
        case DI_AVX_ANDN: result = ~x & y; break;
        case DI_AVX_OR: result = x | y; break;
        case DI_AVX_XOR: result = x ^ y; break;
        case DI_AVX_PADD: result = x + y; break;
        default: result = x - y; break;
        }
        DIANA_MEMCPY(a.u8 + i * elementSize, &result, elementSize);
    }
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, bytes));
    DI_PROC_END
}

#define DI_AVX_INTEGER(name, elementSize, operation) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_AvxInteger(pDianaContext, pCallContext, elementSize, operation); \
}

DI_AVX_INTEGER(vandps, 8, DI_AVX_AND)
DI_AVX_INTEGER(vandpd, 8, DI_AVX_AND)
DI_AVX_INTEGER(vandnps, 8, DI_AVX_ANDN)
DI_AVX_INTEGER(vandnpd, 8, DI_AVX_ANDN)
DI_AVX_INTEGER(vorps, 8, DI_AVX_OR)
DI_AVX_INTEGER(vorpd, 8, DI_AVX_OR)
DI_AVX_INTEGER(vxorps, 8, DI_AVX_XOR)
DI_AVX_INTEGER(vxorpd, 8, DI_AVX_XOR)
DI_AVX_INTEGER(vpand, 8, DI_AVX_AND)
DI_AVX_INTEGER(vpandn, 8, DI_AVX_ANDN)
DI_AVX_INTEGER(vpor, 8, DI_AVX_OR)
DI_AVX_INTEGER(vpxor, 8, DI_AVX_XOR)
DI_AVX_INTEGER(vpaddb, 1, DI_AVX_PADD)
DI_AVX_INTEGER(vpaddw, 2, DI_AVX_PADD)
DI_AVX_INTEGER(vpaddd, 4, DI_AVX_PADD)
DI_AVX_INTEGER(vpaddq, 8, DI_AVX_PADD)
DI_AVX_INTEGER(vpsubb, 1, DI_AVX_PSUB)
DI_AVX_INTEGER(vpsubw, 2, DI_AVX_PSUB)
DI_AVX_INTEGER(vpsubd, 4, DI_AVX_PSUB)
DI_AVX_INTEGER(vpsubq, 8, DI_AVX_PSUB)

// ---------------------------------------------------------------------------------------------------------------
// moves: vmovaps, vmovapd (aligned), vmovups, vmovupd (a load, a store or a register), vmovddup

static int Di_AvxMove(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int aligned)
{
    DianaRegisterYMM_type value;
    int bytes = Di_AvxBytes(pCallContext);
    int alignment = aligned ? bytes : 0;
    DI_CHECK(Di_AvxGetEx(pDianaContext, pCallContext, 1, &value, bytes, alignment));
    DI_CHECK(Di_AvxSetEx(pDianaContext, pCallContext, 0, &value, bytes, alignment));
    DI_PROC_END
}

int Diana_Call_vmovaps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMove(pDianaContext, pCallContext, 1);
}
int Diana_Call_vmovapd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMove(pDianaContext, pCallContext, 1);
}
int Diana_Call_vmovups(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMove(pDianaContext, pCallContext, 0);
}
int Diana_Call_vmovupd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMove(pDianaContext, pCallContext, 0);
}

// the even doubles twice: 64 bits of memory for VEX.128
int Diana_Call_vmovddup(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type value;
    int bytes = Di_AvxBytes(pCallContext);
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, bytes == 16 ? 8 : 32));
    value.u64[1] = value.u64[0];
    value.u64[3] = value.u64[2];
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, bytes));
    DI_PROC_END
}

// ---------------------------------------------------------------------------------------------------------------
// AES, PCLMULQDQ (VEX.128 only: the VEX.256 forms are VAES, VPCLMULQDQ): the state (the first product) is src1

static int Di_AvxAes(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiAesRound round)
{
    DianaRegisterYMM_type state, key;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &state, 16));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &key, 16));
    Diana_AesRoundValue(&state.xmm[0], &key.xmm[0], round);
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &state, 16));
    DI_PROC_END
}

int Diana_Call_vaesenc(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxAes(pDianaContext, pCallContext, DI_AESENC);
}
int Diana_Call_vaesenclast(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxAes(pDianaContext, pCallContext, DI_AESENCLAST);
}
int Diana_Call_vaesdec(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxAes(pDianaContext, pCallContext, DI_AESDEC);
}
int Diana_Call_vaesdeclast(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxAes(pDianaContext, pCallContext, DI_AESDECLAST);
}

int Diana_Call_vaesimc(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type value;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, 16));
    Diana_AesImcValue(&value.xmm[0]);
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, 16));
    DI_PROC_END
}

int Diana_Call_vaeskeygenassist(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type src, dest;
    OPERAND_SIZE imm = 0;
    DIANA_MEMSET(&dest, 0, sizeof(dest));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &src, 16));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 2, &imm));
    Diana_AesKeyGenAssistValue(&dest.xmm[0], &src.xmm[0], (int)imm);
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

int Diana_Call_vpclmulqdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type a, b, dest;
    OPERAND_SIZE imm = 0;
    DIANA_MEMSET(&dest, 0, sizeof(dest));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, 16));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, 16));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 3, &imm));
    Diana_PclmulValue(&dest.xmm[0], &a.xmm[0], &b.xmm[0], (int)imm);
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &dest, 16));
    DI_PROC_END
}

// ---------------------------------------------------------------------------------------------------------------
// B1: moves and state

static int Di_AvxAlignment(DianaProcessor * pCallContext)
{
    return (pCallContext->m_result.pInfo->m_flags & DI_FLAG_CMD_MUST_BE_ALIGNED) ? Di_AvxBytes(pCallContext) : 0;
}

// vmovdqa, vmovdqu, vmovntdq, vmovntps, vmovntpd, vmovntdqa, vlddqu: a load, a store or a register
static int Di_AvxCopy(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type value;
    int bytes = Di_AvxBytes(pCallContext);
    int alignment = Di_AvxAlignment(pCallContext);
    DI_CHECK(Di_AvxGetEx(pDianaContext, pCallContext, 1, &value, bytes, alignment));
    DI_CHECK(Di_AvxSetEx(pDianaContext, pCallContext, 0, &value, bytes, alignment));
    DI_PROC_END
}

#define DI_AVX_COPY(name) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_AvxCopy(pDianaContext, pCallContext); \
}

DI_AVX_COPY(vmovdqa)
DI_AVX_COPY(vmovdqu)
DI_AVX_COPY(vmovntdq)
DI_AVX_COPY(vmovntps)
DI_AVX_COPY(vmovntpd)
DI_AVX_COPY(vmovntdqa)
DI_AVX_COPY(vlddqu)

// bits 255:128 of ymm0..15 (0..7 outside 64-bit mode: ymm8..15 keep their value)
static int Di_AvxZero(DianaProcessor * pCallContext, int all)
{
    int count = pCallContext->m_context.iAMD64Mode ? 16 : 8;
    int i;
    for (i = 0; i < count; ++i)
    {
        DianaUnifiedRegister reg = (DianaUnifiedRegister)(reg_YMM0 + i);
        if (all)
        {
            // zmm0..15 (7): the whole register; zmm16..31 keep their value
            DianaProcessor_ZMM_ClearAbove(pCallContext, reg, 0);
        }
        else
        {
            DianaProcessor_YMM_ClearUpper(pCallContext, reg);
        }
    }
    DI_PROC_END
}

int Diana_Call_vzeroupper(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext;
    return Di_AvxZero(pCallContext, 0);
}
int Diana_Call_vzeroall(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext;
    return Di_AvxZero(pCallContext, 1);
}

// vmovd, vmovq (66 0f 6e, 7e): a general register or memory of the operand size and the low element of xmm; a
// write of xmm zeroes the rest
int Diana_Call_vmovd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type value;
    int size = pCallContext->m_result.iOperandSize == 8 ? 8 : 4;
    DianaLinkedOperand * pDest = pCallContext->m_result.linkedOperands;
    int toXmm = pDest->type == diana_register && pDest->value.recognizedRegister >= reg_XMM0 &&
                pDest->value.recognizedRegister <= reg_XMM15;
    DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 1, &value, 0, toXmm ? size : 16, 0));
    if (toXmm)
    {
        DIANA_MEMSET(value.u8 + size, 0, sizeof(value) - size);
        DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 0, &value, 1, 16, 0));
    }
    else
    {
        DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 0, &value, 1, size, 0));
    }
    DI_PROC_END
}

// vmovq (f3 0f 7e: xmm/m64 to xmm; 66 0f d6: xmm to xmm/m64): 64 bits, a register destination is zero-extended
int Diana_Call_vmovq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type value;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, 8));
    value.u64[1] = value.u64[2] = value.u64[3] = 0;
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, 8));
    DI_PROC_END
}

// vmovss, vmovsd: a register form (3 operands) takes the low element of the third operand and the rest of the
// second; a load zeroes the rest; a store writes the element
static int Di_AvxMoveScalar(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type value, element;
    if (pCallContext->m_result.iLinkedOpCount == 3)
    {
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, 16));
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &element, 16));
        DIANA_MEMCPY(value.u8, element.u8, elementSize);
    }
    else
    {
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, elementSize));
        DIANA_MEMSET(value.u8 + elementSize, 0, sizeof(value) - elementSize);
    }
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, elementSize));
    DI_PROC_END
}

int Diana_Call_vmovss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveScalar(pDianaContext, pCallContext, 4);
}
int Diana_Call_vmovsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveScalar(pDianaContext, pCallContext, 8);
}

// vmovhps, vmovhpd, vmovlps, vmovlpd: a load (3 operands) merges m64 into the high (low) half of src1, a store
// writes the high (low) half
static int Di_AvxMoveHalf(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int high)
{
    DianaRegisterYMM_type value, half;
    if (pCallContext->m_result.iLinkedOpCount == 3)
    {
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, 16));
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &half, 8));
        value.u64[high] = half.u64[0];
        DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, 16));
    }
    else
    {
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, 16));
        value.u64[0] = value.u64[high];
        DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, 8));
    }
    DI_PROC_END
}

int Diana_Call_vmovhps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveHalf(pDianaContext, pCallContext, 1);
}
int Diana_Call_vmovhpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveHalf(pDianaContext, pCallContext, 1);
}
int Diana_Call_vmovlps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveHalf(pDianaContext, pCallContext, 0);
}
int Diana_Call_vmovlpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveHalf(pDianaContext, pCallContext, 0);
}

// vmovlhps: the low halves of src1 and src2; vmovhlps: the high half of src2, then the high half of src1
static int Di_AvxMoveHalves(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int lowHigh)
{
    DianaRegisterYMM_type a, b;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, 16));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, 16));
    if (lowHigh)
        a.u64[1] = b.u64[0];
    else
        a.u64[0] = b.u64[1];
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, 16));
    DI_PROC_END
}

int Diana_Call_vmovlhps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveHalves(pDianaContext, pCallContext, 1);
}
int Diana_Call_vmovhlps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveHalves(pDianaContext, pCallContext, 0);
}

// vmovshdup, vmovsldup: the odd (even) floats twice
static int Di_AvxDuplicate(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int odd)
{
    DianaRegisterYMM_type value;
    int bytes = Di_AvxBytes(pCallContext);
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, bytes));
    for (i = 0; i < bytes / 4; i += 2)
        value.u32[i] = value.u32[i + 1] = value.u32[i + odd];
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, bytes));
    DI_PROC_END
}

int Diana_Call_vmovshdup(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxDuplicate(pDianaContext, pCallContext, 1);
}
int Diana_Call_vmovsldup(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxDuplicate(pDianaContext, pCallContext, 0);
}

// vmovmskps, vmovmskpd: the sign bits to a general register
static int Di_AvxMoveMask(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type value, mask;
    int bytes = Di_AvxBytes(pCallContext);
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, bytes));
    DIANA_MEMSET(&mask, 0, sizeof(mask));
    for (i = 0; i < bytes / elementSize; ++i)
        if (value.u8[i * elementSize + elementSize - 1] & 0x80)
            mask.u32[0] |= 1u << i;
    DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 0, &mask, 1, 8, 0));
    DI_PROC_END
}

int Diana_Call_vmovmskps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveMask(pDianaContext, pCallContext, 4);
}
int Diana_Call_vmovmskpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMoveMask(pDianaContext, pCallContext, 8);
}

// vldmxcsr, vstmxcsr: as ldmxcsr, stmxcsr
int Diana_Call_vldmxcsr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Diana_Call_ldmxcsr(pDianaContext, pCallContext);
}
int Diana_Call_vstmxcsr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Diana_Call_stmxcsr(pDianaContext, pCallContext);
}

// ---------------------------------------------------------------------------------------------------------------
// B2: lanes and shuffles

// vbroadcastss, vbroadcastsd, vbroadcastf128: the memory element in every element of the destination
static int Di_AvxBroadcast(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type value;
    int bytes = Di_AvxBytes(pCallContext);
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, elementSize));
    for (i = elementSize; i < bytes; i += elementSize)
        DIANA_MEMCPY(value.u8 + i, value.u8, elementSize);
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, bytes));
    DI_PROC_END
}

int Diana_Call_vbroadcastss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxBroadcast(pDianaContext, pCallContext, 4);
}
int Diana_Call_vbroadcastsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxBroadcast(pDianaContext, pCallContext, 8);
}
int Diana_Call_vbroadcastf128(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxBroadcast(pDianaContext, pCallContext, 16);
}

// vinsertf128 ymm1, ymm2, xmm3/m128, imm8: ymm2 with the lane imm8[0] replaced
int Diana_Call_vinsertf128(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type value, lane;
    OPERAND_SIZE imm = 0;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, 32));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &lane, 16));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 3, &imm));
    value.xmm[imm & 1] = lane.xmm[0];
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, 32));
    DI_PROC_END
}

// vextractf128 xmm1/m128, ymm2, imm8: the lane imm8[0]
int Diana_Call_vextractf128(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type value;
    OPERAND_SIZE imm = 0;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &value, 32));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 2, &imm));
    value.xmm[0] = value.xmm[imm & 1];
    DIANA_MEMSET(&value.xmm[1], 0, 16);
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &value, 16));
    DI_PROC_END
}

// vperm2f128: every lane of the destination is a lane of src1 or src2 (imm8[1:0], imm8[5:4]) or zero (bit 3, 7)
int Diana_Call_vperm2f128(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type a, b, result;
    OPERAND_SIZE imm = 0;
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, 32));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, 32));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 3, &imm));
    for (i = 0; i < 2; ++i)
    {
        int control = (int)(imm >> (4 * i)) & 0xf;
        if (control & 8)
            DIANA_MEMSET(&result.xmm[i], 0, 16);
        else
            result.xmm[i] = (control & 2 ? b : a).xmm[control & 1];
    }
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &result, 32));
    DI_PROC_END
}

// vpermilps, vpermilpd: the elements of each lane of src1 (or of the memory) chosen by the controls of src2 (the
// register form: bits 1:0 of a dword, bit 1 of a qword) or of imm8 (2 bits a float, 1 bit a double)
static int Di_AvxPermil(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type a, control, result;
    int bytes = Di_AvxBytes(pCallContext);
    int count = bytes / elementSize, perLane = 16 / elementSize;
    OPERAND_SIZE imm = 0;
    int useImm = pCallContext->m_result.iLinkedOpCount == 3 &&
                 pCallContext->m_result.linkedOperands[2].type == diana_imm;
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    if (useImm)
    {
        DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 2, &imm));
    }
    else
    {
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &control, bytes));
    }
    for (i = 0; i < count; ++i)
    {
        int lane = i / perLane, select;
        if (elementSize == 4)
        {
            select = useImm ? (int)(imm >> (2 * (i % 4))) & 3 : (int)(control.u32[i] & 3);
            result.u32[i] = a.u32[lane * 4 + select];
        }
        else
        {
            select = useImm ? (int)(imm >> i) & 1 : (int)(control.u64[i] >> 1) & 1;
            result.u64[i] = a.u64[lane * 2 + select];
        }
    }
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &result, bytes));
    DI_PROC_END
}

int Diana_Call_vpermilps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxPermil(pDianaContext, pCallContext, 4);
}
int Diana_Call_vpermilpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxPermil(pDianaContext, pCallContext, 8);
}

// vshufps: per lane src1[imm 1:0], src1[imm 3:2], src2[imm 5:4], src2[imm 7:6]; vshufpd: per lane
// src1[imm bit], src2[imm bit], one bit an element
int Diana_Call_vshufps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type a, b, result;
    int bytes = Di_AvxBytes(pCallContext);
    OPERAND_SIZE imm = 0;
    int lane;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, bytes));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 3, &imm));
    for (lane = 0; lane < bytes / 16; ++lane)
    {
        const DI_UINT32 * pa = a.u32 + 4 * lane, * pb = b.u32 + 4 * lane;
        DI_UINT32 * pr = result.u32 + 4 * lane;
        pr[0] = pa[imm & 3];
        pr[1] = pa[(imm >> 2) & 3];
        pr[2] = pb[(imm >> 4) & 3];
        pr[3] = pb[(imm >> 6) & 3];
    }
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &result, bytes));
    DI_PROC_END
}

int Diana_Call_vshufpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type a, b, result;
    int bytes = Di_AvxBytes(pCallContext);
    OPERAND_SIZE imm = 0;
    int lane;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, bytes));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 3, &imm));
    for (lane = 0; lane < bytes / 16; ++lane)
    {
        result.u64[2 * lane] = a.u64[2 * lane + ((imm >> (2 * lane)) & 1)];
        result.u64[2 * lane + 1] = b.u64[2 * lane + ((imm >> (2 * lane + 1)) & 1)];
    }
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &result, bytes));
    DI_PROC_END
}

// vunpcklps, vunpckhps, vunpcklpd, vunpckhpd: per lane the low (high) elements of src1 and src2 interleaved
static int Di_AvxUnpack(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                        int high)
{
    DianaRegisterYMM_type a, b, result;
    int bytes = Di_AvxBytes(pCallContext);
    int perLane = 16 / elementSize, lane, i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, bytes));
    for (lane = 0; lane < bytes / 16; ++lane)
        for (i = 0; i < perLane / 2; ++i)
        {
            int from = lane * 16 + (i + (high ? perLane / 2 : 0)) * elementSize;
            DIANA_MEMCPY(result.u8 + lane * 16 + 2 * i * elementSize, a.u8 + from, elementSize);
            DIANA_MEMCPY(result.u8 + lane * 16 + (2 * i + 1) * elementSize, b.u8 + from, elementSize);
        }
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &result, bytes));
    DI_PROC_END
}

int Diana_Call_vunpcklps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxUnpack(pDianaContext, pCallContext, 4, 0);
}
int Diana_Call_vunpckhps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxUnpack(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_vunpcklpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxUnpack(pDianaContext, pCallContext, 8, 0);
}
int Diana_Call_vunpckhpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxUnpack(pDianaContext, pCallContext, 8, 1);
}

// vblendps, vblendpd: the element i of src2 where imm8 bit i is set, else of src1
static int Di_AvxBlend(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type a, b;
    int bytes = Di_AvxBytes(pCallContext);
    OPERAND_SIZE imm = 0;
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, bytes));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 3, &imm));
    for (i = 0; i < bytes / elementSize; ++i)
        if ((imm >> i) & 1)
            DIANA_MEMCPY(a.u8 + i * elementSize, b.u8 + i * elementSize, elementSize);
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, bytes));
    DI_PROC_END
}

int Diana_Call_vblendps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxBlend(pDianaContext, pCallContext, 4);
}
int Diana_Call_vblendpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxBlend(pDianaContext, pCallContext, 8);
}

// vtestps, vtestpd (the sign bits of the elements), vptest (all bits): ZF = (a & b) is 0, CF = (~a & b) is 0, AF
// OF PF SF cleared
static int Di_AvxTest(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type a, b;
    int bytes = Di_AvxBytes(pCallContext);
    OPERAND_SIZE andBits = 0, andNotBits = 0;
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 0, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &b, bytes));
    for (i = 0; i < bytes / 8; ++i)
    {
        OPERAND_SIZE mask = elementSize == 4 ? 0x8000000080000000ULL : elementSize == 8 ? 0x8000000000000000ULL :
                                                                                         ~(OPERAND_SIZE)0;
        andBits |= a.u64[i] & b.u64[i] & mask;
        andNotBits |= ~a.u64[i] & b.u64[i] & mask;
    }
    CLEAR_FLAG_AF; CLEAR_FLAG_OF; CLEAR_FLAG_PF; CLEAR_FLAG_SF; CLEAR_FLAG_ZF; CLEAR_FLAG_CF;
    if (!andBits)
        SET_FLAG_ZF;
    if (!andNotBits)
        SET_FLAG_CF;
    DI_PROC_END
}

int Diana_Call_vtestps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxTest(pDianaContext, pCallContext, 4);
}
int Diana_Call_vtestpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxTest(pDianaContext, pCallContext, 8);
}
int Diana_Call_vptest(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxTest(pDianaContext, pCallContext, 0);
}

// vmaskmovps, vmaskmovpd: the elements whose mask element (vvvv) has the sign bit set; the others are not read
// (a load: 0) or not written, so they can't fault
static int Di_AvxMaskMove(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type mask, data;
    int bytes = Di_AvxBytes(pCallContext);
    int store = Di_AvxIsMemory(pCallContext, 0);
    DianaLinkedOperand * pMemory = pCallContext->m_result.linkedOperands + (store ? 0 : 2);
    const DianaRmIndex * pIndex = pMemory->type == diana_memory ? &pMemory->value.memory.m_index
                                                                 : &pMemory->value.rmIndex;
    OPERAND_SIZE selector = 0, address = 0;
    OPERAND_SIZE addressMask = pDianaContext->iCurrentCmd_addressSize == DIANA_MODE64 ? ~(OPERAND_SIZE)0 :
                               pDianaContext->iCurrentCmd_addressSize == DIANA_MODE32 ? 0xffffffffULL : 0xffffULL;
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &mask, bytes));
    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext, pCallContext, pIndex, &selector, &address));
    if (store)
    {
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &data, bytes));
    }
    else
    {
        DIANA_MEMSET(&data, 0, sizeof(data));
    }
    for (i = 0; i < bytes / elementSize; ++i)
    {
        OPERAND_SIZE element = 0;
        OPERAND_SIZE elementAddress = (address + i * elementSize) & addressMask;
        if (!(mask.u8[i * elementSize + elementSize - 1] & 0x80))
            continue;
        if (store)
        {
            DIANA_MEMCPY(&element, data.u8 + i * elementSize, elementSize);
            DI_CHECK(DianaProcessor_SetMemValue(pCallContext, selector, elementAddress, elementSize, &element, 0,
                                                pIndex->seg_reg));
        }
        else
        {
            DI_CHECK(DianaProcessor_GetMemValue(pCallContext, selector, elementAddress, elementSize, &element, 0,
                                                pIndex->seg_reg));
            DIANA_MEMCPY(data.u8 + i * elementSize, &element, elementSize);
        }
    }
    if (!store)
    {
        DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &data, bytes));
    }
    DI_PROC_END
}

int Diana_Call_vmaskmovps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMaskMove(pDianaContext, pCallContext, 4);
}
int Diana_Call_vmaskmovpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMaskMove(pDianaContext, pCallContext, 8);
}

// ---------------------------------------------------------------------------------------------------------------
// B3: float arithmetic, compares, conversions (the cores of the legacy forms, diana_processor_cmd_fpu_internal.h);
// vrcpps, vrcpss, vrsqrtps, vrsqrtss are not emulated (the CPU's tables, as the legacy forms)

#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU

// vsqrtps, vsqrtpd (dest, src); vsqrtss, vsqrtsd (dest, src1, src2: the rest from src1)
static int Di_AvxSqrt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                      int scalar)
{
    DianaRegisterYMM_type a, b;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = scalar ? 16 : Di_AvxBytes(pCallContext);
    int count = scalar ? 1 : bytes / elementSize;
    int i;
    if (scalar)
    {
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, 16));
    }
    else
    {
        DIANA_MEMSET(&a, 0, sizeof(a));
    }
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, scalar ? 2 : 1, &b, scalar ? elementSize : bytes));
    for (i = 0; i < count; ++i)
    {
        if (elementSize == 4)
            a.u32[i] = Diana_SSE_Float32(DI_SSE_SQRT, 0, b.u32[i], &status);
        else
            a.u64[i] = Diana_SSE_Float64(DI_SSE_SQRT, 0, b.u64[i], &status);
    }
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, bytes));
    DI_PROC_END
}

int Diana_Call_vsqrtps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxSqrt(pDianaContext, pCallContext, 4, 0);
}
int Diana_Call_vsqrtpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxSqrt(pDianaContext, pCallContext, 8, 0);
}
int Diana_Call_vsqrtss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxSqrt(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_vsqrtsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxSqrt(pDianaContext, pCallContext, 8, 1);
}

// vroundps, vroundpd (dest, src, imm8); vroundss, vroundsd (dest, src1, src2, imm8)
static int Di_AvxRound(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                       int scalar)
{
    DianaRegisterYMM_type a, b;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = scalar ? 16 : Di_AvxBytes(pCallContext);
    OPERAND_SIZE imm = 0;
    if (scalar)
    {
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, 16));
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, elementSize));
        DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 3, &imm));
    }
    else
    {
        DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &b, bytes));
        DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 2, &imm));
        a = b;
    }
    Diana_SSE_Round(&a, &b, elementSize, scalar ? 1 : bytes / elementSize, (int)imm, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, bytes));
    DI_PROC_END
}

int Diana_Call_vroundps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxRound(pDianaContext, pCallContext, 4, 0);
}
int Diana_Call_vroundpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxRound(pDianaContext, pCallContext, 8, 0);
}
int Diana_Call_vroundss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxRound(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_vroundsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxRound(pDianaContext, pCallContext, 8, 1);
}

// vdpps, vdppd: per 128-bit lane
static int Di_AvxDotProduct(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type a, b;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = Di_AvxBytes(pCallContext);
    OPERAND_SIZE imm = 0;
    int lane;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, bytes));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 3, &imm));
    for (lane = 0; lane < bytes / 16; ++lane)
        Diana_SSE_DotProduct(&a.xmm[lane], &b.xmm[lane], elementSize, (int)imm, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, bytes));
    DI_PROC_END
}

int Diana_Call_vdpps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxDotProduct(pDianaContext, pCallContext, 4);
}
int Diana_Call_vdppd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxDotProduct(pDianaContext, pCallContext, 8);
}

// vhaddps/pd, vhsubps/pd, vaddsubps/pd: per 128-bit lane
static int Di_AvxHorizontal(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                            DiHorizontal operation)
{
    DianaRegisterYMM_type a, b, result;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = Di_AvxBytes(pCallContext);
    int lane;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, bytes));
    DIANA_MEMSET(&result, 0, sizeof(result));
    for (lane = 0; lane < bytes / 16; ++lane)
        Diana_SSE_Horizontal(&result.xmm[lane], &a.xmm[lane], &b.xmm[lane], elementSize, operation, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &result, bytes));
    DI_PROC_END
}

int Diana_Call_vhaddps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxHorizontal(pDianaContext, pCallContext, 4, DI_HADD);
}
int Diana_Call_vhaddpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxHorizontal(pDianaContext, pCallContext, 8, DI_HADD);
}
int Diana_Call_vhsubps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxHorizontal(pDianaContext, pCallContext, 4, DI_HSUB);
}
int Diana_Call_vhsubpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxHorizontal(pDianaContext, pCallContext, 8, DI_HSUB);
}
int Diana_Call_vaddsubps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxHorizontal(pDianaContext, pCallContext, 4, DI_ADDSUB);
}
int Diana_Call_vaddsubpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxHorizontal(pDianaContext, pCallContext, 8, DI_ADDSUB);
}

// vcmpps/pd, vcmpss/sd: the predicate imm8[4:0]
static int Di_AvxCompare(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                         int scalar)
{
    DianaRegisterYMM_type a, b;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = scalar ? 16 : Di_AvxBytes(pCallContext);
    OPERAND_SIZE imm = 0;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, scalar ? elementSize : bytes));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 3, &imm));
    Diana_SSE_Compare(&a, &b, elementSize, scalar ? 1 : bytes / elementSize, (int)(imm & 31), &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, bytes));
    DI_PROC_END
}

int Diana_Call_vcmpps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxCompare(pDianaContext, pCallContext, 4, 0);
}
int Diana_Call_vcmppd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxCompare(pDianaContext, pCallContext, 8, 0);
}
int Diana_Call_vcmpss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxCompare(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_vcmpsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxCompare(pDianaContext, pCallContext, 8, 1);
}

// vcomiss/sd, vucomiss/sd: the flags of element 0
static int Di_AvxCompareFlags(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                              int quiet)
{
    DianaRegisterYMM_type a, b;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int relation;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 0, &a, elementSize));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &b, elementSize));
    relation = Diana_SSE_CompareScalar(&a.xmm[0], &b.xmm[0], elementSize, quiet, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    Diana_SSE_SetCompareFlags(pCallContext, relation);
    DI_PROC_END
}

int Diana_Call_vcomiss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxCompareFlags(pDianaContext, pCallContext, 4, 0);
}
int Diana_Call_vcomisd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxCompareFlags(pDianaContext, pCallContext, 8, 0);
}
int Diana_Call_vucomiss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxCompareFlags(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_vucomisd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxCompareFlags(pDianaContext, pCallContext, 8, 1);
}

// the packed conversions: the elements of the wider side fill the vector length (VEX.L), the other side has half
// as many bytes when the element sizes differ (vcvtpd2ps xmm1, ymm2; vcvtps2pd ymm1, xmm2)
static int Di_AvxConvert(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiElement from,
                         DiElement to, int truncate)
{
    DianaRegisterYMM_type src, result;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = Di_AvxBytes(pCallContext);
    int fromSize = from == DI_F64 ? 8 : 4, toSize = to == DI_F64 ? 8 : 4;
    int count = bytes / (fromSize > toSize ? fromSize : toSize);
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &src, count * fromSize));
    DIANA_MEMSET(&result, 0, sizeof(result));
    Diana_SSE_Convert(&result, &src, from, to, count, truncate, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &result, count * toSize));
    DI_PROC_END
}

int Diana_Call_vcvtdq2pd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvert(pDianaContext, pCallContext, DI_I32, DI_F64, 0);
}
int Diana_Call_vcvtdq2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvert(pDianaContext, pCallContext, DI_I32, DI_F32, 0);
}
int Diana_Call_vcvtpd2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvert(pDianaContext, pCallContext, DI_F64, DI_I32, 0);
}
int Diana_Call_vcvttpd2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvert(pDianaContext, pCallContext, DI_F64, DI_I32, 1);
}
int Diana_Call_vcvtpd2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvert(pDianaContext, pCallContext, DI_F64, DI_F32, 0);
}
int Diana_Call_vcvtps2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvert(pDianaContext, pCallContext, DI_F32, DI_I32, 0);
}
int Diana_Call_vcvttps2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvert(pDianaContext, pCallContext, DI_F32, DI_I32, 1);
}
int Diana_Call_vcvtps2pd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvert(pDianaContext, pCallContext, DI_F32, DI_F64, 0);
}

// vcvtss2si, vcvttss2si, vcvtsd2si, vcvttsd2si: element 0 to a 32- or 64-bit general register (VEX.W)
static int Di_AvxToInteger(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                           int truncate)
{
    DianaRegisterYMM_type src, value;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int size = pCallContext->m_result.linkedOperands[0].usedSize;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &src, elementSize));
    DIANA_MEMSET(&value, 0, sizeof(value));
    value.u64[0] = Diana_SSE_ToInteger(&src.xmm[0], elementSize, size, truncate, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 0, &value, 1, size, 0));
    DI_PROC_END
}

int Diana_Call_vcvtss2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxToInteger(pDianaContext, pCallContext, 4, 0);
}
int Diana_Call_vcvttss2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxToInteger(pDianaContext, pCallContext, 4, 1);
}
int Diana_Call_vcvtsd2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxToInteger(pDianaContext, pCallContext, 8, 0);
}
int Diana_Call_vcvttsd2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxToInteger(pDianaContext, pCallContext, 8, 1);
}

// vcvtsi2ss, vcvtsi2sd: a 32- or 64-bit integer (VEX.W) to element 0, the rest from src1
static int Di_AvxFromInteger(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type a, integer;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int size = pCallContext->m_result.linkedOperands[2].usedSize;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, 16));
    DI_CHECK(DianaProcessor_YMM_SetGetOperand(pDianaContext, pCallContext, 2, &integer, 0, size, 0));
    Diana_SSE_FromInteger(&a.xmm[0], elementSize, size == 8 ? integer.u64[0] : integer.u32[0], size, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, 16));
    DI_PROC_END
}

int Diana_Call_vcvtsi2ss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxFromInteger(pDianaContext, pCallContext, 4);
}
int Diana_Call_vcvtsi2sd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxFromInteger(pDianaContext, pCallContext, 8);
}

// vcvtss2sd, vcvtsd2ss: element 0 of src2, the rest from src1
static int Di_AvxConvertScalar(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiElement from,
                               DiElement to)
{
    DianaRegisterYMM_type a, b;
    float_status_t status = Diana_SSE_Status(pCallContext);
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, 16));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, from == DI_F64 ? 8 : 4));
    Diana_SSE_Convert(&a, &b, from, to, 1, 0, &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, 16));
    DI_PROC_END
}

int Diana_Call_vcvtss2sd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvertScalar(pDianaContext, pCallContext, DI_F32, DI_F64);
}
int Diana_Call_vcvtsd2ss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxConvertScalar(pDianaContext, pCallContext, DI_F64, DI_F32);
}

#else

#define DI_AVX_FLOAT_STUB(name) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ pDianaContext; pCallContext; return DI_UNSUPPORTED_COMMAND; }

DI_AVX_FLOAT_STUB(vsqrtps) DI_AVX_FLOAT_STUB(vsqrtpd) DI_AVX_FLOAT_STUB(vsqrtss) DI_AVX_FLOAT_STUB(vsqrtsd)
DI_AVX_FLOAT_STUB(vroundps) DI_AVX_FLOAT_STUB(vroundpd) DI_AVX_FLOAT_STUB(vroundss) DI_AVX_FLOAT_STUB(vroundsd)
DI_AVX_FLOAT_STUB(vdpps) DI_AVX_FLOAT_STUB(vdppd) DI_AVX_FLOAT_STUB(vhaddps) DI_AVX_FLOAT_STUB(vhaddpd)
DI_AVX_FLOAT_STUB(vhsubps) DI_AVX_FLOAT_STUB(vhsubpd) DI_AVX_FLOAT_STUB(vaddsubps) DI_AVX_FLOAT_STUB(vaddsubpd)
DI_AVX_FLOAT_STUB(vcmpps) DI_AVX_FLOAT_STUB(vcmppd) DI_AVX_FLOAT_STUB(vcmpss) DI_AVX_FLOAT_STUB(vcmpsd)
DI_AVX_FLOAT_STUB(vcomiss) DI_AVX_FLOAT_STUB(vcomisd) DI_AVX_FLOAT_STUB(vucomiss) DI_AVX_FLOAT_STUB(vucomisd)
DI_AVX_FLOAT_STUB(vcvtdq2pd) DI_AVX_FLOAT_STUB(vcvtdq2ps) DI_AVX_FLOAT_STUB(vcvtpd2dq) DI_AVX_FLOAT_STUB(vcvttpd2dq)
DI_AVX_FLOAT_STUB(vcvtpd2ps) DI_AVX_FLOAT_STUB(vcvtps2dq) DI_AVX_FLOAT_STUB(vcvttps2dq) DI_AVX_FLOAT_STUB(vcvtps2pd)
DI_AVX_FLOAT_STUB(vcvtss2si) DI_AVX_FLOAT_STUB(vcvttss2si) DI_AVX_FLOAT_STUB(vcvtsd2si) DI_AVX_FLOAT_STUB(vcvttsd2si)
DI_AVX_FLOAT_STUB(vcvtsi2ss) DI_AVX_FLOAT_STUB(vcvtsi2sd) DI_AVX_FLOAT_STUB(vcvtss2sd) DI_AVX_FLOAT_STUB(vcvtsd2ss)

#endif

// ---------------------------------------------------------------------------------------------------------------
// B4: the VEX.128 forms of the legacy SSE2..SSE4.2 integer instructions run the legacy handler (its semantics are
// checked against the CPU) on rearranged operands, then clear bits 255:128 of the written xmm register:
// - NDS, dest = src1 op src2[, imm8]: dest = src1, then the legacy dest op= src2[, imm8]. When src2 is the
//   destination register, its value goes to the register of src1 for the call (restored after it).
// - NDD, dest (vvvv) = src (r/m) shifted by imm8: dest = src, then the legacy dest, imm8.
// - the legacy operands (vpshufd, vpmovsx, vpextrw...): the call as it is.
// Every register the adapter changes is restored when the legacy handler fails.

typedef int (*DiLegacyHandler)(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

typedef enum
{
    DI_AVX_LEGACY_NDS,          // dest, src1, src2[, imm8]
    DI_AVX_LEGACY_NDD,          // dest (vvvv), src, imm8
    DI_AVX_LEGACY_SAME,         // the legacy operands, op 0 a written xmm register
    DI_AVX_LEGACY_SAME_XMM0,    // the legacy operands, xmm0 written (vpcmpestrm, vpcmpistrm)
    DI_AVX_LEGACY_SAME_NO_XMM   // the legacy operands, no xmm register written
} DiAvxLegacyForm;

static int Di_AvxLegacy(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiLegacyHandler legacy,
                        DiAvxLegacyForm form)
{
    DianaLinkedOperand saved[DI_MAX_OPERANDS_COUNT];
    DianaLinkedOperand * ops = pCallContext->m_result.linkedOperands;
    int count = pCallContext->m_result.iLinkedOpCount;
    DianaUnifiedRegister dest = ops[0].value.recognizedRegister, holder = reg_none;
    DianaRegisterYMM_type destOld, holderOld, value;
    int status, i;

    if (form == DI_AVX_LEGACY_SAME || form == DI_AVX_LEGACY_SAME_XMM0 || form == DI_AVX_LEGACY_SAME_NO_XMM)
    {
        status = legacy(pDianaContext, pCallContext);
        if (status == DI_SUCCESS && form == DI_AVX_LEGACY_SAME && ops[0].type == diana_register)
            DianaProcessor_YMM_ClearUpper(pCallContext, ops[0].value.recognizedRegister);
        if (status == DI_SUCCESS && form == DI_AVX_LEGACY_SAME_XMM0)
            DianaProcessor_YMM_ClearUpper(pCallContext, reg_XMM0);
        return status;
    }

    for (i = 0; i < count; ++i)
        saved[i] = ops[i];
    destOld = DianaProcessor_YMM_GetValue(pCallContext, dest);
    value = DianaProcessor_YMM_GetValue(pCallContext, ops[1].value.recognizedRegister);
    if (form == DI_AVX_LEGACY_NDS && ops[2].type == diana_register &&
        ops[2].value.recognizedRegister >= reg_XMM0 && ops[2].value.recognizedRegister <= reg_XMM15 &&
        ops[2].value.recognizedRegister == dest && ops[1].value.recognizedRegister != dest)
    {
        // src2 is the destination: its value in the register of src1 for the call
        holder = ops[1].value.recognizedRegister;
        holderOld = value;
        DianaProcessor_YMM_SetValue(pCallContext, holder, &destOld);
        ops[2].value.recognizedRegister = holder;
    }
    DianaProcessor_YMM_SetValue(pCallContext, dest, &value);
    // the legacy operands: dest, then the operands after the first source
    for (i = 2; i < count; ++i)
        ops[i - 1] = ops[i];
    pCallContext->m_result.iLinkedOpCount = count - 1;

    status = legacy(pDianaContext, pCallContext);

    pCallContext->m_result.iLinkedOpCount = count;
    for (i = 0; i < count; ++i)
        ops[i] = saved[i];
    if (holder != reg_none)
        DianaProcessor_YMM_SetValue(pCallContext, holder, &holderOld);
    if (status != DI_SUCCESS)
    {
        DianaProcessor_YMM_SetValue(pCallContext, dest, &destOld);
        return status;
    }
    DianaProcessor_YMM_ClearUpper(pCallContext, dest);
    return status;
}

#define DI_AVX_LEGACY(name, legacyName, form) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_AvxLegacy(pDianaContext, pCallContext, Diana_Call_##legacyName, form); \
}

DI_AVX_LEGACY(vinsertps, insertps, DI_AVX_LEGACY_NDS)
DI_AVX_LEGACY(vpinsrw, pinsrw, DI_AVX_LEGACY_NDS)
DI_AVX_LEGACY(vpinsrb, pinsrb, DI_AVX_LEGACY_NDS)
DI_AVX_LEGACY(vpinsrd, pinsrd, DI_AVX_LEGACY_NDS)
DI_AVX_LEGACY(vphminposuw, phminposuw, DI_AVX_LEGACY_SAME)
DI_AVX_LEGACY(vpextrw, pextrw, DI_AVX_LEGACY_SAME_NO_XMM)
DI_AVX_LEGACY(vpextrb, pextrb, DI_AVX_LEGACY_SAME_NO_XMM)
DI_AVX_LEGACY(vpextrd, pextrd, DI_AVX_LEGACY_SAME_NO_XMM)
DI_AVX_LEGACY(vextractps, extractps, DI_AVX_LEGACY_SAME_NO_XMM)
DI_AVX_LEGACY(vmaskmovdqu, maskmovdqu, DI_AVX_LEGACY_SAME_NO_XMM)
DI_AVX_LEGACY(vpcmpestri, pcmpestri, DI_AVX_LEGACY_SAME_NO_XMM)
DI_AVX_LEGACY(vpcmpistri, pcmpistri, DI_AVX_LEGACY_SAME_NO_XMM)
DI_AVX_LEGACY(vpcmpestrm, pcmpestrm, DI_AVX_LEGACY_SAME_XMM0)
DI_AVX_LEGACY(vpcmpistrm, pcmpistrm, DI_AVX_LEGACY_SAME_XMM0)

// the shifts: by xmm/m128 (NDS) or by imm8 (NDD)
static int Di_AvxShift(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiLegacyHandler legacy)
{
    int byImm = pCallContext->m_result.iLinkedOpCount == 3 &&
                pCallContext->m_result.linkedOperands[2].type == diana_imm;
    return Di_AvxLegacy(pDianaContext, pCallContext, legacy, byImm ? DI_AVX_LEGACY_NDD : DI_AVX_LEGACY_NDS);
}

#define DI_AVX_SHIFT(name, legacyName) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_AvxShift(pDianaContext, pCallContext, Diana_Call_##legacyName); \
}


// ---------------------------------------------------------------------------------------------------------------
// the VEX forms of SSSE3 and of the SSE4.1 rest (the legacy handlers), vblendvps/pd/vpblendvb


// vblendvps, vblendvpd, vpblendvb: dest = the element of src2 where the mask (the is4 register) has the sign bit
// set, else of src1
static int Di_AvxBlendVariable(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterYMM_type a, b, mask;
    int bytes = Di_AvxBytes(pCallContext);
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &a, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &b, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 3, &mask, bytes));
    for (i = 0; i < bytes / elementSize; ++i)
        if (mask.u8[i * elementSize + elementSize - 1] & 0x80)
            DIANA_MEMCPY(a.u8 + i * elementSize, b.u8 + i * elementSize, elementSize);
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &a, bytes));
    DI_PROC_END
}

int Diana_Call_vblendvps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxBlendVariable(pDianaContext, pCallContext, 4);
}
int Diana_Call_vblendvpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxBlendVariable(pDianaContext, pCallContext, 8);
}

// ---------------------------------------------------------------------------------------------------------------
// F16C: vcvtph2ps (MXCSR.DAZ ignored, no DE), vcvtps2ph (MXCSR.FTZ ignored; imm8[1:0] the rounding unless imm8[2]: MXCSR's)

#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU

int Diana_Call_vcvtph2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type src, result;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = Di_AvxBytes(pCallContext);
    int i;
    status.denormals_are_zeros = 0;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &src, bytes / 2));
    for (i = 0; i < bytes / 4; ++i)
        result.u32[i] = float16_to_float32(src.u16[i], &status);
    // a denormal half is exact as a float: no DE (the CPU)
    status.float_exception_flags &= ~float_flag_denormal;
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &result, bytes));
    DI_PROC_END
}

int Diana_Call_vcvtps2ph(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterYMM_type src, result;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = Di_AvxBytes(pCallContext);
    OPERAND_SIZE imm = 0;
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &src, bytes));
    DI_CHECK(Di_AvxImm(pDianaContext, pCallContext, 2, &imm));
    status.flush_underflow_to_zero = 0;
    if (!(imm & 4))
        status.float_rounding_mode = (int)(imm & 3);
    DIANA_MEMSET(&result, 0, sizeof(result));
    for (i = 0; i < bytes / 4; ++i)
        result.u16[i] = float32_to_float16(src.u32[i], &status);
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    // xmm1/m64 (VEX.128), xmm1/m128 (VEX.256); a register destination is xmm, its rest zeroed
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &result, bytes / 2));
    DI_PROC_END
}

#else

int Diana_Call_vcvtph2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; pCallContext; return DI_UNSUPPORTED_COMMAND; }
int Diana_Call_vcvtps2ph(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; pCallContext; return DI_UNSUPPORTED_COMMAND; }

#endif

// ---------------------------------------------------------------------------------------------------------------
// AVX2: the integer forms of the f128 lane instructions, vpmaskmovd/q, the gathers

int Diana_Call_vbroadcasti128(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Diana_Call_vbroadcastf128(pDianaContext, pCallContext);
}
int Diana_Call_vinserti128(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Diana_Call_vinsertf128(pDianaContext, pCallContext);
}
int Diana_Call_vextracti128(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Diana_Call_vextractf128(pDianaContext, pCallContext);
}
int Diana_Call_vperm2i128(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Diana_Call_vperm2f128(pDianaContext, pCallContext);
}
int Diana_Call_vpmaskmovd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMaskMove(pDianaContext, pCallContext, 4);
}
int Diana_Call_vpmaskmovq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxMaskMove(pDianaContext, pCallContext, 8);
}

// the gathers (dest, VSIB, mask): the elements whose mask element has the sign bit set, loaded from base + the
// index element (sign-extended) * scale + disp, in order; each loaded element clears its mask element. A fault
// keeps what was loaded so far and the mask of the rest (restartable). At the end the mask is 0; the elements
// beyond the count (an xmm destination of qword indices) and bits 255:128 of an xmm destination are zeroed.
static int Di_AvxGather(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int indexSize,
                        int elementSize)
{
    DianaLinkedOperand * ops = pCallContext->m_result.linkedOperands;
    DianaUnifiedRegister destReg = ops[0].value.recognizedRegister, maskReg = ops[2].value.recognizedRegister;
    DianaUnifiedRegister indexReg = ops[1].value.rmIndex.indexed_reg;
    DianaRmIndex baseIndex = ops[1].value.rmIndex;
    int destBytes = destReg >= reg_YMM0 && destReg <= reg_YMM15 ? 32 : 16;
    int indexBytes = indexReg >= reg_YMM0 && indexReg <= reg_YMM15 ? 32 : 16;
    int count = destBytes / elementSize < indexBytes / indexSize ? destBytes / elementSize : indexBytes / indexSize;
    DianaRegisterYMM_type dest = DianaProcessor_YMM_GetValue(pCallContext, destReg);
    DianaRegisterYMM_type mask = DianaProcessor_YMM_GetValue(pCallContext, maskReg);
    DianaRegisterYMM_type index = DianaProcessor_YMM_GetValue(pCallContext, indexReg);
    OPERAND_SIZE selector = 0, base = 0;
    OPERAND_SIZE addressMask = pDianaContext->iCurrentCmd_addressSize == DIANA_MODE64 ? ~(OPERAND_SIZE)0 : 0xffffffffULL;
    int status = DI_SUCCESS, i;

    // base + disp (without the index)
    baseIndex.indexed_reg = reg_none;
    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext, pCallContext, &baseIndex, &selector, &base));
    for (i = 0; i < count; ++i)
    {
        OPERAND_SIZE offset, element = 0;
        if (!(mask.u8[i * elementSize + elementSize - 1] & 0x80))
            continue;
        if (indexSize == 4)
            offset = (OPERAND_SIZE)(OPERAND_SIZE_SIGNED)index.s32[i];
        else
            offset = index.u64[i];
        status = DianaProcessor_GetMemValue(pCallContext, selector,
                                            (base + offset * ops[1].value.rmIndex.index) & addressMask,
                                            elementSize, &element, 0, ops[1].value.rmIndex.seg_reg);
        if (status != DI_SUCCESS)
            break;
        DIANA_MEMCPY(dest.u8 + i * elementSize, &element, elementSize);
        DIANA_MEMSET(mask.u8 + i * elementSize, 0, elementSize);
    }
    if (status == DI_SUCCESS)
    {
        DIANA_MEMSET(&mask, 0, sizeof(mask));
        DIANA_MEMSET(dest.u8 + count * elementSize, 0, sizeof(dest) - count * elementSize);
    }
    else if (destBytes == 16)
    {
        // a VEX.128 write: bits 255:128 zeroed in both registers
        DIANA_MEMSET(dest.u8 + 16, 0, 16);
        DIANA_MEMSET(mask.u8 + 16, 0, 16);
    }
    DianaProcessor_YMM_SetValue(pCallContext, destReg, &dest);
    DianaProcessor_YMM_SetValue(pCallContext, maskReg, &mask);
    DianaProcessor_ZMM_ClearAbove(pCallContext, destReg, 32);
    DianaProcessor_ZMM_ClearAbove(pCallContext, maskReg, 32);
    DI_CHECK(status);
    DI_PROC_END
}

int Diana_Call_vgatherdps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxGather(pDianaContext, pCallContext, 4, 4);
}
int Diana_Call_vgatherdpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxGather(pDianaContext, pCallContext, 4, 8);
}
int Diana_Call_vgatherqps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxGather(pDianaContext, pCallContext, 8, 4);
}
int Diana_Call_vgatherqpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxGather(pDianaContext, pCallContext, 8, 8);
}
int Diana_Call_vpgatherdd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxGather(pDianaContext, pCallContext, 4, 4);
}
int Diana_Call_vpgatherdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxGather(pDianaContext, pCallContext, 4, 8);
}
int Diana_Call_vpgatherqd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxGather(pDianaContext, pCallContext, 8, 4);
}
int Diana_Call_vpgatherqq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AvxGather(pDianaContext, pCallContext, 8, 8);
}

// ---------------------------------------------------------------------------------------------------------------
// FMA3: one rounding of a * b + c (softfloat's muladd), the order 132 (dest * src3 + src2), 213 (src2 * dest + src3),
// 231 (src2 * src3 + dest); vfmsub negates c, vfnmadd the product, vfnmsub both; vfmaddsub subtracts in the even
// elements, vfmsubadd in the odd ones. A scalar form keeps the rest of the destination. FMA4 (AMD) is decoded only.

#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU

typedef enum { DI_FMA_ADD, DI_FMA_SUB, DI_FMA_NADD, DI_FMA_NSUB, DI_FMA_ADDSUB, DI_FMA_SUBADD } DiFma;

static int Di_AvxFma(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                     int scalar, int order, DiFma operation)
{
    DianaRegisterYMM_type d, s2, s3;
    float_status_t status = Diana_SSE_Status(pCallContext);
    int bytes = scalar ? 16 : Di_AvxBytes(pCallContext);
    int count = scalar ? 1 : bytes / elementSize;
    int i;
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 0, &d, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 1, &s2, bytes));
    DI_CHECK(Di_AvxGet(pDianaContext, pCallContext, 2, &s3, scalar ? elementSize : bytes));
    for (i = 0; i < count; ++i)
    {
        const DianaRegisterYMM_type * pa = order == 132 ? &d : &s2;
        const DianaRegisterYMM_type * pb = order == 213 ? &d : &s3;
        const DianaRegisterYMM_type * pc = order == 132 ? &s2 : order == 213 ? &s3 : &d;
        int flags = 0;
        switch (operation)
        {
        case DI_FMA_SUB: flags = float_muladd_negate_c; break;
        case DI_FMA_NADD: flags = float_muladd_negate_product; break;
        case DI_FMA_NSUB: flags = float_muladd_negate_result; break;
        case DI_FMA_ADDSUB: flags = i & 1 ? 0 : float_muladd_negate_c; break;
        case DI_FMA_SUBADD: flags = i & 1 ? float_muladd_negate_c : 0; break;
        default: break;
        }
        if (elementSize == 4)
            d.u32[i] = float32_muladd(pa->u32[i], pb->u32[i], pc->u32[i], flags, &status);
        else
            d.u64[i] = float64_muladd(pa->u64[i], pb->u64[i], pc->u64[i], flags, &status);
    }
    DI_CHECK(Diana_SSE_Exceptions(pCallContext, &status));
    DI_CHECK(Di_AvxSet(pDianaContext, pCallContext, 0, &d, bytes));
    DI_PROC_END
}

#define DI_AVX_FMA(name, elementSize, scalar, order, operation) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ \
    return Di_AvxFma(pDianaContext, pCallContext, elementSize, scalar, order, operation); \
}

#else

#define DI_AVX_FMA(name, elementSize, scalar, order, operation) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ pDianaContext; pCallContext; return DI_UNSUPPORTED_COMMAND; }

#endif

DI_AVX_FMA(vfmaddsub132ps, 4, 0, 132, DI_FMA_ADDSUB)
DI_AVX_FMA(vfmaddsub132pd, 8, 0, 132, DI_FMA_ADDSUB)
DI_AVX_FMA(vfmaddsub213ps, 4, 0, 213, DI_FMA_ADDSUB)
DI_AVX_FMA(vfmaddsub213pd, 8, 0, 213, DI_FMA_ADDSUB)
DI_AVX_FMA(vfmaddsub231ps, 4, 0, 231, DI_FMA_ADDSUB)
DI_AVX_FMA(vfmaddsub231pd, 8, 0, 231, DI_FMA_ADDSUB)
DI_AVX_FMA(vfmsubadd132ps, 4, 0, 132, DI_FMA_SUBADD)
DI_AVX_FMA(vfmsubadd132pd, 8, 0, 132, DI_FMA_SUBADD)
DI_AVX_FMA(vfmsubadd213ps, 4, 0, 213, DI_FMA_SUBADD)
DI_AVX_FMA(vfmsubadd213pd, 8, 0, 213, DI_FMA_SUBADD)
DI_AVX_FMA(vfmsubadd231ps, 4, 0, 231, DI_FMA_SUBADD)
DI_AVX_FMA(vfmsubadd231pd, 8, 0, 231, DI_FMA_SUBADD)
DI_AVX_FMA(vfmadd132ps, 4, 0, 132, DI_FMA_ADD)
DI_AVX_FMA(vfmadd132pd, 8, 0, 132, DI_FMA_ADD)
DI_AVX_FMA(vfmadd132ss, 4, 1, 132, DI_FMA_ADD)
DI_AVX_FMA(vfmadd132sd, 8, 1, 132, DI_FMA_ADD)
DI_AVX_FMA(vfmadd213ps, 4, 0, 213, DI_FMA_ADD)
DI_AVX_FMA(vfmadd213pd, 8, 0, 213, DI_FMA_ADD)
DI_AVX_FMA(vfmadd213ss, 4, 1, 213, DI_FMA_ADD)
DI_AVX_FMA(vfmadd213sd, 8, 1, 213, DI_FMA_ADD)
DI_AVX_FMA(vfmadd231ps, 4, 0, 231, DI_FMA_ADD)
DI_AVX_FMA(vfmadd231pd, 8, 0, 231, DI_FMA_ADD)
DI_AVX_FMA(vfmadd231ss, 4, 1, 231, DI_FMA_ADD)
DI_AVX_FMA(vfmadd231sd, 8, 1, 231, DI_FMA_ADD)
DI_AVX_FMA(vfmsub132ps, 4, 0, 132, DI_FMA_SUB)
DI_AVX_FMA(vfmsub132pd, 8, 0, 132, DI_FMA_SUB)
DI_AVX_FMA(vfmsub132ss, 4, 1, 132, DI_FMA_SUB)
DI_AVX_FMA(vfmsub132sd, 8, 1, 132, DI_FMA_SUB)
DI_AVX_FMA(vfmsub213ps, 4, 0, 213, DI_FMA_SUB)
DI_AVX_FMA(vfmsub213pd, 8, 0, 213, DI_FMA_SUB)
DI_AVX_FMA(vfmsub213ss, 4, 1, 213, DI_FMA_SUB)
DI_AVX_FMA(vfmsub213sd, 8, 1, 213, DI_FMA_SUB)
DI_AVX_FMA(vfmsub231ps, 4, 0, 231, DI_FMA_SUB)
DI_AVX_FMA(vfmsub231pd, 8, 0, 231, DI_FMA_SUB)
DI_AVX_FMA(vfmsub231ss, 4, 1, 231, DI_FMA_SUB)
DI_AVX_FMA(vfmsub231sd, 8, 1, 231, DI_FMA_SUB)
DI_AVX_FMA(vfnmadd132ps, 4, 0, 132, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd132pd, 8, 0, 132, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd132ss, 4, 1, 132, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd132sd, 8, 1, 132, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd213ps, 4, 0, 213, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd213pd, 8, 0, 213, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd213ss, 4, 1, 213, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd213sd, 8, 1, 213, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd231ps, 4, 0, 231, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd231pd, 8, 0, 231, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd231ss, 4, 1, 231, DI_FMA_NADD)
DI_AVX_FMA(vfnmadd231sd, 8, 1, 231, DI_FMA_NADD)
DI_AVX_FMA(vfnmsub132ps, 4, 0, 132, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub132pd, 8, 0, 132, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub132ss, 4, 1, 132, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub132sd, 8, 1, 132, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub213ps, 4, 0, 213, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub213pd, 8, 0, 213, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub213ss, 4, 1, 213, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub213sd, 8, 1, 213, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub231ps, 4, 0, 231, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub231pd, 8, 0, 231, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub231ss, 4, 1, 231, DI_FMA_NSUB)
DI_AVX_FMA(vfnmsub231sd, 8, 1, 231, DI_FMA_NSUB)
