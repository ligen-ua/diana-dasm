// AArch32 text output (A32 and T32): objdump style, with the aliases objdump prints (push, lsl, nop...).
// objdump's own rules are followed where they are not UAL: a decimal immediate gets a "@ 0x..." comment when it is
// above 32 or below -16, literal loads get the address as a comment. Comments follow after one space, where
// objdump has tabs, like the A64 text has no tabs
#include "diana_arm32.h"
#include "diana_text_output_masm.h"

#ifndef DIANA_NO_ARM

typedef struct _DianaA32Printer
{
    DianaTextOutputContext * pContext;
    const DianaParserResult * pResult;
    const DianaA32CmdInfo * pCmd;
    DI_UINT32 word;             // A32: the word; T32: hw1 << 16 | hw2
    int isThumb;
    int operandsPrinted;
    DI_INT64 commentValue;      // objdump's value_in_comment
    const char * pSuffix;       // the data type to print instead of the command's (vshl.s8 #imm), 0: the command's
}DianaA32Printer;

static const char * g_conditions[16] =
{
    "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "al", "<und>"
};

static const char * g_shifts[4] =
{
    "lsl", "lsr", "asr", "ror"
};

static const char * g_registers[16] =
{
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "sl", "fp", "ip", "sp", "lr", "pc"
};

// ------------------------------------------------------------------ basic output
static int PrintText(DianaA32Printer * pPrinter, const char * pText)
{
    return pPrinter->pContext->pTextOutputFnc(pPrinter->pContext, pText);
}

