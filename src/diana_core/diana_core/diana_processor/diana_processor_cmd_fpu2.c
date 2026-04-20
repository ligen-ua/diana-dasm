#include "diana_processor_cmd_fpu2.h"
#include "diana_processor_cmd_fpu_internal.h"

/* 80-bit extended-precision constants */
static const floatx80_t c_fld1   = { (DI_UINT64)0x8000000000000000ULL, 0x3FFF }; /* 1.0 */
static const floatx80_t c_fldl2t = { (DI_UINT64)0xD49A784BCD1B8AFEULL, 0x4000 }; /* log2(10) */
static const floatx80_t c_fldl2e = { (DI_UINT64)0xB8AA3B295C17F0BBULL, 0x3FFF }; /* log2(e)  */
static const floatx80_t c_fldpi  = { (DI_UINT64)0xC90FDAA22168C235ULL, 0x4000 }; /* pi       */
static const floatx80_t c_fldlg2 = { (DI_UINT64)0x9A209A84FBCFF799ULL, 0x3FFD }; /* log10(2) */
static const floatx80_t c_fldln2 = { (DI_UINT64)0xB17217F7D1CF79ACULL, 0x3FFE }; /* ln(2)    */

/*---------------------------------------------------------------------------*/
/* Helper: push constant onto FPU stack                                      */
/*---------------------------------------------------------------------------*/
static int Diana_FPU_PushConst(DianaProcessor * pCallContext,
                               const floatx80_t * pConst)
{
    DI_FPU_CLEAR_C1;
    if (DI_FPU_REG_IS_EMPTY(-1))
    {
        Diana_FPU_Push(pCallContext);
        DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, pConst);
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
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        result = floatx80_compare(st0, Const_Z, &status);
        switch (result)
        {
        case float_relation_less:
            DI_FPU_CLEAR_C3; DI_FPU_CLEAR_C2; DI_FPU_SET_C0;
            break;
        case float_relation_equal:
            DI_FPU_SET_C3; DI_FPU_CLEAR_C2; DI_FPU_CLEAR_C0;
            break;
        case float_relation_greater:
            DI_FPU_CLEAR_C3; DI_FPU_CLEAR_C2; DI_FPU_CLEAR_C0;
            break;
        default: /* unordered */
            DI_FPU_SET_C3; DI_FPU_SET_C2; DI_FPU_SET_C0;
            break;
        }
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

        if (pCallContext->m_result.linkedOperands->type == diana_register)
            bReg = pCallContext->m_result.linkedOperands->value.recognizedRegister;

        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &a);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, bReg, &b);

        result = floatx80_compare_quiet(a, b, &status);
        switch (result)
        {
        case float_relation_less:
            DI_FPU_CLEAR_C3; DI_FPU_CLEAR_C2; DI_FPU_SET_C0;
            break;
        case float_relation_equal:
            DI_FPU_SET_C3; DI_FPU_CLEAR_C2; DI_FPU_CLEAR_C0;
            break;
        case float_relation_greater:
            DI_FPU_CLEAR_C3; DI_FPU_CLEAR_C2; DI_FPU_CLEAR_C0;
            break;
        default: /* unordered */
            DI_FPU_SET_C3; DI_FPU_SET_C2; DI_FPU_SET_C0;
            break;
        }

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
/* ficom / ficomp - compare ST(0) with integer operand                      */
/*---------------------------------------------------------------------------*/
static int Diana_Call_ficom_common(struct _dianaContext * pDianaContext,
                                   DianaProcessor * pCallContext,
                                   int bPop)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, arg;
        float_status_t status;
        int result;
        
        DI_DEF_LOCAL_1(src)
        DI_MEM_GET_DEST(src)
        status = FPU_pre_exception_handling(pCallContext);
        
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        arg = int64_to_floatx80((DI_INT64)src);

        result = floatx80_compare(st0, arg, &status);
        switch (result)
        {
        case float_relation_less:
            DI_FPU_CLEAR_C3; DI_FPU_CLEAR_C2; DI_FPU_SET_C0;
            break;
        case float_relation_equal:
            DI_FPU_SET_C3; DI_FPU_CLEAR_C2; DI_FPU_CLEAR_C0;
            break;
        case float_relation_greater:
            DI_FPU_CLEAR_C3; DI_FPU_CLEAR_C2; DI_FPU_CLEAR_C0;
            break;
        default:
            DI_FPU_SET_C3; DI_FPU_SET_C2; DI_FPU_SET_C0;
            break;
        }

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
}
int Diana_Call_ficom(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_ficom_common(pDianaContext, pCallContext, 0); }
int Diana_Call_ficomp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{ return Diana_Call_ficom_common(pDianaContext, pCallContext, 1); }

