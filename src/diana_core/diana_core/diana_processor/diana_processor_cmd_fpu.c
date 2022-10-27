#include "diana_processor_cmd_fpu.h"
#include "diana_processor_cmd_fpu_internal.h"



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
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
    
        DI_UINT16  statusWord = Diana_FPU_QueryStatusWord(pCallContext);
        OPERAND_SIZE doneBytes = 0;

        if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            SET_REG_AX(statusWord);
        }
        else
        {
            DI_CHECK(Diana_WriteRawBufferToArgMem(pDianaContext, 
                                             pCallContext, 
                                             0, 
                                             &statusWord, 
                                             sizeof(statusWord), 
                                             &doneBytes, 
                                             0));
        }
    }
    DI_PROC_END
#else
  return DI_UNSUPPORTED_COMMAND;
#endif

}

int Diana_Call_fnstcw(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        DI_UINT16  controlWord  = pCallContext->m_fpu.controlWord;
        OPERAND_SIZE doneBytes = 0;
        DI_CHECK(Diana_WriteRawBufferToArgMem(pDianaContext, 
                                             pCallContext, 
                                             0, 
                                             &controlWord, 
                                             sizeof(controlWord), 
                                             &doneBytes, 
                                             0));

    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif

}
int Diana_Call_fnclex(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START_IGNORE_EXCEPTIONS
    {
        pCallContext->m_fpu.statusWord &= 0x7F00;
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}
int Diana_Call_fldcw(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        DI_UINT16  controlWord  = 0;
        OPERAND_SIZE doneBytes = 0;
        DI_CHECK(Diana_ReadRawBufferFromArgMem(pDianaContext, 
                                             pCallContext, 
                                             0, 
                                             &controlWord, 
                                             sizeof(controlWord), 
                                             &doneBytes, 
                                             0));

        pCallContext->m_fpu.controlWord = (controlWord &  DI_FPU_CW_RESERVED_BITS) | DI_FPU_CW_RESERVED_40;

        
        if (pCallContext->m_fpu.statusWord & ~controlWord & DI_FPU_CW_ALL_EXCEPTIONS)
        {
            pCallContext->m_fpu.statusWord |= DI_FPU_SW_SUMMARY;
            pCallContext->m_fpu.statusWord |= DI_FPU_SW_BACKWARD;
        }
        else
        {
            pCallContext->m_fpu.statusWord &= ~(DI_FPU_SW_SUMMARY|DI_FPU_SW_BACKWARD);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}

int Diana_Call_fild(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t result;

        DI_DEF_LOCAL_1(src)
        DI_MEM_GET_DEST(src)


        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(-1)) 
        {
            result = int64_to_floatx80(src);
            Diana_FPU_Push(pCallContext);

            DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                reg_fpu_ST0, 
                                                &result);


        }
        else 
        {
            DI_FPU_Overflow(pCallContext);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}

// read float argument
int Diana_Call_fld(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t result;
        if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            // st(i) version
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands->value.recognizedRegister, 
                                                &result);
        }
        else
        {
            DI_CHECK(Diana_FPU_ReadFloatArgument_80(pDianaContext, pCallContext, 0, &result));
        }
       
        DI_FPU_CLEAR_C1;
        if (DI_FPU_REG_IS_EMPTY(-1)) 
        {
            Diana_FPU_Push(pCallContext);
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                reg_fpu_ST0, 
                                                &result);

        }
        else 
        {
            DI_FPU_Overflow(pCallContext);
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}

// fst
static
int Diana_Call_fstp_common(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext,
                           int pop)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
               
        floatx80_t result = floatx80_default_nan;
        if (DI_FPU_REG_IS_EMPTY(0)) 
        {
            DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
        }
        else
        {
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
        }

        DI_FPU_CLEAR_C1;
        if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            // st version
            DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands->value.recognizedRegister, 
                                                &result);
        }
        else
        {
            // mem version
            DI_CHECK(Diana_FPU_WriteFloatArgument(pDianaContext, 
                                                  pCallContext, 
                                                  0, 
                                                  &result));
        }
        if (pop)
        {
            Diana_FPU_Pop(pCallContext);
        }
    }
    DI_PROC_END;
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}
int Diana_Call_fstp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    return Diana_Call_fstp_common(pDianaContext,  pCallContext, 1);
}
int Diana_Call_fst(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    return Diana_Call_fstp_common(pDianaContext,  pCallContext, 0);
}
// done fst

