#include "diana_processor_cmd_fpu_sse.h"
#include "diana_processor_cmd_fpu_internal.h"

// commands
int Diana_Call_addsd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(dest)
        DI_DEF_LOCAL_XMM_FPU(src)

        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)

        {
            float_status_t status = FPU_pre_exception_handling(pCallContext);

            floatx80_t result = {0};
            floatx80_t a = float64_to_floatx80(dest.u64[0], &status);
            floatx80_t b = float64_to_floatx80(src.u64[0], &status);
 
            result = floatx80_add(a, b, &status);
 
            dest.u64[0] = floatx80_to_float64(result, &status);
        }

        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
#else
  return DI_UNSUPPORTED_COMMAND;
#endif
}

int Diana_Call_subsd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(dest)
        DI_DEF_LOCAL_XMM_FPU(src)

        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)

        {
            float_status_t status = FPU_pre_exception_handling(pCallContext);

            floatx80_t result = {0};
            floatx80_t a = float64_to_floatx80(dest.u64[0], &status);
            floatx80_t b = float64_to_floatx80(src.u64[0], &status);
 
            result = floatx80_sub(a, b, &status);
 
            dest.u64[0] = floatx80_to_float64(result, &status);
        }

        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
#else
  return DI_UNSUPPORTED_COMMAND;
#endif
}
int Diana_Call_divsd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(dest)
        DI_DEF_LOCAL_XMM_FPU(src)

        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)

        {
            float_status_t status = FPU_pre_exception_handling(pCallContext);

            floatx80_t result = {0};
            floatx80_t a = float64_to_floatx80(dest.u64[0], &status);
            floatx80_t b = float64_to_floatx80(src.u64[0], &status);
 
            result = floatx80_div(a, b, &status);
 
            if (!DI_FPU_ProcessException(pCallContext, status.float_exception_flags))
            {
                dest.u64[0] = floatx80_to_float64(result, &status);
                DI_MEM_SET_DEST_XMM(dest)
            }
        }
       
    }
    DI_PROC_END
#else
  return DI_UNSUPPORTED_COMMAND;
#endif
}

int Diana_Call_mulsd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(dest)
        DI_DEF_LOCAL_XMM_FPU(src)

        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)

        {
            float_status_t status = FPU_pre_exception_handling(pCallContext);

            floatx80_t result = {0};
            floatx80_t a = float64_to_floatx80(dest.u64[0], &status);
            floatx80_t b = float64_to_floatx80(src.u64[0], &status);
 
            result = floatx80_mul(a, b, &status);
 
            dest.u64[0] = floatx80_to_float64(result, &status);
        }

        DI_MEM_SET_DEST_XMM(dest)
    }
    DI_PROC_END
#else
  return DI_UNSUPPORTED_COMMAND;
#endif
}

int Diana_Call_cvttsd2si(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        DI_DEF_LOCAL(dest)
        DI_DEF_LOCAL_XMM_FPU(src)

        &oldDestValue; 
        DI_MEM_GET_SRC_XMM(src)
        {
            float_status_t status = FPU_pre_exception_handling(pCallContext);
            floatx80_t result = float64_to_floatx80(src.u64[0], &status);            
            dest = floatx80_to_int32(result, &status);
        }

        DI_MEM_SET_DEST(dest)
    }
    DI_PROC_END
#else
  return DI_UNSUPPORTED_COMMAND;
#endif
}

int Diana_Call_comisd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
#ifdef DIANA_PROCESSOR_USE_SOFTFLOAT_FPU
    DI_FPU_START
    {
        DI_DEF_LOCAL_XMM_FPU(dest)
        DI_DEF_LOCAL_XMM_FPU(src)

        DI_MEM_GET_SRC_XMM(src)
        DI_MEM_GET_DEST_XMM(dest)

        {
            float_status_t status = FPU_pre_exception_handling(pCallContext);
            int result = float64_compare(dest.u64[0], src.u64[0], &status);

            switch(result)
            {
            case float_relation_less:
                {
                    CLEAR_FLAG_ZF;
                    CLEAR_FLAG_PF;
                    SET_FLAG_CF;
                    break;
                }
            case float_relation_equal:
                {
                    SET_FLAG_ZF;
                    CLEAR_FLAG_PF;
                    CLEAR_FLAG_CF;
                    break;
                }
            case float_relation_greater:
                {
                    CLEAR_FLAG_ZF;
                    CLEAR_FLAG_PF;
                    CLEAR_FLAG_CF;
                    break;
                }
            case float_relation_unordered:
                {
                    SET_FLAG_ZF;
                    SET_FLAG_PF;
                    SET_FLAG_CF;
                    break;
                }
            default:
                {
                    CLEAR_FLAG_OF;
                    CLEAR_FLAG_AF;
                    CLEAR_FLAG_SF;
                }
            }
        }
    }
    DI_PROC_END
#else
  return DI_UNSUPPORTED_COMMAND;
#endif
}

