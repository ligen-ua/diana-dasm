#include "diana_processor_cmd_fpu.h"
#include "diana_processor_cmd_fpu_internal.h"

// The x87 instructions follow the Intel SDM and Bochs (where the softfloat library comes from): an empty source
// register is a stack underflow (IE and SF; with IE masked the default NaN is the result), a full ST(7) on a load
// is a stack overflow, every conversion and operation reports its flags through DI_FPU_ProcessException (DE, IE of
// a SNaN, PE and C1 of a rounded result), an unmasked exception writes nothing. The memory forms read m32, m64,
// m80 and the integers m16, m32, m64 (sign-extended); the register forms are dest, src as the decoder names them
// (fdivr st(i), st: st(i) = st / st(i)).

#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU

typedef enum
{
    DI_FPU_ADD, DI_FPU_SUB, DI_FPU_SUBR, DI_FPU_MUL, DI_FPU_DIV, DI_FPU_DIVR
} DiFpuOperation;

static floatx80_t Di_FpuRead(DianaProcessor * pCallContext, int st)
{
    floatx80_t value;
    DianaProcessor_FPU_GetSTRegister_80(pCallContext, (DianaUnifiedRegister)(reg_fpu_ST0 + st), &value);
    return value;
}

static void Di_FpuWrite(DianaProcessor * pCallContext, const floatx80_t * pValue, int st)
{
    DianaProcessor_FPU_SetSTRegister_80(pCallContext, (DianaUnifiedRegister)(reg_fpu_ST0 + st), pValue);
}

// the st(i) registers of a register form: dest, src; one register operand: st(0) and it; none: st(0), st(1)
static void Di_FpuRegisters(DianaProcessor * pCallContext, int * pDest, int * pSrc)
{
    const DianaLinkedOperand * pOperands = pCallContext->m_result.linkedOperands;
    int count = pCallContext->m_result.iLinkedOpCount;
    *pDest = 0;
    *pSrc = 1;
    if (count >= 2 && pOperands[0].type == diana_register && pOperands[1].type == diana_register)
    {
        *pDest = pOperands[0].value.recognizedRegister - reg_fpu_ST0;
        *pSrc = pOperands[1].value.recognizedRegister - reg_fpu_ST0;
    }
    else if (count == 1 && pOperands[0].type == diana_register)
    {
        *pSrc = pOperands[0].value.recognizedRegister - reg_fpu_ST0;
    }
}

static int Di_FpuIsMemoryForm(DianaProcessor * pCallContext)
{
    return pCallContext->m_result.iLinkedOpCount >= 1 &&
           (pCallContext->m_result.linkedOperands[0].type == diana_index ||
            pCallContext->m_result.linkedOperands[0].type == diana_memory);
}

// the memory operand: its bytes (m16 ... m80) and size
typedef union _DiFpuMemory
{
    DI_UINT16 i16;
    DI_UINT32 i32;
    DI_UINT64 i64;
    float32 f32;
    float64 f64;
    floatx80_t f80;
    DI_CHAR bytes[16];
} DiFpuMemory;

static int Di_FpuReadMemory(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext,
                            DiFpuMemory * pValue, int * pSize)
{
    OPERAND_SIZE doneBytes = 0;
    DIANA_MEMSET(pValue, 0, sizeof(*pValue));
    *pSize = pCallContext->m_result.linkedOperands[0].usedSize;
    if (*pSize != 2 && *pSize != 4 && *pSize != 8 && *pSize != 10)
        return DI_ERROR;
    return Diana_ReadRawBufferFromArgMem(pDianaContext, pCallContext, 0, pValue->bytes, *pSize, &doneBytes, 0);
}

static int Di_FpuWriteMemory(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext,
                             const void * pValue, int size)
{
    OPERAND_SIZE doneBytes = 0;
    return Diana_WriteRawBufferToArgMem(pDianaContext, pCallContext, 0, (void *)pValue, size, &doneBytes, 0);
}

// a m16/m32/m64 integer, sign-extended
static floatx80_t Di_FpuIntegerToFloatx80(const DiFpuMemory * pValue, int size)
{
    switch (size)
    {
    case 2: return int32_to_floatx80((DI_INT16)pValue->i16);
    case 4: return int32_to_floatx80((DI_INT32)pValue->i32);
    default: return int64_to_floatx80((DI_INT64)pValue->i64);
    }
}

