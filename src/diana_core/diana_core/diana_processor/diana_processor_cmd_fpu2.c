#include "diana_processor_cmd_fpu2.h"
#include "diana_processor_cmd_fpu_internal.h"

/* 80-bit extended-precision constants */
static const floatx80_t c_fld1   = { (DI_UINT64)0x8000000000000000ULL, 0x3FFF }; /* 1.0 */
static const floatx80_t c_fldl2t = { (DI_UINT64)0xD49A784BCD1B8AFEULL, 0x4000 }; /* log2(10) */
static const floatx80_t c_fldl2e = { (DI_UINT64)0xB8AA3B295C17F0BCULL, 0x3FFF }; /* log2(e)  */
static const floatx80_t c_fldpi  = { (DI_UINT64)0xC90FDAA22168C235ULL, 0x4000 }; /* pi       */
static const floatx80_t c_fldlg2 = { (DI_UINT64)0x9A209A84FBCFF799ULL, 0x3FFD }; /* log10(2) */
static const floatx80_t c_fldln2 = { (DI_UINT64)0xB17217F7D1CF79ACULL, 0x3FFE }; /* ln(2)    */

/*---------------------------------------------------------------------------*/
/* Helper: push constant onto FPU stack                                      */
/* The constants are the values rounded to nearest; the CPU rounds its        */
/* internal 66-bit constants by RC: down and chop give one less for l2e, pi,  */
/* lg2, ln2 (rounded up to nearest), up one more for l2t (rounded down).      */
/*---------------------------------------------------------------------------*/
#define DI_FPU_CONST_ROUND_NONE   0
#define DI_FPU_CONST_ROUND_DOWN   1
#define DI_FPU_CONST_ROUND_UP     2
static int Diana_FPU_PushConst(DianaProcessor * pCallContext,
                               const floatx80_t * pConst,
                               int rounding)
{
    floatx80_t value = *pConst;
    int rc = (pCallContext->m_fpu.controlWord & DI_FPU_CW_ROUNDING_CONTROL) >> 10;
    if (rounding == DI_FPU_CONST_ROUND_DOWN && (rc == float_round_down || rc == float_round_to_zero))
        --value.fraction;
    if (rounding == DI_FPU_CONST_ROUND_UP && rc == float_round_up)
        ++value.fraction;
    DI_FPU_CLEAR_C1;
    if (DI_FPU_REG_IS_EMPTY(-1))
    {
        Diana_FPU_Push(pCallContext);
        DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &value);
        return DI_SUCCESS;
    }
    DI_FPU_Overflow(pCallContext);
    return DI_SUCCESS;
}

