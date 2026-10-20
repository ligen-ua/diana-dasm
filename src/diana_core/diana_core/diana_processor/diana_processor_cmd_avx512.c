#include "diana_processor_cmd_avx512.h"
#include "diana_processor_cmd_avx2.h"
#include "diana_processor_cmd_fpu_internal.h"
#include "diana_processor_commands.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"
#include "diana_disable_warnings.h"

// AVX-512 (EVEX): AVX-512F, BW, DQ, VL, CD and the opmask instructions (VEX). One operand layer for every EVEX
// instruction: the sources (a register in full, memory with the broadcast element repeated, the masked-off elements
// of memory not read: no fault), the destination (the elements of the opmask, the others zeroed or kept, the bits
// above the vector length zero; memory: only the elements of the opmask), the rounding of {er} and the suppressed
// exceptions of {sae}. The integer work goes through the element cores of diana_processor_cmd_avx2.c, the float work
// through softfloat with MXCSR (Diana_SSE_Status).

typedef struct _DiEvex
{
    int vl;                 // the vector length in bytes: 16, 32, 64 (16 for the scalars)
    OPERAND_SIZE mask;      // the opmask; all ones without one
    int zeroing;            // {z}
    int rounding;           // DIANA_EVEX_ROUND_*
}DiEvex;

#define DI_EVEX_ALL_ONES ((OPERAND_SIZE)-1)

static OPERAND_SIZE Di_LowBits(int count)
{
    return count >= 64 ? DI_EVEX_ALL_ONES : (((OPERAND_SIZE)1 << count) - 1);
}

static OPERAND_SIZE Di_GetK(DianaProcessor * pCallContext, int k)
{
    return DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, (DianaUnifiedRegister)(reg_K0 + k)));
}

static void Di_SetK(DianaProcessor * pCallContext, int k, OPERAND_SIZE value)
{
    DianaUnifiedRegister reg = (DianaUnifiedRegister)(reg_K0 + k);
    DianaProcessor_SetValue(pCallContext, reg, DianaProcessor_QueryReg(pCallContext, reg), value);
}

static void Di_EvexInit(DianaProcessor * pCallContext, DiEvex * e)
{
    const DianaEvexInfo * pInfo = &pCallContext->m_result.evex;
    e->vl = pInfo->vectorSize ? pInfo->vectorSize : 16;
    e->mask = pInfo->mask ? Di_GetK(pCallContext, pInfo->mask) : DI_EVEX_ALL_ONES;
    e->zeroing = pInfo->zeroing;
    e->rounding = pInfo->rounding;
}

static int Di_EvexIsMemory(DianaProcessor * pCallContext, int number)
{
    return pCallContext->m_result.linkedOperands[number].type == diana_index ||
           pCallContext->m_result.linkedOperands[number].type == diana_memory;
}

static int Di_EvexIsVector(DianaProcessor * pCallContext, int number)
{
    const DianaLinkedOperand * pOp = pCallContext->m_result.linkedOperands + number;
    return pOp->type == diana_register && Diana_VectorRegisterNumber(pOp->value.recognizedRegister, 0) >= 0;
}

static int Di_EvexIsK(DianaProcessor * pCallContext, int number)
{
    const DianaLinkedOperand * pOp = pCallContext->m_result.linkedOperands + number;
    return pOp->type == diana_register && pOp->value.recognizedRegister >= reg_K0 &&
           pOp->value.recognizedRegister <= reg_K7;
}

static int Di_EvexKNumber(DianaProcessor * pCallContext, int number)
{
    return pCallContext->m_result.linkedOperands[number].value.recognizedRegister - reg_K0;
}

static int Di_EvexImm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number)
{
    OPERAND_SIZE imm = 0;
    int size = 0;
    DianaProcessor_SetGetOperand(pDianaContext, pCallContext, number, &imm, 0, &size, 0);
    return (int)(imm & 0xff);
}

// the memory of operand number: selector, address, segment
static int Di_EvexAddress(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                          OPERAND_SIZE * pSelector, OPERAND_SIZE * pAddress, DianaUnifiedRegister * pSegment)
{
    const DianaLinkedOperand * pOp = pCallContext->m_result.linkedOperands + number;
    const DianaRmIndex * pIndex = pOp->type == diana_memory ? &pOp->value.memory.m_index : &pOp->value.rmIndex;
    *pSegment = pIndex->seg_reg;
    *pSelector = 0;
    return DianaProcessor_CalcIndex(pDianaContext, pCallContext, pIndex, pSelector, pAddress);
}

static int Di_EvexMemory(DianaProcessor * pCallContext, OPERAND_SIZE selector, OPERAND_SIZE address,
                         DianaUnifiedRegister segment, void * pData, int size, int bSet)
{
    OPERAND_SIZE done = 0;
    if (bSet)
    {
        DI_CHECK(pCallContext->m_pMemoryStream->pWriteFnc(pCallContext->m_pMemoryStream, selector, address, pData,
                                                          size, &done, pCallContext, segment));
    }
    else
    {
        DI_CHECK(pCallContext->m_pMemoryStream->pReadFnc(pCallContext->m_pMemoryStream, selector, address, pData,
                                                         size, &done, pCallContext, segment));
    }
    return done == (OPERAND_SIZE)size ? DI_SUCCESS : DI_ERROR;
}

// the alignment of an aligned move (vmovaps, vmovdqa64, vmovntdq...): #GP
static int Di_EvexAligned(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number, int bytes)
{
    OPERAND_SIZE selector = 0, address = 0;
    DianaUnifiedRegister segment;
    if (!Di_EvexIsMemory(pCallContext, number))
        return DI_SUCCESS;
    DI_CHECK(Di_EvexAddress(pDianaContext, pCallContext, number, &selector, &address, &segment));
    return address % (OPERAND_SIZE)bytes ? DI_GP : DI_SUCCESS;
}

// a source operand: a vector register in full (64 bytes), a general register (its value), an opmask; memory:
// "size" bytes (0: the size of the operand), with EVEX.b the element repeated to "size" bytes; the elements
// (elementSize) whose bit in readMask is 0 are not read (zero: a masked-off element doesn't fault)
static int Di_EvexSource(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                         DianaRegisterZMM_type * pValue, int size, int elementSize, OPERAND_SIZE readMask)
{
    const DianaLinkedOperand * pOp = pCallContext->m_result.linkedOperands + number;
    OPERAND_SIZE selector = 0, address = 0;
    DianaUnifiedRegister segment;
    int i;
    DIANA_MEMSET(pValue, 0, sizeof(*pValue));
    if (pOp->type == diana_register)
    {
        DianaUnifiedRegister reg = pOp->value.recognizedRegister;
        if (Diana_VectorRegisterNumber(reg, 0) >= 0)
        {
            *pValue = DianaProcessor_ZMM_GetValue(pCallContext, reg);
            return DI_SUCCESS;
        }
        if (reg >= reg_K0 && reg <= reg_K7)
        {
            pValue->u64[0] = Di_GetK(pCallContext, reg - reg_K0);
            return DI_SUCCESS;
        }
        {
            OPERAND_SIZE value = 0;
            int registerSize = 0;
            DI_CHECK(DianaProcessor_SetGetOperand(pDianaContext, pCallContext, number, &value, 0, &registerSize, 0));
            pValue->u64[0] = value;
            return DI_SUCCESS;
        }
    }
    if (!Di_EvexIsMemory(pCallContext, number))
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }
    if (!size || size > 64)
        size = pOp->usedSize;
    DI_CHECK(Di_EvexAddress(pDianaContext, pCallContext, number, &selector, &address, &segment));
    if (pCallContext->m_result.evex.broadcast)
    {
        int element = pOp->usedSize;
        if (!readMask)
            return DI_SUCCESS;
        DI_CHECK(Di_EvexMemory(pCallContext, selector, address, segment, pValue->u8, element, 0));
        for (i = element; i + element <= size; i += element)
            DIANA_MEMCPY(pValue->u8 + i, pValue->u8, element);
        return DI_SUCCESS;
    }
    if (!elementSize || (readMask & Di_LowBits(size / elementSize)) == Di_LowBits(size / elementSize))
        return Di_EvexMemory(pCallContext, selector, address, segment, pValue->u8, size, 0);
    for (i = 0; i < size / elementSize; ++i)
    {
        if ((readMask >> i) & 1)
            DI_CHECK(Di_EvexMemory(pCallContext, selector, address + i * elementSize, segment,
                                   pValue->u8 + i * elementSize, elementSize, 0));
    }
    return DI_SUCCESS;
}

// a source of the vector length with the opmask of the elements: the operand whose elements match the destination
static int Di_EvexVector(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                         DianaRegisterZMM_type * pValue, const DiEvex * e, int elementSize)
{
    return Di_EvexSource(pDianaContext, pCallContext, number, pValue, e->vl, elementSize, e->mask);
}

// a source of the vector length read whole (the elements don't match the destination: shuffles, permutes)
static int Di_EvexVectorAll(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                            DianaRegisterZMM_type * pValue, const DiEvex * e, int elementSize)
{
    elementSize;
    return Di_EvexSource(pDianaContext, pCallContext, number, pValue, e->vl, 0, DI_EVEX_ALL_ONES);
}

// the destination: a vector register gets the elements (elementSize) of pValue whose mask bit is 1, the others are
// zeroed ({z}) or kept, the bytes from "bytes" up zero; memory: the elements whose mask bit is 1 ("bytes" bytes); a
// general register: the low bytes of pValue (a 32-bit write zero-extends in 64-bit mode); an opmask: the low qword
static int Di_EvexWrite(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                        const DianaRegisterZMM_type * pValue, int bytes, int elementSize, OPERAND_SIZE mask,
                        int zeroing)
{
    const DianaLinkedOperand * pOp = pCallContext->m_result.linkedOperands + number;
    int i, count = elementSize ? bytes / elementSize : 0;
    if (pOp->type == diana_register)
    {
        DianaUnifiedRegister reg = pOp->value.recognizedRegister;
        if (Diana_VectorRegisterNumber(reg, 0) >= 0)
        {
            DianaRegisterZMM_type result = DianaProcessor_ZMM_GetValue(pCallContext, reg);
            for (i = 0; i < count; ++i)
            {
                if ((mask >> i) & 1)
                    DIANA_MEMCPY(result.u8 + i * elementSize, pValue->u8 + i * elementSize, elementSize);
                else if (zeroing)
                    DIANA_MEMSET(result.u8 + i * elementSize, 0, elementSize);
            }
            if (!count)
                DIANA_MEMCPY(result.u8, pValue->u8, bytes);
            DIANA_MEMSET(result.u8 + bytes, 0, 64 - bytes);
            DianaProcessor_ZMM_SetValue(pCallContext, reg, &result);
            return DI_SUCCESS;
        }
        if (reg >= reg_K0 && reg <= reg_K7)
        {
            Di_SetK(pCallContext, reg - reg_K0, pValue->u64[0]);
            return DI_SUCCESS;
        }
        {
            OPERAND_SIZE value = pValue->u64[0];
            int registerSize = 0;
            return DianaProcessor_SetGetOperand(pDianaContext, pCallContext, number, &value, 1, &registerSize, 0);
        }
    }
    if (!Di_EvexIsMemory(pCallContext, number))
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }
    {
        OPERAND_SIZE selector = 0, address = 0;
        DianaUnifiedRegister segment;
        DI_CHECK(Di_EvexAddress(pDianaContext, pCallContext, number, &selector, &address, &segment));
        if (!count || (mask & Di_LowBits(count)) == Di_LowBits(count))
            return Di_EvexMemory(pCallContext, selector, address, segment, (void *)pValue->u8, bytes, 1);
        for (i = 0; i < count; ++i)
        {
            if ((mask >> i) & 1)
                DI_CHECK(Di_EvexMemory(pCallContext, selector, address + i * elementSize, segment,
                                       (void *)(pValue->u8 + i * elementSize), elementSize, 1));
        }
        return DI_SUCCESS;
    }
}

static int Di_EvexDestination(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext,
                              const DianaRegisterZMM_type * pValue, const DiEvex * e, int elementSize)
{
    return Di_EvexWrite(pDianaContext, pCallContext, 0, pValue, e->vl, elementSize, e->mask, e->zeroing);
}

// ---------------------------------------------------------------------------------------------------------------
// the opmask instructions (VEX): the width in bits from the mnemonic (b w d q)

static int Di_KWidth(DianaProcessor * pCallContext)
{
    const char * name = pCallContext->m_result.pInfo->m_pGroupInfo->m_pName;
    int length = 0;
    while (name[length])
        ++length;
    switch (name[length - 1])
    {
    case 'b': return 8;
    case 'w': return 16;
    case 'd': return 32;
    default: return 64;
    }
}

typedef enum { DI_K_AND, DI_K_ANDN, DI_K_OR, DI_K_XOR, DI_K_XNOR, DI_K_ADD, DI_K_NOT } DiKOperation;

static int Di_KLogic(DianaProcessor * pCallContext, DiKOperation operation)
{
    int width = Di_KWidth(pCallContext);
    OPERAND_SIZE a, b = 0, r;
    a = Di_GetK(pCallContext, Di_EvexKNumber(pCallContext, 1));
    if (operation != DI_K_NOT)
        b = Di_GetK(pCallContext, Di_EvexKNumber(pCallContext, 2));
    switch (operation)
    {
    case DI_K_AND: r = a & b; break;
    case DI_K_ANDN: r = ~a & b; break;
    case DI_K_OR: r = a | b; break;
    case DI_K_XOR: r = a ^ b; break;
    case DI_K_XNOR: r = ~(a ^ b); break;
    case DI_K_ADD: r = a + b; break;
    default: r = ~a; break;
    }
    Di_SetK(pCallContext, Di_EvexKNumber(pCallContext, 0), r & Di_LowBits(width));
    DI_PROC_END
}

#define DI_K_LOGIC(name, operation) \
static int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ pDianaContext; return Di_KLogic(pCallContext, operation); }

DI_K_LOGIC(kandb, DI_K_AND) DI_K_LOGIC(kandw, DI_K_AND) DI_K_LOGIC(kandd, DI_K_AND) DI_K_LOGIC(kandq, DI_K_AND)
DI_K_LOGIC(kandnb, DI_K_ANDN) DI_K_LOGIC(kandnw, DI_K_ANDN) DI_K_LOGIC(kandnd, DI_K_ANDN) DI_K_LOGIC(kandnq, DI_K_ANDN)
DI_K_LOGIC(korb, DI_K_OR) DI_K_LOGIC(korw, DI_K_OR) DI_K_LOGIC(kord, DI_K_OR) DI_K_LOGIC(korq, DI_K_OR)
DI_K_LOGIC(kxorb, DI_K_XOR) DI_K_LOGIC(kxorw, DI_K_XOR) DI_K_LOGIC(kxord, DI_K_XOR) DI_K_LOGIC(kxorq, DI_K_XOR)
DI_K_LOGIC(kxnorb, DI_K_XNOR) DI_K_LOGIC(kxnorw, DI_K_XNOR) DI_K_LOGIC(kxnord, DI_K_XNOR) DI_K_LOGIC(kxnorq, DI_K_XNOR)
DI_K_LOGIC(kaddb, DI_K_ADD) DI_K_LOGIC(kaddw, DI_K_ADD) DI_K_LOGIC(kaddd, DI_K_ADD) DI_K_LOGIC(kaddq, DI_K_ADD)
DI_K_LOGIC(knotb, DI_K_NOT) DI_K_LOGIC(knotw, DI_K_NOT) DI_K_LOGIC(knotd, DI_K_NOT) DI_K_LOGIC(knotq, DI_K_NOT)

// kmov*: k from k, memory or a general register; to memory or a general register (zero-extended)
static int Di_KMov(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int width = Di_KWidth(pCallContext);
    DianaRegisterZMM_type value;
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &value, width / 8, 0, DI_EVEX_ALL_ONES));
    value.u64[0] &= Di_LowBits(width);
    if (Di_EvexIsMemory(pCallContext, 0))
        DI_CHECK(Di_EvexWrite(pDianaContext, pCallContext, 0, &value, width / 8, 0, DI_EVEX_ALL_ONES, 0))
    else
        DI_CHECK(Di_EvexWrite(pDianaContext, pCallContext, 0, &value, 8, 0, DI_EVEX_ALL_ONES, 0))
    DI_PROC_END
}

static int Diana_Call_kmovb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KMov(pDianaContext, pCallContext); }
static int Diana_Call_kmovw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KMov(pDianaContext, pCallContext); }
static int Diana_Call_kmovd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KMov(pDianaContext, pCallContext); }
static int Diana_Call_kmovq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KMov(pDianaContext, pCallContext); }

// kortest: ZF the or is 0, CF all ones; ktest: ZF a and b is 0, CF not a and b is 0; OF SF AF PF cleared
static int Di_KTest(DianaProcessor * pCallContext, int isTest)
{
    int width = Di_KWidth(pCallContext);
    OPERAND_SIZE a = Di_GetK(pCallContext, Di_EvexKNumber(pCallContext, 0)) & Di_LowBits(width);
    OPERAND_SIZE b = Di_GetK(pCallContext, Di_EvexKNumber(pCallContext, 1)) & Di_LowBits(width);
    int zf = isTest ? !(a & b) : !(a | b);
    int cf = isTest ? !(~a & b & Di_LowBits(width)) : (a | b) == Di_LowBits(width);
    CLEAR_FLAG_OF;
    CLEAR_FLAG_SF;
    CLEAR_FLAG_AF;
    CLEAR_FLAG_PF;
    if (zf) SET_FLAG_ZF; else CLEAR_FLAG_ZF;
    if (cf) SET_FLAG_CF; else CLEAR_FLAG_CF;
    DI_PROC_END
}