// a m32/m64/m80 float as floatx80; the conversion flags (DE, IE of a SNaN) go to *pStatus
static floatx80_t Di_FpuFloatToFloatx80(const DiFpuMemory * pValue, int size, float_status_t * pStatus)
{
    switch (size)
    {
    case 4: return float32_to_floatx80(pValue->f32, pStatus);
    case 8: return float64_to_floatx80(pValue->f64, pStatus);
    default: return pValue->f80;
    }
}

// an operation with a m32/m64 operand when one of them is a NaN: the x87 rules (a SNaN is IE, two NaNs give the
// one with the larger significand), the conversion of b alone would lose a SNaN. 1: *pResult is the result.
static int Di_FpuHandleNaN(floatx80_t a, const DiFpuMemory * pB, int bSize, floatx80_t * pResult,
                           float_status_t * pStatus)
{
    int aIsNaN, bIsNaN, aIsSignaling, bIsSignaling;
    floatx80_t b;
    if (floatx80_is_unsupported(a))
    {
        float_raise(pStatus, float_flag_invalid);
        *pResult = floatx80_default_nan;
        return 1;
    }
    aIsNaN = floatx80_is_nan(a);
    bIsNaN = bSize == 4 ? float32_is_nan(pB->f32) : float64_is_nan(pB->f64);
    if (!aIsNaN && !bIsNaN)
        return 0;
    aIsSignaling = floatx80_is_signaling_nan(a);
    bIsSignaling = bSize == 4 ? float32_is_signaling_nan(pB->f32) : float64_is_signaling_nan(pB->f64);
    if (aIsSignaling | bIsSignaling)
        float_raise(pStatus, float_flag_invalid);
    a = propagateFloatx80NaN(a, pStatus);
    if (aIsNaN && !bIsNaN)
    {
        *pResult = a;
        return 1;
    }
    b = Di_FpuFloatToFloatx80(pB, bSize, pStatus);
    if (aIsSignaling)
    {
        if (!bIsSignaling)
        {
            *pResult = bIsNaN ? b : a;
            return 1;
        }
    }
    else if (!aIsNaN)
    {
        *pResult = b;
        return 1;
    }
    else if (bIsSignaling)
    {
        *pResult = a;
        return 1;
    }
    // the larger significand
    if (a.fraction < b.fraction)
        *pResult = b;
    else if (b.fraction < a.fraction)
        *pResult = a;
    else
        *pResult = a.exp < b.exp ? a : b;
    return 1;
}

static floatx80_t Di_FpuCalculate(DiFpuOperation operation, floatx80_t a, floatx80_t b, float_status_t * pStatus)
{
    switch (operation)
    {
    case DI_FPU_ADD: return floatx80_add(a, b, pStatus);
    case DI_FPU_SUB: return floatx80_sub(a, b, pStatus);
    case DI_FPU_SUBR: return floatx80_sub(b, a, pStatus);
    case DI_FPU_MUL: return floatx80_mul(a, b, pStatus);
    case DI_FPU_DIV: return floatx80_div(a, b, pStatus);
    default: return floatx80_div(b, a, pStatus);
    }
}

// fadd, fsub, fsubr, fmul, fdiv, fdivr: st(0) op m32/m64, dest = dest op src, the p forms pop
static int Di_FpuArithmetic(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext,
                            DiFpuOperation operation, int pop)
{
    DI_FPU_START
    {
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        floatx80_t result;
        DI_FPU_CLEAR_C1;
        if (Di_FpuIsMemoryForm(pCallContext))
        {
            DiFpuMemory memory;
            int size = 0;
            DI_CHECK(Di_FpuReadMemory(pDianaContext, pCallContext, &memory, &size));
            if (DI_FPU_REG_IS_EMPTY(0))
            {
                DI_FPU_StackUnderflow(pCallContext, 0, 0);
                DI_PROC_END
            }
            if (!Di_FpuHandleNaN(Di_FpuRead(pCallContext, 0), &memory, size, &result, &status))
            {
                result = Di_FpuCalculate(operation, Di_FpuRead(pCallContext, 0),
                                         Di_FpuFloatToFloatx80(&memory, size, &status), &status);
            }
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
                Di_FpuWrite(pCallContext, &result, 0);
        }
        else
        {
            int dest = 0, src = 1;
            Di_FpuRegisters(pCallContext, &dest, &src);
            if (DI_FPU_REG_IS_EMPTY(dest) || DI_FPU_REG_IS_EMPTY(src))
            {
                DI_FPU_StackUnderflow(pCallContext, dest, pop);
                DI_PROC_END
            }
            result = Di_FpuCalculate(operation, Di_FpuRead(pCallContext, dest), Di_FpuRead(pCallContext, src),
                                     &status);
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                Di_FpuWrite(pCallContext, &result, dest);
                if (pop)
                    Diana_FPU_Pop(pCallContext);
            }
        }
    }
    DI_PROC_END
}