/*---------------------------------------------------------------------------*/
/* Helper: set EFLAGS ZF/PF/CF from float compare result (fcomi pattern)    */
/*---------------------------------------------------------------------------*/
static void Diana_FPU_SetEFlagsFromCompare(DianaProcessor * pCallContext,
                                           int result)
{
    CLEAR_FLAG_OF;
    CLEAR_FLAG_SF;
    CLEAR_FLAG_AF;
    switch (result)
    {
    case float_relation_less:
        CLEAR_FLAG_ZF; CLEAR_FLAG_PF; SET_FLAG_CF;
        break;
    case float_relation_equal:
        SET_FLAG_ZF;   CLEAR_FLAG_PF; CLEAR_FLAG_CF;
        break;
    case float_relation_greater:
        CLEAR_FLAG_ZF; CLEAR_FLAG_PF; CLEAR_FLAG_CF;
        break;
    default: /* unordered */
        SET_FLAG_ZF;   SET_FLAG_PF;   SET_FLAG_CF;
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* fxch ST(i)                                                                */
/*---------------------------------------------------------------------------*/
int Diana_Call_fxch(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, sti;
        DianaUnifiedRegister sti_reg = reg_fpu_ST1;

        if (pCallContext->m_result.linkedOperands->type == diana_register)
            sti_reg = pCallContext->m_result.linkedOperands->value.recognizedRegister;

        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, sti_reg, &sti);
        DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &sti);
        DianaProcessor_FPU_SetSTRegister_80(pCallContext, sti_reg, &st0);
        DI_FPU_CLEAR_C1;
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fabs                                                                      */
/*---------------------------------------------------------------------------*/
int Diana_Call_fabs(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0;
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        floatx80_abs(&st0);
        DI_FPU_CLEAR_C1;
        DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fchs                                                                      */
/*---------------------------------------------------------------------------*/
int Diana_Call_fchs(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0;
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        floatx80_chs(&st0);
        DI_FPU_CLEAR_C1;
        DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* ftst - compare ST(0) with +0.0                                            */
/*---------------------------------------------------------------------------*/
int Diana_Call_ftst(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int result;
        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(0))
        {
            DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
            DI_FPU_SetCompareFlags(pCallContext, float_relation_unordered);
            DI_PROC_END
        }
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        result = floatx80_compare(st0, Const_Z, &status);
        DI_FPU_SetCompareFlags(pCallContext, result);
        DI_FPU_ProcessException(pCallContext, status.float_exception_flags);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fxam - examine ST(0)                                                      */
/*---------------------------------------------------------------------------*/
int Diana_Call_fxam(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        floatx80_t st0;
        int sign;
        float_class_t cls;

        if (DI_FPU_REG_IS_EMPTY(0))
        {
            /* empty: C3=1, C2=0, C0=1 */
            DI_FPU_SET_C3; DI_FPU_CLEAR_C2; DI_FPU_SET_C0;
            /* C1 = sign of the "empty" register content */
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
            if (st0.exp & 0x8000) { DI_FPU_SET_C1; } else { DI_FPU_CLEAR_C1; }
        }
        else
        {
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
            sign = (st0.exp >> 15) & 1;
            if (sign) { DI_FPU_SET_C1; } else { DI_FPU_CLEAR_C1; }

            cls = floatx80_class(st0);
            switch (cls)
            {
            case float_zero:
                DI_FPU_SET_C3; DI_FPU_CLEAR_C2; DI_FPU_CLEAR_C0;
                break;
            case float_SNaN:
            case float_QNaN:
                DI_FPU_CLEAR_C3; DI_FPU_CLEAR_C2; DI_FPU_SET_C0;
                break;
            case float_negative_inf:
            case float_positive_inf:
                DI_FPU_CLEAR_C3; DI_FPU_SET_C2; DI_FPU_SET_C0;
                break;
            case float_denormal:
                DI_FPU_SET_C3; DI_FPU_SET_C2; DI_FPU_CLEAR_C0;
                break;
            default: /* float_normalized */
                DI_FPU_CLEAR_C3; DI_FPU_SET_C2; DI_FPU_CLEAR_C0;
                break;
            }
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fucom / fucomp / fucompp  (unordered compare, quiet - no invalid on QNaN) */
/*---------------------------------------------------------------------------*/
static int Diana_Call_fucom_common(struct _dianaContext * pDianaContext,
                                   DianaProcessor * pCallContext,
                                   int popCount)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t a, b;
        int result;
        DianaUnifiedRegister bReg = reg_fpu_ST1;
        float_status_t status = FPU_pre_exception_handling(pCallContext);

        if (pCallContext->m_result.iLinkedOpCount >= 1 &&
            pCallContext->m_result.linkedOperands->type == diana_register)
            bReg = pCallContext->m_result.linkedOperands->value.recognizedRegister;

        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(0) || DI_FPU_REG_IS_EMPTY(bReg - reg_fpu_ST0))
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
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &a);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, bReg, &b);

        result = floatx80_compare_quiet(a, b, &status);
        DI_FPU_SetCompareFlags(pCallContext, result);

        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
        {
            int i = 0;
            for (i = 0; i < popCount; ++i)
                Diana_FPU_Pop(pCallContext);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fucom(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fucom_common(pDianaContext, pCallContext, 0); }
int Diana_Call_fucomp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fucom_common(pDianaContext, pCallContext, 1); }
int Diana_Call_fucompp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fucom_common(pDianaContext, pCallContext, 2); }

/*---------------------------------------------------------------------------*/
/* fcomi / fucomi - compare and set EFLAGS                                   */
/*---------------------------------------------------------------------------*/
static int Diana_Call_fcomi_common(struct _dianaContext * pDianaContext,
                                   DianaProcessor * pCallContext,
                                   int bQuiet,
                                   int bPop)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t a, b;
        int result;
        DianaUnifiedRegister bReg = reg_fpu_ST1;
        float_status_t status = FPU_pre_exception_handling(pCallContext);

        if (pCallContext->m_result.linkedOperands->type == diana_register &&
            pCallContext->m_result.iLinkedOpCount > 1)
        {
            bReg = pCallContext->m_result.linkedOperands[1].value.recognizedRegister;
        }
        else if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            bReg = pCallContext->m_result.linkedOperands[0].value.recognizedRegister;
        }

        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(0) || DI_FPU_REG_IS_EMPTY(bReg - reg_fpu_ST0))
        {
            DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
            if (pCallContext->m_fpu.controlWord & DI_FPU_CW_INVALID)
            {
                Diana_FPU_SetEFlagsFromCompare(pCallContext, float_relation_unordered);
                if (bPop)
                    Diana_FPU_Pop(pCallContext);
            }
            DI_PROC_END
        }
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &a);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, bReg, &b);

        if (bQuiet)
            result = floatx80_compare_quiet(a, b, &status);
        else
            result = floatx80_compare(a, b, &status);

        Diana_FPU_SetEFlagsFromCompare(pCallContext, result);

        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
        {
            if (bPop)
                Diana_FPU_Pop(pCallContext);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fcomi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcomi_common(pDianaContext, pCallContext, 0, 0); }
