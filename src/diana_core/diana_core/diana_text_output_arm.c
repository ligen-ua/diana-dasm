// AArch64 text output: objdump/gdb style, with the preferred disassembly aliases from the ARM ARM
// The decoder returns raw instructions (orr, ubfm, csinc...), aliases (mov, lsl, cset...) are chosen here
#include "diana_arm.h"
#include "diana_text_output_masm.h"

#ifndef DIANA_NO_ARM

typedef struct _DianaA64Printer
{
    DianaTextOutputContext * pContext;
    const DianaParserResult * pResult;
    const DianaA64CmdInfo * pCmd;
    DI_UINT32 word;
    int operandsPrinted;
}DianaA64Printer;

static const char * g_conditions[16] =
{
    "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"
};

static const char * g_extends[8] =
{
    "uxtb", "uxth", "uxtw", "uxtx", "sxtb", "sxth", "sxtw", "sxtx"
};

static const char * g_shifts[] =
{
    "", "lsl", "lsr", "asr", "ror", "msl", "mul"
};

// ------------------------------------------------------------------ basic output
static int PrintText(DianaA64Printer * pPrinter, const char * pText)
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

static int PrintDecimal(DianaA64Printer * pPrinter, OPERAND_SIZE_SIGNED value)
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

// numbers that can be addresses go through pOpOutputFnc, so the user can annotate them
static int PrintNumber(DianaA64Printer * pPrinter, DI_UINT64 value)
{
    return pPrinter->pContext->pOpOutputFnc(pPrinter->pContext, value, 0);
}

static int PrintHexImm(DianaA64Printer * pPrinter, DI_UINT64 value)
{
    DI_CHECK(PrintText(pPrinter, "#"));
    return PrintNumber(pPrinter, value);
}

static int PrintDecimalImm(DianaA64Printer * pPrinter, OPERAND_SIZE_SIGNED value)
{
    DI_CHECK(PrintText(pPrinter, "#"));
    return PrintDecimal(pPrinter, value);
}

static int PrintMnemonic(DianaA64Printer * pPrinter, const char * pName)
{
    return PrintText(pPrinter, pName);
}

static int PrintSeparator(DianaA64Printer * pPrinter)
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

// ------------------------------------------------------------------ registers
static int FormatRegister(DianaUnifiedRegister reg, char * pBuffer)
{
    int index = 0;
    const char * pPrefix = 0;
    if (reg >= reg_A64_X0 && reg < reg_A64_XZR)
    {
        pPrefix = "x";
        index = reg - reg_A64_X0;
    }
    else
    if (reg >= reg_A64_W0 && reg < reg_A64_WZR)
    {
        pPrefix = "w";
        index = reg - reg_A64_W0;
    }
    else
    if (reg >= reg_A64_B0 && reg < reg_A64_Z0 + 32)
    {
        static const char * prefixes[] = {"b", "h", "s", "d", "q", "v", "z"};
        int block = (reg - reg_A64_B0) / 32;
        pPrefix = prefixes[block];
        index = (reg - reg_A64_B0) % 32;
    }
    else
    if (reg >= reg_A64_P0 && reg < reg_A64_ZA)
    {
        // p0..p15, pn0..pn15
        pPrefix = reg < reg_A64_PN0 ? "p" : "pn";
        index = (reg - reg_A64_P0) % 16;
    }
    else
    if (reg >= reg_A64_ZA0 && reg < reg_A64_ZA0 + 16)
    {
        pPrefix = "za";
        index = reg - reg_A64_ZA0;
    }
    if (pPrefix)
    {
        int length = 0;
        for (; pPrefix[length]; ++length)
            pBuffer[length] = pPrefix[length];
        FormatUnsigned(pBuffer + length, (DI_UINT64)index, 10);
        return DI_SUCCESS;
    }
    {
        const char * pName = 0;
        int i = 0;
        switch (reg)
        {
        case reg_A64_XZR: pName = "xzr"; break;
        case reg_A64_WZR: pName = "wzr"; break;
        case reg_A64_SP:  pName = "sp"; break;
        case reg_A64_WSP: pName = "wsp"; break;
        case reg_A64_PC:  pName = "pc"; break;
        case reg_A64_ZA:  pName = "za"; break;
        case reg_A64_ZT0: pName = "zt0"; break;
        default:
            pName = "unknown";
        }
        for (; pName[i]; ++i)
            pBuffer[i] = pName[i];
        pBuffer[i] = 0;
    }
    return DI_SUCCESS;
}

static int PrintRegister(DianaA64Printer * pPrinter, DianaUnifiedRegister reg)
{
    char buffer[16];
    DI_CHECK(FormatRegister(reg, buffer));
    return PrintText(pPrinter, buffer);
}

static int IsZeroRegister(DianaUnifiedRegister reg)
{
    return reg == reg_A64_XZR || reg == reg_A64_WZR;
}

static int IsSPRegister(DianaUnifiedRegister reg)
{
    return reg == reg_A64_SP || reg == reg_A64_WSP;
}

static int RegisterIndex(DianaUnifiedRegister reg)
{
    if (reg >= reg_A64_X0 && reg <= reg_A64_XZR)
        return reg - reg_A64_X0;
    if (reg >= reg_A64_W0 && reg <= reg_A64_WZR)
        return reg - reg_A64_W0;
    if (IsSPRegister(reg))
        return 31;
    return -1;
}

// ------------------------------------------------------------------ system names
typedef struct _DianaA64Name
{
    DI_UINT32 value;
    const char * pName;
}DianaA64Name;

// system register names, value = o0:op1:CRn:CRm:op2 (op0 = 2 + o0), sorted by value
typedef struct _DianaA64SysRegName
{
    DI_UINT16 value;
    DI_UINT16 readName;     // offset in g_sysRegNames, 0xFFFF = none
    DI_UINT16 writeName;
}DianaA64SysRegName;

#include "diana_text_output_arm_sysregs.inc"

static const char * FindName(const DianaA64Name * pNames, int count, DI_UINT32 value)
{
    int i = 0;
    for (; i < count; ++i)
    {
        if (pNames[i].value == value)
            return pNames[i].pName;
    }
    return 0;
}

// s<op0>_<op1>_c<CRn>_c<CRm>_<op2>
static int PrintGenericSysReg(DianaA64Printer * pPrinter, int op0, int op1, int crn, int crm, int op2)
{
    char buffer[8];
    DI_CHECK(PrintText(pPrinter, "s"));
    FormatUnsigned(buffer, (DI_UINT64)op0, 10);
    DI_CHECK(PrintText(pPrinter, buffer));
    DI_CHECK(PrintText(pPrinter, "_"));
    FormatUnsigned(buffer, (DI_UINT64)op1, 10);
    DI_CHECK(PrintText(pPrinter, buffer));
    DI_CHECK(PrintText(pPrinter, "_c"));
    FormatUnsigned(buffer, (DI_UINT64)crn, 10);
    DI_CHECK(PrintText(pPrinter, buffer));
    DI_CHECK(PrintText(pPrinter, "_c"));
    FormatUnsigned(buffer, (DI_UINT64)crm, 10);
    DI_CHECK(PrintText(pPrinter, buffer));
    DI_CHECK(PrintText(pPrinter, "_"));
    FormatUnsigned(buffer, (DI_UINT64)op2, 10);
    return PrintText(pPrinter, buffer);
}

static const char * FindSysRegName(DI_UINT32 value, int isWrite)
{
    int low = 0;
    int high = (int)(sizeof(g_sysRegs)/sizeof(g_sysRegs[0])) - 1;
    while (low <= high)
    {
        int middle = (low + high) / 2;
        const DianaA64SysRegName * pName = g_sysRegs + middle;
        if (pName->value == value)
        {
            DI_UINT16 offset = isWrite ? pName->writeName : pName->readName;
            return offset == 0xFFFF ? 0 : g_sysRegNames + offset;
        }
        if (pName->value < value)
            low = middle + 1;
        else
            high = middle - 1;
    }
    return 0;
}

// isWrite: msr names the register it writes, a few encodings have different read and write names
static int PrintSysReg(DianaA64Printer * pPrinter, DI_UINT32 value, int isWrite)
{
    const char * pName = FindSysRegName(value, isWrite);
    if (pName)
        return PrintText(pPrinter, pName);
    return PrintGenericSysReg(pPrinter, 2 + (int)(value >> 14), (int)(value >> 11) & 7, (int)(value >> 7) & 15, (int)(value >> 3) & 15, (int)value & 7);
}

static const char * g_barriers[16] =
{
    0, "oshld", "oshst", "osh", 0, "nshld", "nshst", "nsh",
    0, "ishld", "ishst", "ish", 0, "ld", "st", "sy"
};

static const char * GetPrefetchName(DI_UINT32 value)
{
    static const char * names[32] =
    {
        "pldl1keep", "pldl1strm", "pldl2keep", "pldl2strm", "pldl3keep", "pldl3strm", "pldslckeep", "pldslcstrm",
        "plil1keep", "plil1strm", "plil2keep", "plil2strm", "plil3keep", "plil3strm", "plislckeep", "plislcstrm",
        "pstl1keep", "pstl1strm", "pstl2keep", "pstl2strm", "pstl3keep", "pstl3strm", "pstslckeep", "pstslcstrm",
        0, 0, 0, 0, 0, 0, 0, 0
    };
    return names[value & 31];
}

// ------------------------------------------------------------------ operands
static int PrintShift(DianaA64Printer * pPrinter, const DianaArmOperandExt * pExt)
{
    DI_CHECK(PrintText(pPrinter, ", "));
    DI_CHECK(PrintText(pPrinter, g_shifts[pExt->shiftType]));
    DI_CHECK(PrintText(pPrinter, " "));
    return PrintDecimalImm(pPrinter, pExt->shiftAmount);
}

// is there an SP among the first two register operands (for the extended register "lsl" rule)
static int HasSPOperand(const DianaParserResult * pResult)
{
    int i = 0;
    for (; i < 2 && i < pResult->iLinkedOpCount; ++i)
    {
        if (pResult->linkedOperands[i].type == diana_register && IsSPRegister(pResult->linkedOperands[i].value.recognizedRegister))
            return 1;
    }
    return 0;
}

static int PrintAddress(DianaA64Printer * pPrinter, DI_UINT64 address)
{
    return PrintNumber(pPrinter, address);
}

static int PrintMemory(DianaA64Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    const DianaRmIndex * pIndex = &pOp->value.rmIndex;
    OPERAND_SIZE_SIGNED disp = (OPERAND_SIZE_SIGNED)pIndex->dispValue;

    if (pIndex->reg == reg_A64_PC)
    {
        // literal: the displacement is relative to the next instruction
        return PrintAddress(pPrinter, pPrinter->pContext->instructionRIP + 4 + pIndex->dispValue);
    }

    DI_CHECK(PrintText(pPrinter, "["));
    DI_CHECK(PrintRegister(pPrinter, pIndex->reg));
    if (pIndex->indexed_reg != reg_none && pOp->arm.writeback == DIANA_ARM_WRITEBACK_POST)
    {
        // SIMD structure load/store: [x0], x2
        DI_CHECK(PrintText(pPrinter, "], "));
        return PrintRegister(pPrinter, pIndex->indexed_reg);
    }
    if (pIndex->indexed_reg != reg_none)
    {
        DI_CHECK(PrintText(pPrinter, ", "));
        DI_CHECK(PrintRegister(pPrinter, pIndex->indexed_reg));
        if (pOp->arm.shiftType == DIANA_ARM_EXTEND_UXTX)
        {
            if (pOp->arm.hasAmount)
            {
                DI_CHECK(PrintText(pPrinter, ", lsl "));
                DI_CHECK(PrintDecimalImm(pPrinter, pOp->arm.shiftAmount));
            }
        }
        else
        {
            DI_CHECK(PrintText(pPrinter, ", "));
            DI_CHECK(PrintText(pPrinter, g_extends[pOp->arm.shiftType - DIANA_ARM_EXTEND_UXTB]));
            if (pOp->arm.hasAmount)
            {
                DI_CHECK(PrintText(pPrinter, " "));
                DI_CHECK(PrintDecimalImm(pPrinter, pOp->arm.shiftAmount));
            }
        }
        return PrintText(pPrinter, "]");
    }

    switch (pOp->arm.writeback)
    {
    case DIANA_ARM_WRITEBACK_POST:
        DI_CHECK(PrintText(pPrinter, "], "));
        return PrintDecimalImm(pPrinter, disp);
    case DIANA_ARM_WRITEBACK_PRE:
        DI_CHECK(PrintText(pPrinter, ", "));
        DI_CHECK(PrintDecimalImm(pPrinter, disp));
        return PrintText(pPrinter, "]!");
    }
    if (disp)
    {
        DI_CHECK(PrintText(pPrinter, ", "));
        DI_CHECK(PrintDecimalImm(pPrinter, disp));
    }
    return PrintText(pPrinter, "]");
}