// fiadd, fisub, fisubr, fimul, fidiv, fidivr: st(0) op m16/m32 integer
static int Di_FpuIntegerArithmetic(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext,
                                   DiFpuOperation operation)
{
    DI_FPU_START
    {
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DiFpuMemory memory;
        int size = 0;
        floatx80_t result;
        DI_CHECK(Di_FpuReadMemory(pDianaContext, pCallContext, &memory, &size));
        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(0))
        {
            DI_FPU_StackUnderflow(pCallContext, 0, 0);
            DI_PROC_END
        }
        result = Di_FpuCalculate(operation, Di_FpuRead(pCallContext, 0), Di_FpuIntegerToFloatx80(&memory, size),
                                 &status);
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            Di_FpuWrite(pCallContext, &result, 0);
    }
    DI_PROC_END
}

// fcom, fcomp, fcompp, ficom, ficomp: C3 C2 C0; a NaN is unordered and IE (fcom signals on a QNaN too)
static int Di_FpuCompare(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int popCount,
                         int integer)
{
    DI_FPU_START
    {
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int relation;
        DI_FPU_CLEAR_C1;
        if (Di_FpuIsMemoryForm(pCallContext))
        {
            DiFpuMemory memory;
            int size = 0;
            floatx80_t a;
            DI_CHECK(Di_FpuReadMemory(pDianaContext, pCallContext, &memory, &size));
            if (DI_FPU_REG_IS_EMPTY(0))
            {
                DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
                DI_FPU_SetCompareFlags(pCallContext, float_relation_unordered);
                if ((pCallContext->m_fpu.controlWord & DI_FPU_CW_INVALID) && popCount)
                    Diana_FPU_Pop(pCallContext);
                DI_PROC_END
            }
            a = Di_FpuRead(pCallContext, 0);
            if (integer)
            {
                relation = floatx80_compare(a, Di_FpuIntegerToFloatx80(&memory, size), &status);
            }
            else if (floatx80_is_nan(a) || floatx80_is_unsupported(a) ||
                     (size == 4 ? float32_is_nan(memory.f32) : float64_is_nan(memory.f64)))
            {
                relation = float_relation_unordered;
                float_raise(&status, float_flag_invalid);
            }
            else
            {
                relation = floatx80_compare(a, Di_FpuFloatToFloatx80(&memory, size, &status), &status);
            }
        }
        else
        {
            int dest = 0, src = 1;
            Di_FpuRegisters(pCallContext, &dest, &src);
            if (DI_FPU_REG_IS_EMPTY(0) || DI_FPU_REG_IS_EMPTY(src))
            {
                DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
                DI_FPU_SetCompareFlags(pCallContext, float_relation_unordered);
                if (pCallContext->m_fpu.controlWord & DI_FPU_CW_INVALID)
                {
                    while (popCount--)
                        Diana_FPU_Pop(pCallContext);
                }
                DI_PROC_END
            }
            relation = floatx80_compare(Di_FpuRead(pCallContext, 0), Di_FpuRead(pCallContext, src), &status);
        }
        DI_FPU_SetCompareFlags(pCallContext, relation);
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
        {
            while (popCount--)
                Diana_FPU_Pop(pCallContext);
        }
    }
    DI_PROC_END
}