static int Diana_Call_kortestb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KTest(pCallContext, 0); }
static int Diana_Call_kortestw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KTest(pCallContext, 0); }
static int Diana_Call_kortestd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KTest(pCallContext, 0); }
static int Diana_Call_kortestq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KTest(pCallContext, 0); }
static int Diana_Call_ktestb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KTest(pCallContext, 1); }
static int Diana_Call_ktestw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KTest(pCallContext, 1); }
static int Diana_Call_ktestd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KTest(pCallContext, 1); }
static int Diana_Call_ktestq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KTest(pCallContext, 1); }

// kshiftl, kshiftr: the count is imm8, the bits shifted out of the width are gone
static int Di_KShift(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int left)
{
    int width = Di_KWidth(pCallContext);
    int count = Di_EvexImm(pDianaContext, pCallContext, 2);
    OPERAND_SIZE a = Di_GetK(pCallContext, Di_EvexKNumber(pCallContext, 1)) & Di_LowBits(width), r = 0;
    if (count < width)
        r = (left ? a << count : a >> count) & Di_LowBits(width);
    Di_SetK(pCallContext, Di_EvexKNumber(pCallContext, 0), r);
    DI_PROC_END
}

static int Diana_Call_kshiftlb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KShift(pDianaContext, pCallContext, 1); }
static int Diana_Call_kshiftlw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KShift(pDianaContext, pCallContext, 1); }
static int Diana_Call_kshiftld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KShift(pDianaContext, pCallContext, 1); }
static int Diana_Call_kshiftlq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KShift(pDianaContext, pCallContext, 1); }
static int Diana_Call_kshiftrb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KShift(pDianaContext, pCallContext, 0); }
static int Diana_Call_kshiftrw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KShift(pDianaContext, pCallContext, 0); }
static int Diana_Call_kshiftrd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KShift(pDianaContext, pCallContext, 0); }
static int Diana_Call_kshiftrq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Di_KShift(pDianaContext, pCallContext, 0); }

// kunpckbw, kunpckwd, kunpckdq: the low half of the source 2 under the low half of the source 1
static int Di_KUnpack(DianaProcessor * pCallContext, int half)
{
    OPERAND_SIZE a = Di_GetK(pCallContext, Di_EvexKNumber(pCallContext, 1)) & Di_LowBits(half);
    OPERAND_SIZE b = Di_GetK(pCallContext, Di_EvexKNumber(pCallContext, 2)) & Di_LowBits(half);
    Di_SetK(pCallContext, Di_EvexKNumber(pCallContext, 0), (half == 32 ? a << 32 : a << half) | b);
    DI_PROC_END
}

static int Diana_Call_kunpckbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KUnpack(pCallContext, 8); }
static int Diana_Call_kunpckwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KUnpack(pCallContext, 16); }
static int Diana_Call_kunpckdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ pDianaContext; return Di_KUnpack(pCallContext, 32); }

void Diana_Avx512_RegisterCommands(DianaGroupInfo * p)
{
    switch (p->m_commandId)
    {
    DI_PROC_REGISTER_COMMAND(kandb) DI_PROC_REGISTER_COMMAND(kandw)
    DI_PROC_REGISTER_COMMAND(kandd) DI_PROC_REGISTER_COMMAND(kandq)
    DI_PROC_REGISTER_COMMAND(kandnb) DI_PROC_REGISTER_COMMAND(kandnw)
    DI_PROC_REGISTER_COMMAND(kandnd) DI_PROC_REGISTER_COMMAND(kandnq)
    DI_PROC_REGISTER_COMMAND(korb) DI_PROC_REGISTER_COMMAND(korw)
    DI_PROC_REGISTER_COMMAND(kord) DI_PROC_REGISTER_COMMAND(korq)
    DI_PROC_REGISTER_COMMAND(kxorb) DI_PROC_REGISTER_COMMAND(kxorw)
    DI_PROC_REGISTER_COMMAND(kxord) DI_PROC_REGISTER_COMMAND(kxorq)
    DI_PROC_REGISTER_COMMAND(kxnorb) DI_PROC_REGISTER_COMMAND(kxnorw)
    DI_PROC_REGISTER_COMMAND(kxnord) DI_PROC_REGISTER_COMMAND(kxnorq)
    DI_PROC_REGISTER_COMMAND(kaddb) DI_PROC_REGISTER_COMMAND(kaddw)
    DI_PROC_REGISTER_COMMAND(kaddd) DI_PROC_REGISTER_COMMAND(kaddq)
    DI_PROC_REGISTER_COMMAND(knotb) DI_PROC_REGISTER_COMMAND(knotw)
    DI_PROC_REGISTER_COMMAND(knotd) DI_PROC_REGISTER_COMMAND(knotq)
    DI_PROC_REGISTER_COMMAND(kmovb) DI_PROC_REGISTER_COMMAND(kmovw)
    DI_PROC_REGISTER_COMMAND(kmovd) DI_PROC_REGISTER_COMMAND(kmovq)
    DI_PROC_REGISTER_COMMAND(kortestb) DI_PROC_REGISTER_COMMAND(kortestw)
    DI_PROC_REGISTER_COMMAND(kortestd) DI_PROC_REGISTER_COMMAND(kortestq)
    DI_PROC_REGISTER_COMMAND(ktestb) DI_PROC_REGISTER_COMMAND(ktestw)
    DI_PROC_REGISTER_COMMAND(ktestd) DI_PROC_REGISTER_COMMAND(ktestq)
    DI_PROC_REGISTER_COMMAND(kshiftlb) DI_PROC_REGISTER_COMMAND(kshiftlw)
    DI_PROC_REGISTER_COMMAND(kshiftld) DI_PROC_REGISTER_COMMAND(kshiftlq)
    DI_PROC_REGISTER_COMMAND(kshiftrb) DI_PROC_REGISTER_COMMAND(kshiftrw)
    DI_PROC_REGISTER_COMMAND(kshiftrd) DI_PROC_REGISTER_COMMAND(kshiftrq)
    DI_PROC_REGISTER_COMMAND(kunpckbw) DI_PROC_REGISTER_COMMAND(kunpckwd)
    DI_PROC_REGISTER_COMMAND(kunpckdq)
    default:
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// moves

// vmovaps vmovups vmovdqa32 vmovdqu8...: load, store, register; aligned: the vector length
static int Di_EvexMove(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                       int aligned)
{
    DiEvex e;
    DianaRegisterZMM_type value;
    Di_EvexInit(pCallContext, &e);
    if (aligned)
    {
        DI_CHECK(Di_EvexAligned(pDianaContext, pCallContext, 0, e.vl));
        DI_CHECK(Di_EvexAligned(pDianaContext, pCallContext, 1, e.vl));
    }
    DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 1, &value, &e, elementSize));
    DI_CHECK(Di_EvexDestination(pDianaContext, pCallContext, &value, &e, elementSize));
    DI_PROC_END
}

// vmovss, vmovsd: a load zeroes the elements above 0, a store writes element 0, the register form takes the
// elements above 0 from the source 1
static int Di_EvexMoveScalar(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type value, high;
    Di_EvexInit(pCallContext, &e);
    if (pCallContext->m_result.iLinkedOpCount == 3)
    {
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &high, 16, 0, DI_EVEX_ALL_ONES));
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &value, 16, 0, DI_EVEX_ALL_ONES));
        DIANA_MEMCPY(high.u8, value.u8, elementSize);
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &high, 16, elementSize, e.mask | ~(OPERAND_SIZE)1,
                            e.zeroing);
    }
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &value, elementSize, elementSize, e.mask & 1));
    if (Di_EvexIsMemory(pCallContext, 0))
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &value, elementSize, elementSize, e.mask & 1, 0);
    DIANA_MEMSET(value.u8 + elementSize, 0, 64 - elementSize);
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &value, 16, elementSize, e.mask | ~(OPERAND_SIZE)1,
                        e.zeroing);
}

// vmovd, vmovq (no opmask): to a vector register the low bytes, the rest zero; to a general register or memory the
// low bytes
static int Di_EvexMoveLow(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int size)
{
    DianaRegisterZMM_type value;
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &value, size, 0, DI_EVEX_ALL_ONES));
    DIANA_MEMSET(value.u8 + size, 0, 64 - size);
    if (Di_EvexIsVector(pCallContext, 0))
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &value, 16, 0, DI_EVEX_ALL_ONES, 0);
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &value, size, 0, DI_EVEX_ALL_ONES, 0);
}

// vmovhps vmovhpd vmovlps vmovlpd vmovhlps vmovlhps (no opmask)
static int Di_EvexMoveHalf(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int high,
                           int registers)
{
    DianaRegisterZMM_type a, b, r;
    if (pCallContext->m_result.iLinkedOpCount == 2)
    {
        // the store: the high (low) qword
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, 16, 0, DI_EVEX_ALL_ONES));
        DIANA_MEMSET(&r, 0, sizeof(r));
        r.u64[0] = a.u64[high ? 1 : 0];
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 8, 0, DI_EVEX_ALL_ONES, 0);
    }
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, 16, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, registers ? 16 : 8, 0, DI_EVEX_ALL_ONES));
    DIANA_MEMSET(&r, 0, sizeof(r));
    if (registers)
    {
        // vmovhlps: b high, a high; vmovlhps: a low, b low
        r.u64[0] = high ? b.u64[1] : a.u64[0];
        r.u64[1] = high ? a.u64[1] : b.u64[0];
    }
    else
    {
        // vmovhps: a low, m64; vmovlps: m64, a high
        r.u64[0] = high ? a.u64[0] : b.u64[0];
        r.u64[1] = high ? b.u64[0] : a.u64[1];
    }
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 16, 0, DI_EVEX_ALL_ONES, 0);
}

// vmovddup (the even qword of each pair; the 128-bit form reads m64), vmovsldup, vmovshdup (the even, odd dwords)
static int Di_EvexDuplicate(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                            int odd)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int i;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, elementSize == 8 && e.vl == 16 ? 8 : e.vl, 0,
                           DI_EVEX_ALL_ONES));
    for (i = 0; i < e.vl / elementSize; i += 2)
    {
        if (elementSize == 8)
            r.u64[i] = r.u64[i + 1] = a.u64[i];
        else
            r.u32[i] = r.u32[i + 1] = a.u32[i + odd];
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// ---------------------------------------------------------------------------------------------------------------
// broadcasts

// vbroadcastss/sd, vpbroadcastb/w/d/q (an element of a vector register, memory or a general register),
// vbroadcastf32x2/x4/x8, f64x2/x4 (a block of "block" bytes)
static int Di_EvexBroadcast(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                            int block)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int i;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, block, 0, DI_EVEX_ALL_ONES));
    for (i = 0; i < e.vl; i += block)
        DIANA_MEMCPY(r.u8 + i, a.u8, block);
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpbroadcastmb2q, vpbroadcastmw2d: the low byte (word) of the opmask zero-extended to every element
static int Di_EvexBroadcastMask(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type r;
    OPERAND_SIZE k = Di_GetK(pCallContext, Di_EvexKNumber(pCallContext, 1));
    int i;
    Di_EvexInit(pCallContext, &e);
    for (i = 0; i < e.vl / elementSize; ++i)
    {
        if (elementSize == 8)
            r.u64[i] = k & 0xff;
        else
            r.u32[i] = (DI_UINT32)(k & 0xffff);
    }
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, e.vl, elementSize, DI_EVEX_ALL_ONES, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// integer

// r = a op b element by element: the source 1 (vvvv), the source 2 (r/m, the broadcast)
static int Di_EvexInt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                      DiIntOperation operation)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 2, &b, &e, elementSize));
    if (operation == DI_INT_MULLO && elementSize == 8)
    {
        // vpmullq: the low qword of the product
        int i;
        for (i = 0; i < e.vl / 8; ++i)
            r.u64[i] = a.u64[i] * b.u64[i];
    }
    else
        Diana_Int_Elementwise(&r, &a, &b, e.vl, elementSize, operation);
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

static int Di_EvexAbs(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 1, &a, &e, elementSize));
    Diana_Int_Abs(&r, &a, e.vl, elementSize);
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpternlogd/q: every bit of the result is bit (dest << 2 | src1 << 1 | src2) of imm8
static int Di_EvexTernaryLogic(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, c, r;
    int i, bit, imm;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 0, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &b, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 2, &c, &e, elementSize));
    imm = Di_EvexImm(pDianaContext, pCallContext, 3);
    for (i = 0; i < e.vl / 8; ++i)
    {
        OPERAND_SIZE result = 0;
        for (bit = 0; bit < 64; ++bit)
        {
            int index = (int)(((a.u64[i] >> bit) & 1) << 2 | ((b.u64[i] >> bit) & 1) << 1 | ((c.u64[i] >> bit) & 1));
            result |= (OPERAND_SIZE)((imm >> index) & 1) << bit;
        }
        r.u64[i] = result;
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpblendm*, vblendmps/pd: the elements of the source 2 where the mask bit is 1, else of the source 1 ({z}: zero)
static int Di_EvexBlendMask(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    int i;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 2, &b, &e, elementSize));
    for (i = 0; i < e.vl / elementSize; ++i)
        DIANA_MEMCPY(r.u8 + i * elementSize, ((e.mask >> i) & 1 ? &b : &a)->u8 + i * elementSize, elementSize);
    if (e.zeroing)
        return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, e.vl, elementSize, DI_EVEX_ALL_ONES, 0);
}

// vpmaddwd, vpmaddubsw, vpmuldq, vpmuludq, vpsadbw (no opmask), vpacks*, vpunpck*, vpshufb: a core of two sources
typedef enum
{
    DI_EVEX_MADDWD, DI_EVEX_MADDUBSW, DI_EVEX_MULDQ, DI_EVEX_MULUDQ, DI_EVEX_SADBW, DI_EVEX_PACKSSWB,
    DI_EVEX_PACKUSWB, DI_EVEX_PACKSSDW, DI_EVEX_PACKUSDW, DI_EVEX_UNPCKL, DI_EVEX_UNPCKH, DI_EVEX_SHUFB
} DiEvexTwo;

static int Di_EvexTwoSources(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiEvexTwo kind,
                             int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, e.vl, 0, DI_EVEX_ALL_ONES));
    switch (kind)
    {
    case DI_EVEX_MADDWD: Diana_Int_MultiplyAdd(&r, &a, &b, e.vl, 0); break;
    case DI_EVEX_MADDUBSW: Diana_Int_MultiplyAdd(&r, &a, &b, e.vl, 1); break;
    case DI_EVEX_MULDQ: Diana_Int_MultiplyEven(&r, &a, &b, e.vl, 1); break;
    case DI_EVEX_MULUDQ: Diana_Int_MultiplyEven(&r, &a, &b, e.vl, 0); break;
    case DI_EVEX_SADBW: Diana_Int_SumAbsoluteDifferences(&r, &a, &b, e.vl); break;
    case DI_EVEX_PACKSSWB: Diana_Int_Pack(&r, &a, &b, e.vl, 2, 0); break;
    case DI_EVEX_PACKUSWB: Diana_Int_Pack(&r, &a, &b, e.vl, 2, 1); break;
    case DI_EVEX_PACKSSDW: Diana_Int_Pack(&r, &a, &b, e.vl, 4, 0); break;
    case DI_EVEX_PACKUSDW: Diana_Int_Pack(&r, &a, &b, e.vl, 4, 1); break;
    case DI_EVEX_UNPCKL: Diana_Int_Unpack(&r, &a, &b, e.vl, elementSize, 0); break;
    case DI_EVEX_UNPCKH: Diana_Int_Unpack(&r, &a, &b, e.vl, elementSize, 1); break;
    default: Diana_Int_ShuffleBytes(&r, &a, &b, e.vl); break;
    }
    if (kind == DI_EVEX_SADBW)
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, e.vl, 0, DI_EVEX_ALL_ONES, 0);
    if (kind == DI_EVEX_MADDWD || kind == DI_EVEX_PACKSSDW || kind == DI_EVEX_PACKUSDW)
        elementSize = kind == DI_EVEX_MADDWD ? 4 : 2;
    else if (kind == DI_EVEX_MADDUBSW)
        elementSize = 2;
    else if (kind == DI_EVEX_MULDQ || kind == DI_EVEX_MULUDQ)
        elementSize = 8;
    else if (kind == DI_EVEX_PACKSSWB || kind == DI_EVEX_PACKUSWB || kind == DI_EVEX_SHUFB)
        elementSize = 1;
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vdbpsadbw: the dwords of the source 2 selected by imm8 per lane, then per qword the sums of absolute differences
// of 4 bytes (the source 1 bytes 0..3 with the selected 0..3 and 1..4, the bytes 4..7 with 2..5 and 3..6)
static int Di_EvexDbpsadbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, t, r;
    int imm, i, j, k;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, e.vl, 0, DI_EVEX_ALL_ONES));
    imm = Di_EvexImm(pDianaContext, pCallContext, 3);
    Diana_Int_ShuffleImm(&t, &b, e.vl, DI_SHUFFLE_D, imm);
    for (i = 0; i < e.vl; i += 8)
    {
        static const int aStart[4] = {0, 0, 4, 4}, tStart[4] = {0, 1, 2, 3};
        for (j = 0; j < 4; ++j)
        {
            unsigned sum = 0;
            for (k = 0; k < 4; ++k)
            {
                int x = a.u8[i + aStart[j] + k], y = t.u8[i + tStart[j] + k];
                sum += (unsigned)(x > y ? x - y : y - x);
            }
            r.u16[i / 2 + j] = (DI_UINT16)sum;
        }
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, 2);
}