// fist
static
int Diana_Call_fistp_common(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext,
                           int pop)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        float_status_t status;
        floatx80_t result = floatx80_default_nan;
        DI_DEF_LOCAL_1(dest)
        DI_MEM_GET_DEST(dest)
        
        if (DI_FPU_REG_IS_EMPTY(0)) 
        {
            DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
        }
        else
        {
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &result);
        }

        dest = floatx80_to_int64(result, &status);
        DI_MEM_SET_DEST(dest)

        if (pop)
        {
            Diana_FPU_Pop(pCallContext);
        }
    }
    DI_PROC_END;
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}
int Diana_Call_fistp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    return Diana_Call_fistp_common(pDianaContext,  pCallContext, 1);
}
int Diana_Call_fist(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    return Diana_Call_fistp_common(pDianaContext,  pCallContext, 0);
}
// done fst

// sub
//FSUB m32fp | D8 /4
//FSUB m64fp | DC /4
//FSUB ST(0), ST(i) | D8 E0 + fpu_i
//FSUB ST(i), ST(0) | DC E8 + fpu_i
//FSUBP ST(i), ST(0) | DE E8 + fpu_i

//FSUBR m32fp | D8 /5
//FSUBR m64fp | DC /5
//FSUBR ST(0), ST(i) | D8 E8 + fpu_i
//FSUBR ST(i), ST(0) | DC E0 + fpu_i
//FSUBRP ST(i), ST(0) | DE E0 + fpu_i

//FISUB m32int | DA /4
//FISUB m16int | DE /4
//FISUBR m32int | DA /5
//FISUBR m16int | DE /5
static
int Diana_Call_fsub_common(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext,
                           int pop,
                           int reverse,
                           int add_cmd)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t a, b;
        int bStstVersion = 0;
        if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            // st(i) version
            bStstVersion = 1;
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands[0].value.recognizedRegister, 
                                                &a);
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands[1].value.recognizedRegister, 
                                                &b);

        }
        else
        {
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                reg_fpu_ST0, 
                                                &a);
            DI_CHECK(Diana_FPU_ReadFloatArgument_80(pDianaContext, pCallContext, 0, &b));
        }
       
        DI_FPU_CLEAR_C1;
  
        {
            floatx80_t result = {0};
            float_status_t status = FPU_pre_exception_handling(pCallContext);
            if (reverse)
            {
                if (add_cmd)
                {
                    result = floatx80_add(b, a, &status);
                }
                else
                {
                    result = floatx80_sub(b, a, &status);
                }
            }
            else
            {
                if (add_cmd)
                {
                    result = floatx80_add(a, b, &status);
                }
                else
                {
                    result = floatx80_sub(a, b, &status);
                }
            }
           
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                if (bStstVersion)
                {
                    DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                        pCallContext->m_result.linkedOperands->value.recognizedRegister, 
                                                        &result);
                }
                else
                {
                    DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                reg_fpu_ST0, 
                                                &result);

                }
                if (pop)
                {
                    Diana_FPU_Pop(pCallContext);
                }
            }
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}
int Diana_Call_fsub(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fsub_common(pDianaContext, pCallContext, 0, 0, 0);
}
int Diana_Call_fsubp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fsub_common(pDianaContext, pCallContext, 1, 0, 0);
}

int Diana_Call_fsubr(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fsub_common(pDianaContext, pCallContext, 0, 1, 0);
}
int Diana_Call_fsubrp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fsub_common(pDianaContext, pCallContext, 1, 1, 0);
}

