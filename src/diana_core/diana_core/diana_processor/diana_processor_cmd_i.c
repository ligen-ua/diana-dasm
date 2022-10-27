#include "diana_processor_cmd_i.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

static
int Diana_Call_idiv8(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext,
                     OPERAND_SIZE * pDivisor)
{
    //byte    AX           r/m8       AL          AH
    DI_INT16 src = (DI_INT16)GET_REG_AX;
    DI_INT16 quotient = 0;
    DI_SIGNED_CHAR remainder = 0,
                   divisor = (DI_SIGNED_CHAR)*pDivisor;

    pDianaContext;

    if (divisor == 0)
        return DI_DIVISION_BY_ZERO;

    quotient = src / divisor;
    remainder = src % divisor;

    if (quotient > 0xFF || quotient < -0xFF)
        return DI_DIVISION_BY_ZERO;

    SET_REG_AL(quotient);
    SET_REG_AH(remainder);
    return DI_SUCCESS;
}

static
int Diana_Call_idiv16(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext,
                      OPERAND_SIZE * pDivisor)
{
    //word    DX:AX        r/m16      AX          DX
    DianaRegisterValue32_signed_type src;
    DI_INT32 quotient = 0; 
    DI_INT16 remainder = 0,
               divisor = (DI_INT16)*pDivisor;

    pDianaContext;

    if (divisor == 0)
        return DI_DIVISION_BY_ZERO;

    src.impl.h       = (DI_INT16)GET_REG_DX;
    src.impl.l.value = (DI_INT16)GET_REG_AX;

    quotient = src.value / divisor;
    remainder = src.value % divisor;

    if (quotient > 0xFFFF || quotient < -0xFFFF)
        return DI_DIVISION_OVERFLOW;

    SET_REG_AX(quotient);
    SET_REG_DX(remainder);
    return DI_SUCCESS;
}

static
int Diana_Call_idiv32(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext,
                      OPERAND_SIZE * pDivisor)
{
    //dword   EDX:EAX      r/m32      EAX         EDX 
    DianaRegisterValue_signed_type src;
    DI_INT64  quotient = 0;
    DI_INT32  remainder = 0,
               divisor = (DI_INT32)*pDivisor;

    pDianaContext;

    if (divisor == 0)
        return DI_DIVISION_BY_ZERO;

    src.impl.h       = (DI_INT32)GET_REG_EDX;
    src.impl.l.value = (DI_INT32)GET_REG_EAX;


    quotient = src.value / divisor;
    remainder = (DI_INT32)(src.value % divisor);

    if (quotient > 0xFFFFFFFFLL || quotient < -0xFFFFFFFFLL)
        return DI_DIVISION_OVERFLOW;

    SET_REG_EAX(quotient);
    SET_REG_EDX(remainder);
    return DI_SUCCESS;
}

static
int Diana_Call_idiv64(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext,
                      OPERAND_SIZE * pDivisor)
{
    //qword   RDX:RAX      r/m64      RAX         RDX  
    DI_UINT64 r0 = GET_REG_RAX;
    DI_UINT64 r1 = GET_REG_RDX;
    DI_INT64  quotient = 0,
               remainder = 0,
               divisor = *pDivisor;

    pDianaContext;

    if (divisor == 0)
        return DI_DIVISION_BY_ZERO;

    {
        if( idiv64(&r0, &r1, divisor) )
         return DI_DIVISION_BY_ZERO;    
        quotient = r0;
    remainder = r1;
    }

    SET_REG_RAX(quotient);
    SET_REG_RDX(remainder);
    return DI_SUCCESS;
}

int Diana_Call_idiv(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //temp := dividend / divisor;
    //IF temp does not fit in quotient
    //THEN Interrupt 0;
    //ELSE
    //   quotient := temp;
    //   remainder := dividend MOD (r/m);
    //FI; 
    //Size    Dividend     Divisor   Quotient   Remainder
    //byte    AX           r/m8       AL          AH
    //word    DX:AX        r/m16      AX          DX
    //dword   EDX:EAX      r/m32      EAX         EDX 
    //qword   RDX:RAX      r/m64      RAX         RDX   

    DI_DEF_LOCAL(divisor);
    oldDestValue;

    DI_MEM_GET_DEST(divisor);


    switch(pCallContext->m_result.linkedOperands->usedSize)
    {
    case DIANA_MODE64:
        DI_CHECK(Diana_Call_idiv64(pDianaContext, pCallContext, &divisor));
        break;

    case DIANA_MODE32:
        DI_CHECK(Diana_Call_idiv32(pDianaContext, pCallContext, &divisor));
        break;

    case DIANA_MODE16:
        DI_CHECK(Diana_Call_idiv16(pDianaContext, pCallContext, &divisor));
        break;

    case 1:
        DI_CHECK(Diana_Call_idiv8(pDianaContext, pCallContext, &divisor));
        break;

    default:
        return DI_ERROR;
    }

    DI_PROC_END
}

int Diana_Call_int(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    pDianaContext;
    pCallContext;

    DI_PROC_END
}