// vpconflictd/q: per element the bits of the lower elements equal to it; vplzcntd/q
static int Di_EvexConflict(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                           int lzcnt)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int i, j, count;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexVectorAll(pDianaContext, pCallContext, 1, &a, &e, elementSize));
    count = e.vl / elementSize;
    for (i = 0; i < count; ++i)
    {
        OPERAND_SIZE x = elementSize == 8 ? a.u64[i] : a.u32[i], result = 0;
        if (lzcnt)
        {
            int bits = elementSize * 8;
            result = bits;
            for (j = bits - 1; j >= 0; --j)
                if ((x >> j) & 1)
                {
                    result = bits - 1 - j;
                    break;
                }
        }
        else
        {
            for (j = 0; j < i; ++j)
                if ((elementSize == 8 ? a.u64[j] : a.u32[j]) == x)
                    result |= (OPERAND_SIZE)1 << j;
        }
        if (elementSize == 8)
            r.u64[i] = result;
        else
            r.u32[i] = (DI_UINT32)result;
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// ---------------------------------------------------------------------------------------------------------------
// compares into an opmask (the bits of the masked-off elements and above the elements are 0)

static OPERAND_SIZE Di_EvexGetElement(const DianaRegisterZMM_type * p, int i, int size)
{
    switch (size)
    {
    case 1: return p->u8[i];
    case 2: return p->u16[i];
    case 4: return p->u32[i];
    default: return p->u64[i];
    }
}

static OPERAND_SIZE_SIGNED Di_EvexGetSigned(const DianaRegisterZMM_type * p, int i, int size)
{
    switch (size)
    {
    case 1: return p->s8[i];
    case 2: return p->s16[i];
    case 4: return p->s32[i];
    default: return p->s64[i];
    }
}

static void Di_EvexSetElement(DianaRegisterZMM_type * p, int i, int size, OPERAND_SIZE value)
{
    switch (size)
    {
    case 1: p->u8[i] = (DI_CHAR)value; break;
    case 2: p->u16[i] = (DI_UINT16)value; break;
    case 4: p->u32[i] = (DI_UINT32)value; break;
    default: p->u64[i] = value; break;
    }
}

// vpcmp[u]* (imm8: eq lt le false neq nlt nle true), vpcmpeq*, vpcmpgt*, vptestm*, vptestnm*
typedef enum { DI_EVEX_CMP_IMM, DI_EVEX_CMP_IMMU, DI_EVEX_CMP_EQ, DI_EVEX_CMP_GT, DI_EVEX_TESTM, DI_EVEX_TESTNM } DiEvexCompare;

static int Di_EvexCompareInt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                             DiEvexCompare kind)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    int i, predicate = 0, count;
    OPERAND_SIZE result = 0;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 2, &b, &e, elementSize));
    if (kind == DI_EVEX_CMP_IMM || kind == DI_EVEX_CMP_IMMU)
        predicate = Di_EvexImm(pDianaContext, pCallContext, 3) & 7;
    count = e.vl / elementSize;
    for (i = 0; i < count; ++i)
    {
        int bit = 0;
        OPERAND_SIZE ua = Di_EvexGetElement(&a, i, elementSize), ub = Di_EvexGetElement(&b, i, elementSize);
        OPERAND_SIZE_SIGNED sa = Di_EvexGetSigned(&a, i, elementSize), sb = Di_EvexGetSigned(&b, i, elementSize);
        int less = kind == DI_EVEX_CMP_IMMU ? ua < ub : sa < sb;
        switch (kind)
        {
        case DI_EVEX_CMP_EQ: bit = ua == ub; break;
        case DI_EVEX_CMP_GT: bit = sa > sb; break;
        case DI_EVEX_TESTM: bit = (ua & ub) != 0; break;
        case DI_EVEX_TESTNM: bit = (ua & ub) == 0; break;
        default:
            switch (predicate)
            {
            case 0: bit = ua == ub; break;
            case 1: bit = less; break;
            case 2: bit = less || ua == ub; break;
            case 3: bit = 0; break;
            case 4: bit = ua != ub; break;
            case 5: bit = !less; break;
            case 6: bit = !less && ua != ub; break;
            default: bit = 1; break;
            }
        }
        if (bit && ((e.mask >> i) & 1))
            result |= (OPERAND_SIZE)1 << i;
    }
    r.u64[0] = result;
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 8, 0, DI_EVEX_ALL_ONES, 0);
}

// vpmovb2m/w2m/d2m/q2m: the sign bits; vpmovm2b/w/d/q: all ones where the opmask bit is 1
static int Di_EvexMaskMove(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                           int toMask)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int i, count;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    count = e.vl / elementSize;
    DIANA_MEMSET(&r, 0, sizeof(r));
    for (i = 0; i < count; ++i)
    {
        if (toMask)
        {
            if (Di_EvexGetSigned(&a, i, elementSize) < 0)
                r.u64[0] |= (OPERAND_SIZE)1 << i;
        }
        else
            Di_EvexSetElement(&r, i, elementSize, (a.u64[0] >> i) & 1 ? DI_EVEX_ALL_ONES : 0);
    }
    if (toMask)
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 8, 0, DI_EVEX_ALL_ONES, 0);
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, e.vl, elementSize, DI_EVEX_ALL_ONES, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// shifts

// by the count in the low qword of an xmm or m128 (vpsllw zmm1, zmm2, xmm3/m128), by imm8 (vpsllw zmm1, zmm2/m512,
// imm8: vvvv is the destination)
static int Di_EvexShift(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                        DiIntShift shift)
{
    DiEvex e;
    DianaRegisterZMM_type a, count, r;
    OPERAND_SIZE n;
    Di_EvexInit(pCallContext, &e);
    if (pCallContext->m_result.linkedOperands[2].type == diana_imm)
    {
        DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 1, &a, &e, elementSize));
        n = (OPERAND_SIZE)Di_EvexImm(pDianaContext, pCallContext, 2);
    }
    else
    {
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &count, 16, 0, DI_EVEX_ALL_ONES));
        n = count.u64[0];
    }
    Diana_Int_Shift(&r, &a, e.vl, elementSize, shift, n);
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpsllv*, vpsrlv*, vpsrav*: per element
static int Di_EvexShiftVariable(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                                DiIntShift shift)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 2, &b, &e, elementSize));
    Diana_Int_ShiftVariable(&r, &a, &b, e.vl, elementSize, shift);
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vprold/q, vprord/q (imm8, vvvv the destination), vprolvd/q, vprorvd/q: rotates by the count modulo the bits
static int Di_EvexRotate(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                         int left, int variable)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    int i, bits = elementSize * 8, imm = 0;
    Di_EvexInit(pCallContext, &e);
    if (variable)
    {
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
        DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 2, &b, &e, elementSize));
    }
    else
    {
        DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 1, &a, &e, elementSize));
        imm = Di_EvexImm(pDianaContext, pCallContext, 2);
    }
    for (i = 0; i < e.vl / elementSize; ++i)
    {
        OPERAND_SIZE x = Di_EvexGetElement(&a, i, elementSize);
        int n = (int)((variable ? Di_EvexGetElement(&b, i, elementSize) : (OPERAND_SIZE)imm) % (OPERAND_SIZE)bits);
        if (!left)
            n = (bits - n) % bits;
        if (n)
            x = (x << n) | (x >> (bits - n));
        Di_EvexSetElement(&r, i, elementSize, x & Di_LowBits(bits));
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpslldq, vpsrldq (no opmask; vvvv the destination)
static int Di_EvexShiftBytes(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int left)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    Diana_Int_ShiftBytes(&r, &a, e.vl, left, Di_EvexImm(pDianaContext, pCallContext, 2));
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, e.vl, 0, DI_EVEX_ALL_ONES, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// shuffles and permutes

// vpshufd, vpshufhw, vpshuflw (imm8 per lane)
static int Di_EvexShuffleImm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiIntShuffle shuffle)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int elementSize = shuffle == DI_SHUFFLE_D ? 4 : 2;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexVectorAll(pDianaContext, pCallContext, 1, &a, &e, elementSize));
    Diana_Int_ShuffleImm(&r, &a, e.vl, shuffle, Di_EvexImm(pDianaContext, pCallContext, 2));
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpalignr (per lane, bytes), valignd/q (the whole vector: source 1 above source 2, shifted right by imm8 elements)
static int Di_EvexAlign(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    int imm, i, count;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, e.vl, 0, DI_EVEX_ALL_ONES));
    imm = Di_EvexImm(pDianaContext, pCallContext, 3);
    if (elementSize == 1)
        Diana_Int_Align(&r, &a, &b, e.vl, imm);
    else
    {
        count = e.vl / elementSize;
        imm &= count - 1;
        for (i = 0; i < count; ++i)
        {
            int from = i + imm;
            const DianaRegisterZMM_type * p = from < count ? &b : &a;
            DIANA_MEMCPY(r.u8 + i * elementSize, p->u8 + (from % count) * elementSize, elementSize);
        }
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vshufps (dwords: two of the source 1, two of the source 2 per lane), vshufpd (a bit an element)
static int Di_EvexShufp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    int imm, lane, i;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexVectorAll(pDianaContext, pCallContext, 2, &b, &e, elementSize));
    imm = Di_EvexImm(pDianaContext, pCallContext, 3);
    for (lane = 0; lane < e.vl / 16; ++lane)
    {
        if (elementSize == 4)
            for (i = 0; i < 4; ++i)
                r.u32[lane * 4 + i] = (i < 2 ? &a : &b)->u32[lane * 4 + ((imm >> (2 * i)) & 3)];
        else
            for (i = 0; i < 2; ++i)
                r.u64[lane * 2 + i] = (i ? &b : &a)->u64[lane * 2 + ((imm >> (lane * 2 + i)) & 1)];
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vshuff32x4, vshuff64x2, vshufi32x4, vshufi64x2: 128-bit blocks, the low half of the destination from the source 1
static int Di_EvexShuffleBlocks(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    int imm, i, lanes;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexVectorAll(pDianaContext, pCallContext, 2, &b, &e, elementSize));
    imm = Di_EvexImm(pDianaContext, pCallContext, 3);
    lanes = e.vl / 16;
    for (i = 0; i < lanes; ++i)
    {
        int select = lanes == 4 ? (imm >> (2 * i)) & 3 : (imm >> i) & 1;
        r.xmm[i] = (i < lanes / 2 ? &a : &b)->xmm[select];
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpermd/ps/q/pd/w (the index in the source 1, the table the source 2), vpermq/pd imm8 (per 256-bit half)
static int Di_EvexPermute(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type index, table, r;
    int i, count;
    Di_EvexInit(pCallContext, &e);
    count = e.vl / elementSize;
    if (pCallContext->m_result.linkedOperands[2].type == diana_imm)
    {
        int imm = Di_EvexImm(pDianaContext, pCallContext, 2);
        DI_CHECK(Di_EvexVectorAll(pDianaContext, pCallContext, 1, &table, &e, elementSize));
        for (i = 0; i < count; ++i)
            r.u64[i] = table.u64[(i & ~3) + ((imm >> (2 * (i & 3))) & 3)];
    }
    else
    {
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &index, e.vl, 0, DI_EVEX_ALL_ONES));
        DI_CHECK(Di_EvexVectorAll(pDianaContext, pCallContext, 2, &table, &e, elementSize));
        for (i = 0; i < count; ++i)
        {
            int from = (int)(Di_EvexGetElement(&index, i, elementSize) & (OPERAND_SIZE)(count - 1));
            Di_EvexSetElement(&r, i, elementSize, Di_EvexGetElement(&table, from, elementSize));
        }
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpermi2* (the destination is the index: tables the sources 1 and 2), vpermt2* (the destination is table 1, the
// source 1 the index, the source 2 table 2): the index bit log2(count) selects the table
static int Di_EvexPermute2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                           int indexIsDestination)
{
    DiEvex e;
    DianaRegisterZMM_type d, s1, s2, r;
    int i, count;
    Di_EvexInit(pCallContext, &e);
    count = e.vl / elementSize;
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 0, &d, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &s1, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexVectorAll(pDianaContext, pCallContext, 2, &s2, &e, elementSize));
    for (i = 0; i < count; ++i)
    {
        const DianaRegisterZMM_type * pIndex = indexIsDestination ? &d : &s1;
        const DianaRegisterZMM_type * pTable1 = indexIsDestination ? &s1 : &d;
        OPERAND_SIZE index = Di_EvexGetElement(pIndex, i, elementSize);
        int from = (int)(index & (OPERAND_SIZE)(count - 1));
        Di_EvexSetElement(&r, i, elementSize,
                          Di_EvexGetElement(index & (OPERAND_SIZE)count ? &s2 : pTable1, from, elementSize));
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpermilps/pd: per lane, the control in the source 2 (bits 1:0 of a dword, bit 1 of a qword) or imm8
static int Di_EvexPermil(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, control, r;
    int i, imm = -1, per = 16 / elementSize;
    Di_EvexInit(pCallContext, &e);
    if (pCallContext->m_result.linkedOperands[2].type == diana_imm)
    {
        DI_CHECK(Di_EvexVectorAll(pDianaContext, pCallContext, 1, &a, &e, elementSize));
        imm = Di_EvexImm(pDianaContext, pCallContext, 2);
    }
    else
    {
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
        DI_CHECK(Di_EvexVectorAll(pDianaContext, pCallContext, 2, &control, &e, elementSize));
    }
    for (i = 0; i < e.vl / elementSize; ++i)
    {
        int lane = i - i % per, select;
        if (elementSize == 4)
        {
            select = imm >= 0 ? (imm >> (2 * (i % 4))) & 3 : (int)(control.u32[i] & 3);
            r.u32[i] = a.u32[lane + select];
        }
        else
        {
            select = imm >= 0 ? (imm >> (i % 8)) & 1 : (int)((control.u64[i] >> 1) & 1);
            r.u64[i] = a.u64[lane + select];
        }
    }
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vinsert*x4, *x2, *x8: the source 1 with the block imm8 selects from the source 2
static int Di_EvexInsert(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                         int block)
{
    DiEvex e;
    DianaRegisterZMM_type a, b;
    int imm;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, block, 0, DI_EVEX_ALL_ONES));
    imm = Di_EvexImm(pDianaContext, pCallContext, 3) & (e.vl / block - 1);
    DIANA_MEMCPY(a.u8 + imm * block, b.u8, block);
    return Di_EvexDestination(pDianaContext, pCallContext, &a, &e, elementSize);
}

// vextract*: the block imm8 selects to an xmm, ymm or memory
static int Di_EvexExtract(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                          int block)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int imm;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    imm = Di_EvexImm(pDianaContext, pCallContext, 2) & (e.vl / block - 1);
    DIANA_MEMSET(&r, 0, sizeof(r));
    DIANA_MEMCPY(r.u8, a.u8 + imm * block, block);
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, block, elementSize, e.mask,
                        e.zeroing && !Di_EvexIsMemory(pCallContext, 0));
}

// vinsertps (EVEX: no opmask): the element imm8[7:6] of the source 2 (a register; memory: m32) to imm8[5:4],
// zero the elements of imm8[3:0]
static int Di_EvexInsertps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterZMM_type a, b;
    int imm, i;
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, 16, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, 4, 0, DI_EVEX_ALL_ONES));
    imm = Di_EvexImm(pDianaContext, pCallContext, 3);
    a.u32[(imm >> 4) & 3] = Di_EvexIsMemory(pCallContext, 2) ? b.u32[0] : b.u32[(imm >> 6) & 3];
    for (i = 0; i < 4; ++i)
        if ((imm >> i) & 1)
            a.u32[i] = 0;
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &a, 16, 0, DI_EVEX_ALL_ONES, 0);
}

// vextractps, vpextrb/w/d/q (no opmask): the element imm8 selects to a general register (zero-extended) or memory
static int Di_EvexExtractElement(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterZMM_type a, r;
    int imm;
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, 16, 0, DI_EVEX_ALL_ONES));
    imm = Di_EvexImm(pDianaContext, pCallContext, 2) & (16 / elementSize - 1);
    DIANA_MEMSET(&r, 0, sizeof(r));
    r.u64[0] = Di_EvexGetElement(&a, imm, elementSize);
    if (Di_EvexIsMemory(pCallContext, 0))
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, elementSize, 0, DI_EVEX_ALL_ONES, 0);
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 8, 0, DI_EVEX_ALL_ONES, 0);
}

// vpinsrb/w/d/q (no opmask): the source 1 with the element imm8 selects from a general register or memory
static int Di_EvexInsertElement(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DianaRegisterZMM_type a, b;
    int imm;
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, 16, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, elementSize, 0, DI_EVEX_ALL_ONES));
    imm = Di_EvexImm(pDianaContext, pCallContext, 3) & (16 / elementSize - 1);
    Di_EvexSetElement(&a, imm, elementSize, b.u64[0]);
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &a, 16, 0, DI_EVEX_ALL_ONES, 0);
}

// vcompress*, vpcompress*: the elements of the opmask packed to the low elements; a register: the rest zeroed
// ({z}) or kept; memory: only the packed elements are written
static int Di_EvexCompress(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int i, n = 0, count;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    count = e.vl / elementSize;
    DIANA_MEMSET(&r, 0, sizeof(r));
    for (i = 0; i < count; ++i)
        if ((e.mask >> i) & 1)
            Di_EvexSetElement(&r, n++, elementSize, Di_EvexGetElement(&a, i, elementSize));
    if (Di_EvexIsMemory(pCallContext, 0))
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, e.vl, elementSize, Di_LowBits(n), 0);
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, e.vl, elementSize, Di_LowBits(n), e.zeroing);
}

// vexpand*, vpexpand*: the low elements of the source to the elements of the opmask (memory: only those read)
static int Di_EvexExpand(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int i, n = 0, count, selected = 0;
    Di_EvexInit(pCallContext, &e);
    count = e.vl / elementSize;
    for (i = 0; i < count; ++i)
        selected += (int)((e.mask >> i) & 1);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, elementSize, Di_LowBits(selected)));
    DIANA_MEMSET(&r, 0, sizeof(r));
    for (i = 0; i < count; ++i)
        if ((e.mask >> i) & 1)
            Di_EvexSetElement(&r, i, elementSize, Di_EvexGetElement(&a, n++, elementSize));
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vpmovzx*, vpmovsx*: the low elements of the source extended
static int Di_EvexExtend(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int fromSize,
                         int toSize, int isSigned)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl * fromSize / toSize, fromSize, e.mask));
    Diana_Int_Extend(&r, &a, e.vl, fromSize, toSize, isSigned);
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, toSize);
}