static
int Diana_Call_common_fisub(struct _dianaContext * pDianaContext,
                            DianaProcessor * pCallContext,
                            int reverse,
                            int add_cmd)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t st0 = floatx80_default_nan;
        DI_DEF_LOCAL_1(src)
        DI_MEM_GET_DEST(src)
        
        if (DI_FPU_REG_IS_EMPTY(0)) 
        {
            DI_FPU_ProcessException(pCallContext, DI_FPU_EX_STACK_UNDERFLOW);
        }
        else
        {
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, reg_fpu_ST0, &st0);
        }

        {
            floatx80_t arg = {0}, result = {0};
            float_status_t status = FPU_pre_exception_handling(pCallContext);

            arg = int64_to_floatx80(src);
            if (reverse)
            {
                if (add_cmd)
                {
                    result = floatx80_add(arg, st0, &status);
                }
                else
                {
                    result = floatx80_sub(arg, st0, &status);
                }
            }
            else
            {
                if (add_cmd)
                {
                    result = floatx80_add(st0, arg, &status);
                }
                else
                {
                    result = floatx80_sub(st0, arg, &status);
                }
            }
           
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                    reg_fpu_ST0, 
                                                    &result);

            }
        }
    }
    DI_PROC_END;
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}

int Diana_Call_fisub(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_common_fisub(pDianaContext, pCallContext, 0, 0);
}
int Diana_Call_fisubr(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_common_fisub(pDianaContext, pCallContext, 1, 0);
}
// done sub
// mul
// FMUL m32fp | D8 /1
// FMUL m64fp | DC /1
// FMUL ST(0), ST(i) | D8 C8 + fpu_i
// FMUL ST(i), ST(0) | DC C8 + fpu_i
// FMULP ST(i), ST(0) | DE C8 + fpu_i

static
int Diana_Call_fmul_common(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext,
                           int pop)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t a, b;
        int bStstVersion = 0;

        if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            // st(i) version
            bStstVersion = 1;
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands[0].value.recognizedRegister, 
                                                &a);
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands[1].value.recognizedRegister, 
                                                &b);

        }
        else
        {
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                reg_fpu_ST0, 
                                                &a);
            DI_CHECK(Diana_FPU_ReadFloatArgument_80(pDianaContext, pCallContext, 0, &b));
        }
       
        DI_FPU_CLEAR_C1;
  
        {
            floatx80_t result = {0};
            float_status_t status = FPU_pre_exception_handling(pCallContext);

            result = floatx80_mul(a, b, &status);
           
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                if (bStstVersion)
                {
                    DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                        pCallContext->m_result.linkedOperands->value.recognizedRegister, 
                                                        &result);
                }
                else
                {
                    DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                         reg_fpu_ST0, 
                                                         &result);

                }
                if (pop)
                {
                    Diana_FPU_Pop(pCallContext);
                }
            }
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}
int Diana_Call_fmulp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fmul_common(pDianaContext, pCallContext, 1);
}

int Diana_Call_fmul(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    return Diana_Call_fmul_common(pDianaContext, pCallContext, 0);
}
// done mul
// fadd
int Diana_Call_fadd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fsub_common(pDianaContext, pCallContext, 0, 0, 1);
}
int Diana_Call_faddp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fsub_common(pDianaContext, pCallContext, 1, 0, 1);
}
int Diana_Call_fiadd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_common_fisub(pDianaContext, pCallContext, 0, 1);
}
// fcom