// ------------------------------------------------------------------ FP and SIMD operands
// v1.4s, v1.s (an element, the index is printed separately)
static int PrintVectorRegister(DianaA64Printer * pPrinter, int number, int arrangement)
{
    char buffer[16];
    int length = 0;
    buffer[length++] = 'v';
    FormatUnsigned(buffer + length, (DI_UINT64)(number & 31), 10);
    while (buffer[length])
        ++length;
    buffer[length++] = '.';
    if (DIANA_ARM_ARR_HAS_COUNT(arrangement))
    {
        // v1.4s, and a group of elements: v2.4b[1]
        FormatUnsigned(buffer + length, (DI_UINT64)DIANA_ARM_ARR_COUNT(arrangement), 10);
        while (buffer[length])
            ++length;
    }
    buffer[length++] = "bhsdq"[DIANA_ARM_ARR_ELEMENT_LOG2(arrangement)];
    buffer[length] = 0;
    return PrintText(pPrinter, buffer);
}

static int PrintLane(DianaA64Printer * pPrinter, int lane)
{
    DI_CHECK(PrintText(pPrinter, "["));
    DI_CHECK(PrintDecimal(pPrinter, lane));
    return PrintText(pPrinter, "]");
}

// {v0.16b}, {v0.16b-v3.16b}, {v31.2d-v0.2d}, {v1.b-v4.b}[15]
static int PrintRegisterList(DianaA64Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    int first = pOp->value.recognizedRegister - reg_A64_V0;
    DI_CHECK(PrintText(pPrinter, "{"));
    DI_CHECK(PrintVectorRegister(pPrinter, first, pOp->arm.arrangement));
    if (pOp->arm.regCount > 1)
    {
        DI_CHECK(PrintText(pPrinter, "-"));
        DI_CHECK(PrintVectorRegister(pPrinter, first + pOp->arm.regCount - 1, pOp->arm.arrangement));
    }
    DI_CHECK(PrintText(pPrinter, "}"));
    if (DIANA_ARM_ARR_IS_ELEMENT(pOp->arm.arrangement))
        return PrintLane(pPrinter, pOp->arm.lane);
    return DI_SUCCESS;
}

// objdump prints FP immediates with printf("%.18e"). VFPExpandImm values are m/16 * 2^e with a small e,
// so the decimal expansion is exact: digits * 10^-k with no rounding
static int PrintFPImm(DianaA64Printer * pPrinter, DI_UINT64 bits)
{
    char digits[32];
    char buffer[48];
    int exponent = (int)((bits >> 52) & 0x7FF) - 1023 - 52;
    DI_UINT64 mantissa = (bits & ((1ULL << 52) - 1)) | (1ULL << 52);
    int fraction = 0, length = 0, count = 0, i = 0, decimalExponent = 0;

    if (!(bits & ~(1ULL << 63)))
    {
        mantissa = 0;
        exponent = 0;
    }
    for (; mantissa && !(mantissa & 1); mantissa >>= 1)
        ++exponent;
    // mantissa * 2^exponent = mantissa * 5^-exponent / 10^-exponent
    for (; exponent < 0; ++exponent, ++fraction)
        mantissa *= 5;
    for (; exponent > 0; --exponent)
        mantissa <<= 1;
    FormatUnsigned(digits, mantissa, 10);
    while (digits[count])
        ++count;
    decimalExponent = mantissa ? count - 1 - fraction : 0;

    buffer[length++] = '#';
    if (bits >> 63)
        buffer[length++] = '-';
    buffer[length++] = digits[0];
    buffer[length++] = '.';
    for (i = 1; i <= 18; ++i)
        buffer[length++] = i < count ? digits[i] : '0';
    buffer[length++] = 'e';
    buffer[length++] = decimalExponent < 0 ? '-' : '+';
    if (decimalExponent < 0)
        decimalExponent = -decimalExponent;
    buffer[length++] = (char)('0' + decimalExponent / 10);
    buffer[length++] = (char)('0' + decimalExponent % 10);
    buffer[length] = 0;
    return PrintText(pPrinter, buffer);
}

// ------------------------------------------------------------------ SVE and SME operands
// z1.s, p0.b, p0/m, z2 (no element size)
static int PrintScalableRegister(DianaA64Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    static const char * qualifiers[3] = {"", "/m", "/z"};
    char buffer[8];
    DI_CHECK(PrintRegister(pPrinter, pOp->value.recognizedRegister));
    if (DIANA_ARM_ARR_IS_SCALABLE(pOp->arm.arrangement))
    {
        buffer[0] = '.';
        buffer[1] = "bhsdq"[DIANA_ARM_ARR_ELEMENT_LOG2(pOp->arm.arrangement)];
        buffer[2] = 0;
        DI_CHECK(PrintText(pPrinter, buffer));
    }
    return PrintText(pPrinter, qualifiers[pOp->arm.qualifier % 3]);
}

// {z1.b}, {z0.s-z3.s}, {p2.h-p3.h}
static int PrintScalableList(DianaA64Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    DianaLinkedOperand entry = *pOp;
    int isP = pOp->value.recognizedRegister >= reg_A64_P0 && pOp->value.recognizedRegister < reg_A64_PN0;
    DianaUnifiedRegister base = isP ? reg_A64_P0 : reg_A64_Z0;
    int first = pOp->value.recognizedRegister - base;
    DI_CHECK(PrintText(pPrinter, "{"));
    DI_CHECK(PrintScalableRegister(pPrinter, &entry));
    if (pOp->arm.regStride > 1)
    {
        // strided: {z1.s, z9.s}
        int i = 1;
        for (; i < pOp->arm.regCount; ++i)
        {
            DI_CHECK(PrintText(pPrinter, ", "));
            entry.value.recognizedRegister = (DianaUnifiedRegister)(base + ((first + i * pOp->arm.regStride) & 31));
            DI_CHECK(PrintScalableRegister(pPrinter, &entry));
        }
        return PrintText(pPrinter, "}");
    }
    if (pOp->arm.regCount > 1)
    {
        DI_CHECK(PrintText(pPrinter, "-"));
        entry.value.recognizedRegister = (DianaUnifiedRegister)(base + ((first + pOp->arm.regCount - 1) & (isP ? 15 : 31)));
        DI_CHECK(PrintScalableRegister(pPrinter, &entry));
    }
    return PrintText(pPrinter, "}");
}

// SVE predicate patterns: vl1..vl8, vl16..vl256, mul4, mul3, all; the others print as #n
static int PrintPattern(DianaA64Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    static const char * names[32] =
    {
        "pow2", "vl1", "vl2", "vl3", "vl4", "vl5", "vl6", "vl7", "vl8", "vl16", "vl32", "vl64", "vl128", "vl256", 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "mul4", "mul3", "all"
    };
    const char * pName = names[pOp->value.imm & 31];
    if (pName)
    {
        DI_CHECK(PrintText(pPrinter, pName));
    }
    else
    {
        DI_CHECK(PrintDecimalImm(pPrinter, (OPERAND_SIZE_SIGNED)pOp->value.imm));
    }
    if (pOp->arm.shiftAmount != 1)
        return PrintShift(pPrinter, &pOp->arm);
    return DI_SUCCESS;
}

// a register of an SVE address: x0, sp, z1.d; p2.h (PSEL)
static int PrintSveAddressRegister(DianaA64Printer * pPrinter, DianaUnifiedRegister reg, int arrangement)
{
    char buffer[4];
    DI_CHECK(PrintRegister(pPrinter, reg));
    if (reg < reg_A64_Z0 || reg >= reg_A64_PN0)
        return DI_SUCCESS;
    buffer[0] = '.';
    buffer[1] = "bhsdq"[DIANA_ARM_ARR_ELEMENT_LOG2(arrangement)];
    buffer[2] = 0;
    return PrintText(pPrinter, buffer);
}

// [x0], [x0, #-2, mul vl], [x4, #37], [z1.d, #8], [x0, x1, lsl #3], [x0, z1.s, sxtw #2], [z1.d, z2.d, uxtw]
static int PrintSveMemory(DianaA64Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    const DianaRmIndex * pIndex = &pOp->value.rmIndex;
    DI_CHECK(PrintText(pPrinter, "["));
    DI_CHECK(PrintSveAddressRegister(pPrinter, pIndex->reg, pOp->arm.arrangement));
    if (pIndex->indexed_reg != reg_none)
    {
        DI_CHECK(PrintText(pPrinter, ", "));
        DI_CHECK(PrintSveAddressRegister(pPrinter, pIndex->indexed_reg, pOp->arm.arrangement));
        if (pOp->arm.shiftType != DIANA_ARM_SHIFT_LSL)
        {
            DI_CHECK(PrintText(pPrinter, pOp->arm.shiftType == DIANA_ARM_EXTEND_SXTW ? ", sxtw" : ", uxtw"));
            if (pOp->arm.hasAmount)
            {
                DI_CHECK(PrintText(pPrinter, " "));
                DI_CHECK(PrintDecimalImm(pPrinter, pOp->arm.shiftAmount));
            }
        }
        else
        if (pOp->arm.hasAmount)
        {
            DI_CHECK(PrintText(pPrinter, ", lsl "));
            DI_CHECK(PrintDecimalImm(pPrinter, pOp->arm.shiftAmount));
        }
        return PrintText(pPrinter, "]");
    }
    if (pIndex->dispValue)
    {
        DI_CHECK(PrintText(pPrinter, ", "));
        DI_CHECK(PrintDecimalImm(pPrinter, (OPERAND_SIZE_SIGNED)pIndex->dispValue));
        if (pOp->arm.shiftType == DIANA_ARM_SHIFT_MUL_VL)
            DI_CHECK(PrintText(pPrinter, ", mul vl"));
    }
    return PrintText(pPrinter, "]");
}

// SVE prefetch operations
static int PrintSvePrefetch(DianaA64Printer * pPrinter, const DianaLinkedOperand * pOp)
{
    static const char * names[16] =
    {
        "pldl1keep", "pldl1strm", "pldl2keep", "pldl2strm", "pldl3keep", "pldl3strm", 0, 0,
        "pstl1keep", "pstl1strm", "pstl2keep", "pstl2strm", "pstl3keep", "pstl3strm", 0, 0
    };
    if (names[pOp->value.imm & 15])
        return PrintText(pPrinter, names[pOp->value.imm & 15]);
    return PrintDecimalImm(pPrinter, (OPERAND_SIZE_SIGNED)pOp->value.imm);
}

// za1h.s[w12, 3], {za0v.b[w13, 7]}, za[w12, 5]
static int PrintZaSlice(DianaA64Printer * pPrinter, const DianaLinkedOperand * pOp, const DianaA64OperandDesc * pDesc)
{
    char buffer[4];
    if (pDesc->param & DIANA_A64_ZA_BRACES)
        DI_CHECK(PrintText(pPrinter, "{"));
    DI_CHECK(PrintRegister(pPrinter, pOp->value.rmIndex.reg));
    if (pOp->arm.qualifier == DIANA_ARM_QUAL_HORIZONTAL || pOp->arm.qualifier == DIANA_ARM_QUAL_VERTICAL)
        DI_CHECK(PrintText(pPrinter, pOp->arm.qualifier == DIANA_ARM_QUAL_VERTICAL ? "v" : "h"));
    if (DIANA_ARM_ARR_IS_SCALABLE(pOp->arm.arrangement))
    {
        buffer[0] = '.';
        buffer[1] = "bhsdq"[DIANA_ARM_ARR_ELEMENT_LOG2(pOp->arm.arrangement)];
        buffer[2] = 0;
        DI_CHECK(PrintText(pPrinter, buffer));
    }
    DI_CHECK(PrintText(pPrinter, "["));
    DI_CHECK(PrintRegister(pPrinter, pOp->value.rmIndex.indexed_reg));
    DI_CHECK(PrintText(pPrinter, ", "));
    DI_CHECK(PrintDecimal(pPrinter, (OPERAND_SIZE_SIGNED)pOp->value.rmIndex.dispValue));
    if (pOp->arm.lane)
    {
        // a range of slices: 4:7
        DI_CHECK(PrintText(pPrinter, ":"));
        DI_CHECK(PrintDecimal(pPrinter, (OPERAND_SIZE_SIGNED)(pOp->value.rmIndex.dispValue + pOp->arm.lane)));
    }
    if (pOp->arm.regCount)
        DI_CHECK(PrintText(pPrinter, pOp->arm.regCount == 4 ? ", vgx4" : ", vgx2"));
    DI_CHECK(PrintText(pPrinter, "]"));
    if (pDesc->param & DIANA_A64_ZA_BRACES)
        return PrintText(pPrinter, "}");
    return DI_SUCCESS;
}