// vpmov*: the elements truncated (0), saturated signed (1) or unsigned (2) to a narrower vector or memory
static int Di_EvexTruncate(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int fromSize,
                           int toSize, int saturate)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int i, count;
    Di_EvexInit(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
    count = e.vl / fromSize;
    DIANA_MEMSET(&r, 0, sizeof(r));
    for (i = 0; i < count; ++i)
    {
        OPERAND_SIZE value = Di_EvexGetElement(&a, i, fromSize);
        int bits = toSize * 8;
        if (saturate == 1)
        {
            OPERAND_SIZE_SIGNED s = Di_EvexGetSigned(&a, i, fromSize);
            OPERAND_SIZE_SIGNED high = ((OPERAND_SIZE_SIGNED)1 << (bits - 1)) - 1, low = -high - 1;
            value = (OPERAND_SIZE)(s > high ? high : s < low ? low : s);
        }
        else if (saturate == 2)
        {
            OPERAND_SIZE high = Di_LowBits(bits);
            value = value > high ? high : value;
        }
        Di_EvexSetElement(&r, i, toSize, value & Di_LowBits(bits));
    }
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, count * toSize, toSize, e.mask,
                        e.zeroing && !Di_EvexIsMemory(pCallContext, 0));
}

// ---------------------------------------------------------------------------------------------------------------
// floating point: MXCSR, with {er} the rounding of the instruction and no exception, with {sae} no exception (the
// flags are not set); the masked-off elements are not computed

#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU

static float_status_t Di_EvexStatus(DianaProcessor * pCallContext, const DiEvex * e)
{
    float_status_t status = Diana_SSE_Status(pCallContext);
    if (e->rounding >= DIANA_EVEX_ROUND_RN)
        status.float_rounding_mode = e->rounding - DIANA_EVEX_ROUND_RN;
    if (e->rounding)
        status.float_exception_masks = float_all_exceptions_mask;
    return status;
}

static int Di_EvexFloatDone(DianaProcessor * pCallContext, const DiEvex * e, float_status_t * pStatus)
{
    if (e->rounding)
        return DI_SUCCESS;
    pStatus->float_exception_flags &= ~pStatus->float_suppress_exception;
    return Diana_SSE_Exceptions(pCallContext, pStatus);
}

static OPERAND_SIZE Di_EvexFloatOp(DiSseOperation operation, int elementSize, OPERAND_SIZE a, OPERAND_SIZE b,
                                   float_status_t * pStatus)
{
    if (elementSize == 4)
        return Diana_SSE_Float32(operation, (float32)a, (float32)b, pStatus);
    return Diana_SSE_Float64(operation, a, b, pStatus);
}

// vaddps ... vmaxsd, vsqrtps (one source): packed, or a scalar (element 0, the rest from the source 1)
static int Di_EvexFloat(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                        int scalar, DiSseOperation operation)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    float_status_t status;
    int i, count, unary = pCallContext->m_result.iLinkedOpCount == 2;
    Di_EvexInit(pCallContext, &e);
    status = Di_EvexStatus(pCallContext, &e);
    if (scalar)
    {
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, 16, 0, DI_EVEX_ALL_ONES));
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, elementSize, 0, e.mask & 1));
        r = a;
        if (e.mask & 1)
            Di_EvexSetElement(&r, 0, elementSize, Di_EvexFloatOp(operation, elementSize,
                                                                 Di_EvexGetElement(&a, 0, elementSize),
                                                                 Di_EvexGetElement(&b, 0, elementSize), &status));
        DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 16, elementSize, e.mask | ~(OPERAND_SIZE)1,
                            e.zeroing);
    }
    if (unary)
    {
        DIANA_MEMSET(&a, 0, sizeof(a));
        DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 1, &b, &e, elementSize));
    }
    else
    {
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
        DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 2, &b, &e, elementSize));
    }
    count = e.vl / elementSize;
    DIANA_MEMSET(&r, 0, sizeof(r));
    for (i = 0; i < count; ++i)
        if ((e.mask >> i) & 1)
            Di_EvexSetElement(&r, i, elementSize, Di_EvexFloatOp(operation, elementSize,
                                                                 Di_EvexGetElement(&a, i, elementSize),
                                                                 Di_EvexGetElement(&b, i, elementSize), &status));
    DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// FMA: 132 (dest * src3 + src2), 213 (src2 * dest + src3), 231 (src2 * src3 + dest)
typedef enum { DI_EVEX_FMA_ADD, DI_EVEX_FMA_SUB, DI_EVEX_FMA_NADD, DI_EVEX_FMA_NSUB, DI_EVEX_FMA_ADDSUB,
               DI_EVEX_FMA_SUBADD } DiEvexFma;

static int Di_EvexFma(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                      int scalar, int order, DiEvexFma operation)
{
    DiEvex e;
    DianaRegisterZMM_type d, s2, s3, r;
    float_status_t status;
    int i, count, bytes;
    Di_EvexInit(pCallContext, &e);
    status = Di_EvexStatus(pCallContext, &e);
    bytes = scalar ? 16 : e.vl;
    count = scalar ? 1 : bytes / elementSize;
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 0, &d, bytes, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &s2, bytes, 0, DI_EVEX_ALL_ONES));
    if (scalar)
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &s3, elementSize, 0, e.mask & 1))
    else
        DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 2, &s3, &e, elementSize))
    r = d;
    for (i = 0; i < count; ++i)
    {
        const DianaRegisterZMM_type * pa = order == 132 ? &d : &s2;
        const DianaRegisterZMM_type * pb = order == 213 ? &d : &s3;
        const DianaRegisterZMM_type * pc = order == 132 ? &s2 : order == 213 ? &s3 : &d;
        int flags = 0;
        if (!((e.mask >> i) & 1))
            continue;
        switch (operation)
        {
        case DI_EVEX_FMA_SUB: flags = float_muladd_negate_c; break;
        case DI_EVEX_FMA_NADD: flags = float_muladd_negate_product; break;
        case DI_EVEX_FMA_NSUB: flags = float_muladd_negate_result; break;
        case DI_EVEX_FMA_ADDSUB: flags = i & 1 ? 0 : float_muladd_negate_c; break;
        case DI_EVEX_FMA_SUBADD: flags = i & 1 ? float_muladd_negate_c : 0; break;
        default: break;
        }
        if (elementSize == 4)
            r.u32[i] = float32_muladd(pa->u32[i], pb->u32[i], pc->u32[i], flags, &status);
        else
            r.u64[i] = float64_muladd(pa->u64[i], pb->u64[i], pc->u64[i], flags, &status);
    }
    DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
    if (scalar)
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 16, elementSize, e.mask | ~(OPERAND_SIZE)1,
                            e.zeroing);
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vcmpps/pd/ss/sd: the predicate imm8[4:0] into an opmask
static int Di_EvexCompareFloat(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                               int scalar)
{
    DiEvex e;
    DianaRegisterZMM_type a, b, r;
    float_status_t status;
    int i, count, predicate;
    Di_EvexInit(pCallContext, &e);
    status = Di_EvexStatus(pCallContext, &e);
    count = scalar ? 1 : e.vl / elementSize;
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, scalar ? 16 : e.vl, 0, DI_EVEX_ALL_ONES));
    if (scalar)
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, elementSize, 0, e.mask & 1))
    else
        DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 2, &b, &e, elementSize))
    predicate = Di_EvexImm(pDianaContext, pCallContext, 3) & 31;
    DIANA_MEMSET(&r, 0, sizeof(r));
    for (i = 0; i < count; ++i)
    {
        OPERAND_SIZE x = Di_EvexGetElement(&a, i, elementSize), y = Di_EvexGetElement(&b, i, elementSize);
        if (!((e.mask >> i) & 1))
            continue;
        Diana_SSE_Compare(&x, &y, elementSize, 1, predicate, &status);
        if (x & 1)
            r.u64[0] |= (OPERAND_SIZE)1 << i;
    }
    DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 8, 0, DI_EVEX_ALL_ONES, 0);
}

// vcomiss/sd, vucomiss/sd: the flags of element 0
static int Di_EvexComi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize, int quiet)
{
    DiEvex e;
    DianaRegisterZMM_type a, b;
    float_status_t status;
    int relation;
    Di_EvexInit(pCallContext, &e);
    status = Di_EvexStatus(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 0, &a, 16, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &b, elementSize, 0, DI_EVEX_ALL_ONES));
    relation = Diana_SSE_CompareScalar(&a.xmm[0], &b.xmm[0], elementSize, quiet, &status);
    DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
    Diana_SSE_SetCompareFlags(pCallContext, relation);
    DI_PROC_END
}

// the conversions: the element types
typedef enum { DI_EVEX_I32, DI_EVEX_U32, DI_EVEX_I64, DI_EVEX_U64, DI_EVEX_F32, DI_EVEX_F64 } DiEvexType;

static int Di_EvexTypeSize(DiEvexType type)
{
    return type == DI_EVEX_I32 || type == DI_EVEX_U32 || type == DI_EVEX_F32 ? 4 : 8;
}

static OPERAND_SIZE Di_EvexConvertElement(OPERAND_SIZE x, DiEvexType from, DiEvexType to, int truncate,
                                          float_status_t * pStatus);

// one element with its own flags, ored into the status (softfloat's float32_to_uint32 replaces the flags with IE)
static OPERAND_SIZE Di_EvexConvertOne(OPERAND_SIZE x, DiEvexType from, DiEvexType to, int truncate,
                                      float_status_t * pStatus)
{
    float_status_t local = *pStatus;
    OPERAND_SIZE r;
    local.float_exception_flags = 0;
    r = Di_EvexConvertElement(x, from, to, truncate, &local);
    pStatus->float_exception_flags |= local.float_exception_flags;
    return r;
}

static OPERAND_SIZE Di_EvexConvertElement(OPERAND_SIZE x, DiEvexType from, DiEvexType to, int truncate,
                                          float_status_t * pStatus)
{
    switch (from)
    {
    case DI_EVEX_I32:
        if (to == DI_EVEX_F32) return int32_to_float32((DI_INT32)x, pStatus);
        return int32_to_float64((DI_INT32)x);
    case DI_EVEX_U32:
        if (to == DI_EVEX_F32) return uint32_to_float32((DI_UINT32)x, pStatus);
        return uint32_to_float64((DI_UINT32)x);
    case DI_EVEX_I64:
        if (to == DI_EVEX_F32) return int64_to_float32((DI_INT64)x, pStatus);
        return int64_to_float64((DI_INT64)x, pStatus);
    case DI_EVEX_U64:
        if (to == DI_EVEX_F32) return uint64_to_float32(x, pStatus);
        return uint64_to_float64(x, pStatus);
    case DI_EVEX_F32:
        switch (to)
        {
        case DI_EVEX_I32: return (DI_UINT32)(truncate ? float32_to_int32_round_to_zero((float32)x, pStatus)
                                                      : float32_to_int32((float32)x, pStatus));
        case DI_EVEX_U32: return truncate ? float32_to_uint32_round_to_zero((float32)x, pStatus)
                                          : float32_to_uint32((float32)x, pStatus);
        case DI_EVEX_I64: return (OPERAND_SIZE)(truncate ? float32_to_int64_round_to_zero((float32)x, pStatus)
                                                         : float32_to_int64((float32)x, pStatus));
        case DI_EVEX_U64: return truncate ? float32_to_uint64_round_to_zero((float32)x, pStatus)
                                          : float32_to_uint64((float32)x, pStatus);
        case DI_EVEX_F64: return float32_to_float64((float32)x, pStatus);
        default: return x;
        }
    default:
        switch (to)
        {
        case DI_EVEX_I32: return (DI_UINT32)(truncate ? float64_to_int32_round_to_zero(x, pStatus)
                                                      : float64_to_int32(x, pStatus));
        case DI_EVEX_U32: return truncate ? float64_to_uint32_round_to_zero(x, pStatus)
                                          : float64_to_uint32(x, pStatus);
        case DI_EVEX_I64: return (OPERAND_SIZE)(truncate ? float64_to_int64_round_to_zero(x, pStatus)
                                                         : float64_to_int64(x, pStatus));
        case DI_EVEX_U64: return truncate ? float64_to_uint64_round_to_zero(x, pStatus)
                                          : float64_to_uint64(x, pStatus);
        case DI_EVEX_F32: return float64_to_float32(x, pStatus);
        default: return x;
        }
    }
}

// the packed conversions: the elements of the vector length (the wider type), the narrower vector is half of it
static int Di_EvexConvert(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiEvexType from,
                          DiEvexType to, int truncate)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    float_status_t status;
    int i, fromSize = Di_EvexTypeSize(from), toSize = Di_EvexTypeSize(to);
    int count;
    Di_EvexInit(pCallContext, &e);
    status = Di_EvexStatus(pCallContext, &e);
    count = e.vl / (fromSize > toSize ? fromSize : toSize);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, count * fromSize, fromSize, e.mask));
    DIANA_MEMSET(&r, 0, sizeof(r));
    for (i = 0; i < count; ++i)
        if ((e.mask >> i) & 1)
            Di_EvexSetElement(&r, i, toSize, Di_EvexConvertOne(Di_EvexGetElement(&a, i, fromSize), from, to,
                                                               truncate, &status));
    DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, count * toSize, toSize, e.mask, e.zeroing);
}

// vcvtss2sd, vcvtsd2ss: element 0 converted, the rest from the source 1
static int Di_EvexConvertScalar(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiEvexType from,
                                DiEvexType to)
{
    DiEvex e;
    DianaRegisterZMM_type a, b;
    float_status_t status;
    Di_EvexInit(pCallContext, &e);
    status = Di_EvexStatus(pCallContext, &e);
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, 16, 0, DI_EVEX_ALL_ONES));
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, Di_EvexTypeSize(from), 0, e.mask & 1));
    if (e.mask & 1)
        Di_EvexSetElement(&a, 0, Di_EvexTypeSize(to),
                          Di_EvexConvertOne(Di_EvexGetElement(&b, 0, Di_EvexTypeSize(from)), from, to, 0, &status));
    DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &a, 16, Di_EvexTypeSize(to), e.mask | ~(OPERAND_SIZE)1,
                        e.zeroing);
}

// vcvtsi2ss/sd, vcvtusi2ss/sd (the integer size of the operand), vcvt[t]ss2[u]si, vcvt[t]sd2[u]si (to a general
// register: 4 or 8 bytes by the register)
static int Di_EvexConvertInteger(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int toFloat,
                                 DiEvexType floatType, int isUnsigned, int truncate)
{
    DiEvex e;
    DianaRegisterZMM_type a, b;
    float_status_t status;
    Di_EvexInit(pCallContext, &e);
    status = Di_EvexStatus(pCallContext, &e);
    if (toFloat)
    {
        int size = pCallContext->m_result.linkedOperands[2].usedSize == 8 ? 8 : 4;
        DiEvexType from = size == 8 ? (isUnsigned ? DI_EVEX_U64 : DI_EVEX_I64) : (isUnsigned ? DI_EVEX_U32 : DI_EVEX_I32);
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, 16, 0, DI_EVEX_ALL_ONES));
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 2, &b, size, 0, DI_EVEX_ALL_ONES));
        Di_EvexSetElement(&a, 0, Di_EvexTypeSize(floatType),
                          Di_EvexConvertOne(size == 4 ? (OPERAND_SIZE)(DI_UINT32)b.u64[0] : b.u64[0], from, floatType,
                                            0, &status));
        DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &a, 16, 0, DI_EVEX_ALL_ONES, 0);
    }
    {
        int size = pCallContext->m_result.linkedOperands[0].usedSize == 8 ? 8 : 4;
        DiEvexType to = size == 8 ? (isUnsigned ? DI_EVEX_U64 : DI_EVEX_I64) : (isUnsigned ? DI_EVEX_U32 : DI_EVEX_I32);
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, Di_EvexTypeSize(floatType), 0, DI_EVEX_ALL_ONES));
        DIANA_MEMSET(&b, 0, sizeof(b));
        b.u64[0] = Di_EvexConvertOne(Di_EvexGetElement(&a, 0, Di_EvexTypeSize(floatType)), floatType, to, truncate,
                                     &status);
        if (size == 4)
            b.u64[0] &= 0xffffffffULL;
        DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &b, size, 0, DI_EVEX_ALL_ONES, 0);
    }
}

// vcvtph2ps (no DE: a denormal half is exact), vcvtps2ph (imm8[1:0] the rounding unless imm8[2]; to memory: the
// elements of the opmask)
static int Di_EvexHalf(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int toHalf)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    float_status_t status;
    int i, count;
    Di_EvexInit(pCallContext, &e);
    status = Di_EvexStatus(pCallContext, &e);
    count = e.vl / 4;
    DIANA_MEMSET(&r, 0, sizeof(r));
    if (toHalf)
    {
        int imm = Di_EvexImm(pDianaContext, pCallContext, 2);
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, e.vl, 0, DI_EVEX_ALL_ONES));
        status.flush_underflow_to_zero = 0;
        if (!(imm & 4))
            status.float_rounding_mode = imm & 3;
        for (i = 0; i < count; ++i)
            if ((e.mask >> i) & 1)
                r.u16[i] = float32_to_float16(a.u32[i], &status);
        DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, count * 2, 2, e.mask,
                            e.zeroing && !Di_EvexIsMemory(pCallContext, 0));
    }
    status.denormals_are_zeros = 0;
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, count * 2, 2, e.mask));
    for (i = 0; i < count; ++i)
        if ((e.mask >> i) & 1)
            r.u32[i] = float16_to_float32(a.u16[i], &status);
    status.float_exception_flags &= ~float_flag_denormal;
    DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, 4);
}