// fist, fistp, fisttp: m16/m32/m64, rounded by the control word or truncated; NaN and out of range are IE and the
// integer indefinite
static int Di_FpuStoreInteger(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int pop,
                              int truncate)
{
    DI_FPU_START
    {
        int size = pCallContext->m_result.linkedOperands[0].usedSize;
        DiFpuMemory memory;
        DI_FPU_CLEAR_C1;
        DIANA_MEMSET(&memory, 0, sizeof(memory));
        if (size != 2 && size != 4 && size != 8)
            return DI_ERROR;
        // the masked response: the integer indefinite
        memory.i64 = size == 8 ? DI_CONST64(0x8000000000000000) : size == 4 ? 0x80000000UL : 0x8000;
        if (DI_FPU_REG_IS_EMPTY(0))
        {
            DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
            if (!(pCallContext->m_fpu.controlWord & DI_FPU_CW_INVALID))
                DI_PROC_END
        }
        else
        {
            float_status_t status = FPU_pre_exception_handling(pCallContext);
            floatx80_t st0 = Di_FpuRead(pCallContext, 0);
            if (size == 8)
            {
                memory.i64 = (DI_UINT64)(truncate ? floatx80_to_int64_round_to_zero(st0, &status)
                                                  : floatx80_to_int64(st0, &status));
            }
            else
            {
                DI_INT32 value = floatx80_is_unsupported(st0) ? int32_indefinite
                                 : truncate ? floatx80_to_int32_round_to_zero(st0, &status)
                                            : floatx80_to_int32(st0, &status);
                if (floatx80_is_unsupported(st0))
                    float_raise(&status, float_flag_invalid);
                if (size == 2 && (value > 32767 || value < -32768))
                {
                    // out of the m16 range: only IE
                    status.float_exception_flags = float_flag_invalid;
                    value = -32768;
                }
                memory.i32 = (DI_UINT32)value;
            }
            if (DI_FPU_ProcessExceptionEx(pCallContext, status.float_exception_flags, 1))
                DI_PROC_END
        }
        DI_CHECK(Di_FpuWriteMemory(pDianaContext, pCallContext, memory.bytes, size));
        if (pop)
            Diana_FPU_Pop(pCallContext);
    }
    DI_PROC_END
}

// commands
int Diana_Call_wait(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    DI_FPU_START
    DI_PROC_END
}

int Diana_Call_fnstsw(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        DI_UINT16 statusWord = Diana_FPU_QueryStatusWord(pCallContext);
        if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            SET_REG_AX(statusWord);
        }
        else
        {
            DI_CHECK(Di_FpuWriteMemory(pDianaContext, pCallContext, &statusWord, sizeof(statusWord)));
        }
    }
    DI_PROC_END
}

int Diana_Call_fnstcw(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        DI_UINT16 controlWord = pCallContext->m_fpu.controlWord;
        DI_CHECK(Di_FpuWriteMemory(pDianaContext, pCallContext, &controlWord, sizeof(controlWord)));
    }
    DI_PROC_END
}

int Diana_Call_fnclex(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        // the exception flags, SF, ES and B
        pCallContext->m_fpu.statusWord &= 0x7F00;
    }
    DI_PROC_END
}

int Diana_Call_fldcw(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DiFpuMemory memory;
        int size = 0;
        DI_CHECK(Di_FpuReadMemory(pDianaContext, pCallContext, &memory, &size));
        // the reserved bits are 0, bit 6 is 1
        pCallContext->m_fpu.controlWord = (DI_UINT16)((memory.i16 & ~DI_FPU_CW_RESERVED_BITS) | DI_FPU_CW_RESERVED_40);
        // ES and B follow the unmasked exceptions
        if (pCallContext->m_fpu.statusWord & ~pCallContext->m_fpu.controlWord & DI_FPU_CW_ALL_EXCEPTIONS)
        {
            pCallContext->m_fpu.statusWord |= DI_FPU_SW_SUMMARY | DI_FPU_SW_BACKWARD;
        }
        else
        {
            pCallContext->m_fpu.statusWord &= ~(DI_FPU_SW_SUMMARY | DI_FPU_SW_BACKWARD);
        }
    }
    DI_PROC_END
}

int Diana_Call_fild(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        DiFpuMemory memory;
        int size = 0;
        DI_CHECK(Di_FpuReadMemory(pDianaContext, pCallContext, &memory, &size));
        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(-1))
        {
            floatx80_t result = Di_FpuIntegerToFloatx80(&memory, size);
            Diana_FPU_Push(pCallContext);
            Di_FpuWrite(pCallContext, &result, 0);
        }
        else
        {
            DI_FPU_Overflow(pCallContext);
        }
    }
    DI_PROC_END
}