static
int Diana_Call_common_fcom(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext,
                           int popCount)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t a, b;
        int bStstVersion = 0;

        if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            // st(i) version
            bStstVersion = 1;
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands[0].value.recognizedRegister, 
                                                &a);
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands[1].value.recognizedRegister, 
                                                &b);

        }
        else
        {
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                reg_fpu_ST0, 
                                                &a);
            DI_CHECK(Diana_FPU_ReadFloatArgument_80(pDianaContext, pCallContext, 0, &b));
        }

        {
            float64 a_16, b_16;
            int result = 0;
            float_status_t status = FPU_pre_exception_handling(pCallContext);

            a_16 = floatx80_to_float64(a, &status);
            b_16 = floatx80_to_float64(b, &status);

            result = float64_compare(a_16, b_16, &status);

            switch(result)
            {
            case float_relation_less:
                {
                    DI_FPU_CLEAR_C3
                    DI_FPU_CLEAR_C2
                    DI_FPU_SET_C0
                    break;
                }
            case float_relation_equal:
                {
                    DI_FPU_SET_C3
                    DI_FPU_CLEAR_C2
                    DI_FPU_CLEAR_C0
                    break;
                }
            case float_relation_greater:
                {
                    DI_FPU_CLEAR_C3
                    DI_FPU_CLEAR_C2
                    DI_FPU_CLEAR_C0
                    break;
                }
            case float_relation_unordered:
                {
                    break;
                }
            }
           
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                {
                    int i = 0;
                    for(i = 0; i<popCount; ++i)
                    {
                        Diana_FPU_Pop(pCallContext);
                    }
                }
            }
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}

int Diana_Call_fcom(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    return Diana_Call_common_fcom(pDianaContext, pCallContext, 0);
}
int Diana_Call_fcomp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    return Diana_Call_common_fcom(pDianaContext, pCallContext, 1);
}
int Diana_Call_fcompp(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    return Diana_Call_common_fcom(pDianaContext, pCallContext, 2);
}
// done fcomp
int Diana_Call_fsqrt(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t argument;

        DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                reg_fpu_ST0, 
                                                &argument);
       
  
        {
            floatx80_t result = {0};
            float_status_t status = FPU_pre_exception_handling(pCallContext);

            result = floatx80_sqrt(argument, &status);
           
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                    DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                         reg_fpu_ST0, 
                                                         &result);
            }
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif

}

// fdiv
static
int Diana_Call_fdiv_common(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext,
                           int pop)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        floatx80_t a, b;
        int bStstVersion = 0;

        if (pCallContext->m_result.linkedOperands->type == diana_register)
        {
            // st(i) version
            bStstVersion = 1;
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands[0].value.recognizedRegister, 
                                                &a);
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                pCallContext->m_result.linkedOperands[1].value.recognizedRegister, 
                                                &b);

        }
        else
        {
            DianaProcessor_FPU_GetSTRegister_80(pCallContext, 
                                                reg_fpu_ST0, 
                                                &a);
            DI_CHECK(Diana_FPU_ReadFloatArgument_80(pDianaContext, pCallContext, 0, &b));
        }
       
        DI_FPU_CLEAR_C1;
  
        {
            floatx80_t result = {0};
            float_status_t status = FPU_pre_exception_handling(pCallContext);

            result = floatx80_div(a, b, &status);
           
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                if (bStstVersion)
                {
                    DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                        pCallContext->m_result.linkedOperands->value.recognizedRegister, 
                                                        &result);
                }
                else
                {
                    DianaProcessor_FPU_SetSTRegister_80(pCallContext, 
                                                         reg_fpu_ST0, 
                                                         &result);

                }
                if (pop)
                {
                    Diana_FPU_Pop(pCallContext);
                }
            }
        }
    }
    DI_PROC_END
#else
    return DI_UNSUPPORTED_COMMAND;
#endif
}

int Diana_Call_fdiv(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fdiv_common(pDianaContext, pCallContext, 0);
}

int Diana_Call_fdivp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fdiv_common(pDianaContext, pCallContext, 1);
}

int Diana_Call_fidiv(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fdiv_common(pDianaContext, pCallContext, 0);
}

int Diana_Call_fdivr(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    return Diana_Call_fdiv_common(pDianaContext, pCallContext, 0);
}

int Diana_Call_fdivrp(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    return Diana_Call_fdiv_common(pDianaContext, pCallContext, 1);
}

int Diana_Call_fidivr(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    return Diana_Call_fdiv_common(pDianaContext, pCallContext, 0);
}

