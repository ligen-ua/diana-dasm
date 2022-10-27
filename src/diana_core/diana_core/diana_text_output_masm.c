#include "diana_commands.h"
#include "diana_core.h"
#include "diana_text_output_masm.h"

#define DIANA_TEXT_OUTPUT_FLAG_OPERAND_PRINTED  1
static const char * g_registers[] = {
 "none",
 "AL",    "CL",    "DL",    "BL",    "AH",    "CH",    "DH",    "BH",
 "AX",    "CX",    "DX",    "BX",    "SP",    "BP",    "SI",    "DI",
 "EAX",   "ECX",   "EDX",   "EBX",   "ESP",   "EBP",   "ESI",   "EDI",
 "ES",    "CS",    "SS",    "DS",    "FS",    "GS",
 "CR0",   "CR1",   "CR2",   "CR3",   "CR4",   "CR5",   "CR6",   "CR7",
 "DR0",   "DR1",   "DR2",   "DR3",   "DR4",   "DR5",   "DR6",   "DR7",
 "TR0",   "TR1",   "TR2",   "TR3",   "TR4",   "TR5",   "TR6",   "TR7",
 "IP",
 // x64 part
 "RAX",   "RCX",   "RDX",   "RBX",   "RSP",   "RBP",   "RSI",   "RDI",
 "SIL",   "DIL",   "BPL",   "SPL",
 "R8",    "R9",    "R10",   "R11",   "R12",   "R13",   "R14",   "R15",
 "R8D",   "R9D",   "R10D",  "R11D",  "R12D",  "R13D",  "R14D",  "R15D",
 "R8W",   "R9W",   "R10W",  "R11W",  "R12W",  "R13W",  "R14W",  "R15W",
 "R8B",   "R9B",   "R10B",  "R11B",  "R12B",  "R13B",  "R14B",  "R15B",
 "RIP",
 "MM0",   "MM1",   "MM2",   "MM3",   "MM4",   "MM5",   "MM6",   "MM7",
 "MM8",   "MM9",   "MM10",  "MM11",  "MM12",  "MM13",  "MM14",  "MM15",
 "XMM0",  "XMM1",  "XMM2",  "XMM3",  "XMM4",  "XMM5",  "XMM6",  "XMM7",
 "XMM8",  "XMM9",  "XMM10", "XMM11", "XMM12", "XMM13", "XMM14", "XMM15",
 "fpu_ST0", "fpu_ST1", "fpu_ST2", "fpu_ST3", "fpu_ST4", "fpu_ST5", "fpu_ST6", "fpu_ST7",
 "CR8",   "CR9",   "CR10",  "CR11",  "CR12",  "CR13",  "CR14",  "CR15",
 "DR8",   "DR9",   "DR10",  "DR11",  "DR12",  "DR13",  "DR14",  "DR15"};

static const char * GetRegisterName(DianaUnifiedRegister reg)
{
    if ((unsigned int)reg >= sizeof(g_registers)/sizeof(g_registers[0]))
        return "unknown";
    return g_registers[(unsigned int)reg];
}

typedef int (* DianaTextOutputCmd_fnc)(DianaTextOutputContext * pContext);

static int ConvertDispToSigned(const DianaRmIndex * pRmIndex, 
                               DI_UINT64 * pUsedValue,
                               int * pWasConvert)
{
    *pWasConvert = 0;
    switch(pRmIndex->dispSize)
    {
    case 1:
        *pUsedValue = *(unsigned char*)&pRmIndex->dispValue;
        if (*pUsedValue & 0x80)
        {
            *pWasConvert = 1;
            *pUsedValue = 0xFFUL - (DI_UINT64)*pUsedValue + 1;
        }
        break;
    case 2:
        *pUsedValue = *(unsigned short*)&pRmIndex->dispValue;
        if (*pUsedValue & 0x8000)
        {
            *pWasConvert = 1;
            *pUsedValue = 0xFFFFUL - (DI_UINT64)*pUsedValue + 1;
        }
        break;
    case 4:
        *pUsedValue = *(unsigned int*)&pRmIndex->dispValue;
        if (*pUsedValue & 0x80000000)
        {
            *pWasConvert = 1;
            *pUsedValue = 0xFFFFFFFFUL - (DI_UINT64)*pUsedValue + 1;
        }
        break;

    case 8:
        *pUsedValue = *(DI_UINT64*)&pRmIndex->dispValue;
        if (*pUsedValue & 0x8000000000000000ULL)
        {
            *pWasConvert = 1;
            *pUsedValue = 0xFFFFFFFFFFFFFFFFULL - (DI_UINT64)*pUsedValue + 1;
        }
        break;
    default:
        return DI_ERROR;
    };
    return DI_SUCCESS;
}