int Diana_Call_fld(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        floatx80_t result = floatx80_default_nan;
        if (Di_FpuIsMemoryForm(pCallContext))
        {
            float_status_t status = FPU_pre_exception_handling(pCallContext);
            DiFpuMemory memory;
            int size = 0;
            DI_CHECK(Di_FpuReadMemory(pDianaContext, pCallContext, &memory, &size));
            DI_FPU_CLEAR_C1;
            if (!DI_FPU_REG_IS_EMPTY(-1))
            {
                DI_FPU_Overflow(pCallContext);
                DI_PROC_END
            }
            result = Di_FpuFloatToFloatx80(&memory, size, &status);
            // DE, IE of a SNaN; an unmasked IE loads nothing
            if (DI_FPU_ProcessException(pCallContext, status.float_exception_flags) & DI_FPU_EX_INVALID)
                DI_PROC_END
        }
        else
        {
            int dest = 0, src = 1;
            Di_FpuRegisters(pCallContext, &dest, &src);
            DI_FPU_CLEAR_C1;
            if (!DI_FPU_REG_IS_EMPTY(-1))
            {
                DI_FPU_Overflow(pCallContext);
                DI_PROC_END
            }
            if (DI_FPU_REG_IS_EMPTY(src))
            {
                DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
                if (!(pCallContext->m_fpu.controlWord & DI_FPU_CW_INVALID))
                    DI_PROC_END
            }
            else
            {
                result = Di_FpuRead(pCallContext, src);
            }
        }
        Diana_FPU_Push(pCallContext);
        Di_FpuWrite(pCallContext, &result, 0);
    }
    DI_PROC_END
}

// fst, fstp: st(i), m32, m64, m80 (fstp only)
static int Di_FpuStore(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int pop)
{
    DI_FPU_START
    {
        DI_FPU_CLEAR_C1;
        if (!Di_FpuIsMemoryForm(pCallContext))
        {
            int dest = 0, src = 1;
            floatx80_t st0;
            Di_FpuRegisters(pCallContext, &dest, &src);
            if (DI_FPU_REG_IS_EMPTY(0))
            {
                DI_FPU_StackUnderflow(pCallContext, src, pop);
                DI_PROC_END
            }
            st0 = Di_FpuRead(pCallContext, 0);
            Di_FpuWrite(pCallContext, &st0, src);
        }
        else
        {
            int size = pCallContext->m_result.linkedOperands[0].usedSize;
            DiFpuMemory memory;
            DIANA_MEMSET(&memory, 0, sizeof(memory));
            // the masked response of an empty st(0): the default NaN
            switch (size)
            {
            case 4: memory.f32 = float32_default_nan; break;
            case 8: memory.f64 = (float64)float64_default_nan; break;
            case 10: memory.f80 = floatx80_default_nan; break;
            default: return DI_ERROR;
            }
            if (DI_FPU_REG_IS_EMPTY(0))
            {
                DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
                if (!(pCallContext->m_fpu.controlWord & DI_FPU_CW_INVALID))
                    DI_PROC_END
            }
            else if (size == 10)
            {
                // m80: no conversion, no exception
                memory.f80 = Di_FpuRead(pCallContext, 0);
            }
            else
            {
                float_status_t status = FPU_pre_exception_handling(pCallContext);
                floatx80_t st0 = Di_FpuRead(pCallContext, 0);
                if (size == 4)
                    memory.f32 = floatx80_to_float32(st0, &status);
                else
                    memory.f64 = floatx80_to_float64(st0, &status);
                if (DI_FPU_ProcessExceptionEx(pCallContext, status.float_exception_flags, 1))
                    DI_PROC_END
            }
            DI_CHECK(Di_FpuWriteMemory(pDianaContext, pCallContext, memory.bytes, size));
        }
        if (pop)
            Diana_FPU_Pop(pCallContext);
    }
    DI_PROC_END
}

int Diana_Call_fstp(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    return Di_FpuStore(pDianaContext, pCallContext, 1);
}
int Diana_Call_fst(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    return Di_FpuStore(pDianaContext, pCallContext, 0);
}

int Diana_Call_fistp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Di_FpuStoreInteger(pDianaContext, pCallContext, 1, 0);
}
int Diana_Call_fist(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    return Di_FpuStoreInteger(pDianaContext, pCallContext, 0, 0);
}
int Diana_Call_fisttp(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    return Di_FpuStoreInteger(pDianaContext, pCallContext, 1, 1);
}