int Diana_Call_xgetbv(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    pDianaContext;
    if (GET_REG_ECX == 0)
    {
        SET_REG_EAX(7);
        SET_REG_EDX(0);
    }
    DI_PROC_END
}

static const DI_UINT32 * GetTarget(const DianaRegisterXMM_type * pRegister, OPERAND_SIZE index)
{
    return (DI_UINT32 *)pRegister + (DI_CHAR)index;
}

int Diana_Call_pshufd(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext)
{
    //     DEST[31 - 0](SRC >> (ORDER[1 - 0] * 32))[31 - 0]
    //     DEST[63 - 32](SRC >> (ORDER[3 - 2] * 32))[31 - 0]
    //     DEST[95 - 64](SRC >> (ORDER[5 - 4] * 32))[31 - 0]
    //     DEST[127 - 96](SRC >> (ORDER[7 - 6] * 32))[31 - 0]
    
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)
    DI_DEF_LOCAL(order)

    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)
    DI_MEM_GET(order, 2)
    oldDestValue;

    dest.u32[0] = *GetTarget(&src, (order & 0x03));
    dest.u32[1] = *GetTarget(&src, (order & 0x0C) >> 2);
    dest.u32[2] = *GetTarget(&src, (order & 0x30) >> 4);
    dest.u32[3] = *GetTarget(&src, (order & 0xC0) >> 6);

    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}

// unpackers
int Diana_Call_punpcklbw(struct _dianaContext * pDianaContext,
                         DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest2;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)

    if (pCallContext->m_result.linkedOperands->usedSize == 0x10)
    {
        // xmm
        dest2.u8[0x0] = dest.u8[0];
        dest2.u8[0x1] = src.u8[0];
        dest2.u8[0x2] = dest.u8[1];
        dest2.u8[0x3] = src.u8[1];
        dest2.u8[0x4] = dest.u8[2];
        dest2.u8[0x5] = src.u8[2];
        dest2.u8[0x6] = dest.u8[3];
        dest2.u8[0x7] = src.u8[3];
        dest2.u8[0x8] = dest.u8[4];
        dest2.u8[0x9] = src.u8[4];
        dest2.u8[0xa] = dest.u8[5];
        dest2.u8[0xb] = src.u8[5];
        dest2.u8[0xc] = dest.u8[6];
        dest2.u8[0xd] = src.u8[6];
        dest2.u8[0xe] = dest.u8[7];
        dest2.u8[0xf] = src.u8[7];
    }
    else
    {
        // mmx
        dest2.u8[7] = src.u8[3];
        dest2.u8[6] = dest.u8[3];
        dest2.u8[5] = src.u8[2];
        dest2.u8[4] = dest.u8[2];
        dest2.u8[3] = src.u8[1];
        dest2.u8[2] = dest.u8[1];
        dest2.u8[1] = src.u8[0];
        dest2.u8[0] = dest.u8[0];
    }

    dest = dest2;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}

int Diana_Call_punpcklwd(struct _dianaContext * pDianaContext,
                         DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest2;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (pCallContext->m_result.linkedOperands->usedSize == 0x10)
    {
        // xmm
        dest2.u16[0x0] = dest.u16[0];
        dest2.u16[0x1] = src.u16[0];
        dest2.u16[0x2] = dest.u16[1];
        dest2.u16[0x3] = src.u16[1];
        dest2.u16[0x4] = dest.u16[2];
        dest2.u16[0x5] = src.u16[2];
        dest2.u16[0x6] = dest.u16[3];
        dest2.u16[0x7] = src.u16[3];
    }
    else
    {
        // mmx
        dest2.u16[3] = src.u16[1];
        dest2.u16[2] = dest.u16[1];
        dest2.u16[1] = src.u16[0];
        dest2.u16[0] = dest.u16[0];
    }

    dest = dest2;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_punpckldq(struct _dianaContext * pDianaContext,
                         DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest2;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    DI_MEM_GET_SRC_ANY(src)
    DI_MEM_GET_DEST_ANY(dest)
    if (pCallContext->m_result.linkedOperands->usedSize == 0x10)
    {
        // xmm
        dest2.u32[0x0] = dest.u32[0];
        dest2.u32[0x1] = src.u32[0];
        dest2.u32[0x2] = dest.u32[1];
        dest2.u32[0x3] = src.u32[1];
    }
    else
    {
        // mmx
        dest2.u32[1] = src.u32[0];
        dest2.u32[0] = dest.u32[0];
    }
    dest = dest2;
    DI_MEM_SET_DEST_ANY(dest)
    DI_PROC_END
}
int Diana_Call_punpcklqdq(struct _dianaContext * pDianaContext,
                         DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest2;
    DI_DEF_LOCAL_XMM(dest)
    DI_DEF_LOCAL_XMM(src)

    DI_MEM_GET_SRC_XMM(src)
    DI_MEM_GET_DEST_XMM(dest)

    dest2.u64[1] = src.u64[0];

    dest = dest2;
    DI_MEM_SET_DEST_XMM(dest)
    DI_PROC_END
}


int Diana_Call_stmxcsr(struct _dianaContext * pDianaContext,
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