static int DianaTextOutput(DianaTextOutputContext * pContext, 
                           const char * pText)
{
    return (*pContext->pTextOutputFnc)(pContext, pText);
}
static int DianaOpOutput(DianaTextOutputContext * pContext, 
                         OPERAND_SIZE operand, 
                         int size)
{
    DI_CHECK((*pContext->pOpOutputFnc)(pContext, operand, size));
    if (size == 1 && operand < 10)
    {
        return DI_SUCCESS;
    }
    return DianaTextOutput(pContext, "h");
}
static int PrintRegister(DianaTextOutputContext * pContext, DianaUnifiedRegister recognizedRegister)
{
    return DianaTextOutput(pContext, GetRegisterName(recognizedRegister));
}
static int PrintIndex(DianaTextOutputContext * pContext, DianaLinkedOperand * pLinkedOp)
{
    int bWasSomething = 0;

    switch(pLinkedOp->usedSize)
    {
    default:
        break;
    case 1:
        DI_CHECK(DianaTextOutput(pContext, "byte ptr "));
        break;
    case 2:
        DI_CHECK(DianaTextOutput(pContext, "word ptr "));
        break;
    case 4:
        DI_CHECK(DianaTextOutput(pContext, "dword ptr "));
        break;
    case 8:
        DI_CHECK(DianaTextOutput(pContext, "qword ptr "));
        break;
    };
    DI_CHECK(PrintRegister(pContext, pLinkedOp->value.rmIndex.seg_reg));
    DI_CHECK(DianaTextOutput(pContext, ":["));
    
    if (pLinkedOp->value.rmIndex.reg != reg_none)
    {
        bWasSomething = 1;
        DI_CHECK(PrintRegister(pContext, pLinkedOp->value.rmIndex.reg));
    }
    if (pLinkedOp->value.rmIndex.indexed_reg != reg_none && pLinkedOp->value.rmIndex.index)
    {
        if (bWasSomething)
        {
            DI_CHECK(DianaTextOutput(pContext, "+"));
        }
        bWasSomething = 1;
        DI_CHECK(PrintRegister(pContext, pLinkedOp->value.rmIndex.indexed_reg));
        DI_CHECK(DianaTextOutput(pContext, "*"));
        DI_CHECK(DianaOpOutput(pContext, pLinkedOp->value.rmIndex.index, 1));
    }
    if (pLinkedOp->value.rmIndex.dispSize)
    {
        int positive = 1;
        int wasConvert = 0;
        DI_UINT64 usedValue = pLinkedOp->value.rmIndex.dispValue;
        DI_CHAR usedSize = pLinkedOp->value.rmIndex.dispSize;
        if (pLinkedOp->pInfo->m_type != diana_orOffset && pLinkedOp->pInfo->m_type != diana_orPtr)
        {
            DI_CHECK(ConvertDispToSigned(&pLinkedOp->value.rmIndex, &usedValue, &wasConvert));
            if (wasConvert)
            {
                positive = 0;
                usedSize = 0;
            }
        }
        if (positive)
        {
            if (bWasSomething)
            {
                DI_CHECK(DianaTextOutput(pContext, "+"));
            }
        }
        else
        {
            DI_CHECK(DianaTextOutput(pContext, "-"));
        }
        bWasSomething = 1;
        DI_CHECK(DianaOpOutput(pContext, usedValue, usedSize));
    }
    DI_CHECK(DianaTextOutput(pContext, "]"));
    return DI_SUCCESS;
}
static int PrintImmediate(DianaTextOutputContext * pContext, DianaLinkedOperand * pLinkedOp)
{
    return DianaOpOutput(pContext, pLinkedOp->value.imm, 0);
}
static int PrintCallPtr(DianaTextOutputContext * pContext, DianaLinkedOperand * pLinkedOp)
{
    DI_CHECK(DianaOpOutput(pContext, pLinkedOp->value.ptr.m_segment, pLinkedOp->value.ptr.m_segment_size));
    DI_CHECK(DianaTextOutput(pContext, ":"));
    DI_CHECK(DianaOpOutput(pContext, pLinkedOp->value.ptr.m_address, pLinkedOp->value.ptr.m_address_size));
    return DI_SUCCESS;
}
static int PrintRelative(DianaTextOutputContext * pContext, DianaLinkedOperand * pLinkedOp)
{
    OPERAND_SIZE res = pLinkedOp->value.rel.m_value + pContext->instructionRIP + pContext->pResult->iFullCmdSize;
    DI_CHECK(DianaOpOutput(pContext, res, pLinkedOp->usedAddressSize));
    return DI_SUCCESS;
}
static int PrintMemory(DianaTextOutputContext * pContext, DianaLinkedOperand * pLinkedOp)
{
    return PrintIndex(pContext, pLinkedOp);
}
static int PrintOpSeparator(DianaTextOutputContext * pContext)
{
    if (pContext->flags & DIANA_TEXT_OUTPUT_FLAG_OPERAND_PRINTED)
    {
        DI_CHECK(DianaTextOutput(pContext, ", "));
    }
    else
    {
        int i = 0;
        for(; i < pContext->spacesCount; ++i)
        {
            DI_CHECK(DianaTextOutput(pContext, " "));
        }
    }
    pContext->flags |= DIANA_TEXT_OUTPUT_FLAG_OPERAND_PRINTED;
    return DI_SUCCESS;
}
static int PrintOperands(DianaTextOutputContext * pContext)
{
    DianaLinkedOperand * pLinkedOp = pContext->pResult->linkedOperands;
    int i = 0;
    for (i; i < pContext->pResult->pInfo->m_operandCount; ++pLinkedOp, ++i)
    {
        switch (pLinkedOp->type)
        {
        case diana_register:
            DI_CHECK(PrintOpSeparator(pContext));
            DI_CHECK(PrintRegister(pContext, pLinkedOp->value.recognizedRegister));
            break;
        case diana_index:
            DI_CHECK(PrintOpSeparator(pContext));
            DI_CHECK(PrintIndex(pContext, pLinkedOp));
            break;
        case diana_imm:
            DI_CHECK(PrintOpSeparator(pContext));
            DI_CHECK(PrintImmediate(pContext, pLinkedOp));
            break;
        case diana_call_ptr:
            DI_CHECK(PrintOpSeparator(pContext));
            DI_CHECK(PrintCallPtr(pContext, pLinkedOp));
            break;
        case diana_rel:
            DI_CHECK(PrintOpSeparator(pContext));
            DI_CHECK(PrintRelative(pContext, pLinkedOp));
            break;
        case diana_memory:
            DI_CHECK(PrintOpSeparator(pContext));
            DI_CHECK(PrintMemory(pContext, pLinkedOp));
            break;
        case diana_reserved_reg:
        default:
        case diana_none:;
        }
    }
    return DI_SUCCESS;
}
static int diana_cmd_output_generic(DianaTextOutputContext * pContext)
{    
    DI_CHECK(DianaTextOutput(pContext, pContext->pResult->pInfo->m_pGroupInfo->m_pName));    
    return PrintOperands(pContext);
}