int Diana_Call_fcomip(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcomi_common(pDianaContext, pCallContext, 0, 1); }
int Diana_Call_fucomi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcomi_common(pDianaContext, pCallContext, 1, 0); }
int Diana_Call_fucomip(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcomi_common(pDianaContext, pCallContext, 1, 1); }

/*---------------------------------------------------------------------------*/
/* Load constants                                                            */
/*---------------------------------------------------------------------------*/
int Diana_Call_fld1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    return Diana_FPU_PushConst(pCallContext, &c_fld1, DI_FPU_CONST_ROUND_NONE);
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fldz(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    return Diana_FPU_PushConst(pCallContext, &Const_Z, DI_FPU_CONST_ROUND_NONE);
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fldl2t(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    return Diana_FPU_PushConst(pCallContext, &c_fldl2t, DI_FPU_CONST_ROUND_UP);
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fldl2e(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    return Diana_FPU_PushConst(pCallContext, &c_fldl2e, DI_FPU_CONST_ROUND_DOWN);
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fldpi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    return Diana_FPU_PushConst(pCallContext, &c_fldpi, DI_FPU_CONST_ROUND_DOWN);
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fldlg2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    return Diana_FPU_PushConst(pCallContext, &c_fldlg2, DI_FPU_CONST_ROUND_DOWN);
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fldln2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    return Diana_FPU_PushConst(pCallContext, &c_fldln2, DI_FPU_CONST_ROUND_DOWN);
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fnop                                                                      */
/*---------------------------------------------------------------------------*/
int Diana_Call_fnop(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_FPU_START
    DI_PROC_END
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fdecstp / fincstp                                                         */
/*---------------------------------------------------------------------------*/
int Diana_Call_fdecstp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        int top = Diana_FPU_GetStackTop(pCallContext);
        DI_FPU_CLEAR_C1;
        Diana_FPU_SetStackTop(pCallContext, (top + 7) & 7);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fincstp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        int top = Diana_FPU_GetStackTop(pCallContext);
        DI_FPU_CLEAR_C1;
        Diana_FPU_SetStackTop(pCallContext, (top + 1) & 7);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* ffree / ffreep                                                             */
/*---------------------------------------------------------------------------*/
int Diana_Call_ffree(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        DianaUnifiedRegister reg = reg_fpu_ST0;
        if (pCallContext->m_result.linkedOperands->type == diana_register)
            reg = pCallContext->m_result.linkedOperands->value.recognizedRegister;
        Diana_FPU_MarkRegState(pCallContext,
                               reg - reg_fpu_ST0,
                               1 /*isEmpty*/);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_ffreep(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        DianaUnifiedRegister reg = reg_fpu_ST0;
        if (pCallContext->m_result.linkedOperands->type == diana_register)
            reg = pCallContext->m_result.linkedOperands->value.recognizedRegister;
        Diana_FPU_MarkRegState(pCallContext, reg - reg_fpu_ST0, 1);
        Diana_FPU_Pop(pCallContext);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fninit - initialise FPU                                                   */
/*---------------------------------------------------------------------------*/
int Diana_Call_fninit(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        int i;
        pCallContext->m_fpu.controlWord = 0x037F;
        pCallContext->m_fpu.statusWord  = 0x0000;
        Diana_FPU_SetStackTop(pCallContext, 0);
        for (i = 0; i < 8; ++i)
            Diana_FPU_MarkRegState(pCallContext, i, 1);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* frndint                                                                   */
/*---------------------------------------------------------------------------*/
int Diana_Call_frndint(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, result;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        result = floatx80_round_to_int(st0, &status);
        DI_FPU_CLEAR_C1;
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fscale  ST(0) *= 2^trunc(ST(1))                                          */
/*---------------------------------------------------------------------------*/
int Diana_Call_fscale(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, st1, result;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST1, &st1);
        result = floatx80_scale(st0, st1, &status);
        DI_FPU_CLEAR_C1;
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fxtract - decompose ST(0) into exponent (ST(1)) and mantissa (ST(0))     */
/*---------------------------------------------------------------------------*/
int Diana_Call_fxtract(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, exp_val;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(0))
        {
            DI_FPU_StackUnderflow(pCallContext, 0, 0);
            DI_PROC_END
        }
        if (!DI_FPU_REG_IS_EMPTY(-1))
        {
            DI_FPU_Overflow(pCallContext);
            DI_PROC_END
        }
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        /* floatx80_extract makes st0 the significand and returns the exponent */
        exp_val = floatx80_extract(&st0, &status);
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
        {
            /* the exponent in ST(1), the significand in ST(0) */
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &exp_val);
            Diana_FPU_Push(pCallContext);
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* f2xm1  ST(0) = 2^ST(0) - 1                                               */
/*---------------------------------------------------------------------------*/
int Diana_Call_f2xm1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, result;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        result = f2xm1(st0, &status);
        DI_FPU_CLEAR_C1;
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fyl2x  ST(1) = ST(1) * log2(ST(0)), pop                                  */
/*---------------------------------------------------------------------------*/
int Diana_Call_fyl2x(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, st1, result;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST1, &st1);
        result = fyl2x(st0, st1, &status);
        DI_FPU_CLEAR_C1;
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
        {
            Diana_FPU_Pop(pCallContext);
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fyl2xp1  ST(1) = ST(1) * log2(ST(0)+1), pop                              */
/*---------------------------------------------------------------------------*/
int Diana_Call_fyl2xp1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, st1, result;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST1, &st1);
        result = fyl2xp1(st0, st1, &status);
        DI_FPU_CLEAR_C1;
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
        {
            Diana_FPU_Pop(pCallContext);
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fpatan  ST(1) = arctan(ST(1)/ST(0)), pop                                  */
/*---------------------------------------------------------------------------*/
int Diana_Call_fpatan(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, st1, result;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST1, &st1);
        result = fpatan(st0, st1, &status);
        DI_FPU_CLEAR_C1;
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
        {
            Diana_FPU_Pop(pCallContext);
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fptan  ST(0) = tan(ST(0)); push 1.0                                       */
/*---------------------------------------------------------------------------*/
int Diana_Call_fptan(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int rc;
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        rc = ftan(&st0, &status);
        DI_FPU_CLEAR_C1;
        if (rc)
        {
            /* out of range: set C2, do not update ST(0) */
            DI_FPU_SET_C2;
        }
        else
        {
            DI_FPU_CLEAR_C2;
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
                /* push 1.0 */
                if (DI_FPU_REG_IS_EMPTY(-1))
                {
                    Diana_FPU_Push(pCallContext);
                    DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &c_fld1);
                }
                else
                {
                    DI_FPU_Overflow(pCallContext);
                }
            }
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* fprem / fprem1                                                             */
/*---------------------------------------------------------------------------*/
static int Diana_Call_fprem_common(struct _dianaContext * pDianaContext,
                                   DianaProcessor * pCallContext,
                                   int bIEEE)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, st1, result;
        DI_UINT64 quotient = 0;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int rc;
        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(0) || DI_FPU_REG_IS_EMPTY(1))
        {
            DI_FPU_StackUnderflow(pCallContext, 0, 0);
            DI_PROC_END
        }
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST1, &st1);
        if (bIEEE)
            rc = floatx80_ieee754_remainder(st0, st1, &result, &quotient, &status);
        else
            rc = floatx80_remainder(st0, st1, &result, &quotient, &status);
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
        {
            if (rc >= 0)
            {
                /* partial: C2; complete: C0, C3, C1 = Q2, Q1, Q0 */
                DI_UINT16 cc = 0;
                if (rc)
                {
                    cc = DI_FPU_SW_C2;
                }
                else
                {
                    if (quotient & 1) cc |= DI_FPU_SW_C1;
                    if (quotient & 2) cc |= DI_FPU_SW_C3;
                    if (quotient & 4) cc |= DI_FPU_SW_C0;
                }
                DI_FPU_SET_CC(cc)
            }
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fprem(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fprem_common(pDianaContext, pCallContext, 0); }
int Diana_Call_fprem1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fprem_common(pDianaContext, pCallContext, 1); }

/*---------------------------------------------------------------------------*/
/* fsin / fcos / fsincos                                                     */
/*---------------------------------------------------------------------------*/
int Diana_Call_fsin(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int rc;
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        rc = fsin(&st0, &status);
        DI_FPU_CLEAR_C1;
        if (rc) { DI_FPU_SET_C2; }
        else
        {
            DI_FPU_CLEAR_C2;
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
                DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fcos(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int rc;
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        rc = fcos(&st0, &status);
        DI_FPU_CLEAR_C1;
        if (rc) { DI_FPU_SET_C2; }
        else
        {
            DI_FPU_CLEAR_C2;
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
                DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fsincos(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0_orig, sin_val, cos_val;
        float_status_t status = FPU_pre_exception_handling(pCallContext);
        int rc;
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0_orig);
        rc = fsincos(st0_orig, &sin_val, &cos_val, &status);
        DI_FPU_CLEAR_C1;
        if (rc) { DI_FPU_SET_C2; }
        else
        {
            DI_FPU_CLEAR_C2;
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                /* ST(0) = sin, push cos */
                DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &sin_val);
                if (DI_FPU_REG_IS_EMPTY(-1))
                {
                    Diana_FPU_Push(pCallContext);
                    DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &cos_val);
                }
                else
                {
                    DI_FPU_Overflow(pCallContext);
                }
            }
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}

/*---------------------------------------------------------------------------*/
/* Conditional float move: fcmovb/e/be/u/nb/ne/nbe/nu                       */
/*---------------------------------------------------------------------------*/
static int Diana_Call_fcmov_common(struct _dianaContext * pDianaContext,
                                   DianaProcessor * pCallContext,
                                   int condition)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        if (condition)
        {
            floatx80_t src;
            DianaUnifiedRegister srcReg = reg_fpu_ST1;
            if (pCallContext->m_result.linkedOperands->type == diana_register &&
                pCallContext->m_result.iLinkedOpCount > 1)
                srcReg = pCallContext->m_result.linkedOperands[1].value.recognizedRegister;
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, srcReg, &src);
            DI_FPU_CLEAR_C1;
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &src);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
    pDianaContext;
}
int Diana_Call_fcmovb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcmov_common(pDianaContext, pCallContext, GET_FLAG_CF != 0); }
int Diana_Call_fcmove(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcmov_common(pDianaContext, pCallContext, GET_FLAG_ZF != 0); }
int Diana_Call_fcmovbe(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcmov_common(pDianaContext, pCallContext, GET_FLAG_CF != 0 || GET_FLAG_ZF != 0); }
int Diana_Call_fcmovu(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcmov_common(pDianaContext, pCallContext, GET_FLAG_PF != 0); }
int Diana_Call_fcmovnb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcmov_common(pDianaContext, pCallContext, GET_FLAG_CF == 0); }
int Diana_Call_fcmovne(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcmov_common(pDianaContext, pCallContext, GET_FLAG_ZF == 0); }
int Diana_Call_fcmovnbe(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcmov_common(pDianaContext, pCallContext, GET_FLAG_CF == 0 && GET_FLAG_ZF == 0); }
int Diana_Call_fcmovnu(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_fcmov_common(pDianaContext, pCallContext, GET_FLAG_PF == 0); }

/*---------------------------------------------------------------------------*/
/* Complex-state stubs: just succeed without doing anything                  */
/*---------------------------------------------------------------------------*/
int Diana_Call_fbld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    return DI_UNSUPPORTED_COMMAND;
}
int Diana_Call_fbstp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    return DI_UNSUPPORTED_COMMAND;
}
int Diana_Call_fldenv(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}
int Diana_Call_fnstenv(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}
int Diana_Call_frstor(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}
int Diana_Call_fnsave(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}

/*---------------------------------------------------------------------------*/
/* fxsave, fxrstor, the xsave family. The state: x87 (FCW, FSW, the abridged tags, ST(i); FIP, FDP, FOP are not   */
/* kept: 0), MXCSR, xmm0..15 (0..7 outside 64-bit mode), bits 255:128 of ymm. XCR0 is 7 (xgetbv), XINUSE is 1 for  */
/* every component (xsave, xsavec write all of RFBM). xsaveopt is xsave (its optimizations are optional).          */
/*---------------------------------------------------------------------------*/
#define DI_XCR0                 7
#define DI_XSAVE_SIZE           832     // the legacy area 512, the header 64, AVX 256 (CPUID 0xD.2: offset 576)
#define DI_XSAVE_HEADER         512
#define DI_XSAVE_AVX            576
#define DI_XCOMP_BV_COMPACTED   0x8000000000000000ULL

static int Di_XsaveRegisters(DianaProcessor * pCallContext)
{
    return pCallContext->m_context.iAMD64Mode ? 16 : 8;
}

// the address of the memory operand, #GP when it isn't a multiple of alignment
static int Di_XsaveCheckAddress(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int alignment)
{
    OPERAND_SIZE selector = 0, address = 0;
    DianaUnifiedRegister segReg = reg_DS;
    DI_CHECK(Diana_QueryAddress(pDianaContext, pCallContext, 0, &selector, &address, &segReg));
    if (address % alignment)
        return DI_GP;
    return DI_SUCCESS;
}

static void Di_XsavePut16(unsigned char * p, DI_UINT32 value)
{
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}
static void Di_XsavePut32(unsigned char * p, DI_UINT32 value)
{
    Di_XsavePut16(p, value);
    Di_XsavePut16(p + 2, value >> 16);
}
static void Di_XsavePut64(unsigned char * p, DI_UINT64 value)
{
    Di_XsavePut32(p, (DI_UINT32)value);
    Di_XsavePut32(p + 4, (DI_UINT32)(value >> 32));
}
static DI_UINT32 Di_XsaveGet16(const unsigned char * p)
{
    return p[0] | (DI_UINT32)p[1] << 8;
}
static DI_UINT32 Di_XsaveGet32(const unsigned char * p)
{
    return Di_XsaveGet16(p) | Di_XsaveGet16(p + 2) << 16;
}
static DI_UINT64 Di_XsaveGet64(const unsigned char * p)
{
    return Di_XsaveGet32(p) | (DI_UINT64)Di_XsaveGet32(p + 4) << 32;
}

// the legacy area: x87 (bytes 0..159 but MXCSR), MXCSR and MXCSR_MASK (24, 28), xmm (160 + 16 i)
static void Di_XsaveX87(DianaProcessor * pCallContext, unsigned char * image)
{
    int i;
    unsigned char tags = 0;
    for (i = 0; i < 8; ++i)
        if (pCallContext->m_fpu.registerFlags[i] & DI_PROCESSOR_FPU_REGISTER_BUSY)
            tags |= (unsigned char)(1 << i);
    Di_XsavePut16(image, pCallContext->m_fpu.controlWord);
    Di_XsavePut16(image + 2, pCallContext->m_fpu.statusWord);
    image[4] = tags;
    image[5] = 0;
    DIANA_MEMSET(image + 6, 0, 18);     // FOP, FIP, FDP
    for (i = 0; i < 8; ++i)
    {
        floatx80_t value;
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, (DianaUnifiedRegister)(reg_fpu_ST0 + i), &value);
        Di_XsavePut64(image + 32 + 16 * i, value.fraction);
        Di_XsavePut16(image + 40 + 16 * i, value.exp);
        DIANA_MEMSET(image + 42 + 16 * i, 0, 6);
    }
}
static void Di_XsaveMxcsr(DianaProcessor * pCallContext, unsigned char * image)
{
    Di_XsavePut32(image + 24, pCallContext->m_mxcsr);
    Di_XsavePut32(image + 28, 0xffff);     // MXCSR_MASK: DAZ supported
}
static void Di_XsaveXmm(DianaProcessor * pCallContext, unsigned char * image)
{
    int i;
    for (i = 0; i < Di_XsaveRegisters(pCallContext); ++i)
    {
        DianaRegisterXMM_type value = DianaProcessor_XMM_GetValue(pCallContext,
                                          DianaProcessor_QueryReg(pCallContext, (DianaUnifiedRegister)(reg_XMM0 + i)));
        DIANA_MEMCPY(image + 160 + 16 * i, &value, 16);
    }
}
static void Di_XsaveYmmHigh(DianaProcessor * pCallContext, unsigned char * pArea)
{
    int i;
    for (i = 0; i < Di_XsaveRegisters(pCallContext); ++i)
    {
        DianaRegisterYMM_type value = DianaProcessor_YMM_GetValue(pCallContext, (DianaUnifiedRegister)(reg_YMM0 + i));
        DIANA_MEMCPY(pArea + 16 * i, &value.xmm[1], 16);
    }
}

// image == 0: the init state of the component
static void Di_XrstorX87(DianaProcessor * pCallContext, const unsigned char * image)
{
    int i;
    pCallContext->m_fpu.controlWord = (DI_UINT16)(image ? Di_XsaveGet16(image) : 0x037f);
    pCallContext->m_fpu.statusWord = (DI_UINT16)(image ? Di_XsaveGet16(image + 2) : 0);
    for (i = 0; i < 8; ++i)
    {
        floatx80_t value;
        value.fraction = image ? Di_XsaveGet64(image + 32 + 16 * i) : 0;
        value.exp = (DI_UINT16)(image ? Di_XsaveGet16(image + 40 + 16 * i) : 0);
        DianaProcessor_FPU_SetSTRegister_80(pCallContext, (DianaUnifiedRegister)(reg_fpu_ST0 + i), &value);
    }
    for (i = 0; i < 8; ++i)
        pCallContext->m_fpu.registerFlags[i] = (DI_CHAR)(image && (image[4] >> i) & 1 ? DI_PROCESSOR_FPU_REGISTER_BUSY : 0);
}
static void Di_XrstorXmm(DianaProcessor * pCallContext, const unsigned char * image)
{
    int i;
    for (i = 0; i < Di_XsaveRegisters(pCallContext); ++i)
    {
        DianaRegisterXMM_type value;
        DianaUnifiedRegister reg = (DianaUnifiedRegister)(reg_XMM0 + i);
        if (image)
            DIANA_MEMCPY(&value, image + 160 + 16 * i, 16);
        else
            DIANA_MEMSET(&value, 0, 16);
        DianaProcessor_XMM_SetValue(pCallContext, reg, DianaProcessor_QueryReg(pCallContext, reg), &value);
    }
}
static void Di_XrstorYmmHigh(DianaProcessor * pCallContext, const unsigned char * pArea)
{
    int i;
    for (i = 0; i < Di_XsaveRegisters(pCallContext); ++i)
    {
        DianaUnifiedRegister reg = (DianaUnifiedRegister)(reg_YMM0 + i);
        DianaRegisterYMM_type value = DianaProcessor_YMM_GetValue(pCallContext, reg);
        if (pArea)
            DIANA_MEMCPY(&value.xmm[1], pArea + 16 * i, 16);
        else
            DIANA_MEMSET(&value.xmm[1], 0, 16);
        DianaProcessor_YMM_SetValue(pCallContext, reg, &value);
    }
}
static int Di_XrstorMxcsr(DianaProcessor * pCallContext, const unsigned char * image)
{
    DI_UINT32 mxcsr = image ? Di_XsaveGet32(image + 24) : DI_MXCSR_DEFAULT;
    if (mxcsr & DI_MXCSR_RESERVED)
        return DI_GP;
    pCallContext->m_mxcsr = mxcsr;
    return DI_SUCCESS;
}

int Diana_Call_fxsave(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    unsigned char image[512];
    OPERAND_SIZE doneBytes = 0;
    DI_CHECK(Di_XsaveCheckAddress(pDianaContext, pCallContext, 16));
    // bytes 416..511 are not written: read them back
    DI_CHECK(Diana_ReadRawBufferFromArgMem(pDianaContext, pCallContext, 0, image, sizeof(image), &doneBytes, 0));
    Di_XsaveX87(pCallContext, image);
    Di_XsaveMxcsr(pCallContext, image);
    Di_XsaveXmm(pCallContext, image);
    DI_CHECK(Diana_WriteRawBufferToArgMem(pDianaContext, pCallContext, 0, image, sizeof(image), &doneBytes, 0));
    DI_PROC_END
}

int Diana_Call_fxrstor(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    unsigned char image[512];
    OPERAND_SIZE doneBytes = 0;
    DI_CHECK(Di_XsaveCheckAddress(pDianaContext, pCallContext, 16));
    DI_CHECK(Diana_ReadRawBufferFromArgMem(pDianaContext, pCallContext, 0, image, sizeof(image), &doneBytes, 0));
    if (Di_XsaveGet32(image + 24) & DI_MXCSR_RESERVED)
        return DI_GP;
    Di_XrstorX87(pCallContext, image);
    DI_CHECK(Di_XrstorMxcsr(pCallContext, image));
    Di_XrstorXmm(pCallContext, image);
    DI_PROC_END
}

// xsave, xsaveopt (compacted 0), xsavec (compacted 1): RFBM = edx:eax & XCR0
static int Di_Xsave(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int compacted)
{
    unsigned char image[DI_XSAVE_SIZE];
    OPERAND_SIZE doneBytes = 0;
    DI_UINT64 rfbm = ((DI_UINT64)(DI_UINT32)GET_REG_EDX << 32 | (DI_UINT32)GET_REG_EAX) & DI_XCR0;
    DI_CHECK(Di_XsaveCheckAddress(pDianaContext, pCallContext, 64));
    DI_CHECK(Diana_ReadRawBufferFromArgMem(pDianaContext, pCallContext, 0, image, sizeof(image), &doneBytes, 0));
    if (rfbm & 1)
        Di_XsaveX87(pCallContext, image);
    // MXCSR: with SSE or AVX, xsavec with SSE only (the CPU)
    if (rfbm & (compacted ? 2 : 6))
        Di_XsaveMxcsr(pCallContext, image);
    if (rfbm & 2)
        Di_XsaveXmm(pCallContext, image);
    if (rfbm & 4)
        Di_XsaveYmmHigh(pCallContext, image + DI_XSAVE_AVX);    // the compacted offset too: the first component
    if (compacted)
    {
        // XSTATE_BV = RFBM & XINUSE, XCOMP_BV = RFBM | bit 63; bytes 63..16 of the header are kept (the CPU)
        Di_XsavePut64(image + DI_XSAVE_HEADER, rfbm);
        Di_XsavePut64(image + DI_XSAVE_HEADER + 8, rfbm | DI_XCOMP_BV_COMPACTED);
    }
    else
    {
        // XSTATE_BV[i] = XINUSE[i] for the bits of RFBM, the others kept
        Di_XsavePut64(image + DI_XSAVE_HEADER, Di_XsaveGet64(image + DI_XSAVE_HEADER) | rfbm);
    }
    DI_CHECK(Diana_WriteRawBufferToArgMem(pDianaContext, pCallContext, 0, image, sizeof(image), &doneBytes, 0));
    DI_PROC_END
}

int Diana_Call_xsave(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Xsave(pDianaContext, pCallContext, 0);
}
int Diana_Call_xsaveopt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Xsave(pDianaContext, pCallContext, 0);
}
int Diana_Call_xsavec(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Xsave(pDianaContext, pCallContext, 1);
}

// xrstor, the standard and the compacted form (bit 63 of XCOMP_BV): a component of RFBM is loaded when its bit of
// XSTATE_BV is 1, else it gets its init state
int Diana_Call_xrstor(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    unsigned char image[DI_XSAVE_SIZE];
    OPERAND_SIZE doneBytes = 0;
    DI_UINT64 rfbm = ((DI_UINT64)(DI_UINT32)GET_REG_EDX << 32 | (DI_UINT32)GET_REG_EAX) & DI_XCR0;
    DI_UINT64 xstateBv, xcompBv;
    int i, compacted;
    DI_CHECK(Di_XsaveCheckAddress(pDianaContext, pCallContext, 64));
    DI_CHECK(Diana_ReadRawBufferFromArgMem(pDianaContext, pCallContext, 0, image, sizeof(image), &doneBytes, 0));
    xstateBv = Di_XsaveGet64(image + DI_XSAVE_HEADER);
    xcompBv = Di_XsaveGet64(image + DI_XSAVE_HEADER + 8);
    compacted = (xcompBv & DI_XCOMP_BV_COMPACTED) != 0;
    // #GP: XSTATE_BV not in XCR0; standard: bytes 23..8 of the header not 0; compacted: XCOMP_BV not in XCR0,
    // XSTATE_BV not in XCOMP_BV, bytes 63..16 not 0
    if (xstateBv & ~(DI_UINT64)DI_XCR0)
        return DI_GP;
    if (compacted && ((xcompBv & ~DI_XCOMP_BV_COMPACTED & ~(DI_UINT64)DI_XCR0) || (xstateBv & ~xcompBv)))
        return DI_GP;
    for (i = compacted ? 16 : 8; i < (compacted ? 64 : 24); ++i)
        if (image[DI_XSAVE_HEADER + i])
            return DI_GP;
    // MXCSR: the standard form loads it for SSE or AVX, the compacted form with the SSE state (init: 0x1f80)
    if (compacted ? (rfbm & 2) : (rfbm & 6))
    {
        if (Di_XsaveGet32(image + 24) & DI_MXCSR_RESERVED && (!compacted || (xstateBv & 2)))
            return DI_GP;
        DI_CHECK(Di_XrstorMxcsr(pCallContext, !compacted || (xstateBv & 2) ? image : 0));
    }
    if (rfbm & 1)
        Di_XrstorX87(pCallContext, xstateBv & 1 ? image : 0);
    if (rfbm & 2)
        Di_XrstorXmm(pCallContext, xstateBv & 2 ? image : 0);
    if (rfbm & 4)
        Di_XrstorYmmHigh(pCallContext, xstateBv & 4 ? image + DI_XSAVE_AVX : 0);
    DI_PROC_END
}