/*---------------------------------------------------------------------------*/
/* Load constants                                                            */
/*---------------------------------------------------------------------------*/
int Diana_Call_fld1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    return Diana_FPU_PushConst(pCallContext, &c_fld1);
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
    return Diana_FPU_PushConst(pCallContext, &Const_Z);
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
    return Diana_FPU_PushConst(pCallContext, &c_fldl2t);
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
    return Diana_FPU_PushConst(pCallContext, &c_fldl2e);
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
    return Diana_FPU_PushConst(pCallContext, &c_fldpi);
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
    return Diana_FPU_PushConst(pCallContext, &c_fldlg2);
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
    return Diana_FPU_PushConst(pCallContext, &c_fldln2);
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
/* fimul m16/32int                                                           */
/*---------------------------------------------------------------------------*/
int Diana_Call_fimul(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0, arg, result;
        float_status_t status;
        DI_DEF_LOCAL_1(src)
        DI_MEM_GET_DEST(src)
        status = FPU_pre_exception_handling(pCallContext);

        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        arg = int64_to_floatx80((DI_INT64)src);
        result = floatx80_mul(st0, arg, &status);
        DI_FPU_CLEAR_C1;
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
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
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        /* floatx80_extract modifies st0 to be the mantissa, returns exponent */
        exp_val = floatx80_extract(&st0, &status);
        DI_FPU_CLEAR_C1;
        if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
        {
            /* Replace ST(0) with mantissa, push exponent */
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
            if (DI_FPU_REG_IS_EMPTY(-1))
            {
                Diana_FPU_Push(pCallContext);
                /* After push, old ST(0)=mantissa is now ST(1), new ST(0)=exponent */
                DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &exp_val);
            }
            else
            {
                DI_FPU_Overflow(pCallContext);
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
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST1, &st1);
        if (bIEEE)
            rc = floatx80_ieee754_remainder(st0, st1, &result, &quotient, &status);
        else
            rc = floatx80_remainder(st0, st1, &result, &quotient, &status);
        DI_FPU_CLEAR_C1;
        if (rc)
        {
            /* partial reduction: set C2, ST(0) updated with partial result */
            DI_FPU_SET_C2;
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
        }
        else
        {
            DI_FPU_CLEAR_C2;
            /* set C0,C3,C1 = Q2,Q1,Q0 */
            if (quotient & 4) { DI_FPU_SET_C0; } else { DI_FPU_CLEAR_C0; }
            if (quotient & 2) { DI_FPU_SET_C3; } else { DI_FPU_CLEAR_C3; }
            if (quotient & 1) { DI_FPU_SET_C1; } else { DI_FPU_CLEAR_C1; }
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
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
/* fisttp - store integer with truncation and pop                            */
/*---------------------------------------------------------------------------*/
int Diana_Call_fisttp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0 = floatx80_default_nan;
        float_status_t status;
        DI_DEF_LOCAL_1(dest)
        DI_MEM_GET_DEST(dest)

        if (DI_FPU_REG_IS_EMPTY(0))
        {
            DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
        }
        else
        {
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        }
        status = FPU_pre_exception_handling(pCallContext);
        dest = (OPERAND_SIZE)(DI_INT64)floatx80_to_int64_round_to_zero(st0, &status);
        DI_MEM_SET_DEST(dest)
        Diana_FPU_Pop(pCallContext);
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
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
int Diana_Call_fxsave(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}
int Diana_Call_fxrstor(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}