// the special float operations, one element: vgetexp, vgetmant, vscalef, vrndscale, vreduce, vrange, vfixupimm
typedef enum { DI_EVEX_GETEXP, DI_EVEX_GETMANT, DI_EVEX_SCALEF, DI_EVEX_RNDSCALE, DI_EVEX_REDUCE, DI_EVEX_RANGE,
               DI_EVEX_FIXUPIMM, DI_EVEX_RCP14, DI_EVEX_RSQRT14 } DiEvexSpecial;

static OPERAND_SIZE Di_EvexFixupimm(OPERAND_SIZE dest, OPERAND_SIZE src, OPERAND_SIZE table, int imm, int elementSize,
                                    float_status_t * pStatus)
{
    int bits = elementSize * 8, mantissaBits = elementSize == 4 ? 23 : 52;
    OPERAND_SIZE sign = (OPERAND_SIZE)1 << (bits - 1);
    OPERAND_SIZE expMask = (Di_LowBits(bits - 1 - mantissaBits)) << mantissaBits;
    OPERAND_SIZE mantissa = src & Di_LowBits(mantissaBits);
    OPERAND_SIZE one = elementSize == 4 ? 0x3f800000ULL : 0x3ff0000000000000ULL;
    int token, response;
    if ((src & expMask) == 0 && mantissa && pStatus->denormals_are_zeros)
        src &= sign;
    mantissa = src & Di_LowBits(mantissaBits);
    if ((src & expMask) == expMask && mantissa)
        token = (mantissa >> (mantissaBits - 1)) & 1 ? 0 : 1;      // QNaN, SNaN
    else if ((src & ~sign) == 0)
        token = 2;                                                  // zero
    else if (src == one)
        token = 3;                                                  // +1
    else if ((src & ~sign) == expMask)
        token = src & sign ? 4 : 5;                                 // -inf, +inf
    else
        token = src & sign ? 6 : 7;                                 // negative, positive
    response = (int)((table >> (4 * token)) & 15);
    switch (token)
    {
    case 2:
        if (imm & 1) float_raise(pStatus, float_flag_divbyzero);
        if (imm & 2) float_raise(pStatus, float_flag_invalid);
        break;
    case 3:
        if (imm & 4) float_raise(pStatus, float_flag_divbyzero);
        if (imm & 8) float_raise(pStatus, float_flag_invalid);
        break;
    case 1: if (imm & 16) float_raise(pStatus, float_flag_invalid); break;
    case 4: if (imm & 32) float_raise(pStatus, float_flag_invalid); break;
    case 6: if (imm & 64) float_raise(pStatus, float_flag_invalid); break;
    case 5: if (imm & 128) float_raise(pStatus, float_flag_invalid); break;
    default: break;
    }
    switch (response)
    {
    case 0: return dest;
    case 1: return src;
    case 2: return src | expMask | ((OPERAND_SIZE)1 << (mantissaBits - 1));
    case 3: return elementSize == 4 ? 0xffc00000ULL : 0xfff8000000000000ULL;
    case 4: return sign | expMask;
    case 5: return expMask;
    case 6: return (src & sign) | expMask;
    case 7: return sign;
    case 8: return 0;
    case 9: return sign | one;
    case 10: return one;
    case 11: return elementSize == 4 ? 0x3f000000ULL : 0x3fe0000000000000ULL;
    case 12: return elementSize == 4 ? 0x42b40000ULL : 0x4056800000000000ULL;
    case 13: return elementSize == 4 ? 0x3fc90fdbULL : 0x3ff921fb54442d18ULL;
    case 14: return elementSize == 4 ? 0x7f7fffffULL : 0x7fefffffffffffffULL;
    default: return elementSize == 4 ? 0xff7fffffULL : 0xffefffffffffffffULL;
    }
}

// vrange: imm8[1:0] min, max, min of the absolute values, max of them; imm8[3:2] the sign: of a, of the compare
// result, cleared, set
static OPERAND_SIZE Di_EvexRange(OPERAND_SIZE a, OPERAND_SIZE b, int imm, int elementSize, float_status_t * pStatus)
{
    int bits = elementSize * 8, mantissaBits = elementSize == 4 ? 23 : 52;
    OPERAND_SIZE sign = (OPERAND_SIZE)1 << (bits - 1);
    OPERAND_SIZE expMask = (Di_LowBits(bits - 1 - mantissaBits)) << mantissaBits;
    OPERAND_SIZE quiet = (OPERAND_SIZE)1 << (mantissaBits - 1);
    int isMax = imm & 1, isAbs = (imm >> 1) & 1, signA = (a & sign) != 0, signB = (b & sign) != 0;
    int aNaN = (a & expMask) == expMask && (a & Di_LowBits(mantissaBits));
    int bNaN = (b & expMask) == expMask && (b & Di_LowBits(mantissaBits));
    OPERAND_SIZE z;
    if (aNaN && !(a & quiet))
    {
        float_raise(pStatus, float_flag_invalid);
        return a | quiet;
    }
    if (bNaN && !(b & quiet))
    {
        float_raise(pStatus, float_flag_invalid);
        return b | quiet;
    }
    if ((a & expMask) == 0 && (a & Di_LowBits(mantissaBits)))
    {
        if (pStatus->denormals_are_zeros)
            a &= sign;
        else if (!bNaN)
            float_raise(pStatus, float_flag_denormal);
    }
    if ((b & expMask) == 0 && (b & Di_LowBits(mantissaBits)))
    {
        if (pStatus->denormals_are_zeros)
            b &= sign;
        else if (!aNaN)
            float_raise(pStatus, float_flag_denormal);
    }
    if (bNaN)
        z = a;
    else if (aNaN)
        z = b;
    else if (signA != signB && !isAbs)
        z = isMax ? (signA ? b : a) : (signA ? a : b);
    else
    {
        OPERAND_SIZE ta = a, tb = b;
        int s = signA;
        if (isAbs)
        {
            ta &= ~sign;
            tb &= ~sign;
            if (ta != tb)
                s = 0;
        }
        if (!isMax)
            z = (s ^ (ta < tb)) ? a : b;
        else
            z = (s ^ (ta < tb)) ? b : a;
    }
    switch ((imm >> 2) & 3)
    {
    case 0: z = (z & ~sign) | (a & sign); break;
    case 1: break;
    case 2: z &= ~sign; break;
    default: z |= sign; break;
    }
    return z;
}

// vreduce: a - round(a * 2^M) / 2^M, M = imm8[7:4]; the rounding imm8[1:0] unless imm8[2]; PE only for the
// subtraction (the CPU: the rounding to an integer doesn't signal it), imm8[3] suppresses that too
static OPERAND_SIZE Di_EvexReduce(OPERAND_SIZE a, int imm, int elementSize, float_status_t * pStatus)
{
    float_status_t rounding = *pStatus;
    rounding.float_exception_flags = 0;
    if (elementSize == 4)
    {
        float32 t;
        if (float32_is_nan((float32)a))
            return float32_round_to_int_with_scale((float32)a, (DI_UINT8)(imm >> 4), pStatus);
        if (((float32)a & 0x7fffffff) == 0x7f800000)
            return 0;
        t = float32_round_to_int_with_scale((float32)a, (DI_UINT8)(imm >> 4), &rounding);
        pStatus->float_exception_flags |= rounding.float_exception_flags & ~float_flag_inexact;
        return float32_sub((float32)a, t, pStatus);
    }
    {
        float64 t;
        if (float64_is_nan(a))
            return float64_round_to_int_with_scale(a, (DI_UINT8)(imm >> 4), pStatus);
        if ((a & 0x7fffffffffffffffULL) == 0x7ff0000000000000ULL)
            return 0;
        t = float64_round_to_int_with_scale(a, (DI_UINT8)(imm >> 4), &rounding);
        pStatus->float_exception_flags |= rounding.float_exception_flags & ~float_flag_inexact;
        return float64_sub(a, t, pStatus);
    }
}

// vrcp14, vrsqrt14: the tables of this CPU (diana_processor_avx512_tables.inc: keyframes and 2-bit steps)
#include "diana_processor_avx512_tables.inc"

static int Di_Base64Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    return c == '+' ? 62 : 63;
}

static OPERAND_SIZE Di_EvexTableEntry(const DI_UINT16 * keyframes, const char * steps, int index)
{
    int value = keyframes[index >> 8], i;
    for (i = (index & ~255) + 1; i <= index; ++i)
        value -= (Di_Base64Value(steps[i / 3]) >> (2 * (i % 3))) & 3;
    return (OPERAND_SIZE)value;
}

// 1/a (rsqrt: 1/sqrt(a)) with the 16 fraction bits of the table (a double: the table of its 23 high fraction bits
// and a sticky bit); no exception; MXCSR.DAZ for a denormal source, FTZ for a denormal result (vrcp14)
static OPERAND_SIZE Di_EvexApproximate(OPERAND_SIZE a, int elementSize, int sqrt, const float_status_t * pStatus)
{
    int bits = elementSize * 8, mantissaBits = elementSize == 4 ? 23 : 52, bias = elementSize == 4 ? 127 : 1023;
    int maxExp = elementSize == 4 ? 0xff : 0x7ff;
    OPERAND_SIZE sign = (a >> (bits - 1)) & 1, mantissa = a & Di_LowBits(mantissaBits), fraction23, result;
    OPERAND_SIZE quiet = (OPERAND_SIZE)1 << (mantissaBits - 1);
    OPERAND_SIZE nan = elementSize == 4 ? 0xffc00000ULL : 0xfff8000000000000ULL;
    int exp = (int)((a >> mantissaBits) & (OPERAND_SIZE)maxExp), resultExp;
    if (exp == maxExp)
    {
        if (mantissa)
            return a | quiet;
        if (sqrt)
            return sign ? nan : 0;
        return sign << (bits - 1);
    }
    if (exp == 0)
    {
        if (!mantissa || pStatus->denormals_are_zeros)
            return (sign << (bits - 1)) | ((OPERAND_SIZE)maxExp << mantissaBits);
        // normalize: the hidden bit in place, the exponent below 1
        exp = 1;
        while (!(mantissa >> mantissaBits))
        {
            mantissa <<= 1;
            --exp;
        }
        mantissa &= Di_LowBits(mantissaBits);
    }
    if (sqrt && sign)
        return nan;
    fraction23 = elementSize == 4 ? mantissa : (mantissa >> 29) | ((mantissa & 0x1fffffff) != 0);
    if (sqrt)
    {
        int odd = exp & 1;
        resultExp = (bias - 1) - ((exp - bias) >> 1);
        if (!fraction23 && odd)
        {
            ++resultExp;
            result = 0;
        }
        else
            result = odd ? Di_EvexTableEntry(g_rsqrt14_odd_keyframes, g_rsqrt14_odd_steps, (int)(fraction23 >> 8))
                         : Di_EvexTableEntry(g_rsqrt14_even_keyframes, g_rsqrt14_even_steps, (int)(fraction23 >> 8));
        result <<= mantissaBits - 16;
        return ((OPERAND_SIZE)resultExp << mantissaBits) | result;
    }
    resultExp = 2 * bias - 1 - exp;
    if (!fraction23)
    {
        ++resultExp;
        result = 0;
    }
    else
        result = Di_EvexTableEntry(g_rcp14_keyframes, g_rcp14_steps, (int)(fraction23 >> 7)) << (mantissaBits - 16);
    if (resultExp >= maxExp)
        return (sign << (bits - 1)) | ((OPERAND_SIZE)maxExp << mantissaBits);
    if (resultExp <= 0)
    {
        if (pStatus->flush_underflow_to_zero)
            return sign << (bits - 1);
        result = (result | ((OPERAND_SIZE)1 << mantissaBits)) >> (1 - resultExp);
        resultExp = 0;
    }
    return (sign << (bits - 1)) | ((OPERAND_SIZE)resultExp << mantissaBits) | result;
}

static OPERAND_SIZE Di_EvexSpecialOne(DiEvexSpecial kind, OPERAND_SIZE dest, OPERAND_SIZE a, OPERAND_SIZE b,
                                      int imm, int elementSize, float_status_t * pStatus)
{
    switch (kind)
    {
    case DI_EVEX_GETEXP:
        return elementSize == 4 ? float32_getexp((float32)b, pStatus) : float64_getexp(b, pStatus);
    case DI_EVEX_GETMANT:
        return elementSize == 4 ? float32_getmant((float32)b, pStatus, (imm >> 2) & 3, imm & 3)
                                : float64_getmant(b, pStatus, (imm >> 2) & 3, imm & 3);
    case DI_EVEX_SCALEF:
        return elementSize == 4 ? float32_scalef((float32)a, (float32)b, pStatus) : float64_scalef(a, b, pStatus);
    case DI_EVEX_RNDSCALE:
        return elementSize == 4 ? float32_round_to_int_with_scale((float32)b, (DI_UINT8)(imm >> 4), pStatus)
                                : float64_round_to_int_with_scale(b, (DI_UINT8)(imm >> 4), pStatus);
    case DI_EVEX_REDUCE:
        {
            // no DE for a denormal source (the CPU)
            float_status_t local = *pStatus;
            OPERAND_SIZE r;
            local.float_exception_flags = 0;
            r = Di_EvexReduce(b, imm, elementSize, &local);
            pStatus->float_exception_flags |= local.float_exception_flags & ~float_flag_denormal;
            return r;
        }
    case DI_EVEX_RANGE:
        return Di_EvexRange(a, b, imm, elementSize, pStatus);
    case DI_EVEX_FIXUPIMM:
        return Di_EvexFixupimm(dest, a, b, imm, elementSize, pStatus);
    case DI_EVEX_RCP14:
        return Di_EvexApproximate(b, elementSize, 0, pStatus);
    default:
        return Di_EvexApproximate(b, elementSize, 1, pStatus);
    }
}

// the operands: one source (vgetexp zmm1, zmm2/m), two (vscalef zmm1, zmm2, zmm3/m), the destination too
// (vfixupimm); the scalar forms take the rest from the source 1
static int Di_EvexSpecial(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                          int scalar, DiEvexSpecial kind)
{
    DiEvex e;
    DianaRegisterZMM_type d, a, b, r;
    float_status_t status;
    int i, count, imm = 0, two, last;
    Di_EvexInit(pCallContext, &e);
    status = Di_EvexStatus(pCallContext, &e);
    last = pCallContext->m_result.iLinkedOpCount - 1;
    if (pCallContext->m_result.linkedOperands[last].type == diana_imm)
    {
        imm = Di_EvexImm(pDianaContext, pCallContext, last);
        --last;
    }
    two = last == 2;
    if (kind == DI_EVEX_RNDSCALE || kind == DI_EVEX_REDUCE)
    {
        if (!(imm & 4))
            status.float_rounding_mode = imm & 3;
        if (imm & 8)
            status.float_suppress_exception |= float_flag_inexact;
    }
    DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 0, &d, scalar ? 16 : e.vl, 0, DI_EVEX_ALL_ONES));
    DIANA_MEMSET(&a, 0, sizeof(a));
    if (two)
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, scalar ? 16 : e.vl, 0, DI_EVEX_ALL_ONES));
    if (scalar)
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, last, &b, elementSize, 0, e.mask & 1))
    else
        DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, last, &b, &e, elementSize))
    count = scalar ? 1 : e.vl / elementSize;
    r = scalar ? a : d;
    for (i = 0; i < count; ++i)
        if ((e.mask >> i) & 1)
            Di_EvexSetElement(&r, i, elementSize,
                              Di_EvexSpecialOne(kind, Di_EvexGetElement(&d, i, elementSize),
                                                Di_EvexGetElement(&a, i, elementSize),
                                                Di_EvexGetElement(&b, i, elementSize), imm, elementSize, &status));
    DI_CHECK(Di_EvexFloatDone(pCallContext, &e, &status));
    if (scalar)
        return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 16, elementSize, e.mask | ~(OPERAND_SIZE)1,
                            e.zeroing);
    return Di_EvexDestination(pDianaContext, pCallContext, &r, &e, elementSize);
}

// vfpclass: the classes of imm8 (QNaN, +0, -0, +inf, -inf, denormal, negative finite, SNaN) into an opmask
static int Di_EvexFpclass(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int elementSize,
                          int scalar)
{
    DiEvex e;
    DianaRegisterZMM_type a, r;
    int i, count, imm;
    Di_EvexInit(pCallContext, &e);
    if (scalar)
        DI_CHECK(Di_EvexSource(pDianaContext, pCallContext, 1, &a, elementSize, 0, DI_EVEX_ALL_ONES))
    else
        DI_CHECK(Di_EvexVector(pDianaContext, pCallContext, 1, &a, &e, elementSize))
    imm = Di_EvexImm(pDianaContext, pCallContext, 2);
    count = scalar ? 1 : e.vl / elementSize;
    DIANA_MEMSET(&r, 0, sizeof(r));
    for (i = 0; i < count; ++i)
    {
        OPERAND_SIZE x = Di_EvexGetElement(&a, i, elementSize);
		int negative = 0, hit = 0;
        float_class_t c = elementSize == 4 ? float32_class((float32)x) : float64_class(x);
        // MXCSR.DAZ: a denormal is a zero
        if (c == float_denormal && (pCallContext->m_mxcsr & DI_MXCSR_DAZ))
            c = float_zero;
        negative = (int)((x >> (elementSize * 8 - 1)) & 1);
        switch (c)
        {
        case float_QNaN: hit = imm & 1; break;
        case float_zero: hit = negative ? imm & 4 : imm & 2; break;
        case float_positive_inf: hit = imm & 8; break;
        case float_negative_inf: hit = imm & 16; break;
        case float_denormal: hit = (imm & 32) || (negative && (imm & 64)); break;
        case float_SNaN: hit = imm & 128; break;
        default: hit = negative && (imm & 64); break;
        }
        if (hit && ((e.mask >> i) & 1))
            r.u64[0] |= (OPERAND_SIZE)1 << i;
    }
    return Di_EvexWrite(pDianaContext, pCallContext, 0, &r, 8, 0, DI_EVEX_ALL_ONES, 0);
}

#endif