// ZERO {za0.s, za2.d}: binutils' print_sme_za_list, the biggest groups of 64-bit tiles first
static int PrintZaMask(DianaA64Printer * pPrinter, DI_UINT32 mask)
{
    static const char * names[15] =
    {
        "za", "za0.h", "za1.h", "za0.s", "za1.s", "za2.s", "za3.s",
        "za0.d", "za1.d", "za2.d", "za3.d", "za4.d", "za5.d", "za6.d", "za7.d"
    };
    static const DI_UINT8 bits[15] = {0xFF, 0x55, 0xAA, 0x11, 0x22, 0x44, 0x88, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    int printed = 0;
    int i = 0;
    DI_CHECK(PrintText(pPrinter, "{"));
    for (; i < 15 && mask; ++i)
    {
        if ((mask & bits[i]) != bits[i])
            continue;
        mask &= ~(DI_UINT32)bits[i];
        if (printed++)
            DI_CHECK(PrintText(pPrinter, ", "));
        DI_CHECK(PrintText(pPrinter, names[i]));
    }
    return PrintText(pPrinter, "}");
}

// an operand objdump leaves out: the pattern "all" with mul #1 (cntb x0, ptrue p0.b)
static int IsOmittedOperand(const DianaA64Printer * pPrinter, int index)
{
    const DianaLinkedOperand * pOp = pPrinter->pResult->linkedOperands + index;
    return pPrinter->pCmd->operands[index].kind == DIANA_A64_OPK_SVE_PATTERN && pOp->value.imm == 31 &&
           pOp->arm.shiftAmount == 1;
}

static int PrintSimdOperand(DianaA64Printer * pPrinter, const DianaLinkedOperand * pOp, const DianaA64OperandDesc * pDesc)
{
    switch (pDesc->kind)
    {
    case DIANA_A64_OPK_ZREG:
        return PrintScalableRegister(pPrinter, pOp);

    case DIANA_A64_OPK_PREG:
        DI_CHECK(PrintScalableRegister(pPrinter, pOp));
        // pext p1.s, pn13[2]
        if (pDesc->param == 4)
            return PrintLane(pPrinter, pOp->arm.lane);
        return DI_SUCCESS;

    case DIANA_A64_OPK_PREG_INDEX:
        // psel p0, p1, p2.h[w12, 3]
        DI_CHECK(PrintSveAddressRegister(pPrinter, pOp->value.rmIndex.reg, pOp->arm.arrangement));
        DI_CHECK(PrintText(pPrinter, "["));
        DI_CHECK(PrintRegister(pPrinter, pOp->value.rmIndex.indexed_reg));
        DI_CHECK(PrintText(pPrinter, ", "));
        DI_CHECK(PrintDecimal(pPrinter, (OPERAND_SIZE_SIGNED)pOp->value.rmIndex.dispValue));
        return PrintText(pPrinter, "]");

    case DIANA_A64_OPK_SVE_VLX:
        return PrintText(pPrinter, pOp->value.imm == 4 ? "vlx4" : "vlx2");

    case DIANA_A64_OPK_ZA_TILE:
        return PrintScalableRegister(pPrinter, pOp);

    case DIANA_A64_OPK_ZA_SLICE:
        return PrintZaSlice(pPrinter, pOp, pDesc);

    case DIANA_A64_OPK_ZA_MASK:
        return PrintZaMask(pPrinter, (DI_UINT32)pOp->value.imm);

    case DIANA_A64_OPK_ZT0:
        // zt0, {zt0}, zt0[56]
        DI_CHECK(PrintText(pPrinter, (pDesc->param & 1) ? "{zt0}" : "zt0"));
        if (pDesc->param & 2)
            return PrintLane(pPrinter, pOp->arm.lane);
        return DI_SUCCESS;

    case DIANA_A64_OPK_ZELEM:
        DI_CHECK(PrintScalableRegister(pPrinter, pOp));
        return PrintLane(pPrinter, pOp->arm.lane);

    case DIANA_A64_OPK_ZLIST:
        return PrintScalableList(pPrinter, pOp);

    case DIANA_A64_OPK_SVE_PATTERN:
        return PrintPattern(pPrinter, pOp);

    case DIANA_A64_OPK_SVE_IMM8SH:
        // the shift is folded into the value, only #0, lsl #8 shows it
        if (!pOp->value.imm && pOp->arm.shiftAmount)
            return PrintText(pPrinter, "#0, lsl #8");
        return PrintDecimalImm(pPrinter, (OPERAND_SIZE_SIGNED)pOp->value.imm);

    case DIANA_A64_OPK_GPREG_SIZE:
        return PrintRegister(pPrinter, pOp->value.recognizedRegister);

    case DIANA_A64_OPK_SVE_BIMM:
        return PrintHexImm(pPrinter, pOp->value.imm);

    case DIANA_A64_OPK_SVE_MEM_IMM:
    case DIANA_A64_OPK_SVE_MEM_REG:
    case DIANA_A64_OPK_SVE_MEM_ZIMM:
        return PrintSveMemory(pPrinter, pOp);

    case DIANA_A64_OPK_SVE_PRFOP:
        return PrintSvePrefetch(pPrinter, pOp);

    case DIANA_A64_OPK_SVE_FPCONST:
        // objdump prints these short: #0.5, #2.0
        return PrintText(pPrinter, pOp->value.imm == 0x3FE0000000000000ULL ? "#0.5" :
                                   pOp->value.imm == 0x3FF0000000000000ULL ? "#1.0" :
                                   pOp->value.imm == 0x4000000000000000ULL ? "#2.0" : "#0.0");

    case DIANA_A64_OPK_FPREG:
    case DIANA_A64_OPK_GPREG_IMM5:
        return PrintRegister(pPrinter, pOp->value.recognizedRegister);

    case DIANA_A64_OPK_VREG:
        return PrintVectorRegister(pPrinter, pOp->value.recognizedRegister - reg_A64_V0, pOp->arm.arrangement);

    case DIANA_A64_OPK_VELEM:
        DI_CHECK(PrintVectorRegister(pPrinter, pOp->value.recognizedRegister - reg_A64_V0, pOp->arm.arrangement));
        return PrintLane(pPrinter, pOp->arm.lane);

    case DIANA_A64_OPK_VLIST:
    case DIANA_A64_OPK_VLIST_ELEM:
        return PrintRegisterList(pPrinter, pOp);

    case DIANA_A64_OPK_FPIMM:
        return PrintFPImm(pPrinter, pOp->value.imm);

    case DIANA_A64_OPK_FZERO:
        return PrintText(pPrinter, "#0.0");

    case DIANA_A64_OPK_MODIMM:
        DI_CHECK(PrintHexImm(pPrinter, pOp->value.imm));
        if (pOp->arm.shiftType == DIANA_ARM_SHIFT_MSL || pOp->arm.shiftAmount)
            return PrintShift(pPrinter, &pOp->arm);
        return DI_SUCCESS;

    case DIANA_A64_OPK_SHIFTIMM:
    case DIANA_A64_OPK_CONST:
    case DIANA_A64_OPK_ROTATE:
        return PrintDecimalImm(pPrinter, (OPERAND_SIZE_SIGNED)pOp->value.imm);

    case DIANA_A64_OPK_MEM_POSTIMM:
    case DIANA_A64_OPK_MEM_POSTREG:
    case DIANA_A64_OPK_MEM_PREIMM:
        return PrintMemory(pPrinter, pOp);
    }
    return DI_ERROR;
}

static int PrintOperandValue(DianaA64Printer * pPrinter, int index)
{
    const DianaLinkedOperand * pOp = pPrinter->pResult->linkedOperands + index;
    const DianaA64OperandDesc * pDesc = pPrinter->pCmd->operands + index;

    switch (pDesc->kind)
    {
    case DIANA_A64_OPK_REG_W:
    case DIANA_A64_OPK_REG_X:
    case DIANA_A64_OPK_REG_WSP:
    case DIANA_A64_OPK_REG_XSP:
        DI_CHECK(PrintRegister(pPrinter, pOp->value.recognizedRegister));
        if (pDesc->flags & DIANA_A64_OPF_WRITEBACK)
            return PrintText(pPrinter, "!");
        return DI_SUCCESS;

    case DIANA_A64_OPK_AIMM:
        DI_CHECK(PrintHexImm(pPrinter, pOp->value.imm));
        if (pOp->arm.shiftAmount)
            DI_CHECK(PrintShift(pPrinter, &pOp->arm));
        return DI_SUCCESS;

    case DIANA_A64_OPK_MOVW:
        DI_CHECK(PrintHexImm(pPrinter, pOp->value.imm));
        if (pOp->arm.shiftAmount)
            DI_CHECK(PrintShift(pPrinter, &pOp->arm));
        return DI_SUCCESS;

    case DIANA_A64_OPK_BIMM32:
    case DIANA_A64_OPK_BIMM64:
        return PrintHexImm(pPrinter, pOp->value.imm);

    case DIANA_A64_OPK_UIMM:
        if (pDesc->flags & DIANA_A64_OPF_DECIMAL)
            return PrintDecimalImm(pPrinter, (OPERAND_SIZE_SIGNED)pOp->value.imm);
        return PrintHexImm(pPrinter, pOp->value.imm);

    case DIANA_A64_OPK_TBIT:
    case DIANA_A64_OPK_SIMM:
        return PrintDecimalImm(pPrinter, (OPERAND_SIZE_SIGNED)pOp->value.imm);

    case DIANA_A64_OPK_SHREG_W:
    case DIANA_A64_OPK_SHREG_X:
        DI_CHECK(PrintRegister(pPrinter, pOp->value.recognizedRegister));
        if (pOp->arm.shiftType != DIANA_ARM_SHIFT_LSL || pOp->arm.shiftAmount)
            DI_CHECK(PrintShift(pPrinter, &pOp->arm));
        return DI_SUCCESS;

    case DIANA_A64_OPK_EXTREG_W:
    case DIANA_A64_OPK_EXTREG_X:
        {
            int lslForm = HasSPOperand(pPrinter->pResult) &&
                          pOp->arm.shiftType == (pDesc->kind == DIANA_A64_OPK_EXTREG_X ? DIANA_ARM_EXTEND_UXTX : DIANA_ARM_EXTEND_UXTW);
            DI_CHECK(PrintRegister(pPrinter, pOp->value.recognizedRegister));
            if (lslForm)
            {
                if (pOp->arm.shiftAmount)
                {
                    DI_CHECK(PrintText(pPrinter, ", lsl "));
                    DI_CHECK(PrintDecimalImm(pPrinter, pOp->arm.shiftAmount));
                }
                return DI_SUCCESS;
            }
            DI_CHECK(PrintText(pPrinter, ", "));
            DI_CHECK(PrintText(pPrinter, g_extends[pOp->arm.shiftType - DIANA_ARM_EXTEND_UXTB]));
            if (pOp->arm.shiftAmount)
            {
                DI_CHECK(PrintText(pPrinter, " "));
                DI_CHECK(PrintDecimalImm(pPrinter, pOp->arm.shiftAmount));
            }
            return DI_SUCCESS;
        }

    case DIANA_A64_OPK_LABEL:
    case DIANA_A64_OPK_ADR:
        return PrintAddress(pPrinter, pPrinter->pContext->instructionRIP + pPrinter->pResult->iFullCmdSize + pOp->value.rel.m_value);

    case DIANA_A64_OPK_ADRP:
        return PrintAddress(pPrinter, (pPrinter->pContext->instructionRIP & ~(OPERAND_SIZE)0xFFF) + pOp->value.imm);

    case DIANA_A64_OPK_COND:
        return PrintText(pPrinter, g_conditions[pOp->arm.cond & 15]);

    case DIANA_A64_OPK_MEM_PRE:
    case DIANA_A64_OPK_MEM_BASE:
        if ((pDesc->flags & (DIANA_A64_OPF_OPTIONAL_OFFSET|DIANA_A64_OPF_WRITEBACK)) && !pOp->value.rmIndex.dispValue)
        {
            // ldraa x0, [x1]!, cpyfp [x0]!, [x1]!, x2!
            DI_CHECK(PrintText(pPrinter, "["));
            DI_CHECK(PrintRegister(pPrinter, pOp->value.rmIndex.reg));
            return PrintText(pPrinter, "]!");
        }
        return PrintMemory(pPrinter, pOp);

    case DIANA_A64_OPK_LITERAL:
    case DIANA_A64_OPK_MEM_UOFF:
    case DIANA_A64_OPK_MEM_SOFF:
    case DIANA_A64_OPK_MEM_POST:
    case DIANA_A64_OPK_MEM_REGOFF:
        return PrintMemory(pPrinter, pOp);

    case DIANA_A64_OPK_SYSREG:
        // msr <sysreg>, Xt / mrs Xt, <sysreg>
        return PrintSysReg(pPrinter, (DI_UINT32)pOp->value.imm, index == 0);

    case DIANA_A64_OPK_CREG:
        {
            char buffer[8];
            buffer[0] = 'C';
            FormatUnsigned(buffer + 1, pOp->value.imm, 10);
            return PrintText(pPrinter, buffer);
        }

    case DIANA_A64_OPK_BARRIER:
        if (pDesc->param)
        {
            // DSB <option>nXS
            static const char * nxs[4] = {"oshnxs", "nshnxs", "ishnxs", "synxs"};
            return PrintText(pPrinter, nxs[pOp->value.imm & 3]);
        }
        if (g_barriers[pOp->value.imm & 15])
            return PrintText(pPrinter, g_barriers[pOp->value.imm & 15]);
        {
            // objdump prints two digits: #0x08
            char buffer[8];
            buffer[0] = '#';
            buffer[1] = '0';
            buffer[2] = 'x';
            buffer[3] = '0';
            FormatUnsigned(buffer + 4, pOp->value.imm & 15, 16);
            return PrintText(pPrinter, buffer);
        }

    case DIANA_A64_OPK_PRFOP:
        if (GetPrefetchName((DI_UINT32)pOp->value.imm))
            return PrintText(pPrinter, GetPrefetchName((DI_UINT32)pOp->value.imm));
        return PrintHexImm(pPrinter, pOp->value.imm);
    }
    if (pDesc->kind >= DIANA_A64_OPK_FPREG)
        return PrintSimdOperand(pPrinter, pOp, pDesc);
    return DI_ERROR;
}

static int PrintOperand(DianaA64Printer * pPrinter, int index)
{
    DI_CHECK(PrintSeparator(pPrinter));
    return PrintOperandValue(pPrinter, index);
}

static int PrintOperandsFrom(DianaA64Printer * pPrinter, int first)
{
    int i = first;
    for (; i < pPrinter->pResult->iLinkedOpCount; ++i)
    {
        if (IsOmittedOperand(pPrinter, i))
            continue;
        DI_CHECK(PrintOperand(pPrinter, i));
    }
    return DI_SUCCESS;
}

static int PrintGeneric(DianaA64Printer * pPrinter)
{
    DI_CHECK(PrintMnemonic(pPrinter, pPrinter->pCmd->header.m_pGroupInfo->m_pName));
    // the upper half forms: sqxtn2, ushll2, fcvtl2
    if ((pPrinter->pCmd->flags & DIANA_A64_CMDF_SUFFIX2) && (pPrinter->word >> 30 & 1))
        DI_CHECK(PrintText(pPrinter, "2"));
    return PrintOperandsFrom(pPrinter, 0);
}

// prints the mnemonic and the selected operands
static int PrintAlias(DianaA64Printer * pPrinter, const char * pName, const int * pOperands, int count)
{
    int i = 0;
    DI_CHECK(PrintMnemonic(pPrinter, pName));
    for (; i < count; ++i)
    {
        DI_CHECK(PrintOperand(pPrinter, pOperands[i]));
    }
    return DI_SUCCESS;
}

static int PrintRegOperand(DianaA64Printer * pPrinter, DianaUnifiedRegister reg)
{
    DI_CHECK(PrintSeparator(pPrinter));
    return PrintRegister(pPrinter, reg);
}

static int PrintImmOperand(DianaA64Printer * pPrinter, DI_UINT64 value, int decimal)
{
    DI_CHECK(PrintSeparator(pPrinter));
    if (decimal)
        return PrintDecimalImm(pPrinter, (OPERAND_SIZE_SIGNED)value);
    return PrintHexImm(pPrinter, value);
}

static int PrintTextOperand(DianaA64Printer * pPrinter, const char * pText)
{
    DI_CHECK(PrintSeparator(pPrinter));
    return PrintText(pPrinter, pText);
}

// ------------------------------------------------------------------ aliases
static DianaUnifiedRegister OpReg(const DianaA64Printer * pPrinter, int index)
{
    return pPrinter->pResult->linkedOperands[index].value.recognizedRegister;
}

static DI_UINT64 OpImm(const DianaA64Printer * pPrinter, int index)
{
    return pPrinter->pResult->linkedOperands[index].value.imm;
}

static const DianaArmOperandExt * OpExt(const DianaA64Printer * pPrinter, int index)
{
    return &pPrinter->pResult->linkedOperands[index].arm;
}

static int Is64(const DianaA64Printer * pPrinter)
{
    return (pPrinter->word >> 31) != 0;
}

static int IsShiftNone(const DianaArmOperandExt * pExt)
{
    return pExt->shiftType == DIANA_ARM_SHIFT_LSL && pExt->shiftAmount == 0;
}

// add/sub immediate: mov, cmp, cmn
static int PrintAddSubImm(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    static const int ops12[] = {1, 2};
    static const int ops01[] = {0, 1};
    if (id == diana_a64_cmd_add && OpImm(pPrinter, 2) == 0 && !OpExt(pPrinter, 2)->shiftAmount &&
        (IsSPRegister(OpReg(pPrinter, 0)) || IsSPRegister(OpReg(pPrinter, 1))))
    {
        return PrintAlias(pPrinter, "mov", ops01, 2);
    }
    if (id == diana_a64_cmd_subs && IsZeroRegister(OpReg(pPrinter, 0)))
        return PrintAlias(pPrinter, "cmp", ops12, 2);
    if (id == diana_a64_cmd_adds && IsZeroRegister(OpReg(pPrinter, 0)))
        return PrintAlias(pPrinter, "cmn", ops12, 2);
    return PrintGeneric(pPrinter);
}

// add/sub shifted and extended register: cmp, cmn, neg, negs
static int PrintAddSubReg(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    static const int ops12[] = {1, 2};
    static const int ops02[] = {0, 2};
    int isShifted = pPrinter->pCmd->operands[2].kind == DIANA_A64_OPK_SHREG_W || pPrinter->pCmd->operands[2].kind == DIANA_A64_OPK_SHREG_X;
    if (id == diana_a64_cmd_subs && IsZeroRegister(OpReg(pPrinter, 0)))
        return PrintAlias(pPrinter, "cmp", ops12, 2);
    if (id == diana_a64_cmd_adds && IsZeroRegister(OpReg(pPrinter, 0)))
        return PrintAlias(pPrinter, "cmn", ops12, 2);
    if (isShifted && id == diana_a64_cmd_sub && IsZeroRegister(OpReg(pPrinter, 1)))
        return PrintAlias(pPrinter, "neg", ops02, 2);
    if (isShifted && id == diana_a64_cmd_subs && IsZeroRegister(OpReg(pPrinter, 1)))
        return PrintAlias(pPrinter, "negs", ops02, 2);
    return PrintGeneric(pPrinter);
}

static int PrintSbcNgc(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    static const int ops02[] = {0, 2};
    if (IsZeroRegister(OpReg(pPrinter, 1)))
        return PrintAlias(pPrinter, id == diana_a64_cmd_sbc ? "ngc" : "ngcs", ops02, 2);
    return PrintGeneric(pPrinter);
}

// the value fits movz: only one 16-bit part is not zero
static int IsMovzConstant(DI_UINT64 value, int is64)
{
    int parts = is64 ? 4 : 2;
    int nonZero = 0, i = 0;
    for (; i < parts; ++i)
    {
        if ((value >> (i * 16)) & 0xFFFF)
            ++nonZero;
    }
    return nonZero <= 1;
}

// orr Rd, zr, #imm is "mov" only when movz/movn can't produce the value, like objdump does
static int IsWideMovConstant(DI_UINT64 value, int is64)
{
    DI_UINT64 mask = is64 ? ~0ULL : 0xFFFFFFFFULL;
    return IsMovzConstant(value & mask, is64) || IsMovzConstant(~value & mask, is64);
}

static int PrintLogicalImm(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    static const int ops02[] = {0, 2};
    static const int ops12[] = {1, 2};
    if (id == diana_a64_cmd_orr && IsZeroRegister(OpReg(pPrinter, 1)) && !IsWideMovConstant(OpImm(pPrinter, 2), Is64(pPrinter)))
    {
        return PrintAlias(pPrinter, "mov", ops02, 2);
    }
    if (id == diana_a64_cmd_ands && IsZeroRegister(OpReg(pPrinter, 0)))
        return PrintAlias(pPrinter, "tst", ops12, 2);
    return PrintGeneric(pPrinter);
}

static int PrintLogicalReg(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    static const int ops02[] = {0, 2};
    static const int ops12[] = {1, 2};
    if (id == diana_a64_cmd_orr && IsZeroRegister(OpReg(pPrinter, 1)) && IsShiftNone(OpExt(pPrinter, 2)))
        return PrintAlias(pPrinter, "mov", ops02, 2);
    if (id == diana_a64_cmd_orn && IsZeroRegister(OpReg(pPrinter, 1)))
        return PrintAlias(pPrinter, "mvn", ops02, 2);
    if (id == diana_a64_cmd_ands && IsZeroRegister(OpReg(pPrinter, 0)))
        return PrintAlias(pPrinter, "tst", ops12, 2);
    return PrintGeneric(pPrinter);
}

static int PrintMoveWide(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    DI_UINT64 imm16 = OpImm(pPrinter, 1);
    int shift = OpExt(pPrinter, 1)->shiftAmount;
    int sf = Is64(pPrinter);
    DI_UINT64 value = imm16 << shift;

    if (imm16 == 0 && shift != 0)
        return PrintGeneric(pPrinter);
    if (id == diana_a64_cmd_movn)
    {
        if (!sf && imm16 == 0xFFFF)
            return PrintGeneric(pPrinter);
        value = ~value;
        if (!sf)
            value &= 0xFFFFFFFFULL;
    }
    DI_CHECK(PrintMnemonic(pPrinter, "mov"));
    DI_CHECK(PrintOperand(pPrinter, 0));
    return PrintImmOperand(pPrinter, value, 0);
}

// BFXPreferred() from the ARM ARM
static int BFXPreferred(int sf, int uns, int imms, int immr)
{
    if (imms < immr)
        return 0;
    if (imms == (sf ? 63 : 31))
        return 0;
    if (immr == 0)
    {
        if (sf == 0 && (imms == 7 || imms == 15))
            return 0;
        if (sf == 1 && uns == 0 && (imms == 7 || imms == 15 || imms == 31))
            return 0;
    }
    return 1;
}

static int PrintBitfield(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    int sf = Is64(pPrinter);
    int size = sf ? 64 : 32;
    int immr = (int)OpImm(pPrinter, 2);
    int imms = (int)OpImm(pPrinter, 3);
    DianaUnifiedRegister rn = OpReg(pPrinter, 1);
    DianaUnifiedRegister rnW = (DianaUnifiedRegister)(sf && RegisterIndex(rn) < 31 ? reg_A64_W0 + RegisterIndex(rn) : (sf ? reg_A64_WZR : rn));

    if (id == diana_a64_cmd_sbfm)
    {
        if (imms == size - 1)
        {
            DI_CHECK(PrintMnemonic(pPrinter, "asr"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintOperand(pPrinter, 1));
            return PrintImmOperand(pPrinter, immr, 1);
        }
        if (imms < immr)
        {
            DI_CHECK(PrintMnemonic(pPrinter, "sbfiz"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintOperand(pPrinter, 1));
            DI_CHECK(PrintImmOperand(pPrinter, (size - immr) % size, 1));
            return PrintImmOperand(pPrinter, imms + 1, 1);
        }
        if (BFXPreferred(sf, 0, imms, immr))
        {
            DI_CHECK(PrintMnemonic(pPrinter, "sbfx"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintOperand(pPrinter, 1));
            DI_CHECK(PrintImmOperand(pPrinter, immr, 1));
            return PrintImmOperand(pPrinter, imms - immr + 1, 1);
        }
        if (immr == 0 && (imms == 7 || imms == 15 || (sf && imms == 31)))
        {
            DI_CHECK(PrintMnemonic(pPrinter, imms == 7 ? "sxtb" : imms == 15 ? "sxth" : "sxtw"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            return PrintRegOperand(pPrinter, rnW);
        }
        return PrintGeneric(pPrinter);
    }
    if (id == diana_a64_cmd_ubfm)
    {
        if (imms != size - 1 && imms + 1 == immr)
        {
            DI_CHECK(PrintMnemonic(pPrinter, "lsl"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintOperand(pPrinter, 1));
            return PrintImmOperand(pPrinter, size - 1 - imms, 1);
        }
        if (imms == size - 1)
        {
            DI_CHECK(PrintMnemonic(pPrinter, "lsr"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintOperand(pPrinter, 1));
            return PrintImmOperand(pPrinter, immr, 1);
        }
        if (imms < immr)
        {
            DI_CHECK(PrintMnemonic(pPrinter, "ubfiz"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintOperand(pPrinter, 1));
            DI_CHECK(PrintImmOperand(pPrinter, (size - immr) % size, 1));
            return PrintImmOperand(pPrinter, imms + 1, 1);
        }
        if (BFXPreferred(sf, 1, imms, immr))
        {
            DI_CHECK(PrintMnemonic(pPrinter, "ubfx"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintOperand(pPrinter, 1));
            DI_CHECK(PrintImmOperand(pPrinter, immr, 1));
            return PrintImmOperand(pPrinter, imms - immr + 1, 1);
        }
        if (!sf && immr == 0 && (imms == 7 || imms == 15))
        {
            DI_CHECK(PrintMnemonic(pPrinter, imms == 7 ? "uxtb" : "uxth"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            return PrintOperand(pPrinter, 1);
        }
        return PrintGeneric(pPrinter);
    }
    // bfm
    if (imms < immr)
    {
        if (IsZeroRegister(rn))
        {
            DI_CHECK(PrintMnemonic(pPrinter, "bfc"));
            DI_CHECK(PrintOperand(pPrinter, 0));
        }
        else
        {
            DI_CHECK(PrintMnemonic(pPrinter, "bfi"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintOperand(pPrinter, 1));
        }
        DI_CHECK(PrintImmOperand(pPrinter, (size - immr) % size, 1));
        return PrintImmOperand(pPrinter, imms + 1, 1);
    }
    DI_CHECK(PrintMnemonic(pPrinter, "bfxil"));
    DI_CHECK(PrintOperand(pPrinter, 0));
    DI_CHECK(PrintOperand(pPrinter, 1));
    DI_CHECK(PrintImmOperand(pPrinter, immr, 1));
    return PrintImmOperand(pPrinter, imms - immr + 1, 1);
}

static int PrintExtr(DianaA64Printer * pPrinter)
{
    static const int ops013[] = {0, 1, 3};
    if (OpReg(pPrinter, 1) == OpReg(pPrinter, 2))
        return PrintAlias(pPrinter, "ror", ops013, 3);
    return PrintGeneric(pPrinter);
}

static int PrintCondSelect(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    DianaUnifiedRegister rn = OpReg(pPrinter, 1);
    DianaUnifiedRegister rm = OpReg(pPrinter, 2);
    int cond = OpExt(pPrinter, 3)->cond;
    const char * pName = 0;

    if ((cond & 0xE) == 0xE)
        return PrintGeneric(pPrinter);

    if (id == diana_a64_cmd_csinc || id == diana_a64_cmd_csinv)
    {
        if (IsZeroRegister(rn) && IsZeroRegister(rm))
        {
            DI_CHECK(PrintMnemonic(pPrinter, id == diana_a64_cmd_csinc ? "cset" : "csetm"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            return PrintTextOperand(pPrinter, g_conditions[cond ^ 1]);
        }
        if (rn == rm && !IsZeroRegister(rn))
            pName = id == diana_a64_cmd_csinc ? "cinc" : "cinv";
    }
    if (id == diana_a64_cmd_csneg && rn == rm)
        pName = "cneg";

    if (pName)
    {
        DI_CHECK(PrintMnemonic(pPrinter, pName));
        DI_CHECK(PrintOperand(pPrinter, 0));
        DI_CHECK(PrintOperand(pPrinter, 1));
        return PrintTextOperand(pPrinter, g_conditions[cond ^ 1]);
    }
    return PrintGeneric(pPrinter);
}

static int PrintMulAdd(DianaA64Printer * pPrinter, const char * pAlias)
{
    static const int ops012[] = {0, 1, 2};
    if (IsZeroRegister(OpReg(pPrinter, 3)))
        return PrintAlias(pPrinter, pAlias, ops012, 3);
    return PrintGeneric(pPrinter);
}

static int PrintRet(DianaA64Printer * pPrinter)
{
    if (RegisterIndex(OpReg(pPrinter, 0)) == 30)
        return PrintMnemonic(pPrinter, "ret");
    return PrintGeneric(pPrinter);
}

static int PrintBCond(DianaA64Printer * pPrinter, const char * pPrefix)
{
    DI_CHECK(PrintMnemonic(pPrinter, pPrefix));
    DI_CHECK(PrintMnemonic(pPrinter, g_conditions[OpExt(pPrinter, 1)->cond & 15]));
    return PrintOperand(pPrinter, 0);
}

static int PrintHint(DianaA64Printer * pPrinter)
{
    static const char * hints[] =
    {
        "nop", "yield", "wfe", "wfi", "sev", "sevl", 0, "xpaclri",
        "pacia1716", 0, "pacib1716", 0, "autia1716", 0, "autib1716", 0,
        "esb", "psb csync", "tsb csync", "gcsb dsync", "csdb", 0, "clrbhb", 0,
        "paciaz", "paciasp", "pacibz", "pacibsp", "autiaz", "autiasp", "autibz", "autibsp",
        "bti", 0, "bti c", 0, "bti j", 0, "bti jc", 0,
        "chkfeat x16"
    };
    DI_UINT64 value = OpImm(pPrinter, 0);
    if (value < sizeof(hints)/sizeof(hints[0]) && hints[value])
        return PrintMnemonic(pPrinter, hints[value]);
    return PrintGeneric(pPrinter);
}

static int PrintBarrier(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    DI_UINT64 crm = OpImm(pPrinter, 0);
    if (pPrinter->pCmd->operands[0].param)
        return PrintGeneric(pPrinter);      // DSB <option>nXS has no aliases
    if (id == diana_a64_cmd_dsb && crm == 0)
        return PrintMnemonic(pPrinter, "ssbb");
    if (id == diana_a64_cmd_dsb && crm == 4)
        return PrintMnemonic(pPrinter, "pssbb");
    if (id == diana_a64_cmd_isb || id == diana_a64_cmd_clrex)
    {
        DI_CHECK(PrintMnemonic(pPrinter, pPrinter->pCmd->header.m_pGroupInfo->m_pName));
        if (crm == 15)
            return DI_SUCCESS;
        return PrintImmOperand(pPrinter, crm, 0);
    }
    return PrintGeneric(pPrinter);
}

static int PrintMsr(DianaA64Printer * pPrinter)
{
    // the low bit of the value: the field takes only #0 or #1
    static const DianaA64Name pstates[] =
    {
        {(((0 << 3) | 3) << 1) | 1, "uao"},
        {(((0 << 3) | 4) << 1) | 1, "pan"},
        {(((0 << 3) | 5) << 1) | 1, "spsel"},
        {(((3 << 3) | 1) << 1) | 1, "ssbs"},
        {(((3 << 3) | 2) << 1) | 1, "dit"},
        {(((3 << 3) | 4) << 1) | 1, "tco"},
        {(((3 << 3) | 6) << 1) | 0, "daifset"},
        {(((3 << 3) | 7) << 1) | 0, "daifclr"},
    };
    const char * pName = 0;
    DI_UINT32 field = 0;
    int i = 0;
    if (pPrinter->pCmd->operands[0].kind != DIANA_A64_OPK_UIMM)
        return PrintGeneric(pPrinter);

    // msr <pstatefield>, #imm
    field = (DI_UINT32)OpImm(pPrinter, 0);
    if (field == ((3 << 3) | 3))
    {
        // svcr: smstart/smstop {sm|za} for CRm = 0xxx with CRm<2:1> != 00, CRm<0> is start; not an alias for objdump
        DI_UINT64 crm = OpImm(pPrinter, 1);
        if (crm >= 2 && crm <= 7)
        {
            DI_CHECK(PrintMnemonic(pPrinter, (crm & 1) ? "smstart" : "smstop"));
            if ((crm >> 1) == 1)
                return PrintTextOperand(pPrinter, "sm");
            if ((crm >> 1) == 2)
                return PrintTextOperand(pPrinter, "za");
            return DI_SUCCESS;
        }
    }
    for (i = 0; i < (int)(sizeof(pstates)/sizeof(pstates[0])); ++i)
    {
        if ((pstates[i].value >> 1) == field && (!(pstates[i].value & 1) || OpImm(pPrinter, 1) <= 1))
        {
            pName = pstates[i].pName;
            break;
        }
    }
    if (!pName)
    {
        // an unknown field is printed as a system register with op0 = 0
        DI_UINT32 op1op2 = (DI_UINT32)OpImm(pPrinter, 0);
        DI_CHECK(PrintMnemonic(pPrinter, "msr"));
        DI_CHECK(PrintSeparator(pPrinter));
        DI_CHECK(PrintGenericSysReg(pPrinter, 0, (int)(op1op2 >> 3), 4, (int)OpImm(pPrinter, 1), (int)(op1op2 & 7)));
        return PrintTextOperand(pPrinter, "xzr");
    }
    DI_CHECK(PrintMnemonic(pPrinter, "msr"));
    DI_CHECK(PrintTextOperand(pPrinter, pName));
    return PrintOperand(pPrinter, 1);
}

// rprfm <rprfop>, Xm, [Xn|SP]
static int PrintRprfm(DianaA64Printer * pPrinter)
{
    static const char * names[8] = {"pldkeep", "pstkeep", 0, 0, "pldstrm", "pststrm", 0, 0};
    DI_UINT64 op = OpImm(pPrinter, 0);
    DI_CHECK(PrintMnemonic(pPrinter, "rprfm"));
    if (op < 8 && names[op])
    {
        DI_CHECK(PrintTextOperand(pPrinter, names[op]));
    }
    else
    {
        DI_CHECK(PrintImmOperand(pPrinter, op, 1));
    }
    return PrintOperandsFrom(pPrinter, 1);
}

// SYS aliases, op1:CRn:CRm:op2. DIANA_A64_SYS_NO_XT: the alias takes no register, Rt is ignored like objdump and llvm-mc do
#define DIANA_A64_SYS(op1, crn, crm, op2)   (((op1) << 11) | ((crn) << 7) | ((crm) << 3) | (op2))
#define DIANA_A64_SYS_NO_XT                 0x10000
// TLBI without an nXS form (CRn = 9)
#define DIANA_A64_SYS_NO_NXS                0x20000

typedef struct _DianaA64SysAlias
{
    DI_UINT32 value;
    const char * pMnemonic;
    const char * pOperation;
}DianaA64SysAlias;

static const DianaA64SysAlias g_sysAliases[] =
{
    // instruction cache
    {DIANA_A64_SYS(0, 7, 1, 0) | DIANA_A64_SYS_NO_XT,  "ic", "ialluis"},
    {DIANA_A64_SYS(0, 7, 5, 0) | DIANA_A64_SYS_NO_XT,  "ic", "iallu"},
    {DIANA_A64_SYS(3, 7, 5, 1),                        "ic", "ivau"},
    // data cache
    {DIANA_A64_SYS(0, 7, 6, 1),   "dc", "ivac"},
    {DIANA_A64_SYS(0, 7, 6, 2),   "dc", "isw"},
    {DIANA_A64_SYS(0, 7, 6, 3),   "dc", "igvac"},
    {DIANA_A64_SYS(0, 7, 6, 4),   "dc", "igsw"},
    {DIANA_A64_SYS(0, 7, 6, 5),   "dc", "igdvac"},
    {DIANA_A64_SYS(0, 7, 6, 6),   "dc", "igdsw"},
    {DIANA_A64_SYS(0, 7, 10, 2),  "dc", "csw"},
    {DIANA_A64_SYS(0, 7, 10, 4),  "dc", "cgsw"},
    {DIANA_A64_SYS(0, 7, 10, 6),  "dc", "cgdsw"},
    {DIANA_A64_SYS(0, 7, 14, 2),  "dc", "cisw"},
    {DIANA_A64_SYS(0, 7, 14, 4),  "dc", "cigsw"},
    {DIANA_A64_SYS(0, 7, 14, 6),  "dc", "cigdsw"},
    {DIANA_A64_SYS(3, 7, 4, 1),   "dc", "zva"},
    {DIANA_A64_SYS(3, 7, 4, 3),   "dc", "gva"},
    {DIANA_A64_SYS(3, 7, 4, 4),   "dc", "gzva"},
    {DIANA_A64_SYS(3, 7, 10, 1),  "dc", "cvac"},
    {DIANA_A64_SYS(3, 7, 10, 3),  "dc", "cgvac"},
    {DIANA_A64_SYS(3, 7, 10, 5),  "dc", "cgdvac"},
    {DIANA_A64_SYS(3, 7, 11, 1),  "dc", "cvau"},
    {DIANA_A64_SYS(3, 7, 12, 1),  "dc", "cvap"},
    {DIANA_A64_SYS(3, 7, 12, 3),  "dc", "cgvap"},
    {DIANA_A64_SYS(3, 7, 12, 5),  "dc", "cgdvap"},
    {DIANA_A64_SYS(3, 7, 13, 1),  "dc", "cvadp"},
    {DIANA_A64_SYS(3, 7, 13, 3),  "dc", "cgvadp"},
    {DIANA_A64_SYS(3, 7, 13, 5),  "dc", "cgdvadp"},
    {DIANA_A64_SYS(3, 7, 14, 1),  "dc", "civac"},
    {DIANA_A64_SYS(3, 7, 14, 3),  "dc", "cigvac"},
    {DIANA_A64_SYS(3, 7, 14, 5),  "dc", "cigdvac"},
    // address translation
    {DIANA_A64_SYS(0, 7, 8, 0),   "at", "s1e1r"},
    {DIANA_A64_SYS(0, 7, 8, 1),   "at", "s1e1w"},
    {DIANA_A64_SYS(0, 7, 8, 2),   "at", "s1e0r"},
    {DIANA_A64_SYS(0, 7, 8, 3),   "at", "s1e0w"},
    {DIANA_A64_SYS(0, 7, 9, 0),   "at", "s1e1rp"},
    {DIANA_A64_SYS(0, 7, 9, 1),   "at", "s1e1wp"},
    {DIANA_A64_SYS(4, 7, 8, 0),   "at", "s1e2r"},
    {DIANA_A64_SYS(4, 7, 8, 1),   "at", "s1e2w"},
    {DIANA_A64_SYS(4, 7, 8, 4),   "at", "s12e1r"},
    {DIANA_A64_SYS(4, 7, 8, 5),   "at", "s12e1w"},
    {DIANA_A64_SYS(4, 7, 8, 6),   "at", "s12e0r"},
    {DIANA_A64_SYS(4, 7, 8, 7),   "at", "s12e0w"},
    {DIANA_A64_SYS(6, 7, 8, 0),   "at", "s1e3r"},
    {DIANA_A64_SYS(6, 7, 8, 1),   "at", "s1e3w"},
    // prediction restriction
    {DIANA_A64_SYS(3, 7, 3, 4),   "cfp", "rctx"},
    {DIANA_A64_SYS(3, 7, 3, 5),   "dvp", "rctx"},
    {DIANA_A64_SYS(3, 7, 3, 7),   "cpp", "rctx"},
    {DIANA_A64_SYS(3, 7, 3, 6),   "cosp", "rctx"},
    // ATS1A, RME
    {DIANA_A64_SYS(0, 7, 9, 2),   "at", "s1e1a"},
    {DIANA_A64_SYS(4, 7, 9, 2),   "at", "s1e2a"},
    {DIANA_A64_SYS(6, 7, 9, 2),   "at", "s1e3a"},
    {DIANA_A64_SYS(6, 7, 14, 1),  "dc", "cipapa"},
    {DIANA_A64_SYS(6, 7, 14, 5),  "dc", "cigdpapa"},
    {DIANA_A64_SYS(6, 8, 1, 4) | DIANA_A64_SYS_NO_XT | DIANA_A64_SYS_NO_NXS,  "tlbi", "paallos"},
    {DIANA_A64_SYS(6, 8, 4, 3) | DIANA_A64_SYS_NO_NXS,   "tlbi", "rpaos"},
    {DIANA_A64_SYS(6, 8, 4, 7) | DIANA_A64_SYS_NO_NXS,   "tlbi", "rpalos"},
    {DIANA_A64_SYS(6, 8, 7, 4) | DIANA_A64_SYS_NO_XT | DIANA_A64_SYS_NO_NXS,  "tlbi", "paall"},
    // TLB maintenance, EL1
    {DIANA_A64_SYS(0, 8, 1, 0) | DIANA_A64_SYS_NO_XT,  "tlbi", "vmalle1os"},
    {DIANA_A64_SYS(0, 8, 1, 1),   "tlbi", "vae1os"},
    {DIANA_A64_SYS(0, 8, 1, 2),   "tlbi", "aside1os"},
    {DIANA_A64_SYS(0, 8, 1, 3),   "tlbi", "vaae1os"},
    {DIANA_A64_SYS(0, 8, 1, 5),   "tlbi", "vale1os"},
    {DIANA_A64_SYS(0, 8, 1, 7),   "tlbi", "vaale1os"},
    {DIANA_A64_SYS(0, 8, 2, 1),   "tlbi", "rvae1is"},
    {DIANA_A64_SYS(0, 8, 2, 3),   "tlbi", "rvaae1is"},
    {DIANA_A64_SYS(0, 8, 2, 5),   "tlbi", "rvale1is"},
    {DIANA_A64_SYS(0, 8, 2, 7),   "tlbi", "rvaale1is"},
    {DIANA_A64_SYS(0, 8, 3, 0) | DIANA_A64_SYS_NO_XT,  "tlbi", "vmalle1is"},
    {DIANA_A64_SYS(0, 8, 3, 1),   "tlbi", "vae1is"},
    {DIANA_A64_SYS(0, 8, 3, 2),   "tlbi", "aside1is"},
    {DIANA_A64_SYS(0, 8, 3, 3),   "tlbi", "vaae1is"},
    {DIANA_A64_SYS(0, 8, 3, 5),   "tlbi", "vale1is"},
    {DIANA_A64_SYS(0, 8, 3, 7),   "tlbi", "vaale1is"},
    {DIANA_A64_SYS(0, 8, 5, 1),   "tlbi", "rvae1os"},
    {DIANA_A64_SYS(0, 8, 5, 3),   "tlbi", "rvaae1os"},
    {DIANA_A64_SYS(0, 8, 5, 5),   "tlbi", "rvale1os"},
    {DIANA_A64_SYS(0, 8, 5, 7),   "tlbi", "rvaale1os"},
    {DIANA_A64_SYS(0, 8, 6, 1),   "tlbi", "rvae1"},
    {DIANA_A64_SYS(0, 8, 6, 3),   "tlbi", "rvaae1"},
    {DIANA_A64_SYS(0, 8, 6, 5),   "tlbi", "rvale1"},
    {DIANA_A64_SYS(0, 8, 6, 7),   "tlbi", "rvaale1"},
    {DIANA_A64_SYS(0, 8, 7, 0) | DIANA_A64_SYS_NO_XT,  "tlbi", "vmalle1"},
    {DIANA_A64_SYS(0, 8, 7, 1),   "tlbi", "vae1"},
    {DIANA_A64_SYS(0, 8, 7, 2),   "tlbi", "aside1"},
    {DIANA_A64_SYS(0, 8, 7, 3),   "tlbi", "vaae1"},
    {DIANA_A64_SYS(0, 8, 7, 5),   "tlbi", "vale1"},
    {DIANA_A64_SYS(0, 8, 7, 7),   "tlbi", "vaale1"},
    // TLB maintenance, EL2
    {DIANA_A64_SYS(4, 8, 0, 1),   "tlbi", "ipas2e1is"},
    {DIANA_A64_SYS(4, 8, 0, 2),   "tlbi", "ripas2e1is"},
    {DIANA_A64_SYS(4, 8, 0, 5),   "tlbi", "ipas2le1is"},
    {DIANA_A64_SYS(4, 8, 0, 6),   "tlbi", "ripas2le1is"},
    {DIANA_A64_SYS(4, 8, 1, 0) | DIANA_A64_SYS_NO_XT,  "tlbi", "alle2os"},
    {DIANA_A64_SYS(4, 8, 1, 1),   "tlbi", "vae2os"},
    {DIANA_A64_SYS(4, 8, 1, 4) | DIANA_A64_SYS_NO_XT,  "tlbi", "alle1os"},
    {DIANA_A64_SYS(4, 8, 1, 5),   "tlbi", "vale2os"},
    {DIANA_A64_SYS(4, 8, 1, 6) | DIANA_A64_SYS_NO_XT,  "tlbi", "vmalls12e1os"},
    {DIANA_A64_SYS(4, 8, 2, 1),   "tlbi", "rvae2is"},
    {DIANA_A64_SYS(4, 8, 2, 5),   "tlbi", "rvale2is"},
    {DIANA_A64_SYS(4, 8, 3, 0) | DIANA_A64_SYS_NO_XT,  "tlbi", "alle2is"},
    {DIANA_A64_SYS(4, 8, 3, 1),   "tlbi", "vae2is"},
    {DIANA_A64_SYS(4, 8, 3, 4) | DIANA_A64_SYS_NO_XT,  "tlbi", "alle1is"},
    {DIANA_A64_SYS(4, 8, 3, 5),   "tlbi", "vale2is"},
    {DIANA_A64_SYS(4, 8, 3, 6) | DIANA_A64_SYS_NO_XT,  "tlbi", "vmalls12e1is"},
    {DIANA_A64_SYS(4, 8, 4, 0),   "tlbi", "ipas2e1os"},
    {DIANA_A64_SYS(4, 8, 4, 1),   "tlbi", "ipas2e1"},
    {DIANA_A64_SYS(4, 8, 4, 2),   "tlbi", "ripas2e1"},
    {DIANA_A64_SYS(4, 8, 4, 3),   "tlbi", "ripas2e1os"},
    {DIANA_A64_SYS(4, 8, 4, 4),   "tlbi", "ipas2le1os"},
    {DIANA_A64_SYS(4, 8, 4, 5),   "tlbi", "ipas2le1"},
    {DIANA_A64_SYS(4, 8, 4, 6),   "tlbi", "ripas2le1"},
    {DIANA_A64_SYS(4, 8, 4, 7),   "tlbi", "ripas2le1os"},
    {DIANA_A64_SYS(4, 8, 5, 1),   "tlbi", "rvae2os"},
    {DIANA_A64_SYS(4, 8, 5, 5),   "tlbi", "rvale2os"},
    {DIANA_A64_SYS(4, 8, 6, 1),   "tlbi", "rvae2"},
    {DIANA_A64_SYS(4, 8, 6, 5),   "tlbi", "rvale2"},
    {DIANA_A64_SYS(4, 8, 7, 0) | DIANA_A64_SYS_NO_XT,  "tlbi", "alle2"},
    {DIANA_A64_SYS(4, 8, 7, 1),   "tlbi", "vae2"},
    {DIANA_A64_SYS(4, 8, 7, 4) | DIANA_A64_SYS_NO_XT,  "tlbi", "alle1"},
    {DIANA_A64_SYS(4, 8, 7, 5),   "tlbi", "vale2"},
    {DIANA_A64_SYS(4, 8, 7, 6) | DIANA_A64_SYS_NO_XT,  "tlbi", "vmalls12e1"},
    // TLB maintenance, EL3
    {DIANA_A64_SYS(6, 8, 1, 0) | DIANA_A64_SYS_NO_XT,  "tlbi", "alle3os"},
    {DIANA_A64_SYS(6, 8, 1, 1),   "tlbi", "vae3os"},
    {DIANA_A64_SYS(6, 8, 1, 5),   "tlbi", "vale3os"},
    {DIANA_A64_SYS(6, 8, 2, 1),   "tlbi", "rvae3is"},
    {DIANA_A64_SYS(6, 8, 2, 5),   "tlbi", "rvale3is"},
    {DIANA_A64_SYS(6, 8, 3, 0) | DIANA_A64_SYS_NO_XT,  "tlbi", "alle3is"},
    {DIANA_A64_SYS(6, 8, 3, 1),   "tlbi", "vae3is"},
    {DIANA_A64_SYS(6, 8, 3, 5),   "tlbi", "vale3is"},
    {DIANA_A64_SYS(6, 8, 5, 1),   "tlbi", "rvae3os"},
    {DIANA_A64_SYS(6, 8, 5, 5),   "tlbi", "rvale3os"},
    {DIANA_A64_SYS(6, 8, 6, 1),   "tlbi", "rvae3"},
    {DIANA_A64_SYS(6, 8, 6, 5),   "tlbi", "rvale3"},
    {DIANA_A64_SYS(6, 8, 7, 0) | DIANA_A64_SYS_NO_XT,  "tlbi", "alle3"},
    {DIANA_A64_SYS(6, 8, 7, 1),   "tlbi", "vae3"},
    {DIANA_A64_SYS(6, 8, 7, 5),   "tlbi", "vale3"},
};

// TLBI with CRn = 9 is the nXS form (v8.7) of the same operation with CRn = 8.
// pair: SYSP as TLBIP (v9.4), the same TLBI operations with Xt, X(t+1) (operands 4, 5)
static int PrintSysAlias(DianaA64Printer * pPrinter, int pair, int * pPrinted)
{
    DI_UINT32 word = pPrinter->word;
    DI_UINT32 key = (word >> 5) & 0x3FFF;
    int nxs = 0;
    int i = 0;
    *pPrinted = 0;
    if (((key >> 7) & 15) == 9)
    {
        key = (key & ~(15u << 7)) | (8u << 7);
        nxs = 1;
    }
    for (; i < (int)(sizeof(g_sysAliases)/sizeof(g_sysAliases[0])); ++i)
    {
        const DianaA64SysAlias * pAlias = g_sysAliases + i;
        if ((pAlias->value & 0xFFFF) != key)
            continue;
        if (nxs && (pAlias->pMnemonic[0] != 't' || (pAlias->value & DIANA_A64_SYS_NO_NXS)))
            return DI_SUCCESS;
        if (pair && pAlias->pMnemonic[0] != 't')
            return DI_SUCCESS;
        DI_CHECK(PrintMnemonic(pPrinter, pair ? "tlbip" : pAlias->pMnemonic));
        DI_CHECK(PrintSeparator(pPrinter));
        DI_CHECK(PrintText(pPrinter, pAlias->pOperation));
        if (nxs)
        {
            DI_CHECK(PrintText(pPrinter, "nxs"));
        }
        if (pair)
        {
            // like objdump: Xt only for an operation with an address, then X(t+1) unless it is xzr
            // (also for one without an address: tlbip vmalle1, x3)
            if (!(pAlias->value & DIANA_A64_SYS_NO_XT))
                DI_CHECK(PrintOperand(pPrinter, 4));
            if (!IsZeroRegister(OpReg(pPrinter, 5)))
                DI_CHECK(PrintOperand(pPrinter, 5));
        }
        else if (!(pAlias->value & DIANA_A64_SYS_NO_XT))
        {
            DI_CHECK(PrintOperand(pPrinter, 4));
        }
        *pPrinted = 1;
        return DI_SUCCESS;
    }
    return DI_SUCCESS;
}

// GCS instructions in the SYS/SYSL space, printed by name also without aliases (objdump does), and TRCIT, an alias.
// Xt: always printed, except gcspopm without it when it is xzr; gcspushx, gcspopx, gcspopcx exist only with Rt = 31
static int PrintSysInstruction(DianaA64Printer * pPrinter, int isSysl, int * pPrinted)
{
    DI_UINT32 key = (pPrinter->word >> 5) & 0x3FFF;
    int rt = (int)(pPrinter->word & 31);
    int aliases = !(pPrinter->pContext->options & DIANA_TEXT_OUTPUT_OPTION_ARM_NO_ALIASES);
    const char * pName = 0;
    int withXt = 1;
    *pPrinted = 0;
    if (isSysl)
    {
        if (key == DIANA_A64_SYS(3, 7, 7, 1))
        {
            pName = "gcspopm";
            withXt = rt != 31;
        }
        else if (key == DIANA_A64_SYS(3, 7, 7, 3))
        {
            pName = "gcsss2";
        }
    }
    else
    {
        switch (key)
        {
        case DIANA_A64_SYS(3, 7, 7, 0): pName = "gcspushm"; break;
        case DIANA_A64_SYS(3, 7, 7, 2): pName = "gcsss1"; break;
        case DIANA_A64_SYS(0, 7, 7, 4): pName = rt == 31 ? "gcspushx" : 0; withXt = 0; break;
        case DIANA_A64_SYS(0, 7, 7, 5): pName = rt == 31 ? "gcspopcx" : 0; withXt = 0; break;
        case DIANA_A64_SYS(0, 7, 7, 6): pName = rt == 31 ? "gcspopx" : 0; withXt = 0; break;
        case DIANA_A64_SYS(3, 7, 2, 7): pName = aliases ? "trcit" : 0; break;
        }
    }
    if (!pName)
        return DI_SUCCESS;
    *pPrinted = 1;
    DI_CHECK(PrintMnemonic(pPrinter, pName));
    if (withXt)
        return PrintOperand(pPrinter, isSysl ? 0 : 4);
    return DI_SUCCESS;
}

static int PrintSys(DianaA64Printer * pPrinter)
{
    // the optional Xt is not printed when it is XZR
    int count = pPrinter->pResult->iLinkedOpCount;
    int i = 0;
    int printed = 0;
    DI_CHECK(PrintSysInstruction(pPrinter, 0, &printed));
    if (printed)
        return DI_SUCCESS;
    if (!(pPrinter->pContext->options & DIANA_TEXT_OUTPUT_OPTION_ARM_NO_ALIASES))
    {
        DI_CHECK(PrintSysAlias(pPrinter, 0, &printed));
        if (printed)
            return DI_SUCCESS;
    }
    DI_CHECK(PrintMnemonic(pPrinter, "sys"));
    if (IsZeroRegister(OpReg(pPrinter, 4)))
        --count;
    for (; i < count; ++i)
    {
        DI_CHECK(PrintOperand(pPrinter, i));
    }
    return DI_SUCCESS;
}

static int PrintUndefined(DianaA64Printer * pPrinter)
{
    char buffer[16];
    int i = 0;
    DI_UINT32 word = pPrinter->word;
    DI_CHECK(PrintMnemonic(pPrinter, ".inst"));
    DI_CHECK(PrintSeparator(pPrinter));
    buffer[0] = '0';
    buffer[1] = 'x';
    for (; i < 8; ++i)
    {
        int digit = (word >> (28 - i * 4)) & 15;
        buffer[2 + i] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
    }
    buffer[10] = 0;
    return PrintText(pPrinter, buffer);
}

// sxtl2, uxtl2: the upper half forms keep the "2" in the alias
static int PrintSuffixedAlias(DianaA64Printer * pPrinter, const char * pName, const int * pOperands, int count)
{
    int i = 0;
    DI_CHECK(PrintMnemonic(pPrinter, pName));
    if ((pPrinter->pCmd->flags & DIANA_A64_CMDF_SUFFIX2) && (pPrinter->word >> 30 & 1))
        DI_CHECK(PrintText(pPrinter, "2"));
    for (; i < count; ++i)
    {
        DI_CHECK(PrintOperand(pPrinter, pOperands[i]));
    }
    return DI_SUCCESS;
}

// FP and SIMD aliases, *pHandled = 0 if the instruction has none
static int PrintSimdAlias(DianaA64Printer * pPrinter, DianaA64CommandId id, int * pHandled)
{
    static const int ops01[] = {0, 1};
    const DianaA64CmdInfo * pCmd = pPrinter->pCmd;
    int count = pCmd->operandCount;
    *pHandled = 1;
    switch (id)
    {
    case diana_a64_cmd_orr:
        // orr v0.16b, v1.16b, v1.16b -> mov v0.16b, v1.16b
        if (count == 3 && pCmd->operands[2].kind == DIANA_A64_OPK_VREG && OpReg(pPrinter, 1) == OpReg(pPrinter, 2))
            return PrintAlias(pPrinter, "mov", ops01, 2);
        break;
    case diana_a64_cmd_not:
        return PrintAlias(pPrinter, "mvn", ops01, 2);
    case diana_a64_cmd_ins:
        return PrintAlias(pPrinter, "mov", ops01, 2);
    case diana_a64_cmd_umov:
        // umov w0, v1.s[1] / umov x0, v1.d[0] -> mov; b and h elements stay umov
        if (DIANA_ARM_ARR_ELEMENT_LOG2(OpExt(pPrinter, 1)->arrangement) >= 2)
            return PrintAlias(pPrinter, "mov", ops01, 2);
        break;
    case diana_a64_cmd_dup:
        // dup b0, v1.b[3] -> mov (the scalar form)
        if (pCmd->operands[0].kind == DIANA_A64_OPK_FPREG)
            return PrintAlias(pPrinter, "mov", ops01, 2);
        break;
    case diana_a64_cmd_sshll:
    case diana_a64_cmd_ushll:
        // shift 0: sxtl/uxtl
        if (count == 3 && OpImm(pPrinter, 2) == 0)
            return PrintSuffixedAlias(pPrinter, id == diana_a64_cmd_sshll ? "sxtl" : "uxtl", ops01, 2);
        break;
    default:
        break;
    }
    *pHandled = 0;
    return DI_SUCCESS;
}

// ldadd x0, xzr, [x1] -> stadd x0, [x1]: LSE ld<op> without acquire (A, bit 23) and with Rt = xzr
static int PrintAtomicStore(DianaA64Printer * pPrinter, int * pHandled)
{
    static const int ops02[] = {0, 2};
    const char * pName = pPrinter->pCmd->header.m_pGroupInfo->m_pName;
    char buffer[16];
    int i = 0;
    *pHandled = 0;
    if (!(pPrinter->pCmd->features & DIANA_A64_FEATURE_LSE) || pName[0] != 'l' || pName[1] != 'd' ||
        pPrinter->pCmd->operandCount != 3 || (pPrinter->word >> 23 & 1) || !IsZeroRegister(OpReg(pPrinter, 1)))
        return DI_SUCCESS;
    buffer[0] = 's';
    buffer[1] = 't';
    for (; pName[i + 2] && i + 3 < (int)sizeof(buffer); ++i)
        buffer[i + 2] = pName[i + 2];
    buffer[i + 2] = 0;
    *pHandled = 1;
    return PrintAlias(pPrinter, buffer, ops02, 2);
}

// the highest operand kind: FP/SIMD kinds start at DIANA_A64_OPK_FPREG, SVE/SME kinds at DIANA_A64_OPK_ZREG
static int MaxOperandKind(const DianaA64CmdInfo * pCmd)
{
    int result = DIANA_A64_OPK_NONE;
    int i = 0;
    for (; i < pCmd->operandCount; ++i)
    {
        if (pCmd->operands[i].kind > result)
            result = pCmd->operands[i].kind;
    }
    return result;
}

// DUPM prints as mov unless DUP can make the value: binutils' aarch64_sve_dupm_mov_immediate_p,
// the value of one element, esize in bytes
static int IsDupmMovPreferred(DI_UINT64 value, int esize)
{
    DI_INT64 signedValue = (DI_INT64)value;
    DI_UINT64 upper = esize >= 8 ? 0 : ~0ULL << (esize * 8);
    if ((value & ~upper) != value && (value | upper) != value)
        return 0;
    if (esize <= 4 || (DI_UINT32)value == (DI_UINT32)(value >> 32))
    {
        signedValue = (int)(DI_UINT32)value;
        if (esize <= 2 || (DI_UINT16)value == (DI_UINT16)(value >> 16))
        {
            signedValue = (short)(DI_UINT16)value;
            if (esize == 1 || (DI_UINT8)value == (DI_UINT8)(value >> 8))
                return 0;
        }
    }
    if ((signedValue & 0xFF) == 0)
        signedValue /= 256;
    return signedValue < -128 || signedValue >= 128;
}

static int SameRegister(const DianaA64Printer * pPrinter, int index1, int index2)
{
    return OpReg(pPrinter, index1) == OpReg(pPrinter, index2);
}

// an operand printed with another predicate qualifier: sel z0.b, p1, z2.b, z0.b -> mov z0.b, p1/m, z2.b
static int PrintQualifiedOperand(DianaA64Printer * pPrinter, int index, int qualifier)
{
    DianaLinkedOperand op = pPrinter->pResult->linkedOperands[index];
    op.arm.qualifier = (DI_UINT8)qualifier;
    DI_CHECK(PrintSeparator(pPrinter));
    return PrintScalableRegister(pPrinter, &op);
}

// SVE and SME aliases
static int PrintSveAlias(DianaA64Printer * pPrinter, DianaA64CommandId id, int * pHandled)
{
    static const int ops01[] = {0, 1};
    static const int ops012[] = {0, 1, 2};
    int count = pPrinter->pResult->iLinkedOpCount;
    int isPredicate = pPrinter->pCmd->operands[0].kind == DIANA_A64_OPK_PREG;
    *pHandled = 1;
    switch (id)
    {
    case diana_a64_cmd_dup:
        // dup z0.h, z1.h[0] -> mov z0.h, h1
        if (pPrinter->pCmd->operands[1].kind == DIANA_A64_OPK_ZELEM && !pPrinter->pResult->linkedOperands[1].arm.lane)
        {
            const DianaLinkedOperand * pOp = pPrinter->pResult->linkedOperands + 1;
            int element = DIANA_ARM_ARR_ELEMENT_LOG2(pOp->arm.arrangement);
            DI_CHECK(PrintMnemonic(pPrinter, "mov"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            return PrintRegOperand(pPrinter, (DianaUnifiedRegister)(reg_A64_B0 + element * 32 + (pOp->value.recognizedRegister - reg_A64_Z0)));
        }
        return PrintAlias(pPrinter, "mov", ops012, count);

    case diana_a64_cmd_cpy:
        return PrintAlias(pPrinter, "mov", ops012, count);

    case diana_a64_cmd_fcpy:
    case diana_a64_cmd_fdup:
        return PrintAlias(pPrinter, "fmov", ops012, count);

    case diana_a64_cmd_dupm:
        {
            const DianaLinkedOperand * pOp = pPrinter->pResult->linkedOperands;
            int esize = 1 << DIANA_ARM_ARR_ELEMENT_LOG2(pOp[0].arm.arrangement);
            if (IsDupmMovPreferred(pOp[1].value.imm, esize))
                return PrintAlias(pPrinter, "mov", ops01, 2);
        }
        break;

    case diana_a64_cmd_sel:
        // sel z0.b, p1, z2.b, z0.b -> mov z0.b, p1/m, z2.b (Z and P registers, not the SME2 lists)
        if (pPrinter->pCmd->operands[0].kind != DIANA_A64_OPK_ZLIST && SameRegister(pPrinter, 0, 3))
        {
            DI_CHECK(PrintMnemonic(pPrinter, "mov"));
            DI_CHECK(PrintOperand(pPrinter, 0));
            DI_CHECK(PrintQualifiedOperand(pPrinter, 1, DIANA_ARM_QUAL_MERGE));
            return PrintOperand(pPrinter, 2);
        }
        break;

    case diana_a64_cmd_orr:
    case diana_a64_cmd_orrs:
        if (!isPredicate && count == 3 && pPrinter->pCmd->operands[2].kind == DIANA_A64_OPK_ZREG && SameRegister(pPrinter, 1, 2))
            return PrintAlias(pPrinter, "mov", ops01, 2);
        // orr p0.b, p1/z, p1.b, p1.b -> mov p0.b, p1.b
        if (isPredicate && count == 4 && SameRegister(pPrinter, 1, 2) && SameRegister(pPrinter, 1, 3))
        {
            static const int ops02[] = {0, 2};
            return PrintAlias(pPrinter, id == diana_a64_cmd_orr ? "mov" : "movs", ops02, 2);
        }
        break;

    case diana_a64_cmd_and:
    case diana_a64_cmd_ands:
        // and p0.b, p1/z, p2.b, p2.b -> mov p0.b, p1/z, p2.b
        if (isPredicate && count == 4 && SameRegister(pPrinter, 2, 3))
            return PrintAlias(pPrinter, id == diana_a64_cmd_and ? "mov" : "movs", ops012, 3);
        break;

    case diana_a64_cmd_eor:
    case diana_a64_cmd_eors:
        // eor p0.b, p1/z, p2.b, p1.b -> not p0.b, p1/z, p2.b
        if (isPredicate && count == 4 && SameRegister(pPrinter, 1, 3))
            return PrintAlias(pPrinter, id == diana_a64_cmd_eor ? "not" : "nots", ops012, 3);
        break;
    }
    *pHandled = 0;
    return DI_SUCCESS;
}

static int PrintAliases(DianaA64Printer * pPrinter, DianaA64CommandId id)
{
    int kind2 = pPrinter->pCmd->operandCount > 2 ? pPrinter->pCmd->operands[2].kind : DIANA_A64_OPK_NONE;
    int maxKind = MaxOperandKind(pPrinter->pCmd);
    if (maxKind >= DIANA_A64_OPK_ZREG)
    {
        int handled = 0;
        DI_CHECK(PrintSveAlias(pPrinter, id, &handled));
        return handled ? DI_SUCCESS : PrintGeneric(pPrinter);
    }
    if (maxKind >= DIANA_A64_OPK_FPREG)
    {
        int handled = 0;
        DI_CHECK(PrintSimdAlias(pPrinter, id, &handled));
        return handled ? DI_SUCCESS : PrintGeneric(pPrinter);
    }
    switch (id)
    {
    case diana_a64_cmd_add:
    case diana_a64_cmd_adds:
    case diana_a64_cmd_sub:
    case diana_a64_cmd_subs:
        if (kind2 == DIANA_A64_OPK_AIMM)
            return PrintAddSubImm(pPrinter, id);
        return PrintAddSubReg(pPrinter, id);

    case diana_a64_cmd_sbc:
    case diana_a64_cmd_sbcs:
        return PrintSbcNgc(pPrinter, id);

    case diana_a64_cmd_subps:
        // subps xzr, x1, x2 -> cmpp x1, x2
        if (IsZeroRegister(OpReg(pPrinter, 0)))
        {
            static const int ops12[] = {1, 2};
            return PrintAlias(pPrinter, "cmpp", ops12, 2);
        }
        break;

    case diana_a64_cmd_and:
    case diana_a64_cmd_ands:
    case diana_a64_cmd_orr:
    case diana_a64_cmd_orn:
    case diana_a64_cmd_eor:
        if (kind2 == DIANA_A64_OPK_BIMM32 || kind2 == DIANA_A64_OPK_BIMM64)
            return PrintLogicalImm(pPrinter, id);
        return PrintLogicalReg(pPrinter, id);

    case diana_a64_cmd_movz:
    case diana_a64_cmd_movn:
        return PrintMoveWide(pPrinter, id);

    case diana_a64_cmd_sbfm:
    case diana_a64_cmd_ubfm:
    case diana_a64_cmd_bfm:
        return PrintBitfield(pPrinter, id);

    case diana_a64_cmd_extr:
        return PrintExtr(pPrinter);

    case diana_a64_cmd_csinc:
    case diana_a64_cmd_csinv:
    case diana_a64_cmd_csneg:
        return PrintCondSelect(pPrinter, id);

    case diana_a64_cmd_madd:   return PrintMulAdd(pPrinter, "mul");
    case diana_a64_cmd_msub:   return PrintMulAdd(pPrinter, "mneg");
    case diana_a64_cmd_smaddl: return PrintMulAdd(pPrinter, "smull");
    case diana_a64_cmd_smsubl: return PrintMulAdd(pPrinter, "smnegl");
    case diana_a64_cmd_umaddl: return PrintMulAdd(pPrinter, "umull");
    case diana_a64_cmd_umsubl: return PrintMulAdd(pPrinter, "umnegl");

    case diana_a64_cmd_lslv:
    case diana_a64_cmd_lsrv:
    case diana_a64_cmd_asrv:
    case diana_a64_cmd_rorv:
        {
            static const int ops012[] = {0, 1, 2};
            static const char * names[] = {"lsl", "lsr", "asr", "ror"};
            int index = id == diana_a64_cmd_lslv ? 0 : id == diana_a64_cmd_lsrv ? 1 : id == diana_a64_cmd_asrv ? 2 : 3;
            return PrintAlias(pPrinter, names[index], ops012, 3);
        }

    case diana_a64_cmd_hint:
        return PrintHint(pPrinter);

    case diana_a64_cmd_dsb:
        return PrintBarrier(pPrinter, id);

    default:
        {
            int handled = 0;
            DI_CHECK(PrintAtomicStore(pPrinter, &handled));
            if (handled)
                return DI_SUCCESS;
        }
        break;
    }
    return PrintGeneric(pPrinter);
}

int DianaA64_TextOut(DianaTextOutputContext * pContext)
{
    DianaA64Printer printer;
    DianaA64CommandId id;

    printer.pContext = pContext;
    printer.pResult = pContext->pResult;
    printer.pCmd = DianaA64_GetCmdInfo(pContext->pResult);
    printer.operandsPrinted = 0;
    if (!printer.pCmd)
        return DI_ERROR;
    printer.word = (DI_UINT32)pContext->pResult->bytes[0] | ((DI_UINT32)pContext->pResult->bytes[1] << 8) |
                   ((DI_UINT32)pContext->pResult->bytes[2] << 16) | ((DI_UINT32)pContext->pResult->bytes[3] << 24);

    pContext->flags |= DIANA_TEXT_OUTPUT_FLAG_C_HEX;

    id = (DianaA64CommandId)printer.pCmd->header.m_pGroupInfo->m_commandId;
    switch (id)
    {
    case diana_a64_cmd_inst:
    case diana_a64_cmd_inst_unsupported:
    case diana_a64_cmd_inst_disabled:
        return PrintUndefined(&printer);
    case diana_a64_cmd_b_cond:
        return PrintBCond(&printer, "b.");
    case diana_a64_cmd_bc_cond:
        return PrintBCond(&printer, "bc.");
    case diana_a64_cmd_texit:
        // texit {nb}: nb is bit 10
        DI_CHECK(PrintMnemonic(&printer, "texit"));
        if (printer.word >> 10 & 1)
        {
            DI_CHECK(PrintSeparator(&printer));
            DI_CHECK(PrintText(&printer, "nb"));
        }
        return DI_SUCCESS;
    case diana_a64_cmd_sys:
        return PrintSys(&printer);
    case diana_a64_cmd_sysp:
        {
            // sysp #op1, Cn, Cm, #op2{, Xt1, Xt2}: registers that are xzr at the end are left out
            int count = printer.pResult->iLinkedOpCount;
            int i = 0;
            if (!(pContext->options & DIANA_TEXT_OUTPUT_OPTION_ARM_NO_ALIASES))
            {
                int printed = 0;
                DI_CHECK(PrintSysAlias(&printer, 1, &printed));
                if (printed)
                    return DI_SUCCESS;
            }
            DI_CHECK(PrintMnemonic(&printer, "sysp"));
            while (count > 4 && IsZeroRegister(OpReg(&printer, count - 1)))
                --count;
            for (; i < count; ++i)
            {
                DI_CHECK(PrintOperand(&printer, i));
            }
            return DI_SUCCESS;
        }
    case diana_a64_cmd_sysl:
        {
            int printed = 0;
            DI_CHECK(PrintSysInstruction(&printer, 1, &printed));
            if (printed)
                return DI_SUCCESS;
        }
        break;
    case diana_a64_cmd_udf:
        DI_CHECK(PrintMnemonic(&printer, "udf"));
        return PrintImmOperand(&printer, OpImm(&printer, 0), 1);
    case diana_a64_cmd_dcps1:
    case diana_a64_cmd_dcps2:
    case diana_a64_cmd_dcps3:
        // the immediate is optional, #0 is left out
        if (OpImm(&printer, 0) == 0)
            return PrintMnemonic(&printer, printer.pCmd->header.m_pGroupInfo->m_pName);
        break;
    case diana_a64_cmd_rmif:
        // rmif xn, #shift, #mask: both in decimal
        DI_CHECK(PrintMnemonic(&printer, "rmif"));
        DI_CHECK(PrintOperand(&printer, 0));
        DI_CHECK(PrintImmOperand(&printer, OpImm(&printer, 1), 1));
        return PrintImmOperand(&printer, OpImm(&printer, 2), 1);
    // not aliases, the same with DIANA_TEXT_OUTPUT_OPTION_ARM_NO_ALIASES
    case diana_a64_cmd_isb:
    case diana_a64_cmd_clrex:
        return PrintBarrier(&printer, id);
    case diana_a64_cmd_msr:
        return PrintMsr(&printer);
    case diana_a64_cmd_rprfm:
        return PrintRprfm(&printer);
    case diana_a64_cmd_ret:
        // x30 is the default operand, not an alias
        return PrintRet(&printer);
    case diana_a64_cmd_irg:
        // the optional Xm is not printed when it is XZR
        if (IsZeroRegister(OpReg(&printer, 2)))
        {
            static const int ops01[] = {0, 1};
            return PrintAlias(&printer, "irg", ops01, 2);
        }
        break;
    case diana_a64_cmd_tenter:
        // tenter #imm{, nb}: nb is bit 17
        DI_CHECK(PrintMnemonic(&printer, "tenter"));
        DI_CHECK(PrintOperand(&printer, 0));
        if (printer.word >> 17 & 1)
        {
            DI_CHECK(PrintSeparator(&printer));
            DI_CHECK(PrintText(&printer, "nb"));
        }
        return DI_SUCCESS;
    case diana_a64_cmd_hint:
        // a separate instruction in the hint space, not an alias
        if (OpImm(&printer, 0) == 0x28)
            return PrintMnemonic(&printer, "chkfeat x16");
        break;
    case diana_a64_cmd_mova:
        {
            // SME MOVA prints as mov, objdump does so also with -M no-aliases
            static const int ops012[] = {0, 1, 2};
            return PrintAlias(&printer, "mov", ops012, printer.pResult->iLinkedOpCount);
        }
    default:
        break;
    }
    if (pContext->options & DIANA_TEXT_OUTPUT_OPTION_ARM_NO_ALIASES)
        return PrintGeneric(&printer);
    return PrintAliases(&printer, id);
}

#endif