static DianaTextOutputCmd_fnc g_functions[] = 
{
    0,
    diana_cmd_output_generic,     //   diana_cmd_output_nop,
    diana_cmd_output_generic,     //   diana_cmd_output_addpd,
    diana_cmd_output_generic,     //   diana_cmd_output_addps,
    diana_cmd_output_generic,     //   diana_cmd_output_addsd,
    diana_cmd_output_generic,     //   diana_cmd_output_addss,
    diana_cmd_output_generic,     //   diana_cmd_output_addsubpd,
    diana_cmd_output_generic,     //   diana_cmd_output_addsubps,
    diana_cmd_output_generic,     //   diana_cmd_output_andnpd,
    diana_cmd_output_generic,     //   diana_cmd_output_andnps,
    diana_cmd_output_generic,     //   diana_cmd_output_andpd,
    diana_cmd_output_generic,     //   diana_cmd_output_andps,
    diana_cmd_output_generic,     //   diana_cmd_output_cmppd,
    diana_cmd_output_generic,     //   diana_cmd_output_cmpps,
    diana_cmd_output_generic,     //   diana_cmd_output_cmpsd,
    diana_cmd_output_generic,     //   diana_cmd_output_cmpss,
    diana_cmd_output_generic,     //   diana_cmd_output_comisd,
    diana_cmd_output_generic,     //   diana_cmd_output_comiss,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtdq2pd,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtdq2ps,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtpd2dq,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtpd2ps,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtpi2pd,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtpi2ps,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtps2dq,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtps2pd,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtps2pi,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtpd2pi,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtsd2ss,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtsi2sd,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtsi2ss,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtss2sd,
    diana_cmd_output_generic,     //   diana_cmd_output_cvttpd2dq,
    diana_cmd_output_generic,     //   diana_cmd_output_cvttpd2pi,
    diana_cmd_output_generic,     //   diana_cmd_output_cvttps2dq,
    diana_cmd_output_generic,     //   diana_cmd_output_cvttps2pi,
    diana_cmd_output_generic,     //   diana_cmd_output_divpd,
    diana_cmd_output_generic,     //   diana_cmd_output_divps,
    diana_cmd_output_generic,     //   diana_cmd_output_divss,
    diana_cmd_output_generic,     //   diana_cmd_output_divsd,
    diana_cmd_output_generic,     //   diana_cmd_output_extrq,
    diana_cmd_output_generic,     //   diana_cmd_output_haddpd,
    diana_cmd_output_generic,     //   diana_cmd_output_haddps,
    diana_cmd_output_generic,     //   diana_cmd_output_hsubpd,
    diana_cmd_output_generic,     //   diana_cmd_output_hsubps,
    diana_cmd_output_generic,     //   diana_cmd_output_insertq,
    diana_cmd_output_generic,     //   diana_cmd_output_lddqu,
    diana_cmd_output_generic,     //   diana_cmd_output_maskmovdqu,
    diana_cmd_output_generic,     //   diana_cmd_output_maskmovq,
    diana_cmd_output_generic,     //   diana_cmd_output_maxpd,
    diana_cmd_output_generic,     //   diana_cmd_output_maxps,
    diana_cmd_output_generic,     //   diana_cmd_output_maxsd,
    diana_cmd_output_generic,     //   diana_cmd_output_maxss,
    diana_cmd_output_generic,     //   diana_cmd_output_minpd,
    diana_cmd_output_generic,     //   diana_cmd_output_minps,
    diana_cmd_output_generic,     //   diana_cmd_output_minsd,
    diana_cmd_output_generic,     //   diana_cmd_output_minss,
    diana_cmd_output_generic,     //   diana_cmd_output_movapd,
    diana_cmd_output_generic,     //   diana_cmd_output_movaps,
    diana_cmd_output_generic,     //   diana_cmd_output_movddup,
    diana_cmd_output_generic,     //   diana_cmd_output_movdq2q,
    diana_cmd_output_generic,     //   diana_cmd_output_movdqa,
    diana_cmd_output_generic,     //   diana_cmd_output_movdqu,
    diana_cmd_output_generic,     //   diana_cmd_output_movhpd,
    diana_cmd_output_generic,     //   diana_cmd_output_movhps,
    diana_cmd_output_generic,     //   diana_cmd_output_movlpd,
    diana_cmd_output_generic,     //   diana_cmd_output_movlps,
    diana_cmd_output_generic,     //   diana_cmd_output_movmskpd,
    diana_cmd_output_generic,     //   diana_cmd_output_movmskps,
    diana_cmd_output_generic,     //   diana_cmd_output_movntq,
    diana_cmd_output_generic,     //   diana_cmd_output_movntdq,
    diana_cmd_output_generic,     //   diana_cmd_output_movntpd,
    diana_cmd_output_generic,     //   diana_cmd_output_movntps,
    diana_cmd_output_generic,     //   diana_cmd_output_movntsd,
    diana_cmd_output_generic,     //   diana_cmd_output_movntss,
    diana_cmd_output_generic,     //   diana_cmd_output_movq,
    diana_cmd_output_generic,     //   diana_cmd_output_movq2dq,
    diana_cmd_output_generic,     //   diana_cmd_output_movsd,
    diana_cmd_output_generic,     //   diana_cmd_output_movshdup,
    diana_cmd_output_generic,     //   diana_cmd_output_movsldup,
    diana_cmd_output_generic,     //   diana_cmd_output_movss,
    diana_cmd_output_generic,     //   diana_cmd_output_movupd,
    diana_cmd_output_generic,     //   diana_cmd_output_movups,
    diana_cmd_output_generic,     //   diana_cmd_output_mulpd,
    diana_cmd_output_generic,     //   diana_cmd_output_mulps,
    diana_cmd_output_generic,     //   diana_cmd_output_mulsd,
    diana_cmd_output_generic,     //   diana_cmd_output_mulss,
    diana_cmd_output_generic,     //   diana_cmd_output_orpd,
    diana_cmd_output_generic,     //   diana_cmd_output_orps,
    diana_cmd_output_generic,     //   diana_cmd_output_packssdw,
    diana_cmd_output_generic,     //   diana_cmd_output_packsswb,
    diana_cmd_output_generic,     //   diana_cmd_output_packuswb,
    diana_cmd_output_generic,     //   diana_cmd_output_paddb,
    diana_cmd_output_generic,     //   diana_cmd_output_paddd,
    diana_cmd_output_generic,     //   diana_cmd_output_paddq,
    diana_cmd_output_generic,     //   diana_cmd_output_paddsb,
    diana_cmd_output_generic,     //   diana_cmd_output_paddsw,
    diana_cmd_output_generic,     //   diana_cmd_output_paddusb,
    diana_cmd_output_generic,     //   diana_cmd_output_paddusw,
    diana_cmd_output_generic,     //   diana_cmd_output_paddw,
    diana_cmd_output_generic,     //   diana_cmd_output_pand,
    diana_cmd_output_generic,     //   diana_cmd_output_pandn,
    diana_cmd_output_generic,     //   diana_cmd_output_pavgb,
    diana_cmd_output_generic,     //   diana_cmd_output_pavgw,
    diana_cmd_output_generic,     //   diana_cmd_output_pcmpeqb,
    diana_cmd_output_generic,     //   diana_cmd_output_pcmpeqd,
    diana_cmd_output_generic,     //   diana_cmd_output_pcmpeqw,
    diana_cmd_output_generic,     //   diana_cmd_output_pcmpgtb,
    diana_cmd_output_generic,     //   diana_cmd_output_pcmpgtd,
    diana_cmd_output_generic,     //   diana_cmd_output_pcmpgtw,
    diana_cmd_output_generic,     //   diana_cmd_output_pextrw,
    diana_cmd_output_generic,     //   diana_cmd_output_pinsrw,
    diana_cmd_output_generic,     //   diana_cmd_output_pmaddwd,
    diana_cmd_output_generic,     //   diana_cmd_output_pmaxsw,
    diana_cmd_output_generic,     //   diana_cmd_output_pmaxub,
    diana_cmd_output_generic,     //   diana_cmd_output_pminsw,
    diana_cmd_output_generic,     //   diana_cmd_output_pminub,
    diana_cmd_output_generic,     //   diana_cmd_output_pmovmskb,
    diana_cmd_output_generic,     //   diana_cmd_output_pmulhuw,
    diana_cmd_output_generic,     //   diana_cmd_output_pmulhw,
    diana_cmd_output_generic,     //   diana_cmd_output_pmullw,
    diana_cmd_output_generic,     //   diana_cmd_output_pmuludq,
    diana_cmd_output_generic,     //   diana_cmd_output_por,
    diana_cmd_output_generic,     //   diana_cmd_output_psadbw,
    diana_cmd_output_generic,     //   diana_cmd_output_pshufhw,
    diana_cmd_output_generic,     //   diana_cmd_output_pslld,
    diana_cmd_output_generic,     //   diana_cmd_output_pslldq,
    diana_cmd_output_generic,     //   diana_cmd_output_psllq,
    diana_cmd_output_generic,     //   diana_cmd_output_psllw,
    diana_cmd_output_generic,     //   diana_cmd_output_psrad,
    diana_cmd_output_generic,     //   diana_cmd_output_psraw,
    diana_cmd_output_generic,     //   diana_cmd_output_psrld,
    diana_cmd_output_generic,     //   diana_cmd_output_psrldq,
    diana_cmd_output_generic,     //   diana_cmd_output_psrlq,
    diana_cmd_output_generic,     //   diana_cmd_output_psrlw,
    diana_cmd_output_generic,     //   diana_cmd_output_psubb,
    diana_cmd_output_generic,     //   diana_cmd_output_psubd,
    diana_cmd_output_generic,     //   diana_cmd_output_psubq,
    diana_cmd_output_generic,     //   diana_cmd_output_psubsb,
    diana_cmd_output_generic,     //   diana_cmd_output_psubsw,
    diana_cmd_output_generic,     //   diana_cmd_output_psubusb,
    diana_cmd_output_generic,     //   diana_cmd_output_psubusw,
    diana_cmd_output_generic,     //   diana_cmd_output_psubw,
    diana_cmd_output_generic,     //   diana_cmd_output_punpckhbw,
    diana_cmd_output_generic,     //   diana_cmd_output_punpckhdq,
    diana_cmd_output_generic,     //   diana_cmd_output_punpckhqdq,
    diana_cmd_output_generic,     //   diana_cmd_output_punpckhwd,
    diana_cmd_output_generic,     //   diana_cmd_output_punpcklbw,
    diana_cmd_output_generic,     //   diana_cmd_output_punpckldq,
    diana_cmd_output_generic,     //   diana_cmd_output_punpcklqdq,
    diana_cmd_output_generic,     //   diana_cmd_output_punpcklwd,
    diana_cmd_output_generic,     //   diana_cmd_output_pxor,
    diana_cmd_output_generic,     //   diana_cmd_output_rcpps,
    diana_cmd_output_generic,     //   diana_cmd_output_rcpss,
    diana_cmd_output_generic,     //   diana_cmd_output_rsqrtps,
    diana_cmd_output_generic,     //   diana_cmd_output_rsqrtss,
    diana_cmd_output_generic,     //   diana_cmd_output_shufpd,
    diana_cmd_output_generic,     //   diana_cmd_output_shufps,
    diana_cmd_output_generic,     //   diana_cmd_output_sqrtpd,
    diana_cmd_output_generic,     //   diana_cmd_output_sqrtps,
    diana_cmd_output_generic,     //   diana_cmd_output_sqrtsd,
    diana_cmd_output_generic,     //   diana_cmd_output_sqrtss,
    diana_cmd_output_generic,     //   diana_cmd_output_subpd,
    diana_cmd_output_generic,     //   diana_cmd_output_subps,
    diana_cmd_output_generic,     //   diana_cmd_output_subsd,
    diana_cmd_output_generic,     //   diana_cmd_output_subss,
    diana_cmd_output_generic,     //   diana_cmd_output_ucomisd,
    diana_cmd_output_generic,     //   diana_cmd_output_ucomiss,
    diana_cmd_output_generic,     //   diana_cmd_output_unpckhpd,
    diana_cmd_output_generic,     //   diana_cmd_output_unpckhps,
    diana_cmd_output_generic,     //   diana_cmd_output_unpcklpd,
    diana_cmd_output_generic,     //   diana_cmd_output_unpcklps,
    diana_cmd_output_generic,     //   diana_cmd_output_xorpd,
    diana_cmd_output_generic,     //   diana_cmd_output_xorps,
    diana_cmd_output_generic,     //   diana_cmd_output_pshufw,
    diana_cmd_output_generic,     //   diana_cmd_output_pshufd,
    diana_cmd_output_generic,     //   diana_cmd_output_pshuflw,
    diana_cmd_output_generic,     //   diana_cmd_output_f2xm1,
    diana_cmd_output_generic,     //   diana_cmd_output_fabs,
    diana_cmd_output_generic,     //   diana_cmd_output_fadd,
    diana_cmd_output_generic,     //   diana_cmd_output_faddp,
    diana_cmd_output_generic,     //   diana_cmd_output_fiadd,
    diana_cmd_output_generic,     //   diana_cmd_output_fbld,
    diana_cmd_output_generic,     //   diana_cmd_output_fbstp,
    diana_cmd_output_generic,     //   diana_cmd_output_fchs,
    diana_cmd_output_generic,     //   diana_cmd_output_fnclex,
    diana_cmd_output_generic,     //   diana_cmd_output_fcmovb,
    diana_cmd_output_generic,     //   diana_cmd_output_fcmove,
    diana_cmd_output_generic,     //   diana_cmd_output_fcmovbe,
    diana_cmd_output_generic,     //   diana_cmd_output_fcmovu,
    diana_cmd_output_generic,     //   diana_cmd_output_fcmovnb,
    diana_cmd_output_generic,     //   diana_cmd_output_fcmovne,
    diana_cmd_output_generic,     //   diana_cmd_output_fcmovnbe,
    diana_cmd_output_generic,     //   diana_cmd_output_fcmovnu,
    diana_cmd_output_generic,     //   diana_cmd_output_fcom,
    diana_cmd_output_generic,     //   diana_cmd_output_fcomp,
    diana_cmd_output_generic,     //   diana_cmd_output_fcompp,
    diana_cmd_output_generic,     //   diana_cmd_output_fcomi,
    diana_cmd_output_generic,     //   diana_cmd_output_fcomip,
    diana_cmd_output_generic,     //   diana_cmd_output_fucomi,
    diana_cmd_output_generic,     //   diana_cmd_output_fucomip,
    diana_cmd_output_generic,     //   diana_cmd_output_fcos,
    diana_cmd_output_generic,     //   diana_cmd_output_fdecstp,
    diana_cmd_output_generic,     //   diana_cmd_output_fdiv,
    diana_cmd_output_generic,     //   diana_cmd_output_fdivp,
    diana_cmd_output_generic,     //   diana_cmd_output_fidiv,
    diana_cmd_output_generic,     //   diana_cmd_output_fdivr,
    diana_cmd_output_generic,     //   diana_cmd_output_fdivrp,
    diana_cmd_output_generic,     //   diana_cmd_output_fidivr,
    diana_cmd_output_generic,     //   diana_cmd_output_ffree,
    diana_cmd_output_generic,     //   diana_cmd_output_ffreep,
    diana_cmd_output_generic,     //   diana_cmd_output_ficom,
    diana_cmd_output_generic,     //   diana_cmd_output_ficomp,
    diana_cmd_output_generic,     //   diana_cmd_output_fild,
    diana_cmd_output_generic,     //   diana_cmd_output_fincstp,
    diana_cmd_output_generic,     //   diana_cmd_output_fninit,
    diana_cmd_output_generic,     //   diana_cmd_output_fld,
    diana_cmd_output_generic,     //   diana_cmd_output_fld1,
    diana_cmd_output_generic,     //   diana_cmd_output_fldl2t,
    diana_cmd_output_generic,     //   diana_cmd_output_fldl2e,
    diana_cmd_output_generic,     //   diana_cmd_output_fldpi,
    diana_cmd_output_generic,     //   diana_cmd_output_fldlg2,
    diana_cmd_output_generic,     //   diana_cmd_output_fldln2,
    diana_cmd_output_generic,     //   diana_cmd_output_fldz,
    diana_cmd_output_generic,     //   diana_cmd_output_fldcw,
    diana_cmd_output_generic,     //   diana_cmd_output_fldenv,
    diana_cmd_output_generic,     //   diana_cmd_output_fmul,
    diana_cmd_output_generic,     //   diana_cmd_output_fmulp,
    diana_cmd_output_generic,     //   diana_cmd_output_fimul,
    diana_cmd_output_generic,     //   diana_cmd_output_fnop,
    diana_cmd_output_generic,     //   diana_cmd_output_fpatan,
    diana_cmd_output_generic,     //   diana_cmd_output_fprem,
    diana_cmd_output_generic,     //   diana_cmd_output_fprem1,
    diana_cmd_output_generic,     //   diana_cmd_output_fptan,
    diana_cmd_output_generic,     //   diana_cmd_output_frndint,
    diana_cmd_output_generic,     //   diana_cmd_output_frstor,
    diana_cmd_output_generic,     //   diana_cmd_output_fnsave,
    diana_cmd_output_generic,     //   diana_cmd_output_fscale,
    diana_cmd_output_generic,     //   diana_cmd_output_fsin,
    diana_cmd_output_generic,     //   diana_cmd_output_fsincos,
    diana_cmd_output_generic,     //   diana_cmd_output_fsqrt,
    diana_cmd_output_generic,     //   diana_cmd_output_fst,
    diana_cmd_output_generic,     //   diana_cmd_output_fstp,
    diana_cmd_output_generic,     //   diana_cmd_output_fnstcw,
    diana_cmd_output_generic,     //   diana_cmd_output_fnstenv,
    diana_cmd_output_generic,     //   diana_cmd_output_fnstsw,
    diana_cmd_output_generic,     //   diana_cmd_output_fsub,
    diana_cmd_output_generic,     //   diana_cmd_output_fsubp,
    diana_cmd_output_generic,     //   diana_cmd_output_fisub,
    diana_cmd_output_generic,     //   diana_cmd_output_fsubr,
    diana_cmd_output_generic,     //   diana_cmd_output_fsubrp,
    diana_cmd_output_generic,     //   diana_cmd_output_fisubr,
    diana_cmd_output_generic,     //   diana_cmd_output_ftst,
    diana_cmd_output_generic,     //   diana_cmd_output_fucom,
    diana_cmd_output_generic,     //   diana_cmd_output_fucomp,
    diana_cmd_output_generic,     //   diana_cmd_output_fucompp,
    diana_cmd_output_generic,     //   diana_cmd_output_fxam,
    diana_cmd_output_generic,     //   diana_cmd_output_fxch,
    diana_cmd_output_generic,     //   diana_cmd_output_fxrstor,
    diana_cmd_output_generic,     //   diana_cmd_output_fxsave,
    diana_cmd_output_generic,     //   diana_cmd_output_fxtract,
    diana_cmd_output_generic,     //   diana_cmd_output_fyl2x,
    diana_cmd_output_generic,     //   diana_cmd_output_fyl2xp1,
    diana_cmd_output_generic,     //   diana_cmd_output_fisttp,
    diana_cmd_output_generic,     //   diana_cmd_output_fist,
    diana_cmd_output_generic,     //   diana_cmd_output_fistp,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtsd2si,
    diana_cmd_output_generic,     //   diana_cmd_output_cvtss2si,
    diana_cmd_output_generic,     //   diana_cmd_output_cvttsd2si,
    diana_cmd_output_generic,     //   diana_cmd_output_cvttss2si,
    diana_cmd_output_generic,     //   diana_cmd_output_movd,
    diana_cmd_output_generic,     //   diana_cmd_output_stmxcsr,
    diana_cmd_output_generic,     //   diana_cmd_output_ldmxcsr,
    diana_cmd_output_generic,     //   diana_cmd_output_rdtsc,
    diana_cmd_output_generic,     //   diana_cmd_output_rdtscp,
    diana_cmd_output_generic,     //   diana_cmd_output_rdpmc,
    diana_cmd_output_generic,     //   diana_cmd_output_rdmsr,
    diana_cmd_output_generic,     //   diana_cmd_output_wrmsr,
    diana_cmd_output_generic,     //   diana_cmd_output_cmpxchg16b,
    diana_cmd_output_generic,     //   diana_cmd_output_cpuid,
    diana_cmd_output_generic,     //   diana_cmd_output_sysenter,
    diana_cmd_output_generic,     //   diana_cmd_output_syscall,
    diana_cmd_output_generic,     //   diana_cmd_output_sysexit,
    diana_cmd_output_generic,     //   diana_cmd_output_sysret,
    diana_cmd_output_generic,     //   diana_cmd_output_wbinvd,
    diana_cmd_output_generic,     //   diana_cmd_output_vmsave,
    diana_cmd_output_generic,     //   diana_cmd_output_vmrun,
    diana_cmd_output_generic,     //   diana_cmd_output_vmmcall,
    diana_cmd_output_generic,     //   diana_cmd_output_vmload,
    diana_cmd_output_generic,     //   diana_cmd_output_verw,
    diana_cmd_output_generic,     //   diana_cmd_output_verr,
    diana_cmd_output_generic,     //   diana_cmd_output_ud2,
    diana_cmd_output_generic,     //   diana_cmd_output_monitor,
    diana_cmd_output_generic,     //   diana_cmd_output_mwait,
    diana_cmd_output_generic,     //   diana_cmd_output_skinit,
    diana_cmd_output_generic,     //   diana_cmd_output_movsxd,
    diana_cmd_output_generic,     //   diana_cmd_output_bswap,
    diana_cmd_output_generic,     //   diana_cmd_output_pause,
    diana_cmd_output_generic,     //   diana_cmd_output_invd,
    diana_cmd_output_generic,     //   diana_cmd_output_clgi,
    diana_cmd_output_generic,     //   diana_cmd_output_stgi,
    diana_cmd_output_generic,     //   diana_cmd_output_invlpg,
    diana_cmd_output_generic,     //   diana_cmd_output_invlpga,
    diana_cmd_output_generic,     //   diana_cmd_output_swapgs,
    diana_cmd_output_generic,     //   diana_cmd_output_rsm,
    diana_cmd_output_generic,     //   diana_cmd_output_salc,
    diana_cmd_output_generic,     //   diana_cmd_output_icebp,
    diana_cmd_output_generic,     //   diana_cmd_output_prefetchnta,
    diana_cmd_output_generic,     //   diana_cmd_output_prefetcht0,
    diana_cmd_output_generic,     //   diana_cmd_output_prefetcht1,
    diana_cmd_output_generic,     //   diana_cmd_output_prefetcht2,
    diana_cmd_output_generic,     //   diana_cmd_output_prefetch,
    diana_cmd_output_generic,     //   diana_cmd_output_prefetchw,
    diana_cmd_output_generic,     //   diana_cmd_output_vmread,
    diana_cmd_output_generic,     //   diana_cmd_output_vmwrite,
    diana_cmd_output_generic,     //   diana_cmd_output_vmxon,
    diana_cmd_output_generic,     //   diana_cmd_output_vmptrst,
    diana_cmd_output_generic,     //   diana_cmd_output_vmptrld,
    diana_cmd_output_generic,     //   diana_cmd_output_vmclear,
    diana_cmd_output_generic,     //   diana_cmd_output_vmcall,
    diana_cmd_output_generic,     //   diana_cmd_output_vmresume,
    diana_cmd_output_generic,     //   diana_cmd_output_vmxoff,
    diana_cmd_output_generic,     //   diana_cmd_output_vmlaunch,
    diana_cmd_output_generic,     //   diana_cmd_output_getsec,
    diana_cmd_output_generic,     //   diana_cmd_output_xgetbv,
    diana_cmd_output_generic,     //   diana_cmd_output_xsetbv,
    diana_cmd_output_generic,     //   diana_cmd_output_emms,
    diana_cmd_output_generic,     //   diana_cmd_output_femms,
    diana_cmd_output_generic,     //   diana_cmd_output_mfence,
    diana_cmd_output_generic,     //   diana_cmd_output_hint_nop,
    diana_cmd_output_generic,     //   diana_cmd_output_aaa,
    diana_cmd_output_generic,     //   diana_cmd_output_aad,
    diana_cmd_output_generic,     //   diana_cmd_output_aam,
    diana_cmd_output_generic,     //   diana_cmd_output_aas,
    diana_cmd_output_generic,     //   diana_cmd_output_adc,
    diana_cmd_output_generic,     //   diana_cmd_output_add,
    diana_cmd_output_generic,     //   diana_cmd_output_and,
    diana_cmd_output_generic,     //   diana_cmd_output_arpl,
    diana_cmd_output_generic,     //   diana_cmd_output_bound,
    diana_cmd_output_generic,     //   diana_cmd_output_bsf,
    diana_cmd_output_generic,     //   diana_cmd_output_bsr,
    diana_cmd_output_generic,     //   diana_cmd_output_bt,
    diana_cmd_output_generic,     //   diana_cmd_output_btc,
    diana_cmd_output_generic,     //   diana_cmd_output_btr,
    diana_cmd_output_generic,     //   diana_cmd_output_bts,
    diana_cmd_output_generic,     //   diana_cmd_output_call,
    diana_cmd_output_generic,     //   diana_cmd_output_cbw,
    diana_cmd_output_generic,     //   diana_cmd_output_clc,
    diana_cmd_output_generic,     //   diana_cmd_output_cld,
    diana_cmd_output_generic,     //   diana_cmd_output_cli,
    diana_cmd_output_generic,     //   diana_cmd_output_clts,
    diana_cmd_output_generic,     //   diana_cmd_output_cmc,
    diana_cmd_output_generic,     //   diana_cmd_output_cmova,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovae,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovb,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovbe,
    diana_cmd_output_generic,     //   diana_cmd_output_cmove,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovg,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovge,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovl,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovle,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovne,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovno,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovnp,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovns,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovo,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovp,
    diana_cmd_output_generic,     //   diana_cmd_output_cmovs,
    diana_cmd_output_generic,     //   diana_cmd_output_cmp,
    diana_cmd_output_generic,     //   diana_cmd_output_cmps,
    diana_cmd_output_generic,     //   diana_cmd_output_cmpxchg,
    diana_cmd_output_generic,     //   diana_cmd_output_cwd,
    diana_cmd_output_generic,     //   diana_cmd_output_daa,
    diana_cmd_output_generic,     //   diana_cmd_output_das,
    diana_cmd_output_generic,     //   diana_cmd_output_dec,
    diana_cmd_output_generic,     //   diana_cmd_output_div,
    diana_cmd_output_generic,     //   diana_cmd_output_enter,
    diana_cmd_output_generic,     //   diana_cmd_output_hlt,
    diana_cmd_output_generic,     //   diana_cmd_output_idiv,
    diana_cmd_output_generic,     //   diana_cmd_output_imul,
    diana_cmd_output_generic,     //   diana_cmd_output_in,
    diana_cmd_output_generic,     //   diana_cmd_output_inc,
    diana_cmd_output_generic,     //   diana_cmd_output_ins,
    diana_cmd_output_generic,     //   diana_cmd_output_int,
    diana_cmd_output_generic,     //   diana_cmd_output_into,
    diana_cmd_output_generic,     //   diana_cmd_output_iret,
    diana_cmd_output_generic,     //   diana_cmd_output_ja,
    diana_cmd_output_generic,     //   diana_cmd_output_jae,
    diana_cmd_output_generic,     //   diana_cmd_output_jb,
    diana_cmd_output_generic,     //   diana_cmd_output_jbe,
    diana_cmd_output_generic,     //   diana_cmd_output_je,
    diana_cmd_output_generic,     //   diana_cmd_output_jecxz,
    diana_cmd_output_generic,     //   diana_cmd_output_jg,
    diana_cmd_output_generic,     //   diana_cmd_output_jge,
    diana_cmd_output_generic,     //   diana_cmd_output_jl,
    diana_cmd_output_generic,     //   diana_cmd_output_jle,
    diana_cmd_output_generic,     //   diana_cmd_output_jmp,
    diana_cmd_output_generic,     //   diana_cmd_output_jne,
    diana_cmd_output_generic,     //   diana_cmd_output_jno,
    diana_cmd_output_generic,     //   diana_cmd_output_jnp,
    diana_cmd_output_generic,     //   diana_cmd_output_jns,
    diana_cmd_output_generic,     //   diana_cmd_output_jo,
    diana_cmd_output_generic,     //   diana_cmd_output_jp,
    diana_cmd_output_generic,     //   diana_cmd_output_js,
    diana_cmd_output_generic,     //   diana_cmd_output_lahf,
    diana_cmd_output_generic,     //   diana_cmd_output_lar,
    diana_cmd_output_generic,     //   diana_cmd_output_lds,
    diana_cmd_output_generic,     //   diana_cmd_output_lea,
    diana_cmd_output_generic,     //   diana_cmd_output_leave,
    diana_cmd_output_generic,     //   diana_cmd_output_les,
    diana_cmd_output_generic,     //   diana_cmd_output_lfs,
    diana_cmd_output_generic,     //   diana_cmd_output_lgdt,
    diana_cmd_output_generic,     //   diana_cmd_output_lgs,
    diana_cmd_output_generic,     //   diana_cmd_output_lidt,
    diana_cmd_output_generic,     //   diana_cmd_output_lldt,
    diana_cmd_output_generic,     //   diana_cmd_output_lmsw,
    diana_cmd_output_generic,     //   diana_cmd_output_lods,
    diana_cmd_output_generic,     //   diana_cmd_output_loop,
    diana_cmd_output_generic,     //   diana_cmd_output_loope,
    diana_cmd_output_generic,     //   diana_cmd_output_loopne,
    diana_cmd_output_generic,     //   diana_cmd_output_lsl,
    diana_cmd_output_generic,     //   diana_cmd_output_lss,
    diana_cmd_output_generic,     //   diana_cmd_output_ltr,
    diana_cmd_output_generic,     //   diana_cmd_output_mov,
    diana_cmd_output_generic,     //   diana_cmd_output_movs,
    diana_cmd_output_generic,     //   diana_cmd_output_movsx,
    diana_cmd_output_generic,     //   diana_cmd_output_movzx,
    diana_cmd_output_generic,     //   diana_cmd_output_mul,
    diana_cmd_output_generic,     //   diana_cmd_output_neg,
    diana_cmd_output_generic,     //   diana_cmd_output_not,
    diana_cmd_output_generic,     //   diana_cmd_output_or,
    diana_cmd_output_generic,     //   diana_cmd_output_out,
    diana_cmd_output_generic,     //   diana_cmd_output_outs,
    diana_cmd_output_generic,     //   diana_cmd_output_pop,
    diana_cmd_output_generic,     //   diana_cmd_output_popa,
    diana_cmd_output_generic,     //   diana_cmd_output_popf,
    diana_cmd_output_generic,     //   diana_cmd_output_push,
    diana_cmd_output_generic,     //   diana_cmd_output_pusha,
    diana_cmd_output_generic,     //   diana_cmd_output_pushf,
    diana_cmd_output_generic,     //   diana_cmd_output_rcl,
    diana_cmd_output_generic,     //   diana_cmd_output_rcr,
    diana_cmd_output_generic,     //   diana_cmd_output_ret,
    diana_cmd_output_generic,     //   diana_cmd_output_retf,
    diana_cmd_output_generic,     //   diana_cmd_output_rol,
    diana_cmd_output_generic,     //   diana_cmd_output_ror,
    diana_cmd_output_generic,     //   diana_cmd_output_sahf,
    diana_cmd_output_generic,     //   diana_cmd_output_sar,
    diana_cmd_output_generic,     //   diana_cmd_output_sbb,
    diana_cmd_output_generic,     //   diana_cmd_output_scas,
    diana_cmd_output_generic,     //   diana_cmd_output_seta,
    diana_cmd_output_generic,     //   diana_cmd_output_setae,
    diana_cmd_output_generic,     //   diana_cmd_output_setb,
    diana_cmd_output_generic,     //   diana_cmd_output_setbe,
    diana_cmd_output_generic,     //   diana_cmd_output_sete,
    diana_cmd_output_generic,     //   diana_cmd_output_setg,
    diana_cmd_output_generic,     //   diana_cmd_output_setge,
    diana_cmd_output_generic,     //   diana_cmd_output_setl,
    diana_cmd_output_generic,     //   diana_cmd_output_setle,
    diana_cmd_output_generic,     //   diana_cmd_output_setne,
    diana_cmd_output_generic,     //   diana_cmd_output_setno,
    diana_cmd_output_generic,     //   diana_cmd_output_setnp,
    diana_cmd_output_generic,     //   diana_cmd_output_setns,
    diana_cmd_output_generic,     //   diana_cmd_output_seto,
    diana_cmd_output_generic,     //   diana_cmd_output_setp,
    diana_cmd_output_generic,     //   diana_cmd_output_sets,
    diana_cmd_output_generic,     //   diana_cmd_output_sgdt,
    diana_cmd_output_generic,     //   diana_cmd_output_shl,
    diana_cmd_output_generic,     //   diana_cmd_output_shld,
    diana_cmd_output_generic,     //   diana_cmd_output_shr,
    diana_cmd_output_generic,     //   diana_cmd_output_shrd,
    diana_cmd_output_generic,     //   diana_cmd_output_sidt,
    diana_cmd_output_generic,     //   diana_cmd_output_sldt,
    diana_cmd_output_generic,     //   diana_cmd_output_smsw,
    diana_cmd_output_generic,     //   diana_cmd_output_stc,
    diana_cmd_output_generic,     //   diana_cmd_output_std,
    diana_cmd_output_generic,     //   diana_cmd_output_sti,
    diana_cmd_output_generic,     //   diana_cmd_output_stos,
    diana_cmd_output_generic,     //   diana_cmd_output_str,
    diana_cmd_output_generic,     //   diana_cmd_output_sub,
    diana_cmd_output_generic,     //   diana_cmd_output_test,
    diana_cmd_output_generic,     //   diana_cmd_output_wait,
    diana_cmd_output_generic,     //   diana_cmd_output_xadd,
    diana_cmd_output_generic,     //   diana_cmd_output_xchg,
    diana_cmd_output_generic,     //   diana_cmd_output_xlat,
    diana_cmd_output_generic,     //   diana_cmd_output_xor,
    diana_cmd_output_generic,     //   diana_cmd_output_test_cmd,
    diana_cmd_output_generic,     //   diana_cmd_output_xrstor,
    diana_cmd_output_generic,     //   diana_cmd_output_lfence,
    diana_cmd_output_generic,     //   diana_cmd_output_clflush,
    diana_cmd_output_generic      //   diana_cmd_output_sfence
};