int Diana_Call_fadd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_ADD, 0);
}
int Diana_Call_faddp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_ADD, 1);
}
int Diana_Call_fsub(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_SUB, 0);
}
int Diana_Call_fsubp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_SUB, 1);
}
int Diana_Call_fsubr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_SUBR, 0);
}
int Diana_Call_fsubrp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_SUBR, 1);
}
int Diana_Call_fmul(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_MUL, 0);
}
int Diana_Call_fmulp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_MUL, 1);
}
int Diana_Call_fdiv(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_DIV, 0);
}
int Diana_Call_fdivp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_DIV, 1);
}
int Diana_Call_fdivr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_DIVR, 0);
}
int Diana_Call_fdivrp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuArithmetic(pDianaContext, pCallContext, DI_FPU_DIVR, 1);
}

int Diana_Call_fiadd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuIntegerArithmetic(pDianaContext, pCallContext, DI_FPU_ADD);
}
int Diana_Call_fisub(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuIntegerArithmetic(pDianaContext, pCallContext, DI_FPU_SUB);
}
int Diana_Call_fisubr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuIntegerArithmetic(pDianaContext, pCallContext, DI_FPU_SUBR);
}
int Diana_Call_fimul(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuIntegerArithmetic(pDianaContext, pCallContext, DI_FPU_MUL);
}
int Diana_Call_fidiv(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuIntegerArithmetic(pDianaContext, pCallContext, DI_FPU_DIV);
}
int Diana_Call_fidivr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuIntegerArithmetic(pDianaContext, pCallContext, DI_FPU_DIVR);
}

int Diana_Call_fcom(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuCompare(pDianaContext, pCallContext, 0, 0);
}
int Diana_Call_fcomp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuCompare(pDianaContext, pCallContext, 1, 0);
}
int Diana_Call_fcompp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuCompare(pDianaContext, pCallContext, 2, 0);
}
int Diana_Call_ficom(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuCompare(pDianaContext, pCallContext, 0, 1);
}
int Diana_Call_ficomp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_FpuCompare(pDianaContext, pCallContext, 1, 1);
}

int Diana_Call_fsqrt(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    DI_FPU_START
    {
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        floatx80_t result;
        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(0))
        {
            DI_FPU_StackUnderflow(pCallContext, 0, 0);
            DI_PROC_END
        }
        result = floatx80_sqrt(Di_FpuRead(pCallContext, 0), &status);
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            Di_FpuWrite(pCallContext, &result, 0);
    }
    DI_PROC_END
}

#else /* !DIANA_PROCESSOR_USE_SOFTFLOAT_FPU */

#define DIANA_FPU_STUB(name) \
int Diana_Call_##name(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext) \
{ pDianaContext; pCallContext; return DI_UNSUPPORTED_COMMAND; }

int Diana_Call_wait(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    DI_PROC_END
}
DIANA_FPU_STUB(fnstsw) DIANA_FPU_STUB(fnstcw) DIANA_FPU_STUB(fnclex) DIANA_FPU_STUB(fldcw)
DIANA_FPU_STUB(fild) DIANA_FPU_STUB(fld) DIANA_FPU_STUB(fst) DIANA_FPU_STUB(fstp)
DIANA_FPU_STUB(fist) DIANA_FPU_STUB(fistp) DIANA_FPU_STUB(fisttp)
DIANA_FPU_STUB(fadd) DIANA_FPU_STUB(faddp) DIANA_FPU_STUB(fsub) DIANA_FPU_STUB(fsubp)
DIANA_FPU_STUB(fsubr) DIANA_FPU_STUB(fsubrp) DIANA_FPU_STUB(fmul) DIANA_FPU_STUB(fmulp)
DIANA_FPU_STUB(fdiv) DIANA_FPU_STUB(fdivp) DIANA_FPU_STUB(fdivr) DIANA_FPU_STUB(fdivrp)
DIANA_FPU_STUB(fiadd) DIANA_FPU_STUB(fisub) DIANA_FPU_STUB(fisubr) DIANA_FPU_STUB(fimul)
DIANA_FPU_STUB(fidiv) DIANA_FPU_STUB(fidivr)
DIANA_FPU_STUB(fcom) DIANA_FPU_STUB(fcomp) DIANA_FPU_STUB(fcompp) DIANA_FPU_STUB(ficom) DIANA_FPU_STUB(ficomp)
DIANA_FPU_STUB(fsqrt)

#endif /* DIANA_PROCESSOR_USE_SOFTFLOAT_FPU */