// ---------------------------------------------------------------------------------------------------------------
// gathers and scatters: the elements of the opmask in order, each clears its bit; a fault keeps the elements done
// and the bits of the rest (restartable). The count: the vector length over the wider of the index and the element.

static int Di_EvexGatherScatter(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int indexSize,
                                int elementSize, int scatter)
{
    DianaLinkedOperand * ops = pCallContext->m_result.linkedOperands;
    DiEvex e;
    int memory = scatter ? 0 : 1, vector = scatter ? 1 : 0;
    DianaRmIndex baseIndex = ops[memory].value.rmIndex;
    DianaRegisterZMM_type value = DianaProcessor_ZMM_GetValue(pCallContext, ops[vector].value.recognizedRegister);
    DianaRegisterZMM_type index = DianaProcessor_ZMM_GetValue(pCallContext, baseIndex.indexed_reg);
    OPERAND_SIZE selector = 0, base = 0, k;
    OPERAND_SIZE addressMask = pDianaContext->iCurrentCmd_addressSize == DIANA_MODE64 ? ~(OPERAND_SIZE)0 : 0xffffffffULL;
    int status = DI_SUCCESS, i, count, kNumber = pCallContext->m_result.evex.mask;
    Di_EvexInit(pCallContext, &e);
    count = e.vl / (indexSize > elementSize ? indexSize : elementSize);
    k = Di_GetK(pCallContext, kNumber);
    baseIndex.indexed_reg = reg_none;
    DI_CHECK(DianaProcessor_CalcIndex(pDianaContext, pCallContext, &baseIndex, &selector, &base));
    for (i = 0; i < count; ++i)
    {
        OPERAND_SIZE offset, address, element = 0;
        if (!((k >> i) & 1))
            continue;
        offset = indexSize == 4 ? (OPERAND_SIZE)(OPERAND_SIZE_SIGNED)index.s32[i] : index.u64[i];
        address = (base + offset * baseIndex.index) & addressMask;
        if (scatter)
        {
            element = Di_EvexGetElement(&value, i, elementSize);
            status = DianaProcessor_SetMemValue(pCallContext, selector, address, elementSize, &element, 0,
                                                baseIndex.seg_reg);
        }
        else
        {
            status = DianaProcessor_GetMemValue(pCallContext, selector, address, elementSize, &element, 0,
                                                baseIndex.seg_reg);
            if (status == DI_SUCCESS)
                Di_EvexSetElement(&value, i, elementSize, element);
        }
        if (status != DI_SUCCESS)
            break;
        k &= ~((OPERAND_SIZE)1 << i);
    }
    if (status == DI_SUCCESS)
        k = 0;
    if (!scatter)
    {
        // the elements of the count, the rest of the register zeroed
        DIANA_MEMSET(value.u8 + count * elementSize, 0, 64 - count * elementSize);
        DianaProcessor_ZMM_SetValue(pCallContext, ops[0].value.recognizedRegister, &value);
    }
    Di_SetK(pCallContext, kNumber, k);
    DI_CHECK(status);
    DI_PROC_END
}

// ---------------------------------------------------------------------------------------------------------------
// the EVEX instructions

#define DI_EVEX_CASE(name, call) case diana_cmd_##name: return call;