static void FormatUnsigned(char * pBuffer, DI_UINT64 value, int base)
{
    char temp[32];
    int count = 0, i = 0;
    do
    {
        int digit = (int)(value % (DI_UINT64)base);
        temp[count++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        value /= (DI_UINT64)base;
    } while (value);
    for (; i < count; ++i)
        pBuffer[i] = temp[count - 1 - i];
    pBuffer[count] = 0;
}

static int PrintSigned(DianaA32Printer * pPrinter, DI_INT64 value)
{
    char buffer[40];
    if (value < 0)
    {
        buffer[0] = '-';
        FormatUnsigned(buffer + 1, (DI_UINT64)0 - (DI_UINT64)value, 10);
    }
    else
    {
        FormatUnsigned(buffer, (DI_UINT64)value, 10);
    }
    return PrintText(pPrinter, buffer);
}

// #<decimal>; the value is objdump's comment candidate
static int PrintImm(DianaA32Printer * pPrinter, DI_INT64 value)
{
    DI_CHECK(PrintText(pPrinter, "#"));
    pPrinter->commentValue = value;
    return PrintSigned(pPrinter, value);
}

static int PrintHex(DianaA32Printer * pPrinter, DI_UINT64 value, int digits)
{
    char buffer[24];
    int i = 0;
    buffer[0] = '0';
    buffer[1] = 'x';
    if (digits)
    {
        for (; i < digits; ++i)
        {
            int digit = (int)((value >> ((digits - 1 - i) * 4)) & 15);
            buffer[2 + i] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        }
        buffer[2 + digits] = 0;
    }
    else
    {
        FormatUnsigned(buffer + 2, value, 16);
    }
    return PrintText(pPrinter, buffer);
}

// addresses go through pOpOutputFnc, so the user can annotate them
static int PrintAddress(DianaA32Printer * pPrinter, DI_UINT64 address)
{
    return pPrinter->pContext->pOpOutputFnc(pPrinter->pContext, address & 0xFFFFFFFFu, 0);
}

static int PrintSeparator(DianaA32Printer * pPrinter)
{
    if (pPrinter->operandsPrinted++)
        return PrintText(pPrinter, ", ");
    {
        int i = 0;
        for (; i < pPrinter->pContext->spacesCount; ++i)
        {
            DI_CHECK(PrintText(pPrinter, " "));
        }
    }
    return DI_SUCCESS;
}

static int PrintRegister(DianaA32Printer * pPrinter, DianaUnifiedRegister reg)
{
    int index = (int)reg - (int)reg_A32_R0;
    if (index < 0 || index > 15)
        return PrintText(pPrinter, "?");
    return PrintText(pPrinter, g_registers[index]);
}

static int GetCondition(const DianaA32Printer * pPrinter)
{
    return pPrinter->pResult->iPrefix & DIANA_A32_PREFIX_COND_MASK;
}

static int InItBlock(const DianaA32Printer * pPrinter)
{
    return (pPrinter->pResult->iPrefix & DIANA_A32_PREFIX_IN_IT) != 0;
}

// T32 instructions objdump prints without the IT condition (no %c in its table); the CPS and SETEND ones get
// "@ unpredictable <IT:cond>" instead
static int NoItCondition(const DianaA32Printer * pPrinter)
{
    switch ((DianaA32CommandId)pPrinter->pCmd->header.m_pGroupInfo->m_commandId)
    {
    case diana_a32_cmd_hlt:
    case diana_a32_cmd_setpan:
    case diana_a32_cmd_bkpt:
    case diana_a32_cmd_esb:
    case diana_a32_cmd_csdb:
    case diana_a32_cmd_ssbb:
    case diana_a32_cmd_pssbb:
    case diana_a32_cmd_sb:
    case diana_a32_cmd_dcps1:
    case diana_a32_cmd_dcps2:
    case diana_a32_cmd_dcps3:
    case diana_a32_cmd_crc32b:
    case diana_a32_cmd_crc32h:
    case diana_a32_cmd_crc32w:
    case diana_a32_cmd_crc32cb:
    case diana_a32_cmd_crc32ch:
    case diana_a32_cmd_crc32cw:
    // Advanced SIMD of ARMv8.2 and later without %c in objdump's table
    case diana_a32_cmd_vdot:
    case diana_a32_cmd_vfmal:
    case diana_a32_cmd_vfmsl:
    case diana_a32_cmd_vins:
    case diana_a32_cmd_vmmla:
    case diana_a32_cmd_vsmmla:
    case diana_a32_cmd_vsudot:
    case diana_a32_cmd_vummla:
    case diana_a32_cmd_vusdot:
    case diana_a32_cmd_vusmmla:
        return 1;
    case diana_a32_cmd_cps:
    case diana_a32_cmd_cpsie:
    case diana_a32_cmd_cpsid:
    case diana_a32_cmd_setend:
        return 2;
    default:
        return 0;
    }
}

// name, the condition, the width qualifier: addseq, addeq.w, beq.n
static int PrintMnemonic(DianaA32Printer * pPrinter, const char * pName)
{
    int cond = GetCondition(pPrinter);
    DI_CHECK(PrintText(pPrinter, pName));
    // objdump prints "al" only for an instruction in an IT block
    if ((cond != DIANA_A32_COND_AL || InItBlock(pPrinter)) && !(pPrinter->isThumb && NoItCondition(pPrinter)))
        DI_CHECK(PrintText(pPrinter, g_conditions[cond]));
    if (pPrinter->pCmd->flags & DIANA_A32_CMDF_WIDE)
        DI_CHECK(PrintText(pPrinter, ".w"));
    if (pPrinter->pCmd->flags & DIANA_A32_CMDF_NARROW)
        DI_CHECK(PrintText(pPrinter, ".n"));
    if (pPrinter->pSuffix || pPrinter->pCmd->suffix)
        DI_CHECK(PrintText(pPrinter, pPrinter->pSuffix ? pPrinter->pSuffix : DianaA32_GetTables()->pSuffixes[pPrinter->pCmd->suffix]));
    return DI_SUCCESS;
}

// a T32 instruction that sets the flags outside an IT block: "s" there (adds), nothing inside one (addeq)
static int SetsFlagsOutsideIt(const DianaA32Printer * pPrinter)
{
    return (pPrinter->pCmd->flags & DIANA_A32_CMDF_S_OUTSIDE_IT) && !InItBlock(pPrinter);
}

// pName, "s" if the instruction sets the flags outside an IT block, the condition, the width qualifier
static int PrintNameMnemonic(DianaA32Printer * pPrinter, const char * pName)
{
    char buffer[32];
    int length = 0;
    if (!SetsFlagsOutsideIt(pPrinter))
        return PrintMnemonic(pPrinter, pName);
    for (; pName[length] && length < (int)sizeof(buffer) - 2; ++length)
        buffer[length] = pName[length];
    buffer[length++] = 's';
    buffer[length] = 0;
    return PrintMnemonic(pPrinter, buffer);
}

static int PrintGroupMnemonic(DianaA32Printer * pPrinter)
{
    return PrintNameMnemonic(pPrinter, pPrinter->pCmd->header.m_pGroupInfo->m_pName);
}

// ------------------------------------------------------------------ operands
static const DianaLinkedOperand * Op(const DianaA32Printer * pPrinter, int index)
{
    return pPrinter->pResult->linkedOperands + index;
}

static int OpRegIndex(const DianaA32Printer * pPrinter, int index)
{
    return (int)Op(pPrinter, index)->value.recognizedRegister - (int)reg_A32_R0;
}

static int PrintShift(DianaA32Printer * pPrinter, const DianaArmOperandExt * pExt)
{
    if (pExt->shiftType == DIANA_ARM_SHIFT_NONE)
        return DI_SUCCESS;
    DI_CHECK(PrintText(pPrinter, ", "));
    if (pExt->shiftType == DIANA_ARM_SHIFT_RRX)
        return PrintText(pPrinter, "rrx");
    DI_CHECK(PrintText(pPrinter, g_shifts[(pExt->shiftType - DIANA_ARM_SHIFT_LSL) & 3]));
    DI_CHECK(PrintText(pPrinter, " "));
    if (pExt->qualifier == DIANA_ARM_QUAL_SHIFT_REG)
        return PrintText(pPrinter, g_registers[pExt->shiftAmount & 15]);
    DI_CHECK(PrintText(pPrinter, "#"));
    return PrintSigned(pPrinter, pExt->shiftAmount);
}

// A32 modified immediate: objdump prints "#imm8, rot" when a smaller rotation gives the same value
static int PrintModImm(DianaA32Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    DI_UINT32 value = (DI_UINT32)pOp->value.imm;
    int rotate = pOp->arm.shiftAmount;
    int i = 0;
    for (; i < 32; i += 2)
    {
        DI_UINT32 rotated = i ? (value << i) | (value >> (32 - i)) : value;
        if (rotated <= 0xFF)
            break;
    }
    if (i != rotate)
    {
        DI_CHECK(PrintText(pPrinter, "#"));
        DI_CHECK(PrintSigned(pPrinter, (DI_INT64)(((value << rotate) | (rotate ? value >> (32 - rotate) : 0)) & 0xFF)));
        DI_CHECK(PrintText(pPrinter, ", "));
        DI_CHECK(PrintSigned(pPrinter, rotate));
    }
    else
    {
        DI_CHECK(PrintText(pPrinter, "#"));
        DI_CHECK(PrintSigned(pPrinter, (DI_INT32)value));
    }
    pPrinter->commentValue = value;
    return DI_SUCCESS;
}

// the address a label or a PC-relative memory operand points to
static DI_UINT64 LabelTarget(const DianaA32Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    DI_UINT64 address = pPrinter->pContext->instructionRIP;
    if (pOp->arm.qualifier == DIANA_ARM_QUAL_ALIGN_PC)
        address &= ~(DI_UINT64)3;
    return address + pPrinter->pCmd->size + pOp->value.rel.m_value;
}

static DI_UINT64 PcValue(const DianaA32Printer * pPrinter)
{
    if (pPrinter->isThumb)
        return (pPrinter->pContext->instructionRIP + 4) & ~(DI_UINT64)3;
    return pPrinter->pContext->instructionRIP + 8;
}

// T32 LDRD/STRD [Rn, #+/-imm8 * 4] (objdump's %A): keeps #-0, the comment is the size of the offset
static int IsThumbDoubleForm(const DianaA64OperandDesc * pDesc)
{
    return (pDesc->flags & DIANA_A32_OPF_THUMB) && (pDesc->flags & DIANA_A32_OPF_HAS_U) && pDesc->param == 2;
}

// objdump's comment candidate of an immediate offset. T32 32-bit: an added imm12 offset (the %a of LDR.W), the size of
// a LDRD/STRD offset (%A); the 8-bit offsets of the other forms are not comments
static DI_INT64 MemoryComment(const DianaA32Printer * pPrinter, const DianaA64OperandDesc * pDesc, DI_INT64 offset,
                              int negative)
{
    if (!pPrinter->isThumb || pPrinter->pCmd->size == 2)
        return offset;
    if (IsThumbDoubleForm(pDesc))
        return offset < 0 ? -offset : offset;
    if (pDesc->param == 2)
        return offset;          // LDREX, STREX [Rn, #imm8 * 4]
    if (!negative && pDesc->width[pDesc->fieldCount - 1] == 12)
        return offset;
    return 0;
}

// A32 memory with imm4H:imm4L (LDRD, LDRH...): objdump's %s address rules
static int IsHalfwordForm(const DianaA64OperandDesc * pDesc)
{
    int first = (pDesc->arr ? 0 : 1) + ((pDesc->flags & DIANA_A32_OPF_HAS_U) ? 1 : 0);
    return !(pDesc->flags & DIANA_A32_OPF_THUMB) && pDesc->fieldCount - first == 2;
}

static int PrintMemory(DianaA32Printer * pPrinter, int index)
{
    const DianaLinkedOperand * pOp = Op(pPrinter, index);
    const DianaA64OperandDesc * pDesc = pPrinter->pCmd->operands + index;
    const DianaRmIndex * pIndex = &pOp->value.rmIndex;
    int negative = pOp->arm.qualifier == DIANA_ARM_QUAL_NEGATIVE;
    int isPc = pIndex->reg == reg_A32_PC;
    int post = pOp->arm.writeback == DIANA_ARM_WRITEBACK_POST;
    DI_INT64 offset = (DI_INT64)pIndex->dispValue;

    DI_CHECK(PrintText(pPrinter, "["));
    DI_CHECK(PrintRegister(pPrinter, pIndex->reg));
    if (post)
        DI_CHECK(PrintText(pPrinter, "]"));
    if (pDesc->kind == DIANA_A32_OPK_MEM_REG)
    {
        DI_CHECK(PrintText(pPrinter, negative ? ", -" : ", "));
        DI_CHECK(PrintRegister(pPrinter, pIndex->indexed_reg));
        DI_CHECK(PrintShift(pPrinter, &pOp->arm));
    }
    else
    if (pPrinter->isThumb && pPrinter->pCmd->size == 4 && !IsThumbDoubleForm(pDesc))
    {
        // T32 32-bit (objdump's %a): no -0, a zero offset is left out except after a post-index
        if (pDesc->fieldCount > (pDesc->arr ? 0 : 1) && (offset || post))
        {
            DI_CHECK(PrintText(pPrinter, ", #"));
            DI_CHECK(PrintSigned(pPrinter, offset));
            if (!isPc || pDesc->param == 2)
                pPrinter->commentValue = MemoryComment(pPrinter, pDesc, offset, negative);
        }
    }
    else
    if (pDesc->fieldCount > (pDesc->arr ? 0 : 1) &&
        (!((pDesc->flags & DIANA_A32_OPF_OPTIONAL) || (isPc && IsHalfwordForm(pDesc) && !post) ||
           (IsThumbDoubleForm(pDesc) && !post)) || offset || negative))
    {
        DI_CHECK(PrintText(pPrinter, negative ? ", #-" : ", #"));
        DI_CHECK(PrintSigned(pPrinter, negative ? -offset : offset));
        if (!isPc || (pPrinter->isThumb && IsThumbDoubleForm(pDesc)))
            pPrinter->commentValue = MemoryComment(pPrinter, pDesc, offset, negative);
    }
    // objdump's %s prints no "!" for a literal
    if (!post)
        DI_CHECK(PrintText(pPrinter, pOp->arm.writeback == DIANA_ARM_WRITEBACK_PRE && !(isPc && IsHalfwordForm(pDesc)) &&
                                     !(IsThumbDoubleForm(pDesc) && !offset && !negative) ? "]!" : "]"));
    // objdump: a literal gets its address as a comment, except a base without offset (LDREX [pc]) and the
    // post-index form of %s (LDRD, LDRH: imm4H:imm4L)
    // T32 LDREX/STREX [pc, #imm]: no address either
    if (isPc && pDesc->kind == DIANA_A32_OPK_MEM_IMM && pDesc->fieldCount > (pDesc->arr ? 0 : 1) &&
        !(post && IsHalfwordForm(pDesc)) &&
        !(pPrinter->isThumb && pPrinter->pCmd->size == 4 && pDesc->param == 2 && !(pDesc->flags & DIANA_A32_OPF_HAS_U)) &&
        !(IsThumbDoubleForm(pDesc) && !offset && !negative))
    {
        // the literal address; a 16-bit T32 load has it in parentheses
        int parentheses = pPrinter->isThumb && pPrinter->pCmd->size == 2;
        if (!IsThumbDoubleForm(pDesc))
            pPrinter->commentValue = 0;
        DI_CHECK(PrintText(pPrinter, parentheses ? " @ (" : " @ "));
        // a T32 LDRD post-index literal adds the offset too
        DI_CHECK(PrintAddress(pPrinter, PcValue(pPrinter) + (post && !IsThumbDoubleForm(pDesc) ? 0 : offset)));
        if (parentheses)
            DI_CHECK(PrintText(pPrinter, ")"));
    }
    return DI_SUCCESS;
}

static int PrintRegList(DianaA32Printer * pPrinter, DI_UINT32 mask)
{
    int started = 0;
    int reg = 0;
    DI_CHECK(PrintText(pPrinter, "{"));
    for (; reg < 16; ++reg)
    {
        if (!(mask & (1u << reg)))
            continue;
        if (started)
            DI_CHECK(PrintText(pPrinter, ", "));
        started = 1;
        DI_CHECK(PrintText(pPrinter, g_registers[reg]));
    }
    return PrintText(pPrinter, "}");
}

// objdump's names of SYSm (MRS/MSR banked registers, CPSR, SPSR)
static const char * BankedName(DI_UINT32 sysm)
{
    static const struct
    {
        DI_UINT32 sysm;
        const char * pName;
    } names[] =
    {
        {15, "CPSR"}, {32, "R8_usr"}, {33, "R9_usr"}, {34, "R10_usr"}, {35, "R11_usr"}, {36, "R12_usr"}, {37, "SP_usr"},
        {38, "LR_usr"}, {40, "R8_fiq"}, {41, "R9_fiq"}, {42, "R10_fiq"}, {43, "R11_fiq"}, {44, "R12_fiq"}, {45, "SP_fiq"},
        {46, "LR_fiq"}, {48, "LR_irq"}, {49, "SP_irq"}, {50, "LR_svc"}, {51, "SP_svc"}, {52, "LR_abt"}, {53, "SP_abt"},
        {54, "LR_und"}, {55, "SP_und"}, {60, "LR_mon"}, {61, "SP_mon"}, {62, "ELR_hyp"}, {63, "SP_hyp"}, {79, "SPSR"},
        {110, "SPSR_fiq"}, {112, "SPSR_irq"}, {114, "SPSR_svc"}, {116, "SPSR_abt"}, {118, "SPSR_und"}, {124, "SPSR_mon"},
        {126, "SPSR_hyp"}
    };
    int i = 0;
    for (; i < (int)(sizeof(names)/sizeof(names[0])); ++i)
    {
        if (names[i].sysm == sysm)
            return names[i].pName;
    }
    return 0;
}

static int PrintSysm(DianaA32Printer * pPrinter, DI_UINT32 sysm)
{
    const char * pName = BankedName(sysm);
    if (pName)
        return PrintText(pPrinter, pName);
    DI_CHECK(PrintText(pPrinter, "(UNDEF: "));
    DI_CHECK(PrintSigned(pPrinter, sysm));
    return PrintText(pPrinter, ")");
}

// DMB, DSB options by name, ISB only sy
static int PrintBarrier(DianaA32Printer * pPrinter, DI_UINT32 option)
{
    static const char * names[16] =
    {
        0, "oshld", "oshst", "osh", 0, "nshld", "unst", "un", 0, "ishld", "ishst", "ish", 0, "ld", "st", "sy"
    };
    const char * pName = names[option & 15];
    if (pPrinter->pCmd->header.m_pGroupInfo->m_commandId == (DianaCommandId)diana_a32_cmd_isb && option != 15)
        pName = 0;
    if (pName)
        return PrintText(pPrinter, pName);
    DI_CHECK(PrintText(pPrinter, "#"));
    return PrintSigned(pPrinter, option);
}

// ------------------------------------------------------------------ VFP
static int PrintUnsignedPadded(DianaA32Printer * pPrinter, DI_UINT32 value, int digits)
{
    char buffer[16];
    int i = digits;
    buffer[digits] = 0;
    while (i--)
    {
        buffer[i] = (char)('0' + value % 10);
        value /= 10;
    }
    return PrintText(pPrinter, buffer);
}

static int PrintFpRegister(DianaA32Printer * pPrinter, DianaUnifiedRegister reg)
{
    int index = (int)reg - (int)reg_A32_S0;
    if (index >= 0 && index < 32)
    {
        DI_CHECK(PrintText(pPrinter, "s"));
    }
    else
    if ((int)reg >= (int)reg_A32_Q0 && (int)reg < (int)reg_A32_Q0 + 16)
    {
        index = (int)reg - (int)reg_A32_Q0;
        DI_CHECK(PrintText(pPrinter, "q"));
    }
    else
    {
        index = (int)reg - (int)reg_A32_D0;
        if (index < 0 || index > 31)
            return PrintText(pPrinter, "?");
        DI_CHECK(PrintText(pPrinter, "d"));
    }
    return PrintSigned(pPrinter, index);
}

// VMOV immediate, objdump's %E: the imm8 in decimal, then the single precision bits and the value as a comment
// (#112 @ 0x3f800000 1.0) for every size; no fields: #0.0 (VCMP)
static int PrintFpImm(DianaA32Printer * pPrinter, int index)
{
    DI_UINT32 imm8 = ((pPrinter->word >> 12) & 0xF0) | (pPrinter->word & 0xF);
    DI_UINT32 bits = 0, decimal = 0;
    if (!pPrinter->pCmd->operands[index].fieldCount)
        return PrintText(pPrinter, "#0.0");
    bits = ((imm8 & 0x80) << 24) | ((imm8 & 0x3F) << 19) | ((imm8 & 0x40) ? (0xF8u << 22) : (1u << 30));
    // the value times 10^7: 0.125 .. 31.0 fit in 32 bits
    decimal = (78125u << (((bits >> 23) & 0xFF) - 124)) * (16 + (imm8 & 0xF));
    DI_CHECK(PrintText(pPrinter, "#"));
    DI_CHECK(PrintSigned(pPrinter, imm8));
    DI_CHECK(PrintText(pPrinter, " @ "));
    DI_CHECK(PrintHex(pPrinter, bits, 8));
    DI_CHECK(PrintText(pPrinter, (imm8 & 0x80) ? " -" : " "));
    DI_CHECK(PrintSigned(pPrinter, decimal / 10000000));
    DI_CHECK(PrintText(pPrinter, "."));
    if (!(decimal % 1000000))
        return PrintUnsignedPadded(pPrinter, decimal % 10000000 / 1000000, 1);
    if (!(decimal % 10000))
        return PrintUnsignedPadded(pPrinter, decimal % 10000000 / 10000, 3);
    return PrintUnsignedPadded(pPrinter, decimal % 10000000, 7);
}

static int IsFldmx(const DianaA32Printer * pPrinter)
{
    switch ((DianaA32CommandId)pPrinter->pCmd->header.m_pGroupInfo->m_commandId)
    {
    case diana_a32_cmd_fldmiax:
    case diana_a32_cmd_fldmdbx:
    case diana_a32_cmd_fstmiax:
    case diana_a32_cmd_fstmdbx:
        return 1;
    default:
        return 0;
    }
}

// {s0-s7}, {d8}: the first register, then the last if there is more than one. objdump: the D lists of VLDM, VSTM
// (%B) count imm8<6:1> and mark an overflow, the others (%y3, %z3) count all of imm8 and print 0 as first - 1
static int PrintFpList(DianaA32Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    int single = pOp->arm.opKind == DIANA_A32_OPK_SREGLIST;
    int first = (int)pOp->value.recognizedRegister - (int)(single ? reg_A32_S0 : reg_A32_D0);
    int count = pOp->arm.regCount;
    const char * pPrefix = single ? "s" : "d";
    DI_CHECK(PrintText(pPrinter, "{"));
    DI_CHECK(PrintText(pPrinter, pPrefix));
    DI_CHECK(PrintSigned(pPrinter, first));
    if (!single && !IsFldmx(pPrinter) && !(pPrinter->pCmd->operands[pOp - pPrinter->pResult->linkedOperands].flags & DIANA_A32_OPF_COUNT_PLUS1))
    {
        count = (int)((pPrinter->word >> 1) & 0x3F);
        if (count != 1 && first + count > 32)
        {
            DI_CHECK(PrintText(pPrinter, "-<overflow reg d"));
            DI_CHECK(PrintSigned(pPrinter, first + count - 1));
            return PrintText(pPrinter, ">}");
        }
    }
    if (count != 1 && !single && first + count > 32 &&
        (pPrinter->pCmd->operands[pOp - pPrinter->pResult->linkedOperands].flags & DIANA_A32_OPF_COUNT_PLUS1))
    {
        // VTBL past d31, objdump's %F: no ">"
        DI_CHECK(PrintText(pPrinter, "-<overflow reg d"));
        DI_CHECK(PrintSigned(pPrinter, first + count - 1));
        return PrintText(pPrinter, "}");
    }
    if (count != 1)
    {
        DI_CHECK(PrintText(pPrinter, "-"));
        DI_CHECK(PrintText(pPrinter, pPrefix));
        DI_CHECK(PrintSigned(pPrinter, first + count - 1));
    }
    return PrintText(pPrinter, "}");
}

// VMRS/VMSR register names of objdump (with the v8.1-M ones); the others are <impl def 0x..>
static const char * FpSysregName(DI_UINT32 reg)
{
    static const char * names[16] =
    {
        "fpsid", "fpscr", "fpscr_nzcvqc", 0, 0, "mvfr2", "mvfr1", "mvfr0", "fpexc", "fpinst", "fpinst2", 0, 0, 0,
        "fpcxt_ns", "fpcxt_s"
    };
    return names[reg & 15];
}

static int PrintFpSysreg(DianaA32Printer * pPrinter, DI_UINT32 reg)
{
    const char * pName = FpSysregName(reg);
    if (pName)
        return PrintText(pPrinter, pName);
    DI_CHECK(PrintText(pPrinter, "<impl def "));
    DI_CHECK(PrintHex(pPrinter, reg, 0));
    return PrintText(pPrinter, ">");
}

// ------------------------------------------------------------------ Advanced SIMD
static DI_UINT32 DescField(const DianaA32Printer * pPrinter, const DianaA64OperandDesc * pDesc, int index)
{
    return (pPrinter->word >> pDesc->lsb[index]) & ((1u << pDesc->width[index]) - 1);
}

static int PrintHexPadded(DianaA32Printer * pPrinter, DI_UINT64 value, int digits)
{
    return PrintHex(pPrinter, value, digits);
}

// objdump's %E of VMOV, VMVN, VORR, VBIC: the size and form by cmode, not by the data type
static int PrintNeonImm(DianaA32Printer * pPrinter, int index)
{
    const DianaA64OperandDesc * pDesc = pPrinter->pCmd->operands + index;
    int first = pDesc->fieldCount == 5 ? 1 : 0;
    DI_UINT32 op = first ? DescField(pPrinter, pDesc, 0) : (pPrinter->word >> 5) & 1;
    DI_UINT32 cmode = DescField(pPrinter, pDesc, first);
    DI_UINT32 bits = (DescField(pPrinter, pDesc, first + 1) << 7) | (DescField(pPrinter, pDesc, first + 2) << 4) |
                     DescField(pPrinter, pDesc, first + 3);
    DI_UINT64 value = 0;
    int digits = 8;
    pPrinter->commentValue = 0;
    if (cmode < 8)
        value = (DI_UINT64)bits << (8 * ((cmode >> 1) & 3));
    else
    if (cmode < 12)
    {
        value = (DI_UINT64)bits << (8 * ((cmode >> 1) & 1));
        digits = 4;
    }
    else
    if (cmode < 14)
    {
        int shift = (int)(cmode & 1) + 1;
        value = ((DI_UINT64)bits << (8 * shift)) | ((1u << (8 * shift)) - 1);
    }
    else
    if (cmode == 14)
    {
        if (op)
        {
            // bit replication into bytes: printed as one 64-bit hex number
            int i = 0;
            for (; i < 8; ++i)
            {
                if ((bits >> i) & 1)
                    value |= (DI_UINT64)0xFF << (8 * i);
            }
            DI_CHECK(PrintText(pPrinter, "#"));
            return PrintHex(pPrinter, value, 16);
        }
        value = bits;
        digits = 2;
    }
    else
    {
        // a float, %.7g: the values of an 8-bit immediate have at most 7 digits
        DI_UINT32 exponent = (bits & 0x40) ? 0x7C + ((bits >> 4) & 3) : 0x80 + ((bits >> 4) & 3);
        DI_UINT32 decimal = (78125u << (exponent - 124)) * (16 + (bits & 0xF));
        DI_UINT32 fraction = decimal % 10000000;
        int places = 7;
        value = ((DI_UINT64)(bits & 0x80) << 24) | ((bits & 0x40) ? 0x3E000000u : 0x40000000u) | ((DI_UINT64)(bits & 0x3F) << 19);
        DI_CHECK(PrintText(pPrinter, (bits & 0x80) ? "#-" : "#"));
        DI_CHECK(PrintSigned(pPrinter, decimal / 10000000));
        if (fraction)
        {
            while (!(fraction % 10))
            {
                fraction /= 10;
                --places;
            }
            DI_CHECK(PrintText(pPrinter, "."));
            DI_CHECK(PrintUnsignedPadded(pPrinter, fraction, places));
        }
        DI_CHECK(PrintText(pPrinter, " @ "));
        return PrintHexPadded(pPrinter, value, 8);
    }
    DI_CHECK(PrintText(pPrinter, "#"));
    // 32-bit values are signed in objdump
    DI_CHECK(PrintSigned(pPrinter, digits == 8 ? (DI_INT64)(DI_INT32)(DI_UINT32)value : (DI_INT64)value));
    DI_CHECK(PrintText(pPrinter, " @ "));
    return PrintHexPadded(pPrinter, value, digits);
}

static int PrintDRegisterNumber(DianaA32Printer * pPrinter, int number)
{
    DI_CHECK(PrintText(pPrinter, "d"));
    return PrintSigned(pPrinter, number);
}

// VLD/VST lists as objdump: {d0-d3}, {d0,d2}, {d0[1],d1[1]}, {d0[]-d3[]}
static int PrintVList(DianaA32Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    int first = (int)pOp->value.recognizedRegister - (int)reg_A32_D0;
    int count = pOp->arm.regCount;
    int stride = pOp->arm.regStride ? pOp->arm.regStride : 1;
    int lane = DIANA_ARM_ARR_IS_ELEMENT(pOp->arm.arrangement) && pOp->arm.qualifier != DIANA_ARM_QUAL_ALL_LANES;
    const char * pLanes = pOp->arm.qualifier == DIANA_ARM_QUAL_ALL_LANES ? "[]" : "";
    int i = 0;
    DI_CHECK(PrintText(pPrinter, "{"));
    if (lane || stride > 1)
    {
        for (; i < count; ++i)
        {
            if (i)
                DI_CHECK(PrintText(pPrinter, ","));
            DI_CHECK(PrintDRegisterNumber(pPrinter, first + i * stride));
            if (lane)
            {
                DI_CHECK(PrintText(pPrinter, "["));
                DI_CHECK(PrintSigned(pPrinter, pOp->arm.lane));
                DI_CHECK(PrintText(pPrinter, "]"));
            }
            DI_CHECK(PrintText(pPrinter, pLanes));
        }
        return PrintText(pPrinter, "}");
    }
    DI_CHECK(PrintDRegisterNumber(pPrinter, first));
    DI_CHECK(PrintText(pPrinter, pLanes));
    if (count > 1)
    {
        DI_CHECK(PrintText(pPrinter, "-"));
        DI_CHECK(PrintDRegisterNumber(pPrinter, first + count - 1));
        DI_CHECK(PrintText(pPrinter, pLanes));
    }
    return PrintText(pPrinter, "}");
}

// [r0 :64]!, [r0], r2
static int PrintVMem(DianaA32Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    DI_CHECK(PrintText(pPrinter, "["));
    DI_CHECK(PrintRegister(pPrinter, pOp->value.rmIndex.reg));
    if (pOp->arm.shiftAmount)
    {
        DI_CHECK(PrintText(pPrinter, " :"));
        DI_CHECK(PrintSigned(pPrinter, pOp->arm.shiftAmount * 8));
    }
    DI_CHECK(PrintText(pPrinter, "]"));
    if (pOp->value.rmIndex.indexed_reg != reg_none)
    {
        DI_CHECK(PrintText(pPrinter, ", "));
        return PrintRegister(pPrinter, pOp->value.rmIndex.indexed_reg);
    }
    if (pOp->arm.writeback)
        return PrintText(pPrinter, "!");
    return DI_SUCCESS;
}

static int PrintOperandValue(DianaA32Printer * pPrinter, int index)
{
    const DianaLinkedOperand * pOp = Op(pPrinter, index);
    switch (pOp->arm.opKind)
    {
    case DIANA_A32_OPK_REG:
        DI_CHECK(PrintRegister(pPrinter, pOp->value.recognizedRegister));
        if (pOp->arm.writeback)
            DI_CHECK(PrintText(pPrinter, "!"));
        return DI_SUCCESS;
    case DIANA_A32_OPK_IMM:
        return PrintImm(pPrinter, (DI_INT64)pOp->value.imm);
    case DIANA_A32_OPK_MODIMM:
        return PrintModImm(pPrinter, pOp);
    case DIANA_A32_OPK_TMODIMM:
        return PrintImm(pPrinter, (DI_INT64)(DI_UINT32)pOp->value.imm);
    case DIANA_A32_OPK_SHREG:
    case DIANA_A32_OPK_SHREG_REG:
        DI_CHECK(PrintRegister(pPrinter, pOp->value.recognizedRegister));
        return PrintShift(pPrinter, &pOp->arm);
    case DIANA_A32_OPK_LABEL:
        pPrinter->commentValue = 0;
        return PrintAddress(pPrinter, LabelTarget(pPrinter, pOp));
    case DIANA_A32_OPK_MEM_IMM:
    case DIANA_A32_OPK_MEM_REG:
        return PrintMemory(pPrinter, index);
    case DIANA_A32_OPK_REGLIST:
        DI_CHECK(PrintRegList(pPrinter, (DI_UINT32)pOp->value.imm));
        if (pOp->arm.qualifier == DIANA_ARM_QUAL_USER)
            return PrintText(pPrinter, "^");
        return DI_SUCCESS;
    case DIANA_A32_OPK_COPROC:
        // objdump: a number, no "p"
        pPrinter->commentValue = (DI_INT64)pOp->value.imm;
        return PrintSigned(pPrinter, (DI_INT64)pOp->value.imm);
    case DIANA_A32_OPK_CREG:
        DI_CHECK(PrintText(pPrinter, "cr"));
        return PrintSigned(pPrinter, (DI_INT64)pOp->value.imm);
    case DIANA_A32_OPK_SYSM:
        return PrintSysm(pPrinter, (DI_UINT32)pOp->value.imm);
    case DIANA_A32_OPK_PSRMASK:
        {
            DI_UINT32 value = (DI_UINT32)pOp->value.imm;
            DI_CHECK(PrintText(pPrinter, (value & 0x10) ? "SPSR_" : "CPSR_"));
            if (value & 8)
                DI_CHECK(PrintText(pPrinter, "f"));
            if (value & 4)
                DI_CHECK(PrintText(pPrinter, "s"));
            if (value & 2)
                DI_CHECK(PrintText(pPrinter, "x"));
            if (value & 1)
                DI_CHECK(PrintText(pPrinter, "c"));
            return DI_SUCCESS;
        }
    case DIANA_A32_OPK_BARRIER:
        return PrintBarrier(pPrinter, (DI_UINT32)pOp->value.imm);
    case DIANA_A32_OPK_IFLAGS:
        if (pOp->value.imm & 4)
            DI_CHECK(PrintText(pPrinter, "a"));
        if (pOp->value.imm & 2)
            DI_CHECK(PrintText(pPrinter, "i"));
        if (pOp->value.imm & 1)
            DI_CHECK(PrintText(pPrinter, "f"));
        return DI_SUCCESS;
    case DIANA_A32_OPK_ENDIAN:
        return PrintText(pPrinter, pOp->value.imm ? "be" : "le");
    case DIANA_A32_OPK_BFWIDTH:
        return PrintImm(pPrinter, (DI_INT64)pOp->value.imm);
    case DIANA_A32_OPK_SREG:
        // the second of a pair (VMOV Sm, Sm1): objdump prints s32 after s31
        if (pPrinter->pCmd->operands[index].param && index &&
            Op(pPrinter, index - 1)->value.recognizedRegister == reg_A32_S0 + 31)
            return PrintText(pPrinter, "s32");
        return PrintFpRegister(pPrinter, pOp->value.recognizedRegister);
    case DIANA_A32_OPK_DREG:
        return PrintFpRegister(pPrinter, pOp->value.recognizedRegister);
    case DIANA_A32_OPK_FPIMM:
        return PrintFpImm(pPrinter, index);
    case DIANA_A32_OPK_FBITS:
        DI_CHECK(PrintText(pPrinter, "#"));
        return PrintSigned(pPrinter, (DI_INT64)pOp->value.imm);
    case DIANA_A32_OPK_SREGLIST:
    case DIANA_A32_OPK_DREGLIST:
        return PrintFpList(pPrinter, pOp);
    case DIANA_A32_OPK_FPSYSREG:
        return PrintFpSysreg(pPrinter, (DI_UINT32)pOp->value.imm);
    case DIANA_A32_OPK_QREG:
        return PrintFpRegister(pPrinter, pOp->value.recognizedRegister);
    case DIANA_A32_OPK_DSCALAR:
        DI_CHECK(PrintFpRegister(pPrinter, pOp->value.recognizedRegister));
        DI_CHECK(PrintText(pPrinter, "["));
        DI_CHECK(PrintSigned(pPrinter, pOp->arm.lane));
        return PrintText(pPrinter, "]");
    case DIANA_A32_OPK_NEONIMM:
        return PrintNeonImm(pPrinter, index);
    case DIANA_A32_OPK_ROTATE:
        DI_CHECK(PrintText(pPrinter, "#"));
        return PrintSigned(pPrinter, (DI_INT64)pOp->value.imm);
    case DIANA_A32_OPK_VLIST:
        return PrintVList(pPrinter, pOp);
    case DIANA_A32_OPK_VMEM:
        return PrintVMem(pPrinter, pOp);
    }
    return DI_ERROR;
}

static int PrintOperand(DianaA32Printer * pPrinter, int index)
{
    DI_CHECK(PrintSeparator(pPrinter));
    return PrintOperandValue(pPrinter, index);
}

static int PrintOperandsFrom(DianaA32Printer * pPrinter, int first)
{
    int i = first;
    for (; i < pPrinter->pResult->iLinkedOpCount; ++i)
    {
        DI_CHECK(PrintOperand(pPrinter, i));
    }
    return DI_SUCCESS;
}

static int PrintGeneric(DianaA32Printer * pPrinter)
{
    DI_CHECK(PrintGroupMnemonic(pPrinter));
    return PrintOperandsFrom(pPrinter, 0);
}

static int PrintComment(DianaA32Printer * pPrinter)
{
    if (pPrinter->commentValue > 32 || pPrinter->commentValue < -16)
    {
        DI_CHECK(PrintText(pPrinter, " @ "));
        return PrintHex(pPrinter, (DI_UINT64)pPrinter->commentValue & 0xFFFFFFFFu, 0);
    }
    return DI_SUCCESS;
}

// T32 branches in an IT block, objdump's diagnostics
static int PrintBranchInIt(DianaA32Printer * pPrinter)
{
    if (pPrinter->pResult->iPrefix & DIANA_A32_PREFIX_NOT_LAST)
        return PrintText(pPrinter, " @ unpredictable branch in IT block");
    return DI_SUCCESS;
}

// a B<c> or an IT in an IT block: "@ unpredictable <IT:cond>"
static int PrintCondBranchInIt(DianaA32Printer * pPrinter)
{
    if (!InItBlock(pPrinter))
        return DI_SUCCESS;
    DI_CHECK(PrintText(pPrinter, " @ unpredictable <IT:"));
    DI_CHECK(PrintText(pPrinter, g_conditions[(pPrinter->pResult->iPrefix >> DIANA_A32_PREFIX_IT_SHIFT) & 15]));
    return PrintText(pPrinter, ">");
}

// ------------------------------------------------------------------ aliases
static int PrintUndefined(DianaA32Printer * pPrinter)
{
    int is16 = pPrinter->pCmd->size == 2;
    DI_UINT32 value = is16 ? pPrinter->word >> 16 : pPrinter->word;
    if (pPrinter->pCmd->header.m_pGroupInfo->m_commandId == (DianaCommandId)diana_a32_cmd_inst)
    {
        // objdump: no instruction in Arm's spec
        DI_CHECK(PrintText(pPrinter, "@ <UNDEFINED> instruction: "));
        return PrintHex(pPrinter, value, is16 ? 4 : 8);
    }
    // valid, but not decoded or not in the profile: an assembler can read it back
    DI_CHECK(PrintText(pPrinter, !pPrinter->isThumb ? ".inst" : is16 ? ".inst.n" : ".inst.w"));
    DI_CHECK(PrintSeparator(pPrinter));
    return PrintHex(pPrinter, value, is16 ? 4 : 8);
}

// mov with a shift is lsl, lsr, asr, ror, rrx; mov r0, r0 is nop
static int PrintMovShift(DianaA32Printer * pPrinter, int setFlags)
{
    const DianaLinkedOperand * pSource = Op(pPrinter, 1);
    const DianaArmOperandExt * pExt = &pSource->arm;
    char name[8];
    const char * pBase = 0;
    int i = 0;

    if (pExt->shiftType == DIANA_ARM_SHIFT_NONE)
    {
        if (!setFlags && !pPrinter->isThumb && pPrinter->word == 0xE1A00000)
            return PrintText(pPrinter, "nop @ (mov r0, r0)");
        DI_CHECK(PrintGroupMnemonic(pPrinter));
        DI_CHECK(PrintOperand(pPrinter, 0));
        DI_CHECK(PrintSeparator(pPrinter));
        return PrintRegister(pPrinter, pSource->value.recognizedRegister);
    }
    pBase = pExt->shiftType == DIANA_ARM_SHIFT_RRX ? "rrx" : g_shifts[(pExt->shiftType - DIANA_ARM_SHIFT_LSL) & 3];
    for (; pBase[i]; ++i)
        name[i] = pBase[i];
    if (setFlags)
        name[i++] = 's';
    name[i] = 0;
    // T32 16-bit: s outside an IT block
    DI_CHECK(PrintNameMnemonic(pPrinter, name));
    DI_CHECK(PrintOperand(pPrinter, 0));
    DI_CHECK(PrintSeparator(pPrinter));
    DI_CHECK(PrintRegister(pPrinter, pSource->value.recognizedRegister));
    if (pExt->shiftType == DIANA_ARM_SHIFT_RRX)
        return DI_SUCCESS;
    DI_CHECK(PrintText(pPrinter, ", "));
    if (pExt->qualifier == DIANA_ARM_QUAL_SHIFT_REG)
        return PrintText(pPrinter, g_registers[pExt->shiftAmount & 15]);
    return PrintImm(pPrinter, pExt->shiftAmount);
}

// stmdb sp!, {...} is push, ldm sp!, {...} is pop; one register: stmfd, ldmfd
static int PrintStackMultiple(DianaA32Printer * pPrinter, const char * pName, const char * pStack, const char * pSingle)
{
    const DianaLinkedOperand * pBase = Op(pPrinter, 0);
    // not with ^ (user registers, exception return)
    if (!pPrinter->isThumb && pBase->value.recognizedRegister == reg_A32_SP && pBase->arm.writeback &&
        Op(pPrinter, 1)->arm.qualifier != DIANA_ARM_QUAL_USER)
    {
        if (Op(pPrinter, 1)->arm.regCount == 1)
            pName = pSingle;
        else
        {
            DI_CHECK(PrintMnemonic(pPrinter, pStack));
            return PrintOperand(pPrinter, 1);
        }
    }
    DI_CHECK(PrintMnemonic(pPrinter, pName));
    return PrintOperandsFrom(pPrinter, 0);
}

// it, itt, ite...: the t/e pattern from the mask, then the first condition
static int PrintIt(DianaA32Printer * pPrinter)
{
    DI_UINT32 given = (pPrinter->word >> 16) & 0xFF;
    DI_UINT32 tmp = 0;
    DI_CHECK(PrintText(pPrinter, "it"));
    for (tmp = given << 1; tmp & 0xF; tmp <<= 1)
        DI_CHECK(PrintText(pPrinter, ((given ^ tmp) & 0x10) ? "e" : "t"));
    DI_CHECK(PrintSeparator(pPrinter));
    DI_CHECK(PrintText(pPrinter, g_conditions[given >> 4]));
    // an IT in an IT block
    return PrintCondBranchInIt(pPrinter);
}

static int PrintBranch(DianaA32Printer * pPrinter)
{
    DI_CHECK(PrintGeneric(pPrinter));
    if (!pPrinter->isThumb)
        return DI_SUCCESS;
    if (pPrinter->pCmd->condLsb != DIANA_A32_NO_COND)
        return PrintCondBranchInIt(pPrinter);
    return PrintBranchInIt(pPrinter);
}

// a list of operands from the first to the last index, skipping one (-1: none)
static int PrintOperandsSkipping(DianaA32Printer * pPrinter, const char * pName, int skip)
{
    int i = 0;
    DI_CHECK(PrintMnemonic(pPrinter, pName));
    for (; i < pPrinter->pResult->iLinkedOpCount; ++i)
    {
        if (i != skip)
            DI_CHECK(PrintOperand(pPrinter, i));
    }
    return DI_SUCCESS;
}

// hex without "#": bkpt 0x0000, hlt 0x0000, svc 0x00000000
static int PrintHexOperand(DianaA32Printer * pPrinter, const char * pName, int digits)
{
    DI_CHECK(PrintMnemonic(pPrinter, pName));
    DI_CHECK(PrintSeparator(pPrinter));
    return PrintHex(pPrinter, Op(pPrinter, 0)->value.imm, digits);
}

// decimal without "#", a comment candidate: hvc 4660 @ 0x1234
static int PrintBareDecimal(DianaA32Printer * pPrinter, DI_UINT64 value)
{
    pPrinter->commentValue = (DI_INT64)value;
    return PrintSigned(pPrinter, (DI_INT64)value);
}

// mcr 15, 0, r0, cr7, cr10, {4}: no "#", opc2 in braces; mrc with r15 is APSR_nzcv
static int PrintCoprocessorMove(DianaA32Printer * pPrinter)
{
    int i = 0;
    DI_CHECK(PrintGroupMnemonic(pPrinter));
    for (; i < pPrinter->pResult->iLinkedOpCount; ++i)
    {
        const DianaLinkedOperand * pOp = Op(pPrinter, i);
        DI_CHECK(PrintSeparator(pPrinter));
        if (pOp->arm.opKind == DIANA_A32_OPK_IMM)
        {
            int last = i + 1 == pPrinter->pResult->iLinkedOpCount;
            if (last)
                DI_CHECK(PrintText(pPrinter, "{"));
            DI_CHECK(PrintBareDecimal(pPrinter, pOp->value.imm));
            if (last)
                DI_CHECK(PrintText(pPrinter, "}"));
            continue;
        }
        if (pOp->arm.opKind == DIANA_A32_OPK_REG && pOp->value.recognizedRegister == reg_A32_PC &&
            pPrinter->pCmd->header.m_pGroupInfo->m_commandId == (DianaCommandId)diana_a32_cmd_mrc)
        {
            DI_CHECK(PrintText(pPrinter, "APSR_nzcv"));
            continue;
        }
        DI_CHECK(PrintOperandValue(pPrinter, i));
    }
    return DI_SUCCESS;
}

// objdump's coprocessor %A (LDC, STC, VLDR, VSTR): [r0, #-8]!, [r0, #-0], [r0], #8, [r0], {5}; with pc the address
// is the comment, pc + 8 in A32 and Align(pc, 4) + 4 in T32
static int PrintCoprocessorAddress(DianaA32Printer * pPrinter, int unindexed)
{
    int count = pPrinter->pResult->iLinkedOpCount;
    const DianaLinkedOperand * pMem = Op(pPrinter, unindexed ? count - 2 : count - 1);
    DI_INT64 offset = (DI_INT64)pMem->value.rmIndex.dispValue;
    int negative = pMem->arm.qualifier == DIANA_ARM_QUAL_NEGATIVE;
    int isPc = pMem->value.rmIndex.reg == reg_A32_PC;
    DI_UINT64 pc = pPrinter->pContext->instructionRIP;
    DI_CHECK(PrintSeparator(pPrinter));
    DI_CHECK(PrintText(pPrinter, "["));
    DI_CHECK(PrintRegister(pPrinter, pMem->value.rmIndex.reg));
    pPrinter->commentValue = 0;
    if (unindexed)
    {
        DI_CHECK(PrintText(pPrinter, "], {"));
        DI_CHECK(PrintBareDecimal(pPrinter, Op(pPrinter, count - 1)->value.imm));
        return PrintText(pPrinter, "}");
    }
    if (pMem->arm.writeback == DIANA_ARM_WRITEBACK_POST)
    {
        DI_CHECK(PrintText(pPrinter, "]"));
        if (offset || negative)
        {
            DI_CHECK(PrintText(pPrinter, offset ? ", #" : ", #-0"));
            if (offset)
                DI_CHECK(PrintSigned(pPrinter, offset));
        }
    }
    else
    if (offset)
    {
        DI_CHECK(PrintText(pPrinter, ", #"));
        DI_CHECK(PrintSigned(pPrinter, offset));
        DI_CHECK(PrintText(pPrinter, pMem->arm.writeback == DIANA_ARM_WRITEBACK_PRE ? "]!" : "]"));
    }
    else
    {
        DI_CHECK(PrintText(pPrinter, negative ? ", #-0]" : "]"));
    }
    if (!isPc)
    {
        pPrinter->commentValue = offset;
        return DI_SUCCESS;
    }
    DI_CHECK(PrintText(pPrinter, " @ "));
    return PrintAddress(pPrinter, pc + (pPrinter->isThumb ? 4 - (pc & 3) : 8) + offset);
}

static int PrintCoprocessorMemory(DianaA32Printer * pPrinter)
{
    int count = pPrinter->pResult->iLinkedOpCount;
    int unindexed = Op(pPrinter, count - 1)->arm.opKind == DIANA_A32_OPK_IMM;
    DI_CHECK(PrintGroupMnemonic(pPrinter));
    DI_CHECK(PrintSeparator(pPrinter));
    DI_CHECK(PrintBareDecimal(pPrinter, Op(pPrinter, 0)->value.imm));
    DI_CHECK(PrintOperand(pPrinter, 1));
    return PrintCoprocessorAddress(pPrinter, unindexed);
}

// VLDR, VSTR: the register, then the %A address
static int PrintVfpMemory(DianaA32Printer * pPrinter)
{
    DI_CHECK(PrintGroupMnemonic(pPrinter));
    DI_CHECK(PrintOperand(pPrinter, 0));
    return PrintCoprocessorAddress(pPrinter, 0);
}

// VLDM, VSTM: vldmia, vpush/vpop for sp with writeback (not FLDMX); FLDMX is deprecated
static int PrintVfpMultiple(DianaA32Printer * pPrinter, const char * pName, const char * pStack)
{
    const DianaLinkedOperand * pBase = Op(pPrinter, 0);
    if (pStack && pBase->value.recognizedRegister == reg_A32_SP && pBase->arm.writeback)
    {
        DI_CHECK(PrintMnemonic(pPrinter, pStack));
        return PrintOperandsFrom(pPrinter, 1);
    }
    DI_CHECK(PrintMnemonic(pPrinter, pName));
    DI_CHECK(PrintOperandsFrom(pPrinter, 0));
    if (IsFldmx(pPrinter))
        return PrintText(pPrinter, " @ Deprecated");
    return DI_SUCCESS;
}

// VMRS APSR_nzcv, fpscr; fpinst, fpinst2 are "@ Impl def"
static int PrintVmrs(DianaA32Printer * pPrinter, int isRead)
{
    DI_UINT32 reg = (pPrinter->word >> 16) & 15;
    DI_CHECK(PrintGroupMnemonic(pPrinter));
    if (isRead && reg == 1 && Op(pPrinter, 0)->value.recognizedRegister == reg_A32_PC)
    {
        DI_CHECK(PrintSeparator(pPrinter));
        DI_CHECK(PrintText(pPrinter, "APSR_nzcv"));
        DI_CHECK(PrintOperandsFrom(pPrinter, 1));
    }
    else
        DI_CHECK(PrintOperandsFrom(pPrinter, 0));
    if (reg == 9 || reg == 10)
        return PrintText(pPrinter, " @ Impl def");
    return DI_SUCCESS;
}

// A32 ADR: add/sub rd, pc, #imm in objdump's modified immediate form
static int PrintA32Adr(DianaA32Printer * pPrinter)
{
    DianaLinkedOperand imm;
    int subtract = (pPrinter->pCmd->operands[1].flags & DIANA_A32_OPF_SUBTRACT) != 0;
    DI_UINT32 imm12 = pPrinter->word & 0xFFF;
    DIANA_MEMSET(&imm, 0, sizeof(imm));
    imm.arm.shiftAmount = (DI_UINT8)((imm12 >> 8) * 2);
    imm.value.imm = imm.arm.shiftAmount ? ((imm12 & 0xFF) >> imm.arm.shiftAmount) |
                                          ((imm12 & 0xFF) << (32 - imm.arm.shiftAmount)) : (imm12 & 0xFF);
    imm.value.imm &= 0xFFFFFFFFu;
    DI_CHECK(PrintMnemonic(pPrinter, subtract ? "sub" : "add"));
    DI_CHECK(PrintOperand(pPrinter, 0));
    DI_CHECK(PrintSeparator(pPrinter));
    DI_CHECK(PrintText(pPrinter, "pc, "));
    return PrintModImm(pPrinter, &imm);
}

// rfe and srs always name the mode: rfeia, srsia
static int PrintWithMode(DianaA32Printer * pPrinter, const char * pName)
{
    DI_CHECK(PrintMnemonic(pPrinter, pName));
    return PrintOperandsFrom(pPrinter, 0);
}

// push {r0} @ (str r0, [sp, #-4]!), pop {r0} @ (ldr r0, [sp], #4): a single register through LDR/STR
static int PrintSingleStack(DianaA32Printer * pPrinter, int isPush, int * pHandled)
{
    char cond[8];
    int c = GetCondition(pPrinter);
    const DianaLinkedOperand * pMem = Op(pPrinter, 1);
    *pHandled = 0;
    if (pMem->value.rmIndex.reg != reg_A32_SP || pMem->value.rmIndex.indexed_reg != reg_none)
        return DI_SUCCESS;
    if (isPush ? (pMem->arm.writeback != DIANA_ARM_WRITEBACK_PRE || (DI_INT64)pMem->value.rmIndex.dispValue != -4)
               : (pMem->arm.writeback != DIANA_ARM_WRITEBACK_POST || pMem->value.rmIndex.dispValue != 4))
        return DI_SUCCESS;
    *pHandled = 1;
    cond[0] = 0;
    if (c != DIANA_A32_COND_AL)
    {
        cond[0] = g_conditions[c][0];
        cond[1] = g_conditions[c][1];
        cond[2] = 0;
    }
    DI_CHECK(PrintMnemonic(pPrinter, isPush ? "push" : "pop"));
    DI_CHECK(PrintSeparator(pPrinter));
    DI_CHECK(PrintText(pPrinter, "{"));
    DI_CHECK(PrintOperandValue(pPrinter, 0));
    DI_CHECK(PrintText(pPrinter, isPush ? "} @ (str" : "} @ (ldr"));
    DI_CHECK(PrintText(pPrinter, cond));
    DI_CHECK(PrintText(pPrinter, " "));
    DI_CHECK(PrintOperandValue(pPrinter, 0));
    DI_CHECK(PrintText(pPrinter, isPush ? ", [sp, #-4]!)" : ", [sp], #4)"));
    pPrinter->commentValue = 0;
    return DI_SUCCESS;
}

// ldrex r12: objdump prints the register number, not ip
static int PrintRawRegister(DianaA32Printer * pPrinter, int index)
{
    DI_CHECK(PrintSeparator(pPrinter));
    DI_CHECK(PrintText(pPrinter, "r"));
    return PrintSigned(pPrinter, OpRegIndex(pPrinter, index));
}

// T32 16-bit lsls r0, r1 (a shift by a register: Rdm, Rs)
static int PrintThumbShiftByRegister(DianaA32Printer * pPrinter)
{
    const DianaArmOperandExt * pExt = &Op(pPrinter, 1)->arm;
    char name[8];
    const char * pBase = g_shifts[(pExt->shiftType - DIANA_ARM_SHIFT_LSL) & 3];
    int i = 0;
    for (; pBase[i]; ++i)
        name[i] = pBase[i];
    name[i] = 0;
    DI_CHECK(PrintNameMnemonic(pPrinter, name));
    DI_CHECK(PrintOperand(pPrinter, 0));
    DI_CHECK(PrintSeparator(pPrinter));
    return PrintText(pPrinter, g_registers[pExt->shiftAmount & 15]);
}

// T32 addw, subw: ADD/SUB with a 12-bit immediate, also ADR T2/T3 (addw r0, pc, #4)
static int PrintWideImmediate(DianaA32Printer * pPrinter, const char * pName)
{
    DI_CHECK(PrintMnemonic(pPrinter, pName));
    DI_CHECK(PrintOperand(pPrinter, 0));
    DI_CHECK(PrintOperand(pPrinter, 1));
    return PrintOperand(pPrinter, 2);
}

static int PrintThumbAlias(DianaA32Printer * pPrinter, DianaA32CommandId id, int * pHandled)
{
    int is16 = pPrinter->pCmd->size == 2;
    *pHandled = 1;
    switch (id)
    {
    case diana_a32_cmd_mov:
    case diana_a32_cmd_movs:
        if (is16 && pPrinter->pCmd->operands[1].kind == DIANA_A32_OPK_SHREG_REG)
            return PrintThumbShiftByRegister(pPrinter);
        // 32-bit: mov.w r0, r1, lsl #2 (no alias), the shift by a register is lsl.w r0, r1, r2
        if (!is16 && pPrinter->pCmd->operands[1].kind == DIANA_A32_OPK_SHREG)
        {
            DI_CHECK(PrintGroupMnemonic(pPrinter));
            return PrintOperandsFrom(pPrinter, 0);
        }
        break;
    case diana_a32_cmd_rsb:
        // 16-bit rsb r0, r1, #0 is negs r0, r1
        if (is16)
        {
            DI_CHECK(PrintNameMnemonic(pPrinter, "neg"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            return PrintOperand(pPrinter, 1);
        }
        break;
    case diana_a32_cmd_mul:
        // 16-bit muls r0, r1: the destination is not repeated
        if (is16)
        {
            DI_CHECK(PrintGroupMnemonic(pPrinter));
            DI_CHECK(PrintOperand(pPrinter, 0));
            return PrintOperand(pPrinter, 1);
        }
        break;
    case diana_a32_cmd_svc:
        // svc 28: decimal without #
        DI_CHECK(PrintMnemonic(pPrinter, "svc"));
        DI_CHECK(PrintSeparator(pPrinter));
        return PrintBareDecimal(pPrinter, Op(pPrinter, 0)->value.imm);
    case diana_a32_cmd_stm:
        return PrintOperandsSkipping(pPrinter, "stmia", -1);
    case diana_a32_cmd_ldm:
        return PrintOperandsSkipping(pPrinter, "ldmia", -1);
    case diana_a32_cmd_eret:
        // objdump: subs pc, lr, #0
        DI_CHECK(PrintMnemonic(pPrinter, "subs"));
        DI_CHECK(PrintSeparator(pPrinter));
        return PrintText(pPrinter, "pc, lr, #0");
    case diana_a32_cmd_add:
    case diana_a32_cmd_sub:
        if (!is16 && pPrinter->pResult->iLinkedOpCount == 3 && Op(pPrinter, 2)->arm.opKind == DIANA_A32_OPK_IMM)
            return PrintWideImmediate(pPrinter, id == diana_a32_cmd_add ? "addw" : "subw");
        break;
    case diana_a32_cmd_adr:
        if (!is16)
        {
            // adr.w: addw/subw rd, pc, #imm12
            int subtract = (pPrinter->pCmd->operands[1].flags & DIANA_A32_OPF_SUBTRACT) != 0;
            DI_UINT32 imm = ((pPrinter->word >> 15) & 0x800) | ((pPrinter->word >> 4) & 0x700) | (pPrinter->word & 0xFF);
            DI_CHECK(PrintMnemonic(pPrinter, subtract ? "subw" : "addw"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintSeparator(pPrinter));
            DI_CHECK(PrintText(pPrinter, "pc, "));
            return PrintImm(pPrinter, imm);
        }
        break;
    case diana_a32_cmd_bfi:
    case diana_a32_cmd_bfc:
        {
            // a width of 0 or less: objdump prints it as an unsigned number
            int count = pPrinter->pResult->iLinkedOpCount;
            const DianaLinkedOperand * pWidth = Op(pPrinter, count - 1);
            if ((DI_INT64)pWidth->value.imm <= 0)
            {
                DI_CHECK(PrintOperandsSkipping(pPrinter, pPrinter->pCmd->header.m_pGroupInfo->m_pName, count - 1));
                DI_CHECK(PrintSeparator(pPrinter));
                DI_CHECK(PrintImm(pPrinter, (DI_INT64)(DI_UINT32)pWidth->value.imm));
                pPrinter->commentValue = 0;
                return DI_SUCCESS;
            }
        }
        break;
    default:
        break;
    }
    *pHandled = 0;
    return DI_SUCCESS;
}

static int PrintAlias(DianaA32Printer * pPrinter, DianaA32CommandId id, int * pHandled)
{
    *pHandled = 1;
    if (!pPrinter->isThumb)
    {
        switch (id)
        {
        case diana_a32_cmd_adr:
            return PrintA32Adr(pPrinter);
        case diana_a32_cmd_ldrd:
        case diana_a32_cmd_strd:
        case diana_a32_cmd_ldrexd:
            // objdump leaves out Rt2
            return PrintOperandsSkipping(pPrinter, pPrinter->pCmd->header.m_pGroupInfo->m_pName, 1);
        case diana_a32_cmd_strexd:
            return PrintOperandsSkipping(pPrinter, "strexd", 2);
        case diana_a32_cmd_ldrex:
            DI_CHECK(PrintMnemonic(pPrinter, "ldrex"));
            DI_CHECK(PrintRawRegister(pPrinter, 0));
            return PrintOperandsFrom(pPrinter, 1);
        case diana_a32_cmd_svc:
            return PrintHexOperand(pPrinter, "svc", 8);
        case diana_a32_cmd_hvc:
        case diana_a32_cmd_smc:
            DI_CHECK(PrintGroupMnemonic(pPrinter));
            DI_CHECK(PrintSeparator(pPrinter));
            return PrintBareDecimal(pPrinter, Op(pPrinter, 0)->value.imm);
        case diana_a32_cmd_str:
            DI_CHECK(PrintSingleStack(pPrinter, 1, pHandled));
            if (*pHandled)
                return DI_SUCCESS;
            *pHandled = 1;
            break;
        case diana_a32_cmd_ldr:
            DI_CHECK(PrintSingleStack(pPrinter, 0, pHandled));
            if (*pHandled)
                return DI_SUCCESS;
            *pHandled = 1;
            break;
        case diana_a32_cmd_nop:
            // nop {0}: the hint number in braces
            DI_CHECK(PrintMnemonic(pPrinter, "nop"));
            DI_CHECK(PrintSeparator(pPrinter));
            DI_CHECK(PrintText(pPrinter, "{"));
            DI_CHECK(PrintBareDecimal(pPrinter, pPrinter->word & 0xFF));
            return PrintText(pPrinter, "}");
        case diana_a32_cmd_stm:
            // objdump: stm without writeback and ^, else stmia
            if (Op(pPrinter, 0)->arm.writeback || Op(pPrinter, 1)->arm.qualifier == DIANA_ARM_QUAL_USER)
                return PrintOperandsSkipping(pPrinter, "stmia", -1);
            break;
        case diana_a32_cmd_cpsie:
        case diana_a32_cmd_cpsid:
            // objdump: cpsie/cpsid with a mode field that is not 0 (no M) print as cps #mode
            if (pPrinter->pResult->iLinkedOpCount == 1 && (pPrinter->word & 0x3F))
            {
                DI_CHECK(PrintMnemonic(pPrinter, "cps"));
                DI_CHECK(PrintSeparator(pPrinter));
                return PrintImm(pPrinter, pPrinter->word & 0x1F);
            }
            break;
        case diana_a32_cmd_bfi:
        case diana_a32_cmd_bfc:
            // a width of 0 or less: objdump prints the fields
            {
                int count = pPrinter->pResult->iLinkedOpCount;
                if ((DI_INT64)Op(pPrinter, count - 1)->value.imm <= 0)
                {
                    int i = 0;
                    DI_CHECK(PrintGroupMnemonic(pPrinter));
                    for (; i < count - 2; ++i)
                        DI_CHECK(PrintOperand(pPrinter, i));
                    DI_CHECK(PrintSeparator(pPrinter));
                    DI_CHECK(PrintText(pPrinter, "(invalid: "));
                    DI_CHECK(PrintSigned(pPrinter, (pPrinter->word >> 7) & 0x1F));
                    DI_CHECK(PrintText(pPrinter, ":"));
                    DI_CHECK(PrintSigned(pPrinter, (pPrinter->word >> 16) & 0x1F));
                    pPrinter->commentValue = 0;
                    return PrintText(pPrinter, ")");
                }
            }
            break;
        case diana_a32_cmd_ssat:
        case diana_a32_cmd_usat:
            // objdump prints the shift amount as encoded: asr #0
            if (Op(pPrinter, 2)->arm.shiftType == DIANA_ARM_SHIFT_ASR && !(pPrinter->word & 0xF80))
            {
                DI_CHECK(PrintOperandsSkipping(pPrinter, pPrinter->pCmd->header.m_pGroupInfo->m_pName, 2));
                DI_CHECK(PrintSeparator(pPrinter));
                DI_CHECK(PrintRegister(pPrinter, Op(pPrinter, 2)->value.recognizedRegister));
                return PrintText(pPrinter, ", asr #0");
            }
            break;
        case diana_a32_cmd_tsb:
            DI_CHECK(PrintMnemonic(pPrinter, "tsb"));
            DI_CHECK(PrintSeparator(pPrinter));
            return PrintText(pPrinter, "csync");
        case diana_a32_cmd_uxtab16:
            // objdump 2.42 prints this one rotation in capitals
            if ((pPrinter->word & 0xC00) == 0xC00)
            {
                DI_CHECK(PrintMnemonic(pPrinter, "uxtab16"));
                DI_CHECK(PrintOperand(pPrinter, 0));
                DI_CHECK(PrintOperand(pPrinter, 1));
                DI_CHECK(PrintSeparator(pPrinter));
                DI_CHECK(PrintRegister(pPrinter, Op(pPrinter, 2)->value.recognizedRegister));
                return PrintText(pPrinter, ", ROR #24");
            }
            break;
        default:
            break;
        }
    }
    if (pPrinter->isThumb)
    {
        DI_CHECK(PrintThumbAlias(pPrinter, id, pHandled));
        if (*pHandled)
            return DI_SUCCESS;
        *pHandled = 1;
    }
    switch (id)
    {
    case diana_a32_cmd_nop:
        // T32: the hints without a name (spec/thumb/hints.txt) are nop {n}, nop.w {n}; NOP itself is nop, nop.w
        if (pPrinter->isThumb)
        {
            DI_CHECK(PrintMnemonic(pPrinter, "nop"));
            if (!pPrinter->pResult->iLinkedOpCount)
                return DI_SUCCESS;
            DI_CHECK(PrintSeparator(pPrinter));
            DI_CHECK(PrintText(pPrinter, "{"));
            DI_CHECK(PrintBareDecimal(pPrinter, Op(pPrinter, 0)->value.imm));
            return PrintText(pPrinter, "}");
        }
        break;
    case diana_a32_cmd_vshl:
        // objdump prints VSHL by an immediate as .s8 (Arm: .i8)
        if (pPrinter->pCmd->operands[pPrinter->pResult->iLinkedOpCount - 1].kind == DIANA_A32_OPK_FBITS ||
            pPrinter->pCmd->operands[pPrinter->pResult->iLinkedOpCount - 1].kind == DIANA_A32_OPK_IMM)
        {
            static const char * names[4] = {".s8", ".s16", ".s32", ".s64"};
            const char * pType = DianaA32_GetTables()->pSuffixes[pPrinter->pCmd->suffix];
            int i = 0;
            for (; i < 4; ++i)
            {
                const char * pA = pType + 2;
                const char * pB = names[i] + 2;
                while (*pA && *pA == *pB)
                {
                    ++pA;
                    ++pB;
                }
                if (pType[1] == 'i' && !*pA && !*pB)
                    pPrinter->pSuffix = names[i];
            }
        }
        *pHandled = 0;
        return DI_SUCCESS;
    case diana_a32_cmd_mcr:
    case diana_a32_cmd_mrc:
    case diana_a32_cmd_mcr2:
    case diana_a32_cmd_mrc2:
    case diana_a32_cmd_mcrr:
    case diana_a32_cmd_mrrc:
    case diana_a32_cmd_mcrr2:
    case diana_a32_cmd_mrrc2:
    case diana_a32_cmd_cdp:
    case diana_a32_cmd_cdp2:
        return PrintCoprocessorMove(pPrinter);
    case diana_a32_cmd_ldc:
    case diana_a32_cmd_ldcl:
    case diana_a32_cmd_stc:
    case diana_a32_cmd_stcl:
    case diana_a32_cmd_ldc2:
    case diana_a32_cmd_ldc2l:
    case diana_a32_cmd_stc2:
    case diana_a32_cmd_stc2l:
        return PrintCoprocessorMemory(pPrinter);
    case diana_a32_cmd_vldr:
    case diana_a32_cmd_vstr:
        return PrintVfpMemory(pPrinter);
    case diana_a32_cmd_vldm:
        return PrintVfpMultiple(pPrinter, "vldmia", "vpop");
    case diana_a32_cmd_vstm:
        return PrintVfpMultiple(pPrinter, "vstmia", 0);
    case diana_a32_cmd_vstmdb:
        return PrintVfpMultiple(pPrinter, "vstmdb", "vpush");
    case diana_a32_cmd_vldmdb:
    case diana_a32_cmd_fldmiax:
    case diana_a32_cmd_fldmdbx:
    case diana_a32_cmd_fstmiax:
    case diana_a32_cmd_fstmdbx:
        return PrintVfpMultiple(pPrinter, pPrinter->pCmd->header.m_pGroupInfo->m_pName, 0);
    case diana_a32_cmd_vmrs:
        return PrintVmrs(pPrinter, 1);
    case diana_a32_cmd_vmsr:
        return PrintVmrs(pPrinter, 0);
    case diana_a32_cmd_bkpt:
    case diana_a32_cmd_hlt:
        return PrintHexOperand(pPrinter, pPrinter->pCmd->header.m_pGroupInfo->m_pName, 4);
    case diana_a32_cmd_dsb:
        if ((pPrinter->word & 0xF) == 12)
            return PrintMnemonic(pPrinter, "dfb");
        break;
    case diana_a32_cmd_rfe:
        return PrintWithMode(pPrinter, "rfeia");
    case diana_a32_cmd_srs:
        return PrintWithMode(pPrinter, "srsia");
    case diana_a32_cmd_mov:
        if (pPrinter->pCmd->operands[1].kind == DIANA_A32_OPK_SHREG ||
            pPrinter->pCmd->operands[1].kind == DIANA_A32_OPK_SHREG_REG)
            return PrintMovShift(pPrinter, 0);
        // the 16-bit immediate of MOV (A2, T3) is movw
        if (pPrinter->pCmd->operands[1].kind == DIANA_A32_OPK_IMM && pPrinter->pCmd->size == 4)
        {
            DI_CHECK(PrintMnemonic(pPrinter, "movw"));
            return PrintOperandsFrom(pPrinter, 0);
        }
        // T32 mov r8, r8
        if (pPrinter->isThumb && pPrinter->pCmd->size == 2 && pPrinter->word >> 16 == 0x46C0)
        {
            DI_CHECK(PrintMnemonic(pPrinter, "nop"));
            return PrintText(pPrinter, " @ (mov r8, r8)");
        }
        break;
    case diana_a32_cmd_movs:
        if (pPrinter->pCmd->operands[1].kind == DIANA_A32_OPK_SHREG ||
            pPrinter->pCmd->operands[1].kind == DIANA_A32_OPK_SHREG_REG)
            return PrintMovShift(pPrinter, 1);
        break;
    case diana_a32_cmd_stmdb:
        return PrintStackMultiple(pPrinter, "stmdb", "push", "stmfd");
    case diana_a32_cmd_ldm:
        return PrintStackMultiple(pPrinter, "ldm", "pop", "ldmfd");
    case diana_a32_cmd_it:
        return PrintIt(pPrinter);
    case diana_a32_cmd_adr:
        // T32: add r0, pc, #4 @ (adr r0, <target>)
        {
            const DianaLinkedOperand * pLabel = Op(pPrinter, 1);
            DI_UINT64 target = LabelTarget(pPrinter, pLabel);
            DI_UINT64 base = (pPrinter->pContext->instructionRIP + 4) & ~(DI_UINT64)3;
            DI_CHECK(PrintMnemonic(pPrinter, "add"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintSeparator(pPrinter));
            DI_CHECK(PrintText(pPrinter, "pc, #"));
            DI_CHECK(PrintSigned(pPrinter, (DI_INT64)(target - base)));
            DI_CHECK(PrintText(pPrinter, " @ (adr "));
            DI_CHECK(PrintOperandValue(pPrinter, 0));
            DI_CHECK(PrintText(pPrinter, ", "));
            DI_CHECK(PrintAddress(pPrinter, target));
            return PrintText(pPrinter, ")");
        }
    case diana_a32_cmd_b:
    case diana_a32_cmd_bl:
    case diana_a32_cmd_blx:
    case diana_a32_cmd_bx:
        return PrintBranch(pPrinter);
    case diana_a32_cmd_cbz:
    case diana_a32_cmd_cbnz:
        // never conditional: in an IT block objdump prints the plain name and marks it
        DI_CHECK(PrintText(pPrinter, pPrinter->pCmd->header.m_pGroupInfo->m_pName));
        DI_CHECK(PrintOperandsFrom(pPrinter, 0));
        return PrintCondBranchInIt(pPrinter);
    default:
        break;
    }
    *pHandled = 0;
    return DI_SUCCESS;
}

int DianaA32_TextOut(DianaTextOutputContext * pContext)
{
    DianaA32Printer printer;
    DianaA32CommandId id;
    const DianaParserResult * pResult = pContext->pResult;
    int handled = 0;

    printer.pContext = pContext;
    printer.pResult = pResult;
    printer.pCmd = DianaA32_GetCmdInfo(pResult);
    printer.operandsPrinted = 0;
    printer.commentValue = 0;
    printer.pSuffix = 0;
    if (!printer.pCmd)
        return DI_ERROR;
    printer.isThumb = (printer.pCmd->flags & DIANA_A32_CMDF_THUMB) != 0;
    if (!printer.isThumb)
    {
        printer.word = (DI_UINT32)pResult->bytes[0] | ((DI_UINT32)pResult->bytes[1] << 8) |
                       ((DI_UINT32)pResult->bytes[2] << 16) | ((DI_UINT32)pResult->bytes[3] << 24);
    }
    else
    {
        printer.word = ((DI_UINT32)pResult->bytes[0] | ((DI_UINT32)pResult->bytes[1] << 8)) << 16;
        if (printer.pCmd->size == 4)
            printer.word |= (DI_UINT32)pResult->bytes[2] | ((DI_UINT32)pResult->bytes[3] << 8);
    }

    pContext->flags |= DIANA_TEXT_OUTPUT_FLAG_C_HEX;

    id = (DianaA32CommandId)printer.pCmd->header.m_pGroupInfo->m_commandId;
    switch (id)
    {
    case diana_a32_cmd_inst:
    case diana_a32_cmd_inst_unsupported:
    case diana_a32_cmd_inst_disabled:
        return PrintUndefined(&printer);
    default:
        break;
    }
    DI_CHECK(PrintAlias(&printer, id, &handled));
    if (!handled)
        DI_CHECK(PrintGeneric(&printer));
    if (printer.isThumb && NoItCondition(&printer) == 2)
        DI_CHECK(PrintCondBranchInIt(&printer));
    return PrintComment(&printer);
}

#endif