void DianaTextOutputContext_Init(DianaTextOutputContext * pContext,
                                 DianaTextOutput_type pTextOut,
                                 DianaOpOutput_type pOpOut,
                                 int spacesCount)
{
    DIANA_MEMSET(pContext, 0, sizeof(*pContext));
    pContext->pTextOutputFnc = pTextOut;
    pContext->pOpOutputFnc = pOpOut;
    pContext->spacesCount = spacesCount;
}

static int DianaTextReset(struct _DianaTextOutputContext * pContext)
{
    DianaStringOutputContext * pStringContext = (DianaStringOutputContext *)pContext;
    pStringContext->pBuffer[0] = 0;
    pStringContext->curSize = 0;
    return DI_SUCCESS;
}

void DianaStringOutputContext_Init(DianaStringOutputContext * pContext,
                                   DianaTextOutput_type pTextOut,
                                   DianaOpOutput_type pOpOut,
                                   int spacesCount,
                                   char * pBuffer,
                                   OPERAND_SIZE maxBufferSize)
{
    DianaTextOutputContext_Init(&pContext->parent, pTextOut, pOpOut, spacesCount);
    pContext->parent.pReset = DianaTextReset;
    pContext->pBuffer = pBuffer;
    pContext->maxBufferSize = maxBufferSize;
}
int DianaTextOutputContext_TextOut(DianaTextOutputContext * pContext,
                                   DianaParserResult * pResult, 
                                   OPERAND_SIZE instructionRIP)
{
    unsigned long cmdId = (unsigned long )pResult->pInfo->m_pGroupInfo->m_commandId;
    pContext->flags = 0;
    pContext->pResult = pResult;
    pContext->instructionRIP = instructionRIP;

    if (pContext->pReset)
        pContext->pReset(pContext);

    if (cmdId >= sizeof(g_functions)/sizeof(g_functions[0]))
    {
        return diana_cmd_output_generic(pContext);
    }
    return (*g_functions[cmdId])(pContext);
}