int Diana_Call_evex(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    switch ((int)pCallContext->m_result.pInfo->m_pGroupInfo->m_commandId)
    {
    // moves
    DI_EVEX_CASE(vmovaps, Di_EvexMove(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vmovapd, Di_EvexMove(pDianaContext, pCallContext, 8, 1))
    DI_EVEX_CASE(vmovdqa32, Di_EvexMove(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vmovdqa64, Di_EvexMove(pDianaContext, pCallContext, 8, 1))
    DI_EVEX_CASE(vmovups, Di_EvexMove(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vmovupd, Di_EvexMove(pDianaContext, pCallContext, 8, 0))
    DI_EVEX_CASE(vmovdqu8, Di_EvexMove(pDianaContext, pCallContext, 1, 0))
    DI_EVEX_CASE(vmovdqu16, Di_EvexMove(pDianaContext, pCallContext, 2, 0))
    DI_EVEX_CASE(vmovdqu32, Di_EvexMove(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vmovdqu64, Di_EvexMove(pDianaContext, pCallContext, 8, 0))
    DI_EVEX_CASE(vmovntps, Di_EvexMove(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vmovntpd, Di_EvexMove(pDianaContext, pCallContext, 8, 1))
    DI_EVEX_CASE(vmovntdq, Di_EvexMove(pDianaContext, pCallContext, 8, 1))
    DI_EVEX_CASE(vmovntdqa, Di_EvexMove(pDianaContext, pCallContext, 8, 1))
    DI_EVEX_CASE(vmovss, Di_EvexMoveScalar(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vmovsd, Di_EvexMoveScalar(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vmovd, Di_EvexMoveLow(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vmovq, Di_EvexMoveLow(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vmovhps, Di_EvexMoveHalf(pDianaContext, pCallContext, 1, 0))
    DI_EVEX_CASE(vmovhpd, Di_EvexMoveHalf(pDianaContext, pCallContext, 1, 0))
    DI_EVEX_CASE(vmovlps, Di_EvexMoveHalf(pDianaContext, pCallContext, 0, 0))
    DI_EVEX_CASE(vmovlpd, Di_EvexMoveHalf(pDianaContext, pCallContext, 0, 0))
    DI_EVEX_CASE(vmovhlps, Di_EvexMoveHalf(pDianaContext, pCallContext, 1, 1))
    DI_EVEX_CASE(vmovlhps, Di_EvexMoveHalf(pDianaContext, pCallContext, 0, 1))
    DI_EVEX_CASE(vmovddup, Di_EvexDuplicate(pDianaContext, pCallContext, 8, 0))
    DI_EVEX_CASE(vmovsldup, Di_EvexDuplicate(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vmovshdup, Di_EvexDuplicate(pDianaContext, pCallContext, 4, 1))
    // broadcasts
    DI_EVEX_CASE(vbroadcastss, Di_EvexBroadcast(pDianaContext, pCallContext, 4, 4))
    DI_EVEX_CASE(vbroadcastsd, Di_EvexBroadcast(pDianaContext, pCallContext, 8, 8))
    DI_EVEX_CASE(vpbroadcastb, Di_EvexBroadcast(pDianaContext, pCallContext, 1, 1))
    DI_EVEX_CASE(vpbroadcastw, Di_EvexBroadcast(pDianaContext, pCallContext, 2, 2))
    DI_EVEX_CASE(vpbroadcastd, Di_EvexBroadcast(pDianaContext, pCallContext, 4, 4))
    DI_EVEX_CASE(vpbroadcastq, Di_EvexBroadcast(pDianaContext, pCallContext, 8, 8))
    DI_EVEX_CASE(vbroadcastf32x2, Di_EvexBroadcast(pDianaContext, pCallContext, 4, 8))
    DI_EVEX_CASE(vbroadcasti32x2, Di_EvexBroadcast(pDianaContext, pCallContext, 4, 8))
    DI_EVEX_CASE(vbroadcastf32x4, Di_EvexBroadcast(pDianaContext, pCallContext, 4, 16))
    DI_EVEX_CASE(vbroadcasti32x4, Di_EvexBroadcast(pDianaContext, pCallContext, 4, 16))
    DI_EVEX_CASE(vbroadcastf64x2, Di_EvexBroadcast(pDianaContext, pCallContext, 8, 16))
    DI_EVEX_CASE(vbroadcasti64x2, Di_EvexBroadcast(pDianaContext, pCallContext, 8, 16))
    DI_EVEX_CASE(vbroadcastf32x8, Di_EvexBroadcast(pDianaContext, pCallContext, 4, 32))
    DI_EVEX_CASE(vbroadcasti32x8, Di_EvexBroadcast(pDianaContext, pCallContext, 4, 32))
    DI_EVEX_CASE(vbroadcastf64x4, Di_EvexBroadcast(pDianaContext, pCallContext, 8, 32))
    DI_EVEX_CASE(vbroadcasti64x4, Di_EvexBroadcast(pDianaContext, pCallContext, 8, 32))
    DI_EVEX_CASE(vpbroadcastmb2q, Di_EvexBroadcastMask(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpbroadcastmw2d, Di_EvexBroadcastMask(pDianaContext, pCallContext, 4))
    // integer
    DI_EVEX_CASE(vpaddb, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_ADD))
    DI_EVEX_CASE(vpaddw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_ADD))
    DI_EVEX_CASE(vpaddd, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_ADD))
    DI_EVEX_CASE(vpaddq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_ADD))
    DI_EVEX_CASE(vpsubb, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_SUB))
    DI_EVEX_CASE(vpsubw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_SUB))
    DI_EVEX_CASE(vpsubd, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_SUB))
    DI_EVEX_CASE(vpsubq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_SUB))
    DI_EVEX_CASE(vpaddsb, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_ADDS))
    DI_EVEX_CASE(vpaddsw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_ADDS))
    DI_EVEX_CASE(vpaddusb, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_ADDUS))
    DI_EVEX_CASE(vpaddusw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_ADDUS))
    DI_EVEX_CASE(vpsubsb, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_SUBS))
    DI_EVEX_CASE(vpsubsw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_SUBS))
    DI_EVEX_CASE(vpsubusb, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_SUBUS))
    DI_EVEX_CASE(vpsubusw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_SUBUS))
    DI_EVEX_CASE(vpavgb, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_AVG))
    DI_EVEX_CASE(vpavgw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_AVG))
    DI_EVEX_CASE(vpmaxsb, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_MAX))
    DI_EVEX_CASE(vpmaxsw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_MAX))
    DI_EVEX_CASE(vpmaxsd, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_MAX))
    DI_EVEX_CASE(vpmaxsq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_MAX))
    DI_EVEX_CASE(vpmaxub, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_MAXU))
    DI_EVEX_CASE(vpmaxuw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_MAXU))
    DI_EVEX_CASE(vpmaxud, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_MAXU))
    DI_EVEX_CASE(vpmaxuq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_MAXU))
    DI_EVEX_CASE(vpminsb, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_MIN))
    DI_EVEX_CASE(vpminsw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_MIN))
    DI_EVEX_CASE(vpminsd, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_MIN))
    DI_EVEX_CASE(vpminsq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_MIN))
    DI_EVEX_CASE(vpminub, Di_EvexInt(pDianaContext, pCallContext, 1, DI_INT_MINU))
    DI_EVEX_CASE(vpminuw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_MINU))
    DI_EVEX_CASE(vpminud, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_MINU))
    DI_EVEX_CASE(vpminuq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_MINU))
    DI_EVEX_CASE(vpmullw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_MULLO))
    DI_EVEX_CASE(vpmulld, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_MULLO))
    DI_EVEX_CASE(vpmullq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_MULLO))
    DI_EVEX_CASE(vpmulhw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_MULHI))
    DI_EVEX_CASE(vpmulhuw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_MULHU))
    DI_EVEX_CASE(vpmulhrsw, Di_EvexInt(pDianaContext, pCallContext, 2, DI_INT_MULHRS))
    DI_EVEX_CASE(vpandd, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_AND))
    DI_EVEX_CASE(vpandq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_AND))
    DI_EVEX_CASE(vpandnd, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_ANDN))
    DI_EVEX_CASE(vpandnq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_ANDN))
    DI_EVEX_CASE(vpord, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_OR))
    DI_EVEX_CASE(vporq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_OR))
    DI_EVEX_CASE(vpxord, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_XOR))
    DI_EVEX_CASE(vpxorq, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_XOR))
    DI_EVEX_CASE(vandps, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_AND))
    DI_EVEX_CASE(vandpd, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_AND))
    DI_EVEX_CASE(vandnps, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_ANDN))
    DI_EVEX_CASE(vandnpd, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_ANDN))
    DI_EVEX_CASE(vorps, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_OR))
    DI_EVEX_CASE(vorpd, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_OR))
    DI_EVEX_CASE(vxorps, Di_EvexInt(pDianaContext, pCallContext, 4, DI_INT_XOR))
    DI_EVEX_CASE(vxorpd, Di_EvexInt(pDianaContext, pCallContext, 8, DI_INT_XOR))
    DI_EVEX_CASE(vpabsb, Di_EvexAbs(pDianaContext, pCallContext, 1))
    DI_EVEX_CASE(vpabsw, Di_EvexAbs(pDianaContext, pCallContext, 2))
    DI_EVEX_CASE(vpabsd, Di_EvexAbs(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpabsq, Di_EvexAbs(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpternlogd, Di_EvexTernaryLogic(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpternlogq, Di_EvexTernaryLogic(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpblendmb, Di_EvexBlendMask(pDianaContext, pCallContext, 1))
    DI_EVEX_CASE(vpblendmw, Di_EvexBlendMask(pDianaContext, pCallContext, 2))
    DI_EVEX_CASE(vpblendmd, Di_EvexBlendMask(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpblendmq, Di_EvexBlendMask(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vblendmps, Di_EvexBlendMask(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vblendmpd, Di_EvexBlendMask(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpmaddwd, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_MADDWD, 0))
    DI_EVEX_CASE(vpmaddubsw, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_MADDUBSW, 0))
    DI_EVEX_CASE(vpmuldq, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_MULDQ, 0))
    DI_EVEX_CASE(vpmuludq, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_MULUDQ, 0))
    DI_EVEX_CASE(vpsadbw, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_SADBW, 0))
    DI_EVEX_CASE(vpacksswb, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_PACKSSWB, 0))
    DI_EVEX_CASE(vpackuswb, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_PACKUSWB, 0))
    DI_EVEX_CASE(vpackssdw, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_PACKSSDW, 0))
    DI_EVEX_CASE(vpackusdw, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_PACKUSDW, 0))
    DI_EVEX_CASE(vpunpcklbw, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKL, 1))
    DI_EVEX_CASE(vpunpcklwd, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKL, 2))
    DI_EVEX_CASE(vpunpckldq, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKL, 4))
    DI_EVEX_CASE(vpunpcklqdq, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKL, 8))
    DI_EVEX_CASE(vpunpckhbw, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKH, 1))
    DI_EVEX_CASE(vpunpckhwd, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKH, 2))
    DI_EVEX_CASE(vpunpckhdq, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKH, 4))
    DI_EVEX_CASE(vpunpckhqdq, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKH, 8))
    DI_EVEX_CASE(vunpcklps, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKL, 4))
    DI_EVEX_CASE(vunpcklpd, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKL, 8))
    DI_EVEX_CASE(vunpckhps, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKH, 4))
    DI_EVEX_CASE(vunpckhpd, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_UNPCKH, 8))
    DI_EVEX_CASE(vpshufb, Di_EvexTwoSources(pDianaContext, pCallContext, DI_EVEX_SHUFB, 0))
    DI_EVEX_CASE(vdbpsadbw, Di_EvexDbpsadbw(pDianaContext, pCallContext))
    DI_EVEX_CASE(vpconflictd, Di_EvexConflict(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vpconflictq, Di_EvexConflict(pDianaContext, pCallContext, 8, 0))
    DI_EVEX_CASE(vplzcntd, Di_EvexConflict(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vplzcntq, Di_EvexConflict(pDianaContext, pCallContext, 8, 1))
    // compares into an opmask
    DI_EVEX_CASE(vpcmpb, Di_EvexCompareInt(pDianaContext, pCallContext, 1, DI_EVEX_CMP_IMM))
    DI_EVEX_CASE(vpcmpw, Di_EvexCompareInt(pDianaContext, pCallContext, 2, DI_EVEX_CMP_IMM))
    DI_EVEX_CASE(vpcmpd, Di_EvexCompareInt(pDianaContext, pCallContext, 4, DI_EVEX_CMP_IMM))
    DI_EVEX_CASE(vpcmpq, Di_EvexCompareInt(pDianaContext, pCallContext, 8, DI_EVEX_CMP_IMM))
    DI_EVEX_CASE(vpcmpub, Di_EvexCompareInt(pDianaContext, pCallContext, 1, DI_EVEX_CMP_IMMU))
    DI_EVEX_CASE(vpcmpuw, Di_EvexCompareInt(pDianaContext, pCallContext, 2, DI_EVEX_CMP_IMMU))
    DI_EVEX_CASE(vpcmpud, Di_EvexCompareInt(pDianaContext, pCallContext, 4, DI_EVEX_CMP_IMMU))
    DI_EVEX_CASE(vpcmpuq, Di_EvexCompareInt(pDianaContext, pCallContext, 8, DI_EVEX_CMP_IMMU))
    DI_EVEX_CASE(vpcmpeqb, Di_EvexCompareInt(pDianaContext, pCallContext, 1, DI_EVEX_CMP_EQ))
    DI_EVEX_CASE(vpcmpeqw, Di_EvexCompareInt(pDianaContext, pCallContext, 2, DI_EVEX_CMP_EQ))
    DI_EVEX_CASE(vpcmpeqd, Di_EvexCompareInt(pDianaContext, pCallContext, 4, DI_EVEX_CMP_EQ))
    DI_EVEX_CASE(vpcmpeqq, Di_EvexCompareInt(pDianaContext, pCallContext, 8, DI_EVEX_CMP_EQ))
    DI_EVEX_CASE(vpcmpgtb, Di_EvexCompareInt(pDianaContext, pCallContext, 1, DI_EVEX_CMP_GT))
    DI_EVEX_CASE(vpcmpgtw, Di_EvexCompareInt(pDianaContext, pCallContext, 2, DI_EVEX_CMP_GT))
    DI_EVEX_CASE(vpcmpgtd, Di_EvexCompareInt(pDianaContext, pCallContext, 4, DI_EVEX_CMP_GT))
    DI_EVEX_CASE(vpcmpgtq, Di_EvexCompareInt(pDianaContext, pCallContext, 8, DI_EVEX_CMP_GT))
    DI_EVEX_CASE(vptestmb, Di_EvexCompareInt(pDianaContext, pCallContext, 1, DI_EVEX_TESTM))
    DI_EVEX_CASE(vptestmw, Di_EvexCompareInt(pDianaContext, pCallContext, 2, DI_EVEX_TESTM))
    DI_EVEX_CASE(vptestmd, Di_EvexCompareInt(pDianaContext, pCallContext, 4, DI_EVEX_TESTM))
    DI_EVEX_CASE(vptestmq, Di_EvexCompareInt(pDianaContext, pCallContext, 8, DI_EVEX_TESTM))
    DI_EVEX_CASE(vptestnmb, Di_EvexCompareInt(pDianaContext, pCallContext, 1, DI_EVEX_TESTNM))
    DI_EVEX_CASE(vptestnmw, Di_EvexCompareInt(pDianaContext, pCallContext, 2, DI_EVEX_TESTNM))
    DI_EVEX_CASE(vptestnmd, Di_EvexCompareInt(pDianaContext, pCallContext, 4, DI_EVEX_TESTNM))
    DI_EVEX_CASE(vptestnmq, Di_EvexCompareInt(pDianaContext, pCallContext, 8, DI_EVEX_TESTNM))
    DI_EVEX_CASE(vpmovb2m, Di_EvexMaskMove(pDianaContext, pCallContext, 1, 1))
    DI_EVEX_CASE(vpmovw2m, Di_EvexMaskMove(pDianaContext, pCallContext, 2, 1))
    DI_EVEX_CASE(vpmovd2m, Di_EvexMaskMove(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vpmovq2m, Di_EvexMaskMove(pDianaContext, pCallContext, 8, 1))
    DI_EVEX_CASE(vpmovm2b, Di_EvexMaskMove(pDianaContext, pCallContext, 1, 0))
    DI_EVEX_CASE(vpmovm2w, Di_EvexMaskMove(pDianaContext, pCallContext, 2, 0))
    DI_EVEX_CASE(vpmovm2d, Di_EvexMaskMove(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vpmovm2q, Di_EvexMaskMove(pDianaContext, pCallContext, 8, 0))
    // shifts
    DI_EVEX_CASE(vpsllw, Di_EvexShift(pDianaContext, pCallContext, 2, DI_SHIFT_LEFT))
    DI_EVEX_CASE(vpslld, Di_EvexShift(pDianaContext, pCallContext, 4, DI_SHIFT_LEFT))
    DI_EVEX_CASE(vpsllq, Di_EvexShift(pDianaContext, pCallContext, 8, DI_SHIFT_LEFT))
    DI_EVEX_CASE(vpsrlw, Di_EvexShift(pDianaContext, pCallContext, 2, DI_SHIFT_RIGHT))
    DI_EVEX_CASE(vpsrld, Di_EvexShift(pDianaContext, pCallContext, 4, DI_SHIFT_RIGHT))
    DI_EVEX_CASE(vpsrlq, Di_EvexShift(pDianaContext, pCallContext, 8, DI_SHIFT_RIGHT))
    DI_EVEX_CASE(vpsraw, Di_EvexShift(pDianaContext, pCallContext, 2, DI_SHIFT_ARITHMETIC))
    DI_EVEX_CASE(vpsrad, Di_EvexShift(pDianaContext, pCallContext, 4, DI_SHIFT_ARITHMETIC))
    DI_EVEX_CASE(vpsraq, Di_EvexShift(pDianaContext, pCallContext, 8, DI_SHIFT_ARITHMETIC))
    DI_EVEX_CASE(vpsllvw, Di_EvexShiftVariable(pDianaContext, pCallContext, 2, DI_SHIFT_LEFT))
    DI_EVEX_CASE(vpsllvd, Di_EvexShiftVariable(pDianaContext, pCallContext, 4, DI_SHIFT_LEFT))
    DI_EVEX_CASE(vpsllvq, Di_EvexShiftVariable(pDianaContext, pCallContext, 8, DI_SHIFT_LEFT))
    DI_EVEX_CASE(vpsrlvw, Di_EvexShiftVariable(pDianaContext, pCallContext, 2, DI_SHIFT_RIGHT))
    DI_EVEX_CASE(vpsrlvd, Di_EvexShiftVariable(pDianaContext, pCallContext, 4, DI_SHIFT_RIGHT))
    DI_EVEX_CASE(vpsrlvq, Di_EvexShiftVariable(pDianaContext, pCallContext, 8, DI_SHIFT_RIGHT))
    DI_EVEX_CASE(vpsravw, Di_EvexShiftVariable(pDianaContext, pCallContext, 2, DI_SHIFT_ARITHMETIC))
    DI_EVEX_CASE(vpsravd, Di_EvexShiftVariable(pDianaContext, pCallContext, 4, DI_SHIFT_ARITHMETIC))
    DI_EVEX_CASE(vpsravq, Di_EvexShiftVariable(pDianaContext, pCallContext, 8, DI_SHIFT_ARITHMETIC))
    DI_EVEX_CASE(vprold, Di_EvexRotate(pDianaContext, pCallContext, 4, 1, 0))
    DI_EVEX_CASE(vprolq, Di_EvexRotate(pDianaContext, pCallContext, 8, 1, 0))
    DI_EVEX_CASE(vprord, Di_EvexRotate(pDianaContext, pCallContext, 4, 0, 0))
    DI_EVEX_CASE(vprorq, Di_EvexRotate(pDianaContext, pCallContext, 8, 0, 0))
    DI_EVEX_CASE(vprolvd, Di_EvexRotate(pDianaContext, pCallContext, 4, 1, 1))
    DI_EVEX_CASE(vprolvq, Di_EvexRotate(pDianaContext, pCallContext, 8, 1, 1))
    DI_EVEX_CASE(vprorvd, Di_EvexRotate(pDianaContext, pCallContext, 4, 0, 1))
    DI_EVEX_CASE(vprorvq, Di_EvexRotate(pDianaContext, pCallContext, 8, 0, 1))
    DI_EVEX_CASE(vpslldq, Di_EvexShiftBytes(pDianaContext, pCallContext, 1))
    DI_EVEX_CASE(vpsrldq, Di_EvexShiftBytes(pDianaContext, pCallContext, 0))
    // shuffles, permutes, inserts, extracts
    DI_EVEX_CASE(vpshufd, Di_EvexShuffleImm(pDianaContext, pCallContext, DI_SHUFFLE_D))
    DI_EVEX_CASE(vpshufhw, Di_EvexShuffleImm(pDianaContext, pCallContext, DI_SHUFFLE_HW))
    DI_EVEX_CASE(vpshuflw, Di_EvexShuffleImm(pDianaContext, pCallContext, DI_SHUFFLE_LW))
    DI_EVEX_CASE(vpalignr, Di_EvexAlign(pDianaContext, pCallContext, 1))
    DI_EVEX_CASE(valignd, Di_EvexAlign(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(valignq, Di_EvexAlign(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vshufps, Di_EvexShufp(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vshufpd, Di_EvexShufp(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vshuff32x4, Di_EvexShuffleBlocks(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vshuff64x2, Di_EvexShuffleBlocks(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vshufi32x4, Di_EvexShuffleBlocks(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vshufi64x2, Di_EvexShuffleBlocks(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpermd, Di_EvexPermute(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpermps, Di_EvexPermute(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpermq, Di_EvexPermute(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpermpd, Di_EvexPermute(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpermw, Di_EvexPermute(pDianaContext, pCallContext, 2))
    DI_EVEX_CASE(vpermi2w, Di_EvexPermute2(pDianaContext, pCallContext, 2, 1))
    DI_EVEX_CASE(vpermi2d, Di_EvexPermute2(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vpermi2q, Di_EvexPermute2(pDianaContext, pCallContext, 8, 1))
    DI_EVEX_CASE(vpermi2ps, Di_EvexPermute2(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vpermi2pd, Di_EvexPermute2(pDianaContext, pCallContext, 8, 1))
    DI_EVEX_CASE(vpermt2w, Di_EvexPermute2(pDianaContext, pCallContext, 2, 0))
    DI_EVEX_CASE(vpermt2d, Di_EvexPermute2(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vpermt2q, Di_EvexPermute2(pDianaContext, pCallContext, 8, 0))
    DI_EVEX_CASE(vpermt2ps, Di_EvexPermute2(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vpermt2pd, Di_EvexPermute2(pDianaContext, pCallContext, 8, 0))
    DI_EVEX_CASE(vpermilps, Di_EvexPermil(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpermilpd, Di_EvexPermil(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vinsertf32x4, Di_EvexInsert(pDianaContext, pCallContext, 4, 16))
    DI_EVEX_CASE(vinserti32x4, Di_EvexInsert(pDianaContext, pCallContext, 4, 16))
    DI_EVEX_CASE(vinsertf64x2, Di_EvexInsert(pDianaContext, pCallContext, 8, 16))
    DI_EVEX_CASE(vinserti64x2, Di_EvexInsert(pDianaContext, pCallContext, 8, 16))
    DI_EVEX_CASE(vinsertf32x8, Di_EvexInsert(pDianaContext, pCallContext, 4, 32))
    DI_EVEX_CASE(vinserti32x8, Di_EvexInsert(pDianaContext, pCallContext, 4, 32))
    DI_EVEX_CASE(vinsertf64x4, Di_EvexInsert(pDianaContext, pCallContext, 8, 32))
    DI_EVEX_CASE(vinserti64x4, Di_EvexInsert(pDianaContext, pCallContext, 8, 32))
    DI_EVEX_CASE(vextractf32x4, Di_EvexExtract(pDianaContext, pCallContext, 4, 16))
    DI_EVEX_CASE(vextracti32x4, Di_EvexExtract(pDianaContext, pCallContext, 4, 16))
    DI_EVEX_CASE(vextractf64x2, Di_EvexExtract(pDianaContext, pCallContext, 8, 16))
    DI_EVEX_CASE(vextracti64x2, Di_EvexExtract(pDianaContext, pCallContext, 8, 16))
    DI_EVEX_CASE(vextractf32x8, Di_EvexExtract(pDianaContext, pCallContext, 4, 32))
    DI_EVEX_CASE(vextracti32x8, Di_EvexExtract(pDianaContext, pCallContext, 4, 32))
    DI_EVEX_CASE(vextractf64x4, Di_EvexExtract(pDianaContext, pCallContext, 8, 32))
    DI_EVEX_CASE(vextracti64x4, Di_EvexExtract(pDianaContext, pCallContext, 8, 32))
    DI_EVEX_CASE(vinsertps, Di_EvexInsertps(pDianaContext, pCallContext))
    DI_EVEX_CASE(vextractps, Di_EvexExtractElement(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpextrb, Di_EvexExtractElement(pDianaContext, pCallContext, 1))
    DI_EVEX_CASE(vpextrw, Di_EvexExtractElement(pDianaContext, pCallContext, 2))
    DI_EVEX_CASE(vpextrd, Di_EvexExtractElement(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpextrq, Di_EvexExtractElement(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpinsrb, Di_EvexInsertElement(pDianaContext, pCallContext, 1))
    DI_EVEX_CASE(vpinsrw, Di_EvexInsertElement(pDianaContext, pCallContext, 2))
    DI_EVEX_CASE(vpinsrd, Di_EvexInsertElement(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpinsrq, Di_EvexInsertElement(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vcompressps, Di_EvexCompress(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vcompresspd, Di_EvexCompress(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpcompressd, Di_EvexCompress(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpcompressq, Di_EvexCompress(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vexpandps, Di_EvexExpand(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vexpandpd, Di_EvexExpand(pDianaContext, pCallContext, 8))
    DI_EVEX_CASE(vpexpandd, Di_EvexExpand(pDianaContext, pCallContext, 4))
    DI_EVEX_CASE(vpexpandq, Di_EvexExpand(pDianaContext, pCallContext, 8))
    // extend, truncate
    DI_EVEX_CASE(vpmovzxbw, Di_EvexExtend(pDianaContext, pCallContext, 1, 2, 0))
    DI_EVEX_CASE(vpmovzxbd, Di_EvexExtend(pDianaContext, pCallContext, 1, 4, 0))
    DI_EVEX_CASE(vpmovzxbq, Di_EvexExtend(pDianaContext, pCallContext, 1, 8, 0))
    DI_EVEX_CASE(vpmovzxwd, Di_EvexExtend(pDianaContext, pCallContext, 2, 4, 0))
    DI_EVEX_CASE(vpmovzxwq, Di_EvexExtend(pDianaContext, pCallContext, 2, 8, 0))
    DI_EVEX_CASE(vpmovzxdq, Di_EvexExtend(pDianaContext, pCallContext, 4, 8, 0))
    DI_EVEX_CASE(vpmovsxbw, Di_EvexExtend(pDianaContext, pCallContext, 1, 2, 1))
    DI_EVEX_CASE(vpmovsxbd, Di_EvexExtend(pDianaContext, pCallContext, 1, 4, 1))
    DI_EVEX_CASE(vpmovsxbq, Di_EvexExtend(pDianaContext, pCallContext, 1, 8, 1))
    DI_EVEX_CASE(vpmovsxwd, Di_EvexExtend(pDianaContext, pCallContext, 2, 4, 1))
    DI_EVEX_CASE(vpmovsxwq, Di_EvexExtend(pDianaContext, pCallContext, 2, 8, 1))
    DI_EVEX_CASE(vpmovsxdq, Di_EvexExtend(pDianaContext, pCallContext, 4, 8, 1))
    DI_EVEX_CASE(vpmovwb, Di_EvexTruncate(pDianaContext, pCallContext, 2, 1, 0))
    DI_EVEX_CASE(vpmovdb, Di_EvexTruncate(pDianaContext, pCallContext, 4, 1, 0))
    DI_EVEX_CASE(vpmovdw, Di_EvexTruncate(pDianaContext, pCallContext, 4, 2, 0))
    DI_EVEX_CASE(vpmovqb, Di_EvexTruncate(pDianaContext, pCallContext, 8, 1, 0))
    DI_EVEX_CASE(vpmovqw, Di_EvexTruncate(pDianaContext, pCallContext, 8, 2, 0))
    DI_EVEX_CASE(vpmovqd, Di_EvexTruncate(pDianaContext, pCallContext, 8, 4, 0))
    DI_EVEX_CASE(vpmovswb, Di_EvexTruncate(pDianaContext, pCallContext, 2, 1, 1))
    DI_EVEX_CASE(vpmovsdb, Di_EvexTruncate(pDianaContext, pCallContext, 4, 1, 1))
    DI_EVEX_CASE(vpmovsdw, Di_EvexTruncate(pDianaContext, pCallContext, 4, 2, 1))
    DI_EVEX_CASE(vpmovsqb, Di_EvexTruncate(pDianaContext, pCallContext, 8, 1, 1))
    DI_EVEX_CASE(vpmovsqw, Di_EvexTruncate(pDianaContext, pCallContext, 8, 2, 1))
    DI_EVEX_CASE(vpmovsqd, Di_EvexTruncate(pDianaContext, pCallContext, 8, 4, 1))
    DI_EVEX_CASE(vpmovuswb, Di_EvexTruncate(pDianaContext, pCallContext, 2, 1, 2))
    DI_EVEX_CASE(vpmovusdb, Di_EvexTruncate(pDianaContext, pCallContext, 4, 1, 2))
    DI_EVEX_CASE(vpmovusdw, Di_EvexTruncate(pDianaContext, pCallContext, 4, 2, 2))
    DI_EVEX_CASE(vpmovusqb, Di_EvexTruncate(pDianaContext, pCallContext, 8, 1, 2))
    DI_EVEX_CASE(vpmovusqw, Di_EvexTruncate(pDianaContext, pCallContext, 8, 2, 2))
    DI_EVEX_CASE(vpmovusqd, Di_EvexTruncate(pDianaContext, pCallContext, 8, 4, 2))
    // gathers, scatters
    DI_EVEX_CASE(vpgatherdd, Di_EvexGatherScatter(pDianaContext, pCallContext, 4, 4, 0))
    DI_EVEX_CASE(vpgatherdq, Di_EvexGatherScatter(pDianaContext, pCallContext, 4, 8, 0))
    DI_EVEX_CASE(vpgatherqd, Di_EvexGatherScatter(pDianaContext, pCallContext, 8, 4, 0))
    DI_EVEX_CASE(vpgatherqq, Di_EvexGatherScatter(pDianaContext, pCallContext, 8, 8, 0))
    DI_EVEX_CASE(vgatherdps, Di_EvexGatherScatter(pDianaContext, pCallContext, 4, 4, 0))
    DI_EVEX_CASE(vgatherdpd, Di_EvexGatherScatter(pDianaContext, pCallContext, 4, 8, 0))
    DI_EVEX_CASE(vgatherqps, Di_EvexGatherScatter(pDianaContext, pCallContext, 8, 4, 0))
    DI_EVEX_CASE(vgatherqpd, Di_EvexGatherScatter(pDianaContext, pCallContext, 8, 8, 0))
    DI_EVEX_CASE(vpscatterdd, Di_EvexGatherScatter(pDianaContext, pCallContext, 4, 4, 1))
    DI_EVEX_CASE(vpscatterdq, Di_EvexGatherScatter(pDianaContext, pCallContext, 4, 8, 1))
    DI_EVEX_CASE(vpscatterqd, Di_EvexGatherScatter(pDianaContext, pCallContext, 8, 4, 1))
    DI_EVEX_CASE(vpscatterqq, Di_EvexGatherScatter(pDianaContext, pCallContext, 8, 8, 1))
    DI_EVEX_CASE(vscatterdps, Di_EvexGatherScatter(pDianaContext, pCallContext, 4, 4, 1))
    DI_EVEX_CASE(vscatterdpd, Di_EvexGatherScatter(pDianaContext, pCallContext, 4, 8, 1))
    DI_EVEX_CASE(vscatterqps, Di_EvexGatherScatter(pDianaContext, pCallContext, 8, 4, 1))
    DI_EVEX_CASE(vscatterqpd, Di_EvexGatherScatter(pDianaContext, pCallContext, 8, 8, 1))
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    // floating point
    DI_EVEX_CASE(vaddps, Di_EvexFloat(pDianaContext, pCallContext, 4, 0, DI_SSE_ADD))
    DI_EVEX_CASE(vaddpd, Di_EvexFloat(pDianaContext, pCallContext, 8, 0, DI_SSE_ADD))
    DI_EVEX_CASE(vaddss, Di_EvexFloat(pDianaContext, pCallContext, 4, 1, DI_SSE_ADD))
    DI_EVEX_CASE(vaddsd, Di_EvexFloat(pDianaContext, pCallContext, 8, 1, DI_SSE_ADD))
    DI_EVEX_CASE(vsubps, Di_EvexFloat(pDianaContext, pCallContext, 4, 0, DI_SSE_SUB))
    DI_EVEX_CASE(vsubpd, Di_EvexFloat(pDianaContext, pCallContext, 8, 0, DI_SSE_SUB))
    DI_EVEX_CASE(vsubss, Di_EvexFloat(pDianaContext, pCallContext, 4, 1, DI_SSE_SUB))
    DI_EVEX_CASE(vsubsd, Di_EvexFloat(pDianaContext, pCallContext, 8, 1, DI_SSE_SUB))
    DI_EVEX_CASE(vmulps, Di_EvexFloat(pDianaContext, pCallContext, 4, 0, DI_SSE_MUL))
    DI_EVEX_CASE(vmulpd, Di_EvexFloat(pDianaContext, pCallContext, 8, 0, DI_SSE_MUL))
    DI_EVEX_CASE(vmulss, Di_EvexFloat(pDianaContext, pCallContext, 4, 1, DI_SSE_MUL))
    DI_EVEX_CASE(vmulsd, Di_EvexFloat(pDianaContext, pCallContext, 8, 1, DI_SSE_MUL))
    DI_EVEX_CASE(vdivps, Di_EvexFloat(pDianaContext, pCallContext, 4, 0, DI_SSE_DIV))
    DI_EVEX_CASE(vdivpd, Di_EvexFloat(pDianaContext, pCallContext, 8, 0, DI_SSE_DIV))
    DI_EVEX_CASE(vdivss, Di_EvexFloat(pDianaContext, pCallContext, 4, 1, DI_SSE_DIV))
    DI_EVEX_CASE(vdivsd, Di_EvexFloat(pDianaContext, pCallContext, 8, 1, DI_SSE_DIV))
    DI_EVEX_CASE(vminps, Di_EvexFloat(pDianaContext, pCallContext, 4, 0, DI_SSE_MIN))
    DI_EVEX_CASE(vminpd, Di_EvexFloat(pDianaContext, pCallContext, 8, 0, DI_SSE_MIN))
    DI_EVEX_CASE(vminss, Di_EvexFloat(pDianaContext, pCallContext, 4, 1, DI_SSE_MIN))
    DI_EVEX_CASE(vminsd, Di_EvexFloat(pDianaContext, pCallContext, 8, 1, DI_SSE_MIN))
    DI_EVEX_CASE(vmaxps, Di_EvexFloat(pDianaContext, pCallContext, 4, 0, DI_SSE_MAX))
    DI_EVEX_CASE(vmaxpd, Di_EvexFloat(pDianaContext, pCallContext, 8, 0, DI_SSE_MAX))
    DI_EVEX_CASE(vmaxss, Di_EvexFloat(pDianaContext, pCallContext, 4, 1, DI_SSE_MAX))
    DI_EVEX_CASE(vmaxsd, Di_EvexFloat(pDianaContext, pCallContext, 8, 1, DI_SSE_MAX))
    DI_EVEX_CASE(vsqrtps, Di_EvexFloat(pDianaContext, pCallContext, 4, 0, DI_SSE_SQRT))
    DI_EVEX_CASE(vsqrtpd, Di_EvexFloat(pDianaContext, pCallContext, 8, 0, DI_SSE_SQRT))
    DI_EVEX_CASE(vsqrtss, Di_EvexFloat(pDianaContext, pCallContext, 4, 1, DI_SSE_SQRT))
    DI_EVEX_CASE(vsqrtsd, Di_EvexFloat(pDianaContext, pCallContext, 8, 1, DI_SSE_SQRT))
    DI_EVEX_CASE(vcmpps, Di_EvexCompareFloat(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vcmppd, Di_EvexCompareFloat(pDianaContext, pCallContext, 8, 0))
    DI_EVEX_CASE(vcmpss, Di_EvexCompareFloat(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vcmpsd, Di_EvexCompareFloat(pDianaContext, pCallContext, 8, 1))
    DI_EVEX_CASE(vcomiss, Di_EvexComi(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vcomisd, Di_EvexComi(pDianaContext, pCallContext, 8, 0))
    DI_EVEX_CASE(vucomiss, Di_EvexComi(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vucomisd, Di_EvexComi(pDianaContext, pCallContext, 8, 1))
#define DI_EVEX_FMA_CASES(op, kind) \
    DI_EVEX_CASE(vf##op##132ps, Di_EvexFma(pDianaContext, pCallContext, 4, 0, 132, kind)) \
    DI_EVEX_CASE(vf##op##132pd, Di_EvexFma(pDianaContext, pCallContext, 8, 0, 132, kind)) \
    DI_EVEX_CASE(vf##op##213ps, Di_EvexFma(pDianaContext, pCallContext, 4, 0, 213, kind)) \
    DI_EVEX_CASE(vf##op##213pd, Di_EvexFma(pDianaContext, pCallContext, 8, 0, 213, kind)) \
    DI_EVEX_CASE(vf##op##231ps, Di_EvexFma(pDianaContext, pCallContext, 4, 0, 231, kind)) \
    DI_EVEX_CASE(vf##op##231pd, Di_EvexFma(pDianaContext, pCallContext, 8, 0, 231, kind))
#define DI_EVEX_FMA_SCALAR_CASES(op, kind) \
    DI_EVEX_CASE(vf##op##132ss, Di_EvexFma(pDianaContext, pCallContext, 4, 1, 132, kind)) \
    DI_EVEX_CASE(vf##op##132sd, Di_EvexFma(pDianaContext, pCallContext, 8, 1, 132, kind)) \
    DI_EVEX_CASE(vf##op##213ss, Di_EvexFma(pDianaContext, pCallContext, 4, 1, 213, kind)) \
    DI_EVEX_CASE(vf##op##213sd, Di_EvexFma(pDianaContext, pCallContext, 8, 1, 213, kind)) \
    DI_EVEX_CASE(vf##op##231ss, Di_EvexFma(pDianaContext, pCallContext, 4, 1, 231, kind)) \
    DI_EVEX_CASE(vf##op##231sd, Di_EvexFma(pDianaContext, pCallContext, 8, 1, 231, kind))
    DI_EVEX_FMA_CASES(madd, DI_EVEX_FMA_ADD)
    DI_EVEX_FMA_CASES(msub, DI_EVEX_FMA_SUB)
    DI_EVEX_FMA_CASES(nmadd, DI_EVEX_FMA_NADD)
    DI_EVEX_FMA_CASES(nmsub, DI_EVEX_FMA_NSUB)
    DI_EVEX_FMA_CASES(maddsub, DI_EVEX_FMA_ADDSUB)
    DI_EVEX_FMA_CASES(msubadd, DI_EVEX_FMA_SUBADD)
    DI_EVEX_FMA_SCALAR_CASES(madd, DI_EVEX_FMA_ADD)
    DI_EVEX_FMA_SCALAR_CASES(msub, DI_EVEX_FMA_SUB)
    DI_EVEX_FMA_SCALAR_CASES(nmadd, DI_EVEX_FMA_NADD)
    DI_EVEX_FMA_SCALAR_CASES(nmsub, DI_EVEX_FMA_NSUB)
    // conversions
    DI_EVEX_CASE(vcvtdq2ps, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_I32, DI_EVEX_F32, 0))
    DI_EVEX_CASE(vcvtudq2ps, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_U32, DI_EVEX_F32, 0))
    DI_EVEX_CASE(vcvtqq2ps, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_I64, DI_EVEX_F32, 0))
    DI_EVEX_CASE(vcvtuqq2ps, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_U64, DI_EVEX_F32, 0))
    DI_EVEX_CASE(vcvtdq2pd, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_I32, DI_EVEX_F64, 0))
    DI_EVEX_CASE(vcvtudq2pd, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_U32, DI_EVEX_F64, 0))
    DI_EVEX_CASE(vcvtqq2pd, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_I64, DI_EVEX_F64, 0))
    DI_EVEX_CASE(vcvtuqq2pd, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_U64, DI_EVEX_F64, 0))
    DI_EVEX_CASE(vcvtps2dq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_I32, 0))
    DI_EVEX_CASE(vcvttps2dq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_I32, 1))
    DI_EVEX_CASE(vcvtps2udq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_U32, 0))
    DI_EVEX_CASE(vcvttps2udq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_U32, 1))
    DI_EVEX_CASE(vcvtps2qq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_I64, 0))
    DI_EVEX_CASE(vcvttps2qq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_I64, 1))
    DI_EVEX_CASE(vcvtps2uqq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_U64, 0))
    DI_EVEX_CASE(vcvttps2uqq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_U64, 1))
    DI_EVEX_CASE(vcvtpd2dq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_I32, 0))
    DI_EVEX_CASE(vcvttpd2dq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_I32, 1))
    DI_EVEX_CASE(vcvtpd2udq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_U32, 0))
    DI_EVEX_CASE(vcvttpd2udq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_U32, 1))
    DI_EVEX_CASE(vcvtpd2qq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_I64, 0))
    DI_EVEX_CASE(vcvttpd2qq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_I64, 1))
    DI_EVEX_CASE(vcvtpd2uqq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_U64, 0))
    DI_EVEX_CASE(vcvttpd2uqq, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_U64, 1))
    DI_EVEX_CASE(vcvtps2pd, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_F64, 0))
    DI_EVEX_CASE(vcvtpd2ps, Di_EvexConvert(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_F32, 0))
    DI_EVEX_CASE(vcvtss2sd, Di_EvexConvertScalar(pDianaContext, pCallContext, DI_EVEX_F32, DI_EVEX_F64))
    DI_EVEX_CASE(vcvtsd2ss, Di_EvexConvertScalar(pDianaContext, pCallContext, DI_EVEX_F64, DI_EVEX_F32))
    DI_EVEX_CASE(vcvtsi2ss, Di_EvexConvertInteger(pDianaContext, pCallContext, 1, DI_EVEX_F32, 0, 0))
    DI_EVEX_CASE(vcvtsi2sd, Di_EvexConvertInteger(pDianaContext, pCallContext, 1, DI_EVEX_F64, 0, 0))
    DI_EVEX_CASE(vcvtusi2ss, Di_EvexConvertInteger(pDianaContext, pCallContext, 1, DI_EVEX_F32, 1, 0))
    DI_EVEX_CASE(vcvtusi2sd, Di_EvexConvertInteger(pDianaContext, pCallContext, 1, DI_EVEX_F64, 1, 0))
    DI_EVEX_CASE(vcvtss2si, Di_EvexConvertInteger(pDianaContext, pCallContext, 0, DI_EVEX_F32, 0, 0))
    DI_EVEX_CASE(vcvttss2si, Di_EvexConvertInteger(pDianaContext, pCallContext, 0, DI_EVEX_F32, 0, 1))
    DI_EVEX_CASE(vcvtss2usi, Di_EvexConvertInteger(pDianaContext, pCallContext, 0, DI_EVEX_F32, 1, 0))
    DI_EVEX_CASE(vcvttss2usi, Di_EvexConvertInteger(pDianaContext, pCallContext, 0, DI_EVEX_F32, 1, 1))
    DI_EVEX_CASE(vcvtsd2si, Di_EvexConvertInteger(pDianaContext, pCallContext, 0, DI_EVEX_F64, 0, 0))
    DI_EVEX_CASE(vcvttsd2si, Di_EvexConvertInteger(pDianaContext, pCallContext, 0, DI_EVEX_F64, 0, 1))
    DI_EVEX_CASE(vcvtsd2usi, Di_EvexConvertInteger(pDianaContext, pCallContext, 0, DI_EVEX_F64, 1, 0))
    DI_EVEX_CASE(vcvttsd2usi, Di_EvexConvertInteger(pDianaContext, pCallContext, 0, DI_EVEX_F64, 1, 1))
    DI_EVEX_CASE(vcvtph2ps, Di_EvexHalf(pDianaContext, pCallContext, 0))
    DI_EVEX_CASE(vcvtps2ph, Di_EvexHalf(pDianaContext, pCallContext, 1))
    // the special float operations
    DI_EVEX_CASE(vgetexpps, Di_EvexSpecial(pDianaContext, pCallContext, 4, 0, DI_EVEX_GETEXP))
    DI_EVEX_CASE(vgetexppd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 0, DI_EVEX_GETEXP))
    DI_EVEX_CASE(vgetexpss, Di_EvexSpecial(pDianaContext, pCallContext, 4, 1, DI_EVEX_GETEXP))
    DI_EVEX_CASE(vgetexpsd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 1, DI_EVEX_GETEXP))
    DI_EVEX_CASE(vgetmantps, Di_EvexSpecial(pDianaContext, pCallContext, 4, 0, DI_EVEX_GETMANT))
    DI_EVEX_CASE(vgetmantpd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 0, DI_EVEX_GETMANT))
    DI_EVEX_CASE(vgetmantss, Di_EvexSpecial(pDianaContext, pCallContext, 4, 1, DI_EVEX_GETMANT))
    DI_EVEX_CASE(vgetmantsd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 1, DI_EVEX_GETMANT))
    DI_EVEX_CASE(vscalefps, Di_EvexSpecial(pDianaContext, pCallContext, 4, 0, DI_EVEX_SCALEF))
    DI_EVEX_CASE(vscalefpd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 0, DI_EVEX_SCALEF))
    DI_EVEX_CASE(vscalefss, Di_EvexSpecial(pDianaContext, pCallContext, 4, 1, DI_EVEX_SCALEF))
    DI_EVEX_CASE(vscalefsd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 1, DI_EVEX_SCALEF))
    DI_EVEX_CASE(vrndscaleps, Di_EvexSpecial(pDianaContext, pCallContext, 4, 0, DI_EVEX_RNDSCALE))
    DI_EVEX_CASE(vrndscalepd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 0, DI_EVEX_RNDSCALE))
    DI_EVEX_CASE(vrndscaless, Di_EvexSpecial(pDianaContext, pCallContext, 4, 1, DI_EVEX_RNDSCALE))
    DI_EVEX_CASE(vrndscalesd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 1, DI_EVEX_RNDSCALE))
    DI_EVEX_CASE(vreduceps, Di_EvexSpecial(pDianaContext, pCallContext, 4, 0, DI_EVEX_REDUCE))
    DI_EVEX_CASE(vreducepd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 0, DI_EVEX_REDUCE))
    DI_EVEX_CASE(vreducess, Di_EvexSpecial(pDianaContext, pCallContext, 4, 1, DI_EVEX_REDUCE))
    DI_EVEX_CASE(vreducesd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 1, DI_EVEX_REDUCE))
    DI_EVEX_CASE(vrangeps, Di_EvexSpecial(pDianaContext, pCallContext, 4, 0, DI_EVEX_RANGE))
    DI_EVEX_CASE(vrangepd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 0, DI_EVEX_RANGE))
    DI_EVEX_CASE(vrangess, Di_EvexSpecial(pDianaContext, pCallContext, 4, 1, DI_EVEX_RANGE))
    DI_EVEX_CASE(vrangesd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 1, DI_EVEX_RANGE))
    DI_EVEX_CASE(vfixupimmps, Di_EvexSpecial(pDianaContext, pCallContext, 4, 0, DI_EVEX_FIXUPIMM))
    DI_EVEX_CASE(vfixupimmpd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 0, DI_EVEX_FIXUPIMM))
    DI_EVEX_CASE(vfixupimmss, Di_EvexSpecial(pDianaContext, pCallContext, 4, 1, DI_EVEX_FIXUPIMM))
    DI_EVEX_CASE(vfixupimmsd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 1, DI_EVEX_FIXUPIMM))
    DI_EVEX_CASE(vrcp14ps, Di_EvexSpecial(pDianaContext, pCallContext, 4, 0, DI_EVEX_RCP14))
    DI_EVEX_CASE(vrcp14pd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 0, DI_EVEX_RCP14))
    DI_EVEX_CASE(vrcp14ss, Di_EvexSpecial(pDianaContext, pCallContext, 4, 1, DI_EVEX_RCP14))
    DI_EVEX_CASE(vrcp14sd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 1, DI_EVEX_RCP14))
    DI_EVEX_CASE(vrsqrt14ps, Di_EvexSpecial(pDianaContext, pCallContext, 4, 0, DI_EVEX_RSQRT14))
    DI_EVEX_CASE(vrsqrt14pd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 0, DI_EVEX_RSQRT14))
    DI_EVEX_CASE(vrsqrt14ss, Di_EvexSpecial(pDianaContext, pCallContext, 4, 1, DI_EVEX_RSQRT14))
    DI_EVEX_CASE(vrsqrt14sd, Di_EvexSpecial(pDianaContext, pCallContext, 8, 1, DI_EVEX_RSQRT14))
    DI_EVEX_CASE(vfpclassps, Di_EvexFpclass(pDianaContext, pCallContext, 4, 0))
    DI_EVEX_CASE(vfpclasspd, Di_EvexFpclass(pDianaContext, pCallContext, 8, 0))
    DI_EVEX_CASE(vfpclassss, Di_EvexFpclass(pDianaContext, pCallContext, 4, 1))
    DI_EVEX_CASE(vfpclasssd, Di_EvexFpclass(pDianaContext, pCallContext, 8, 1))
#endif
    default:
        break;
    }
    return DI_UNSUPPORTED_COMMAND;
}
