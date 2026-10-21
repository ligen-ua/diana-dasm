#include "diana_arm.h"
#include "diana_arm32.h"

#ifndef DIANA_NO_ARM

// CPU profiles. The feature sets are meant to hide instructions a CPU certainly doesn't have, not to validate code.
// Architecture levels have their mandatory instruction features only; FP16, FHM, crypto, SVE, MTE are optional.
// The CPU presets follow LLVM's processor definitions (AArch64Processors.td) and the features Linux and macOS report.
#define DIANA_A64_PROFILE_V80   (DIANA_A64_FEATURE_A64|DIANA_A64_FEATURE_FP|DIANA_A64_FEATURE_ASIMD)
#define DIANA_A64_PROFILE_V81   (DIANA_A64_PROFILE_V80|DIANA_A64_FEATURE_CRC|DIANA_A64_FEATURE_LSE|DIANA_A64_FEATURE_RDM|                                  DIANA_A64_FEATURE_LOR|DIANA_A64_FEATURE_PAN)
#define DIANA_A64_PROFILE_V82   (DIANA_A64_PROFILE_V81|DIANA_A64_FEATURE_UAO|DIANA_A64_FEATURE_DPB)
// FEAT_LRCPC is the rcpc bit; the lrcpc bit has no instructions and is set with it
#define DIANA_A64_PROFILE_RCPC  (DIANA_A64_FEATURE_RCPC|DIANA_A64_FEATURE_LRCPC)
#define DIANA_A64_PROFILE_V83   (DIANA_A64_PROFILE_V82|DIANA_A64_FEATURE_PAUTH|DIANA_A64_FEATURE_JSCVT|DIANA_A64_FEATURE_FCMA|                                  DIANA_A64_PROFILE_RCPC)
#define DIANA_A64_PROFILE_V84   (DIANA_A64_PROFILE_V83|DIANA_A64_FEATURE_DOTPROD|DIANA_A64_FEATURE_RCPC2|DIANA_A64_FEATURE_FLAGM)
// v8.5 without BTI: Apple M1
#define DIANA_A64_PROFILE_V85_NO_BTI (DIANA_A64_PROFILE_V84|DIANA_A64_FEATURE_SB|DIANA_A64_FEATURE_SSBS|                                  DIANA_A64_FEATURE_FRINTTS|DIANA_A64_FEATURE_FLAGM2)
#define DIANA_A64_PROFILE_V85   (DIANA_A64_PROFILE_V85_NO_BTI|DIANA_A64_FEATURE_BTI)
#define DIANA_A64_PROFILE_V86   (DIANA_A64_PROFILE_V85|DIANA_A64_FEATURE_BF16|DIANA_A64_FEATURE_I8MM)
#define DIANA_A64_PROFILE_V87   (DIANA_A64_PROFILE_V86|DIANA_A64_FEATURE_WFXT|DIANA_A64_FEATURE_XS)
// v9.0 is v8.5 with SVE2, v9.2 is v8.7 with SVE2
#define DIANA_A64_PROFILE_V90   (DIANA_A64_PROFILE_V85|DIANA_A64_FEATURE_SVE|DIANA_A64_FEATURE_SVE2)
#define DIANA_A64_PROFILE_V92   (DIANA_A64_PROFILE_V87|DIANA_A64_FEATURE_SVE|DIANA_A64_FEATURE_SVE2)

#define DIANA_A64_PROFILE_CRYPTO (DIANA_A64_FEATURE_AES|DIANA_A64_FEATURE_SHA2)
// Cortex-A76, Neoverse N1, Cortex-X1
#define DIANA_A64_PROFILE_A76   (DIANA_A64_PROFILE_V82|DIANA_A64_FEATURE_FP16|DIANA_A64_FEATURE_DOTPROD|                                  DIANA_A64_PROFILE_RCPC|DIANA_A64_FEATURE_SSBS|DIANA_A64_PROFILE_CRYPTO)
#define DIANA_A64_PROFILE_APPLE (DIANA_A64_FEATURE_FP16|DIANA_A64_FEATURE_FHM|DIANA_A64_PROFILE_CRYPTO|DIANA_A64_FEATURE_SHA3)

// AArch32 (M6 step 6): versions are cumulative flags over the ARMv6 core (spec/arm32_flags.txt).
// ARMv6 with VFPv2 (ARM1176JZF-S); ARMv7-A with VFPv3 and NEON (Cortex-A8, Android armeabi-v7a); ARMv7VE with VFPv4
// and multiprocessing (Cortex-A7, A15); ARMv8 drops SWP and the generic coprocessor instructions.
// A CPU without AArch32 (Apple M1, Neoverse V1) has none: its AArch32 words are disabled.
#define DIANA_A32_PROFILE_ALL   DIANA_A32_FEATURE_ALL
#define DIANA_A32_PROFILE_V6    (DIANA_A32_FEATURE_A32|DIANA_A32_FEATURE_FP|DIANA_A32_FEATURE_SWP|DIANA_A32_FEATURE_CP)
#define DIANA_A32_PROFILE_V6K   (DIANA_A32_PROFILE_V6|DIANA_A32_FEATURE_V6K)
#define DIANA_A32_PROFILE_V7    (DIANA_A32_PROFILE_V6K|DIANA_A32_FEATURE_V6T2|DIANA_A32_FEATURE_V7|DIANA_A32_FEATURE_VFPV3| \
                                 DIANA_A32_FEATURE_ASIMD)
#define DIANA_A32_PROFILE_V7VE  (DIANA_A32_PROFILE_V7|DIANA_A32_FEATURE_V7VE|DIANA_A32_FEATURE_MP|DIANA_A32_FEATURE_VFPV4)
#define DIANA_A32_PROFILE_V80   ((DIANA_A32_PROFILE_V7VE & ~(DIANA_A32_FEATURE_SWP|DIANA_A32_FEATURE_CP))|DIANA_A32_FEATURE_V8)
#define DIANA_A32_PROFILE_V82   (DIANA_A32_PROFILE_V80|DIANA_A32_FEATURE_CRC|DIANA_A32_FEATURE_RDM|DIANA_A32_FEATURE_PAN| \
                                 DIANA_A32_FEATURE_RAS)
#define DIANA_A32_PROFILE_V85   (DIANA_A32_PROFILE_V82|DIANA_A32_FEATURE_JSCVT|DIANA_A32_FEATURE_FCMA|DIANA_A32_FEATURE_DOTPROD| \
                                 DIANA_A32_FEATURE_SB)
#define DIANA_A32_PROFILE_CRYPTO (DIANA_A32_FEATURE_AES|DIANA_A32_FEATURE_SHA1|DIANA_A32_FEATURE_SHA2)
#define DIANA_A32_PROFILE_A76   (DIANA_A32_PROFILE_V82|DIANA_A32_FEATURE_FP16|DIANA_A32_FEATURE_DOTPROD|DIANA_A32_PROFILE_CRYPTO)
#define DIANA_A32_PROFILE_NONE  0

static const DianaArmProfile g_profiles[] =
{
    {"all",           "all known features",                          DIANA_A64_FEATURE_ALL, DIANA_A32_PROFILE_ALL},
    {"armv8.0",       "ARMv8.0-A base",                              DIANA_A64_PROFILE_V80, DIANA_A32_PROFILE_V80},
    {"armv8.2",       "ARMv8.2-A, mandatory features",               DIANA_A64_PROFILE_V82, DIANA_A32_PROFILE_V82},
    {"armv8.5",       "ARMv8.5-A, mandatory features",               DIANA_A64_PROFILE_V85, DIANA_A32_PROFILE_V85},
    {"armv9.0",       "ARMv9.0-A, mandatory features",               DIANA_A64_PROFILE_V90, DIANA_A32_PROFILE_V85},
    {"rpi3",          "Raspberry Pi 3, Cortex-A53",                  DIANA_A64_PROFILE_V80|DIANA_A64_FEATURE_CRC,
                                                                     DIANA_A32_PROFILE_V80|DIANA_A32_FEATURE_CRC},
    {"rpi4",          "Raspberry Pi 4, Cortex-A72",                  DIANA_A64_PROFILE_V80|DIANA_A64_FEATURE_CRC,
                                                                     DIANA_A32_PROFILE_V80|DIANA_A32_FEATURE_CRC},
    {"rpi5",          "Raspberry Pi 5, Cortex-A76",                  DIANA_A64_PROFILE_A76, DIANA_A32_PROFILE_A76},
    {"android-arm64", "Android arm64-v8a baseline",                  DIANA_A64_PROFILE_V80, DIANA_A32_PROFILE_V80},
    {"apple-m1",      "Apple M1",                                    DIANA_A64_PROFILE_V85_NO_BTI|DIANA_A64_PROFILE_APPLE, DIANA_A32_PROFILE_NONE},
    {"apple-m2",      "Apple M2/M3",                                 DIANA_A64_PROFILE_V86|DIANA_A64_PROFILE_APPLE, DIANA_A32_PROFILE_NONE},
    {"apple-m4",      "Apple M4",                                    DIANA_A64_PROFILE_V87|DIANA_A64_PROFILE_APPLE|DIANA_A64_FEATURE_SME|
                                                                     DIANA_A64_FEATURE_SME2|DIANA_A64_FEATURE_SME_I16I64|
                                                                     DIANA_A64_FEATURE_SME_F64F64, DIANA_A32_PROFILE_NONE},
    {"cortex-x1",     "Cortex-X1",                                   DIANA_A64_PROFILE_A76, DIANA_A32_PROFILE_A76},
    {"cortex-x4",     "Cortex-X4 (with the crypto extension)",       DIANA_A64_PROFILE_V92|DIANA_A64_FEATURE_FP16|DIANA_A64_FEATURE_FHM|
                                                                     DIANA_A64_FEATURE_MTE|DIANA_A64_PROFILE_CRYPTO|
                                                                     DIANA_A64_FEATURE_SVE_BITPERM, DIANA_A32_PROFILE_NONE},
    {"graviton2",     "AWS Graviton2, Neoverse N1",                  DIANA_A64_PROFILE_A76, DIANA_A32_PROFILE_A76},
    {"graviton3",     "AWS Graviton3, Neoverse V1",                  DIANA_A64_PROFILE_V84|DIANA_A64_FEATURE_FP16|DIANA_A64_FEATURE_FHM|
                                                                     DIANA_A64_FEATURE_BF16|DIANA_A64_FEATURE_I8MM|DIANA_A64_FEATURE_SSBS|
                                                                     DIANA_A64_FEATURE_SVE|DIANA_A64_PROFILE_CRYPTO|DIANA_A64_FEATURE_SHA3|
                                                                     DIANA_A64_FEATURE_SM4, DIANA_A32_PROFILE_NONE},
    {"graviton4",     "AWS Graviton4, Neoverse V2",                  DIANA_A64_PROFILE_V90|DIANA_A64_FEATURE_FP16|DIANA_A64_FEATURE_FHM|
                                                                     DIANA_A64_FEATURE_BF16|DIANA_A64_FEATURE_I8MM|DIANA_A64_PROFILE_CRYPTO|
                                                                     DIANA_A64_FEATURE_SHA3|DIANA_A64_FEATURE_SVE_BITPERM, DIANA_A32_PROFILE_NONE},
    // AArch32 levels and CPUs; the 32-bit ones have no A64
    {"armv6",         "ARMv6 with VFPv2 (AArch32)",                  0, DIANA_A32_PROFILE_V6},
    {"armv7",         "ARMv7-A with VFPv3 and NEON (AArch32)",       0, DIANA_A32_PROFILE_V7},
    {"armv7ve",       "ARMv7-A with virtualization, VFPv4 (AArch32)", 0, DIANA_A32_PROFILE_V7VE},
    {"rpi1",          "Raspberry Pi 1/Zero, ARM1176JZF-S",           0, DIANA_A32_PROFILE_V6K},
    {"rpi2",          "Raspberry Pi 2, Cortex-A7",                   0, DIANA_A32_PROFILE_V7VE},
    {"android-armv7", "Android armeabi-v7a (VFPv3-D16, NEON)",       0, DIANA_A32_PROFILE_V7},
    {"cortex-a7",     "Cortex-A7",                                   0, DIANA_A32_PROFILE_V7VE},
    {"cortex-a53",    "Cortex-A53 (without the crypto extension)",   DIANA_A64_PROFILE_V80|DIANA_A64_FEATURE_CRC,
                                                                     DIANA_A32_PROFILE_V80|DIANA_A32_FEATURE_CRC},
    {"cortex-a72",    "Cortex-A72 (without the crypto extension)",   DIANA_A64_PROFILE_V80|DIANA_A64_FEATURE_CRC,
                                                                     DIANA_A32_PROFILE_V80|DIANA_A32_FEATURE_CRC},
};

const DianaArmProfile * DianaArm_GetProfiles(int * pCount)
{
    *pCount = (int)(sizeof(g_profiles)/sizeof(g_profiles[0]));
    return g_profiles;
}

static int DianaArm_StrEqual(const char * p1, const char * p2)
{
    for (; *p1 && *p1 == *p2; ++p1, ++p2)
    {
    }
    return *p1 == *p2;
}

const DianaArmProfile * DianaArm_FindProfile(const char * pName)
{
    int i = 0;
    for (; i < (int)(sizeof(g_profiles)/sizeof(g_profiles[0])); ++i)
    {
        if (DianaArm_StrEqual(g_profiles[i].pName, pName))
            return g_profiles + i;
    }
    return 0;
}

void Diana_SetArmProfile(DianaContext * pContext, const DianaArmProfile * pProfile)
{
    if (pContext->iArch == DIANA_ARCH_ARM32 || pContext->iArch == DIANA_ARCH_THUMB)
        pContext->armFeatures = pProfile ? pProfile->a32Features : DIANA_A32_FEATURE_ALL;
    else
        pContext->armFeatures = pProfile ? pProfile->features : DIANA_A64_FEATURE_ALL;
}

// operands
static DI_UINT32 ExtractFields(const DianaA64OperandDesc * pDesc, DI_UINT32 word, int * pTotalWidth)
{
    DI_UINT32 value = 0;
    int total = 0;
    int i = 0;
    for (; i < pDesc->fieldCount; ++i)
    {
        int width = pDesc->width[i];
        DI_UINT32 field = word >> pDesc->lsb[i];
        if (width < 32)
        {
            field &= (1u << width) - 1;
            value = (value << width) | field;
        }
        else
        {
            value = field;
        }
        total += width;
    }
    *pTotalWidth = total;
    return value;
}

static DI_UINT64 SignExtend(DI_UINT64 value, int bits)
{
    DI_UINT64 sign = 1ULL << (bits - 1);
    value &= (sign << 1) - 1;
    return (value ^ sign) - sign;
}

static DI_UINT64 Ones(int count)
{
    return count >= 64 ? ~0ULL : ((1ULL << count) - 1);
}

int DianaA64_DecodeBitMasks(int n, int imms, int immr, int regSize, DI_UINT64 * pValue)
{
    // DecodeBitMasks() from the ARM ARM, immediate = TRUE
    int combined = (n << 6) | ((~imms) & 0x3F);
    int len = -1;
    int levels = 0, s = 0, r = 0, esize = 0, i = 0;
    DI_UINT64 welem = 0, element = 0, result = 0;

    for (i = 6; i >= 0; --i)
    {
        if (combined & (1 << i))
        {
            len = i;
            break;
        }
    }
    if (len < 1)
        return 0;
    esize = 1 << len;
    if (esize > regSize)
        return 0;
    levels = (int)Ones(len);
    if ((imms & levels) == levels)
        return 0;

    s = imms & levels;
    r = immr & levels;
    welem = Ones(s + 1);
    // ROR(welem, r) inside esize bits
    element = r ? (((welem >> r) | (welem << (esize - r))) & Ones(esize)) : welem;
    for (i = 0; i < regSize; i += esize)
    {
        result |= element << i;
    }
    *pValue = result & Ones(regSize);
    return 1;
}

static DianaUnifiedRegister GetX(DI_UINT32 index, int spAllowed)
{
    if (index == 31)
        return spAllowed ? reg_A64_SP : reg_A64_XZR;
    return (DianaUnifiedRegister)(reg_A64_X0 + index);
}

static DianaUnifiedRegister GetW(DI_UINT32 index, int spAllowed)
{
    if (index == 31)
        return spAllowed ? reg_A64_WSP : reg_A64_WZR;
    return (DianaUnifiedRegister)(reg_A64_W0 + index);
}

static void SetMemory(DianaLinkedOperand * pOp, DI_UINT32 rn, DI_UINT64 disp, int writeback)
{
    pOp->type = diana_index;
    pOp->value.rmIndex.seg_reg = reg_none;
    pOp->value.rmIndex.reg = GetX(rn, 1);
    pOp->value.rmIndex.indexed_reg = reg_none;
    pOp->value.rmIndex.index = 0;
    pOp->value.rmIndex.dispSize = 8;
    pOp->value.rmIndex.dispValue = disp;
    pOp->usedAddressSize = 8;
    pOp->arm.writeback = (DI_UINT8)writeback;
}

// ------------------------------------------------------------------ FP and SIMD operands
static DI_UINT32 FieldAt(const DianaA64OperandDesc * pDesc, int index, DI_UINT32 word)
{
    return (word >> pDesc->lsb[index]) & ((1u << pDesc->width[index]) - 1);
}

static int HighestBit(DI_UINT32 value)
{
    int result = -1;
    for (; value; value >>= 1)
        ++result;
    return result;
}

static int LowestBit(DI_UINT32 value)
{
    int result = 0;
    if (!value)
        return -1;
    for (; !(value & 1); value >>= 1)
        ++result;
    return result;
}

// element size, log2 of bytes, from DianaA64OperandDesc::arr; *pNext is the next field to read.
// Returns -1 for values that are not an element size (reserved encodings)
static int GetElementLog2(const DianaA64OperandDesc * pDesc, DI_UINT32 word, int * pNext)
{
    int source = pDesc->arr & DIANA_A64_ARR_SRC_MASK;
    int delta = ((pDesc->arr >> DIANA_A64_ARR_DELTA_SHIFT) & 3) - 1;
    int result = 0;
    DI_UINT32 value = 0;
    if (source < DIANA_A64_ARR_SRC_SIZE)
        return source - DIANA_A64_ARR_SRC_FIXED;
    value = FieldAt(pDesc, (*pNext)++, word);
    switch (source)
    {
    case DIANA_A64_ARR_SRC_SIZE: result = (int)value; break;
    case DIANA_A64_ARR_SRC_SZ:   result = 2 + (int)value; break;
    case DIANA_A64_ARR_SRC_IMMH: result = HighestBit(value); break;
    default:                     result = LowestBit(value); break;
    }
    if (result < 0)
        return -1;
    result += delta;
    return result >= 0 && result <= 4 ? result : -1;
}

// vector arrangement: element size and the register width from DianaA64OperandDesc::arr
static int GetArrangement(const DianaA64OperandDesc * pDesc, DI_UINT32 word, int * pNext, int * pBytes)
{
    int element = GetElementLog2(pDesc, word, pNext);
    int countLog2 = 0;
    if (element < 0)
        return -1;
    switch (pDesc->arr >> DIANA_A64_ARR_WIDTH_SHIFT)
    {
    case DIANA_A64_ARR_WIDTH_Q:
        countLog2 = 3 + (int)FieldAt(pDesc, (*pNext)++, word) - element;
        break;
    case DIANA_A64_ARR_WIDTH_128:
        countLog2 = 4 - element;
        break;
    case DIANA_A64_ARR_WIDTH_PAIR:
        countLog2 = 1;
        break;
    default:
        // DIANA_A64_ARR_WIDTH_NONE on a vector: 32 or 64 bits by Q (FMLAL v1.2h, v1.4h)
        countLog2 = 2 + (int)FieldAt(pDesc, (*pNext)++, word) - element;
        break;
    }
    if (countLog2 < 0)
        return -1;
    *pBytes = 1 << (element + countLog2);
    return DIANA_ARM_ARR(element, countLog2);
}

// VFPExpandImm(imm8) as an IEEE double
static DI_UINT64 ExpandFPImm(DI_UINT32 imm8)
{
    DI_UINT64 b = (imm8 >> 6) & 1;
    DI_UINT64 exponent = ((b ^ 1) << 10) | (b ? 0xFFu << 2 : 0) | ((imm8 >> 4) & 3);
    return ((DI_UINT64)(imm8 >> 7) << 63) | (exponent << 52) | ((DI_UINT64)(imm8 & 15) << 48);
}

static int FillSimdOperand(const DianaA64OperandDesc * pDesc,
                           DI_UINT32 word,
                           DI_UINT32 value,
                           int width,
                           DianaLinkedOperand * pOp)
{
    int next = 1;
    int element = 0;
    int bytes = 0;
    int arrangement = 0;
    DI_UINT32 reg = 0;

    switch (pDesc->kind)
    {
    case DIANA_A64_OPK_FPREG:
        element = GetElementLog2(pDesc, word, &next);
        if (element < 0)
            return DI_INVALID_OPCODE;
        pOp->type = diana_register;
        pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A64_B0 + element * 32 + FieldAt(pDesc, 0, word));
        pOp->usedSize = 1 << element;
        return DI_SUCCESS;

    case DIANA_A64_OPK_VREG:
    case DIANA_A64_OPK_VLIST:
        arrangement = GetArrangement(pDesc, word, &next, &bytes);
        if (arrangement < 0)
            return DI_INVALID_OPCODE;
        pOp->type = pDesc->kind == DIANA_A64_OPK_VLIST ? diana_arm_reglist : diana_register;
        pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A64_V0 + FieldAt(pDesc, 0, word));
        pOp->usedSize = bytes;
        pOp->arm.arrangement = (DI_UINT8)arrangement;
        pOp->arm.regCount = (DI_UINT8)(pDesc->kind == DIANA_A64_OPK_VLIST ? pDesc->param : 0);
        return DI_SUCCESS;

    case DIANA_A64_OPK_VELEM:
        element = GetElementLog2(pDesc, word, &next);
        if (element < 0 || element > 3)
            return DI_INVALID_OPCODE;
        reg = FieldAt(pDesc, 0, word);
        switch (pDesc->param & 3)
        {
        case DIANA_A64_INDEX_CONST:
            // a field after the size source is added (SM3TT1A v2.s[imm2])
            pOp->arm.lane = (DI_UINT8)((pDesc->param >> 2) + (next < pDesc->fieldCount ? FieldAt(pDesc, next, word) : 0));
            break;
        case DIANA_A64_INDEX_IMM5:
            pOp->arm.lane = (DI_UINT8)(FieldAt(pDesc, 1, word) >> (element + 1));
            break;
        case DIANA_A64_INDEX_IMM4:
            pOp->arm.lane = (DI_UINT8)(FieldAt(pDesc, 2, word) >> element);
            break;
        default:
            {
                // by element: H uses V0-V15 and H:L:M, S and D use M:Rm and H:L or H. The index can count
                // units of several elements (FCMLA pairs, SDOT 4B groups): the rule of the unit size
                int unit = (pDesc->param >> DIANA_A64_INDEX_UNIT_SHIFT) & 3;
                DI_UINT32 h = FieldAt(pDesc, next, word);
                DI_UINT32 l = FieldAt(pDesc, next + 1, word);
                DI_UINT32 m = FieldAt(pDesc, next + 2, word);
                if (element + unit == 1)
                {
                    pOp->arm.lane = (DI_UINT8)((h << 2) | (l << 1) | m);
                }
                else
                {
                    reg |= m << 4;
                    pOp->arm.lane = (DI_UINT8)(element + unit == 2 ? (h << 1) | l : h);
                }
                if (pDesc->param & DIANA_A64_INDEX_GROUP)
                {
                    pOp->type = diana_register;
                    pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A64_V0 + reg);
                    pOp->usedSize = 1 << (element + unit);
                    pOp->arm.arrangement = (DI_UINT8)DIANA_ARM_ARR_GROUP(element, unit);
                    return DI_SUCCESS;
                }
            }
        }
        pOp->type = diana_register;
        pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A64_V0 + reg);
        pOp->usedSize = 1 << element;
        pOp->arm.arrangement = (DI_UINT8)DIANA_ARM_ARR_ELEMENT(element);
        return DI_SUCCESS;

    case DIANA_A64_OPK_VLIST_ELEM:
        // Rt:index fields, the index is shifted right by param >> 3
        element = GetElementLog2(pDesc, word, &next);
        pOp->type = diana_arm_reglist;
        pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A64_V0 + (value >> (width - 5)));
        pOp->usedSize = 1 << element;
        pOp->arm.arrangement = (DI_UINT8)DIANA_ARM_ARR_ELEMENT(element);
        pOp->arm.regCount = (DI_UINT8)(pDesc->param & 7);
        pOp->arm.lane = (DI_UINT8)((value & ((1u << (width - 5)) - 1)) >> (pDesc->param >> 3));
        return DI_SUCCESS;

    case DIANA_A64_OPK_GPREG_IMM5:
        {
            int isX = LowestBit(FieldAt(pDesc, 1, word)) == 3;
            pOp->type = diana_register;
            pOp->value.recognizedRegister = isX ? GetX(FieldAt(pDesc, 0, word), 0) : GetW(FieldAt(pDesc, 0, word), 0);
            pOp->usedSize = isX ? 8 : 4;
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_FPIMM:
        pOp->type = diana_imm;
        pOp->value.imm = ExpandFPImm(value);
        return DI_SUCCESS;

    case DIANA_A64_OPK_FZERO:
        pOp->type = diana_imm;
        pOp->value.imm = 0;
        return DI_SUCCESS;

    case DIANA_A64_OPK_MODIMM:
        pOp->type = diana_imm;
        switch (pDesc->param)
        {
        case DIANA_A64_MODIMM_LSL:
            // imm8:cmode
            pOp->value.imm = value >> 4;
            pOp->arm.shiftType = DIANA_ARM_SHIFT_LSL;
            pOp->arm.shiftAmount = (DI_UINT8)(((value >> 1) & 3) * 8);
            break;
        case DIANA_A64_MODIMM_MSL:
            pOp->value.imm = value >> 4;
            pOp->arm.shiftType = DIANA_ARM_SHIFT_MSL;
            pOp->arm.shiftAmount = (DI_UINT8)((value & 1) ? 16 : 8);
            break;
        case DIANA_A64_MODIMM_BYTES:
            {
                int i = 0;
                pOp->value.imm = 0;
                for (; i < 8; ++i)
                {
                    if (value & (1u << i))
                        pOp->value.imm |= 0xFFULL << (i * 8);
                }
            }
            break;
        default:
            pOp->value.imm = value;
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_SHIFTIMM:
        {
            // immh:immb, the element size is the highest set bit of immh
            int esize = 8 << HighestBit(value >> 3);
            pOp->type = diana_imm;
            switch (pDesc->param)
            {
            case DIANA_A64_SHIFT_RIGHT:
                if (!(value >> 3))
                    return DI_INVALID_OPCODE;
                pOp->value.imm = (DI_UINT64)(2 * esize - (int)value);
                break;
            case DIANA_A64_SHIFT_LEFT:
                if (!(value >> 3))
                    return DI_INVALID_OPCODE;
                pOp->value.imm = (DI_UINT64)((int)value - esize);
                break;
            case DIANA_A64_SHIFT_RIGHT16:
                pOp->value.imm = (DI_UINT64)(16 - (int)value);
                break;
            case DIANA_A64_SHIFT_RIGHT_WIDE:
                // tsize:imm5, esize 32 or 64
                if (!(value >> 5))
                    return DI_INVALID_OPCODE;
                pOp->value.imm = (DI_UINT64)((64 << HighestBit(value >> 5)) - (int)value);
                break;
            default:
                pOp->value.imm = (DI_UINT64)(64 - (int)value);
            }
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_ROTATE:
        pOp->type = diana_imm;
        pOp->value.imm = width == 1 ? 90 + value * 180 : value * 90;
        return DI_SUCCESS;

    case DIANA_A64_OPK_CONST:
        pOp->type = diana_imm;
        pOp->value.imm = (DI_UINT64)pDesc->param << (pDesc->fieldCount ? value : 0);
        return DI_SUCCESS;

    case DIANA_A64_OPK_MEM_POSTIMM:
        SetMemory(pOp, FieldAt(pDesc, 0, word),
                  (DI_UINT64)pDesc->param << (pDesc->fieldCount > 1 ? FieldAt(pDesc, 1, word) : 0),
                  DIANA_ARM_WRITEBACK_POST);
        return DI_SUCCESS;

    case DIANA_A64_OPK_MEM_PREIMM:
        SetMemory(pOp, FieldAt(pDesc, 0, word), 0 - (DI_UINT64)pDesc->param, DIANA_ARM_WRITEBACK_PRE);
        return DI_SUCCESS;

    case DIANA_A64_OPK_MEM_POSTREG:
        SetMemory(pOp, FieldAt(pDesc, 0, word), 0, DIANA_ARM_WRITEBACK_POST);
        pOp->value.rmIndex.dispSize = 0;
        pOp->value.rmIndex.indexed_reg = GetX(FieldAt(pDesc, 1, word), 0);
        pOp->value.rmIndex.index = 1;
        return DI_SUCCESS;
    }
    return DI_ERROR;
}

// ------------------------------------------------------------------ SVE and SME operands
// log2 of the element size in bits of a bitmask immediate N:immr:imms, -1 if it is reserved.
// 2 and 4 bit patterns are bytes for SVE: z0.b, #0x77
static int BitmaskElementLog2(DI_UINT32 imm13)
{
    int result = HighestBit(((imm13 >> 12) << 6) | (~imm13 & 0x3F));
    return result < 1 ? -1 : result < 3 ? 3 : result;
}

// element size log2 of a scalable register (DIANA_A64_SARR_*), 0x80 if there is none; *pNext is the next field.
// Returns -1 for values that are not an element size (reserved encodings)
static int GetScalableElementLog2(const DianaA64OperandDesc * pDesc, DI_UINT32 word, int * pNext)
{
    int source = pDesc->arr & DIANA_A64_ARR_SRC_MASK;
    int delta = ((pDesc->arr >> DIANA_A64_SARR_DELTA_SHIFT) & 7) - 3;
    int result = 0;
    DI_UINT32 value = 0;
    if (!source)
        return 0x80;
    if (source < DIANA_A64_ARR_SRC_SIZE)
        return source - DIANA_A64_ARR_SRC_FIXED;
    value = FieldAt(pDesc, (*pNext)++, word);
    if (pDesc->arr & DIANA_A64_SARR_TWO_FIELDS)
    {
        value = (value << pDesc->width[*pNext]) | FieldAt(pDesc, *pNext, word);
        ++*pNext;
    }
    switch (source)
    {
    case DIANA_A64_ARR_SRC_SIZE: result = (int)value; break;
    case DIANA_A64_ARR_SRC_SZ:   result = 2 + (int)value; break;
    case DIANA_A64_ARR_SRC_IMMH: result = HighestBit(value); break;
    case DIANA_A64_ARR_SRC_BIMM: result = BitmaskElementLog2(value) - 3; break;
    default:                     result = LowestBit(value); break;
    }
    if (result < 0)
        return -1;
    result += delta;
    return result >= 0 && result <= 4 ? result : -1;
}

// the arrangement byte of a scalable register: 0 without an element size (z0)
static int ScalableArrangement(int element)
{
    return element == 0x80 ? 0 : DIANA_ARM_ARR_SCALABLE(element);
}

static int FillSveOperand(const DianaA64OperandDesc * pDesc,
                          DI_UINT32 word,
                          DianaLinkedOperand * pOp)
{
    int next = 1;
    int element = 0;

    switch (pDesc->kind)
    {
    case DIANA_A64_OPK_ZREG:
    case DIANA_A64_OPK_PREG:
        element = GetScalableElementLog2(pDesc, word, &next);
        if (element < 0)
            return DI_INVALID_OPCODE;
        pOp->type = diana_register;
        pOp->value.recognizedRegister = (DianaUnifiedRegister)((pDesc->kind == DIANA_A64_OPK_ZREG ? reg_A64_Z0 : reg_A64_P0) +
                                                               FieldAt(pDesc, 0, word));
        if (pDesc->flags & DIANA_A64_OPF_SVE_PN)
        {
            // pn8..pn15 from a 3-bit field
            pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A64_PN0 + FieldAt(pDesc, 0, word) +
                                                                   (pDesc->width[0] == 3 ? 8 : 0));
        }
        pOp->arm.arrangement = (DI_UINT8)ScalableArrangement(element);
        pOp->arm.qualifier = pDesc->param < 3 ? pDesc->param : 0;
        if (pDesc->param == 3)
            pOp->arm.qualifier = FieldAt(pDesc, next, word) ? DIANA_ARM_QUAL_MERGE : DIANA_ARM_QUAL_ZERO;
        if (pDesc->param == 4)
        {
            DI_UINT32 index = 0;
            for (; next < pDesc->fieldCount; ++next)
                index = (index << pDesc->width[next]) | FieldAt(pDesc, next, word);
            pOp->arm.arrangement |= 0x40;
            pOp->arm.lane = (DI_UINT8)index;
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_PREG_INDEX:
        {
            DI_UINT32 index = 0;
            DI_UINT32 rv = 0;
            DI_UINT32 sizeValue = 0;
            int sizeBits = 0;
            int i = 1;
            element = GetScalableElementLog2(pDesc, word, &next);
            if (element < 0 || element == 0x80)
                return DI_INVALID_OPCODE;
            // the size source fields, read as one
            for (; i < next; ++i)
            {
                sizeValue = (sizeValue << pDesc->width[i]) | FieldAt(pDesc, i, word);
                sizeBits += pDesc->width[i];
            }
            rv = FieldAt(pDesc, next++, word);
            for (; next < pDesc->fieldCount; ++next)
                index = (index << pDesc->width[next]) | FieldAt(pDesc, next, word);
            // param 0x80: the index ends with the size fields (i1:tszh:tszl)
            if (pDesc->param & 0x80)
                index = (index << sizeBits) | sizeValue;
            SetMemory(pOp, 0, index >> (element + 1), DIANA_ARM_WRITEBACK_NONE);
            pOp->value.rmIndex.reg = (DianaUnifiedRegister)(reg_A64_P0 + FieldAt(pDesc, 0, word));
            pOp->value.rmIndex.indexed_reg = GetW((pDesc->param & 0x7F) + rv, 0);
            pOp->arm.arrangement = (DI_UINT8)DIANA_ARM_ARR_SCALABLE(element);
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_SVE_VLX:
        pOp->type = diana_arm_special;
        pOp->value.imm = FieldAt(pDesc, 0, word) ? 4 : 2;
        return DI_SUCCESS;

    case DIANA_A64_OPK_ZA_TILE:
        element = GetScalableElementLog2(pDesc, word, &next);
        if (element < 0)
            return DI_INVALID_OPCODE;
        pOp->type = diana_register;
        pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A64_ZA0 + FieldAt(pDesc, 0, word));
        pOp->arm.arrangement = (DI_UINT8)ScalableArrangement(element);
        return DI_SUCCESS;

    case DIANA_A64_OPK_ZA_SLICE:
        {
            // [tile, V, size source..., Rs, offset], each one optional by param
            DI_UINT32 tile = 0;
            DI_UINT32 vertical = 0;
            DI_UINT32 rs = 0;
            DI_UINT32 offset = 0;
            next = 0;
            if (pDesc->param & DIANA_A64_ZA_TILE)
                tile = FieldAt(pDesc, next++, word);
            if (pDesc->param & DIANA_A64_ZA_HV)
                vertical = FieldAt(pDesc, next++, word);
            element = GetScalableElementLog2(pDesc, word, &next);
            if (element < 0)
                return DI_INVALID_OPCODE;
            rs = FieldAt(pDesc, next++, word);
            if (pDesc->param & DIANA_A64_ZA_OFFSET)
                offset = FieldAt(pDesc, next++, word);
            // a range of slices: the offset counts ranges (2:3 is offset 1 of two)
            if (pDesc->flags & DIANA_A64_OPF_ZA_RANGE2)
                pOp->arm.lane = 1;
            if (pDesc->flags & DIANA_A64_OPF_ZA_RANGE4)
                pOp->arm.lane = 3;
            offset *= (DI_UINT32)pOp->arm.lane + 1;
            pOp->arm.regCount = (DI_UINT8)((pDesc->flags & DIANA_A64_OPF_ZA_VGX2) ? 2 : (pDesc->flags & DIANA_A64_OPF_ZA_VGX4) ? 4 : 0);
            SetMemory(pOp, 0, offset, DIANA_ARM_WRITEBACK_NONE);
            pOp->value.rmIndex.reg = (pDesc->param & DIANA_A64_ZA_ARRAY) ? reg_A64_ZA : (DianaUnifiedRegister)(reg_A64_ZA0 + tile);
            pOp->value.rmIndex.indexed_reg = GetW(((pDesc->param & DIANA_A64_ZA_W8) ? 8 : 12) + rs, 0);
            pOp->arm.arrangement = (DI_UINT8)ScalableArrangement(element);
            if (pDesc->param & DIANA_A64_ZA_HV)
                pOp->arm.qualifier = (DI_UINT8)(vertical ? DIANA_ARM_QUAL_VERTICAL : DIANA_ARM_QUAL_HORIZONTAL);
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_ZA_MASK:
        pOp->type = diana_arm_special;
        pOp->value.imm = FieldAt(pDesc, 0, word);
        return DI_SUCCESS;

    case DIANA_A64_OPK_ZT0:
        pOp->type = diana_register;
        pOp->value.recognizedRegister = reg_A64_ZT0;
        if (pDesc->param & 2)
            pOp->arm.lane = (DI_UINT8)(FieldAt(pDesc, 0, word) << ((pDesc->param >> 4) & 7));
        return DI_SUCCESS;

    case DIANA_A64_OPK_ZLIST:
        if (pDesc->param & 0x10)
            next = 2;
        element = GetScalableElementLog2(pDesc, word, &next);
        if (element < 0)
            return DI_INVALID_OPCODE;
        {
            // a narrow field counts in steps of the register count: Zn:4 of a pair is z0, z2...
            int isP = (pDesc->param & 8) != 0;
            DI_UINT32 reg = FieldAt(pDesc, 0, word) << ((isP ? 4 : 5) - pDesc->width[0]);
            if (pDesc->param & 0x10)
            {
                // strided: T:0:Zt, {z1.s, z9.s}, {z17.b, z21.b, z25.b, z29.b}
                reg = (FieldAt(pDesc, 0, word) << 4) | FieldAt(pDesc, 1, word);
                pOp->arm.regStride = (DI_UINT8)(16 / (pDesc->param & 7));
            }
            pOp->type = diana_arm_reglist;
            pOp->value.recognizedRegister = (DianaUnifiedRegister)((isP ? reg_A64_P0 : reg_A64_Z0) + reg);
            pOp->arm.arrangement = (DI_UINT8)ScalableArrangement(element);
            pOp->arm.regCount = (DI_UINT8)(pDesc->param & 7);
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_ZELEM:
        {
            DI_UINT32 index = 0;
            element = GetScalableElementLog2(pDesc, word, &next);
            if (element < 0)
                return DI_INVALID_OPCODE;
            for (; next < pDesc->fieldCount; ++next)
                index = (index << pDesc->width[next]) | FieldAt(pDesc, next, word);
            if (pDesc->param & 1)
                index >>= element + 1;
            pOp->type = diana_register;
            pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A64_Z0 + FieldAt(pDesc, 0, word));
            // no element size (LUTI2 z4[1]): only the element flag
            pOp->arm.arrangement = (DI_UINT8)(ScalableArrangement(element) | 0x40);
            pOp->arm.lane = (DI_UINT8)index;
            if (element != 0x80)
                pOp->usedSize = 1 << element;
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_SVE_PATTERN:
        pOp->type = diana_arm_special;
        pOp->value.imm = FieldAt(pDesc, 0, word);
        pOp->arm.shiftType = DIANA_ARM_SHIFT_MUL;
        pOp->arm.shiftAmount = (DI_UINT8)(pDesc->fieldCount > 1 ? FieldAt(pDesc, 1, word) + 1 : 1);
        return DI_SUCCESS;

    case DIANA_A64_OPK_SVE_IMM8SH:
        {
            // sh:imm8
            DI_UINT32 sh = FieldAt(pDesc, 0, word);
            DI_UINT64 imm8 = FieldAt(pDesc, 1, word);
            pOp->type = diana_imm;
            pOp->value.imm = (pDesc->param ? SignExtend(imm8, 8) : imm8) << (8 * sh);
            pOp->arm.shiftType = DIANA_ARM_SHIFT_LSL;
            pOp->arm.shiftAmount = (DI_UINT8)(8 * sh);
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_GPREG_SIZE:
        {
            int isX = FieldAt(pDesc, 1, word) == (1u << pDesc->width[1]) - 1;
            DI_UINT32 reg = FieldAt(pDesc, 0, word);
            pOp->type = diana_register;
            pOp->value.recognizedRegister = isX ? GetX(reg, 0) : GetW(reg, 0);
            pOp->usedSize = isX ? 8 : 4;
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_SVE_MEM_IMM:
        {
            // [Rn, imm...]: the immediate fields read as one
            DI_UINT64 imm = 0;
            int bits = 0;
            for (next = 1; next < pDesc->fieldCount; ++next)
            {
                imm = (imm << pDesc->width[next]) | FieldAt(pDesc, next, word);
                bits += pDesc->width[next];
            }
            if (pDesc->flags & DIANA_A64_OPF_SVE_SIGNED)
                imm = SignExtend(imm, bits);
            SetMemory(pOp, FieldAt(pDesc, 0, word), imm * pDesc->param, DIANA_ARM_WRITEBACK_NONE);
            if (pDesc->flags & DIANA_A64_OPF_SVE_MUL_VL)
                pOp->arm.shiftType = DIANA_ARM_SHIFT_MUL_VL;
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_SVE_MEM_ZIMM:
        element = GetScalableElementLog2(pDesc, word, &next);
        if (element < 0)
            return DI_INVALID_OPCODE;
        SetMemory(pOp, 0, (DI_UINT64)FieldAt(pDesc, next, word) * pDesc->param, DIANA_ARM_WRITEBACK_NONE);
        pOp->value.rmIndex.reg = (DianaUnifiedRegister)(reg_A64_Z0 + FieldAt(pDesc, 0, word));
        pOp->arm.arrangement = (DI_UINT8)ScalableArrangement(element);
        return DI_SUCCESS;

    case DIANA_A64_OPK_SVE_MEM_REG:
        {
            DI_UINT32 base = FieldAt(pDesc, 0, word);
            DI_UINT32 index = FieldAt(pDesc, 1, word);
            int extend = pDesc->flags & DIANA_A64_OPF_SVE_EXTEND_FIELD;
            int amount = pDesc->param;
            next = 2;
            element = 0x80;
            if (pDesc->flags & DIANA_A64_OPF_SVE_INDEX_Z)
            {
                element = GetScalableElementLog2(pDesc, word, &next);
                if (element < 0)
                    return DI_INVALID_OPCODE;
            }
            if (extend == DIANA_A64_OPF_SVE_EXTEND_FIELD)
                extend = FieldAt(pDesc, next++, word) ? DIANA_A64_OPF_SVE_SXTW : DIANA_A64_OPF_SVE_UXTW;
            if (amount & 0x80)
                amount = (int)FieldAt(pDesc, next++, word);
            if (!(pDesc->flags & DIANA_A64_OPF_SVE_INDEX_Z) && (pDesc->flags & DIANA_A64_OPF_SVE_BASE_Z))
            {
                // [Zn.D{, Xm}]: the element size of the base
                element = GetScalableElementLog2(pDesc, word, &next);
                if (element < 0)
                    return DI_INVALID_OPCODE;
            }
            SetMemory(pOp, base, 0, DIANA_ARM_WRITEBACK_NONE);
            pOp->value.rmIndex.dispSize = 0;
            if (pDesc->flags & DIANA_A64_OPF_SVE_BASE_Z)
                pOp->value.rmIndex.reg = (DianaUnifiedRegister)(reg_A64_Z0 + base);
            pOp->value.rmIndex.indexed_reg = (pDesc->flags & DIANA_A64_OPF_SVE_INDEX_Z) ?
                                             (DianaUnifiedRegister)(reg_A64_Z0 + index) : GetX(index, 0);
            if ((amount & 0x40) && index == 31)
                pOp->value.rmIndex.indexed_reg = reg_none;
            amount &= 0x3F;
            pOp->value.rmIndex.index = (DI_CHAR)(1 << amount);
            pOp->arm.arrangement = (DI_UINT8)ScalableArrangement(element);
            pOp->arm.shiftType = extend == DIANA_A64_OPF_SVE_SXTW ? DIANA_ARM_EXTEND_SXTW :
                                 extend == DIANA_A64_OPF_SVE_UXTW ? DIANA_ARM_EXTEND_UXTW : DIANA_ARM_SHIFT_LSL;
            pOp->arm.shiftAmount = (DI_UINT8)amount;
            pOp->arm.hasAmount = (DI_UINT8)(amount != 0);
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_SVE_PRFOP:
        pOp->type = diana_arm_special;
        pOp->value.imm = FieldAt(pDesc, 0, word);
        return DI_SUCCESS;

    case DIANA_A64_OPK_SVE_FPCONST:
        {
            // 0.0, 0.5, 1.0, 2.0 as IEEE doubles
            static const DI_UINT64 values[3][2] =
            {
                {0x3FE0000000000000ULL, 0x3FF0000000000000ULL},
                {0x3FE0000000000000ULL, 0x4000000000000000ULL},
                {0, 0x3FF0000000000000ULL}
            };
            pOp->type = diana_imm;
            pOp->value.imm = values[pDesc->param % 3][FieldAt(pDesc, 0, word) & 1];
        }
        return DI_SUCCESS;

    case DIANA_A64_OPK_SVE_BIMM:
        {
            DI_UINT32 imm13 = FieldAt(pDesc, 0, word);
            int esizeLog2 = BitmaskElementLog2(imm13);
            DI_UINT64 value = 0;
            if (esizeLog2 < 0 || !DianaA64_DecodeBitMasks((int)(imm13 >> 12), (int)(imm13 & 0x3F), (int)((imm13 >> 6) & 0x3F), 64, &value))
                return DI_INVALID_OPCODE;
            pOp->type = diana_imm;
            pOp->value.imm = value & Ones(1 << esizeLog2);
        }
        return DI_SUCCESS;
    }
    return DI_ERROR;
}

static int FillOperand(const DianaA64OperandDesc * pDesc,
                       DI_UINT32 word,
                       DianaLinkedOperand * pOp)
{
    int width = 0;
    DI_UINT32 value = ExtractFields(pDesc, word, &width);
    DI_UINT64 offset = 0;

    DIANA_MEMSET(pOp, 0, sizeof(*pOp));
    pOp->arm.opKind = pDesc->kind;

    switch (pDesc->kind)
    {
    case DIANA_A64_OPK_REG_W:
    case DIANA_A64_OPK_REG_WSP:
        pOp->type = diana_register;
        // param: 1 for the second register of a pair (CASP W(s+1)), the first is even; 31 stays xzr (SYSP)
        if (value != 31)
            value += pDesc->param;
        pOp->value.recognizedRegister = GetW(value, pDesc->kind == DIANA_A64_OPK_REG_WSP);
        pOp->usedSize = 4;
        break;

    case DIANA_A64_OPK_REG_X:
    case DIANA_A64_OPK_REG_XSP:
        pOp->type = diana_register;
        if (value != 31)
            value += pDesc->param;
        pOp->value.recognizedRegister = GetX(value, pDesc->kind == DIANA_A64_OPK_REG_XSP);
        pOp->usedSize = 8;
        if (pDesc->flags & DIANA_A64_OPF_WRITEBACK)
            pOp->arm.writeback = DIANA_ARM_WRITEBACK_PRE;
        break;

    case DIANA_A64_OPK_AIMM:
        // sh:imm12
        pOp->type = diana_imm;
        pOp->value.imm = value & 0xFFF;
        if (value & 0x1000)
        {
            pOp->arm.shiftType = DIANA_ARM_SHIFT_LSL;
            pOp->arm.shiftAmount = 12;
        }
        break;

    case DIANA_A64_OPK_BIMM32:
        // immr:imms
        pOp->type = diana_imm;
        if (!DianaA64_DecodeBitMasks(0, value & 0x3F, (value >> 6) & 0x3F, 32, &pOp->value.imm))
            return DI_INVALID_OPCODE;
        break;

    case DIANA_A64_OPK_BIMM64:
        // N:immr:imms
        pOp->type = diana_imm;
        if (!DianaA64_DecodeBitMasks((value >> 12) & 1, value & 0x3F, (value >> 6) & 0x3F, 64, &pOp->value.imm))
            return DI_INVALID_OPCODE;
        break;

    case DIANA_A64_OPK_UIMM:
        // param: shift (the ADDG tag offset is imm6 << 4)
        pOp->type = diana_imm;
        pOp->value.imm = (DI_UINT64)value << pDesc->param;
        break;

    case DIANA_A64_OPK_TBIT:
        // param: 32 for the X form
        pOp->type = diana_imm;
        pOp->value.imm = (DI_UINT64)value + pDesc->param;
        break;

    case DIANA_A64_OPK_SIMM:
        pOp->type = diana_imm;
        pOp->value.imm = SignExtend(value, width) << pDesc->param;
        break;

    case DIANA_A64_OPK_MOVW:
        // hw:imm16
        pOp->type = diana_imm;
        pOp->value.imm = value & 0xFFFF;
        pOp->arm.shiftType = DIANA_ARM_SHIFT_LSL;
        pOp->arm.shiftAmount = (DI_UINT8)((value >> 16) * 16);
        break;

    case DIANA_A64_OPK_SHREG_W:
    case DIANA_A64_OPK_SHREG_X:
        // Rm:shift:imm6
        pOp->type = diana_arm_shifted_reg;
        pOp->value.recognizedRegister = pDesc->kind == DIANA_A64_OPK_SHREG_X ? GetX(value >> 8, 0) : GetW(value >> 8, 0);
        pOp->arm.shiftType = (DI_UINT8)(DIANA_ARM_SHIFT_LSL + ((value >> 6) & 3));
        pOp->arm.shiftAmount = (DI_UINT8)(value & 0x3F);
        pOp->usedSize = pDesc->kind == DIANA_A64_OPK_SHREG_X ? 8 : 4;
        break;

    case DIANA_A64_OPK_EXTREG_W:
    case DIANA_A64_OPK_EXTREG_X:
        {
            // Rm:option:imm3
            DI_UINT32 option = (value >> 3) & 7;
            int isX = pDesc->kind == DIANA_A64_OPK_EXTREG_X && (option & 3) == 3;
            pOp->type = diana_arm_shifted_reg;
            pOp->value.recognizedRegister = isX ? GetX(value >> 6, 0) : GetW(value >> 6, 0);
            pOp->arm.shiftType = (DI_UINT8)(DIANA_ARM_EXTEND_UXTB + option);
            pOp->arm.shiftAmount = (DI_UINT8)(value & 7);
            pOp->usedSize = isX ? 8 : 4;
        }
        break;

    case DIANA_A64_OPK_LABEL:
    case DIANA_A64_OPK_ADR:
        // relative to the next instruction, like x86 rel operands
        pOp->type = diana_rel;
        pOp->value.rel.m_size = 8;
        if (pDesc->flags & DIANA_A64_OPF_NEGATIVE)
            pOp->value.rel.m_value = 0 - ((DI_UINT64)value << pDesc->param) - 4;
        else
            pOp->value.rel.m_value = (SignExtend(value, width) << pDesc->param) - 4;
        pOp->usedAddressSize = 8;
        break;

    case DIANA_A64_OPK_ADRP:
        // the page delta, the target depends on the instruction address
        pOp->type = diana_arm_special;
        pOp->value.imm = SignExtend(value, width) << 12;
        pOp->usedAddressSize = 8;
        break;

    case DIANA_A64_OPK_LITERAL:
        // [pc + offset], the displacement is relative to the next instruction
        SetMemory(pOp, 0, (SignExtend(value, width) << pDesc->param) - 4, DIANA_ARM_WRITEBACK_NONE);
        pOp->value.rmIndex.reg = reg_A64_PC;
        break;

    case DIANA_A64_OPK_COND:
        pOp->type = diana_arm_cond;
        pOp->arm.cond = (DI_UINT8)value;
        break;

    case DIANA_A64_OPK_MEM_BASE:
        SetMemory(pOp, value, 0, (pDesc->flags & DIANA_A64_OPF_WRITEBACK) ? DIANA_ARM_WRITEBACK_PRE : DIANA_ARM_WRITEBACK_NONE);
        break;

    case DIANA_A64_OPK_MEM_UOFF:
        // Rn:imm12
        SetMemory(pOp, value >> 12, (DI_UINT64)(value & 0xFFF) << pDesc->param, DIANA_ARM_WRITEBACK_NONE);
        break;

    case DIANA_A64_OPK_MEM_SOFF:
    case DIANA_A64_OPK_MEM_PRE:
    case DIANA_A64_OPK_MEM_POST:
        {
            // Rn:imm9 or Rn:imm7
            int immWidth = width - 5;
            offset = SignExtend(value & ((1u << immWidth) - 1), immWidth) << pDesc->param;
            SetMemory(pOp, value >> immWidth, offset,
                      pDesc->kind == DIANA_A64_OPK_MEM_PRE ? DIANA_ARM_WRITEBACK_PRE :
                      pDesc->kind == DIANA_A64_OPK_MEM_POST ? DIANA_ARM_WRITEBACK_POST : DIANA_ARM_WRITEBACK_NONE);
        }
        break;

    case DIANA_A64_OPK_MEM_REGOFF:
        {
            // Rn:Rm:option:S
            DI_UINT32 s = value & 1;
            DI_UINT32 option = (value >> 1) & 7;
            DI_UINT32 rm = (value >> 4) & 0x1F;
            SetMemory(pOp, value >> 9, 0, DIANA_ARM_WRITEBACK_NONE);
            pOp->value.rmIndex.dispSize = 0;
            pOp->value.rmIndex.indexed_reg = (option & 3) == 3 ? GetX(rm, 0) : GetW(rm, 0);
            pOp->arm.shiftType = (DI_UINT8)(DIANA_ARM_EXTEND_UXTB + option);
            pOp->arm.hasAmount = (DI_UINT8)s;
            pOp->arm.shiftAmount = (DI_UINT8)(s ? pDesc->param : 0);
            pOp->value.rmIndex.index = (DI_CHAR)(1 << pOp->arm.shiftAmount);
        }
        break;

    case DIANA_A64_OPK_SYSREG:
        pOp->type = diana_arm_sysreg;
        pOp->value.imm = value;
        break;

    case DIANA_A64_OPK_CREG:
    case DIANA_A64_OPK_BARRIER:
    case DIANA_A64_OPK_PRFOP:
        pOp->type = diana_arm_special;
        pOp->value.imm = value;
        break;

    default:
        if (pDesc->kind >= DIANA_A64_OPK_ZREG)
            return FillSveOperand(pDesc, word, pOp);
        if (pDesc->kind >= DIANA_A64_OPK_FPREG)
            return FillSimdOperand(pDesc, word, value, width, pOp);
        return DI_ERROR;
    }
    return DI_SUCCESS;
}

static int MatchesUnless(const DianaA64Tables * pTables, const DianaA64CmdInfo * pCmd, DI_UINT32 word)
{
    const DianaA64MaskValue * pUnless = pTables->pUnless + pCmd->unlessFirst;
    int i = 0;
    for (; i < pCmd->unlessCount; ++i)
    {
        if ((word & pUnless[i].mask) == pUnless[i].value)
            return 1;
    }
    return 0;
}

static int HasEqualFields(const DianaA64Tables * pTables, const DianaA64CmdInfo * pCmd, DI_UINT32 word)
{
    const DianaA64Distinct * pDistinct = pTables->pDistinct + pCmd->distinctFirst;
    int i = 0;
    for (; i < pCmd->distinctCount; ++i)
    {
        DI_UINT32 mask = (1u << pDistinct[i].width) - 1;
        DI_UINT32 value1 = (word >> pDistinct[i].lsb1) & mask;
        if (value1 != ((word >> pDistinct[i].lsb2) & mask))
            continue;
        if ((pDistinct[i].flags & DIANA_A64_DISTINCT_SP_IS_31) && value1 == mask)
            continue;
        return 1;
    }
    return 0;
}

static int IsCmdEnabled(const DianaA64Tables * pTables, const DianaA64CmdInfo * pCmd, DI_UINT64 profileFeatures)
{
    if (pCmd->features & ~profileFeatures)
        return 0;
    return !pCmd->featuresAny || (pTables->pFeaturesAny[pCmd->featuresAny] & profileFeatures) != 0;
}

int DianaA64_IsCmdEnabled(const DianaA64CmdInfo * pCmd, DI_UINT64 profileFeatures)
{
    return IsCmdEnabled(DianaA64_GetTables(), pCmd, profileFeatures);
}

static int FillResult(DianaA64CmdInfo * pCmd, DI_UINT32 word, DianaParserResult * pResult)
{
    int i = 0;
    for (; i < pCmd->operandCount; ++i)
    {
        DI_CHECK(FillOperand(pCmd->operands + i, word, pResult->linkedOperands + i));
    }
    pResult->pInfo = &pCmd->header;
    pResult->iLinkedOpCount = pCmd->operandCount;
    return DI_SUCCESS;
}

static int GetUndefinedCmd(const DianaContext * pContext, DI_UINT32 word);

int DianaA64_DecodeWord(DianaContext * pContext,
                        DI_UINT32 word,
                        DianaParserResult * pResult)
{
    const DianaA64Tables * pTables = DianaA64_GetTables();
    const DianaA64Node * pNode = pTables->pNodes;
    DI_UINT32 slot = 0;

    pResult->pInfo = 0;
    pResult->iLinkedOpCount = 0;
    pResult->iPrefix = 0;
    pResult->iRexPrefix = 0;
    pResult->iFullCmdSize = 4;
    pResult->bytes[0] = (unsigned char)word;
    pResult->bytes[1] = (unsigned char)(word >> 8);
    pResult->bytes[2] = (unsigned char)(word >> 16);
    pResult->bytes[3] = (unsigned char)(word >> 24);
    pResult->bytesCount = 4;

    for (;;)
    {
        slot = pTables->pSlots[pNode->first + ((word >> pNode->shift) & ((1u << pNode->bits) - 1))];
        if (!(slot & DIANA_A64_SLOT_NODE_FLAG))
            break;
        pNode = pTables->pNodes + (slot & ~DIANA_A64_SLOT_NODE_FLAG);
    }

    if (slot)
    {
        const DianaA64Leaf * pLeaf = pTables->pLeaves + (slot >> DIANA_A64_SLOT_COUNT_BITS);
        const DianaA64Leaf * pEnd = pLeaf + (slot & ((1u << DIANA_A64_SLOT_COUNT_BITS) - 1));
        for (; pLeaf < pEnd; ++pLeaf)
        {
            DianaA64CmdInfo * pCmd = 0;
            if ((word & pLeaf->mask) != pLeaf->value)
                continue;
            pCmd = pTables->pCmds + pLeaf->cmd;
            if (pCmd->unlessCount && MatchesUnless(pTables, pCmd, word))
                continue;
            if (pCmd->distinctCount && HasEqualFields(pTables, pCmd, word))
                continue;
            // a disabled feature can hide a more specific encoding, the next candidate is the generic one
            if (!IsCmdEnabled(pTables, pCmd, pContext->armFeatures))
                continue;
            if (FillResult(pCmd, word, pResult) == DI_SUCCESS)
                return DI_SUCCESS;
        }
    }
    // ".inst 0x..." like objdump
    return FillResult(pTables->pCmds + GetUndefinedCmd(pContext, word), word, pResult);
}

// the slot of the word: 0 (nothing) or (first leaf << 4) | leaf count
static DI_UINT32 WalkTree(const DianaA64Node * pNodes, const DI_UINT32 * pSlots, DI_UINT32 word)
{
    const DianaA64Node * pNode = pNodes;
    DI_UINT32 slot = 0;
    for (;;)
    {
        slot = pSlots[pNode->first + ((word >> pNode->shift) & ((1u << pNode->bits) - 1))];
        if (!(slot & DIANA_A64_SLOT_NODE_FLAG))
            return slot;
        pNode = pNodes + (slot & ~DIANA_A64_SLOT_NODE_FLAG);
    }
}

#if defined(_MSC_VER)
#define DIANA_A64_COLD __declspec(noinline)
#elif defined(__GNUC__)
#define DIANA_A64_COLD __attribute__((noinline, cold))
#else
#define DIANA_A64_COLD
#endif

// no instruction for the word: the index of the ".inst" command that says why.
// Out of line, so the decode loop of DianaA64_DecodeWord stays as small as before
DIANA_A64_COLD
static int GetUndefinedCmd(const DianaContext * pContext, DI_UINT32 word)
{
    const DianaA64Tables * pTables = DianaA64_GetTables();
    DI_UINT32 slot = WalkTree(pTables->pNodes, pTables->pSlots, word);
    const DianaA64Leaf * pLeaf = pTables->pLeaves + (slot >> DIANA_A64_SLOT_COUNT_BITS);
    const DianaA64Leaf * pEnd = pLeaf + (slot & ((1u << DIANA_A64_SLOT_COUNT_BITS) - 1));
    // an instruction of the spec that the profile leaves out
    for (; slot && pLeaf < pEnd; ++pLeaf)
    {
        const DianaA64CmdInfo * pCmd = pTables->pCmds + pLeaf->cmd;
        if ((word & pLeaf->mask) != pLeaf->value || IsCmdEnabled(pTables, pCmd, pContext->armFeatures))
            continue;
        if (pCmd->unlessCount && MatchesUnless(pTables, pCmd, word))
            continue;
        if (pCmd->distinctCount && HasEqualFields(pTables, pCmd, word))
            continue;
        return pTables->disabledCmd;
    }
    // Arm allocates the word, the spec doesn't decode it yet
    slot = WalkTree(pTables->pUnsupportedNodes, pTables->pUnsupportedSlots, word);
    pLeaf = pTables->pUnsupportedLeaves + (slot >> DIANA_A64_SLOT_COUNT_BITS);
    pEnd = pLeaf + (slot & ((1u << DIANA_A64_SLOT_COUNT_BITS) - 1));
    for (; pLeaf < pEnd; ++pLeaf)
    {
        if ((word & pLeaf->mask) == pLeaf->value)
            return pTables->unsupportedCmd;
    }
    return 0;
}

int DianaA64_ParseCmd(DianaContext * pContext,
                      DianaReadStream * readStream,
                      DianaParserResult * pResult)
{
    unsigned char bytes[4];
    int read = 0;
    DI_UINT32 word = 0;

    pResult->pInfo = 0;
    pResult->iLinkedOpCount = 0;
    pResult->iFullCmdSize = 0;
    pResult->bytesCount = 0;

    DI_CHECK(readStream->pReadFnc(readStream, bytes, 4, &read));
    if (read == 0)
        return DI_END;
    if (read != 4)
    {
        pResult->iFullCmdSize = read;
        return DI_END_OF_STREAM;
    }
    word = (DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8) | ((DI_UINT32)bytes[2] << 16) | ((DI_UINT32)bytes[3] << 24);
    return DianaA64_DecodeWord(pContext, word, pResult);
}

const DianaA64CmdInfo * DianaA64_GetCmdInfo(const DianaParserResult * pResult)
{
    long id = 0;
    if (!pResult->pInfo || !pResult->pInfo->m_pGroupInfo)
        return 0;
    id = (long)pResult->pInfo->m_pGroupInfo->m_commandId;
    if (id < DIANA_A64_CMD_BASE || id >= diana_a64_cmd_end)
        return 0;
    return (const DianaA64CmdInfo *)pResult->pInfo;
}

int DianaA64_IsUndefined(const DianaParserResult * pResult)
{
    int wordClass = DianaA64_GetWordClass(pResult);
    return wordClass > DIANA_A64_WORD_SUPPORTED;
}

int DianaA64_GetWordClass(const DianaParserResult * pResult)
{
    const DianaA64CmdInfo * pCmd = DianaA64_GetCmdInfo(pResult);
    if (!pCmd)
        return -1;
    switch ((int)pCmd->header.m_pGroupInfo->m_commandId)
    {
    case diana_a64_cmd_inst:                return DIANA_A64_WORD_UNALLOCATED;
    case diana_a64_cmd_inst_unsupported:    return DIANA_A64_WORD_UNSUPPORTED;
    case diana_a64_cmd_inst_disabled:       return DIANA_A64_WORD_DISABLED;
    }
    return DIANA_A64_WORD_SUPPORTED;
}

DianaGroupInfo * DianaA64_GetGroupInfo(long lId)
{
    const DianaA64Tables * pTables = DianaA64_GetTables();
    if (lId < DIANA_A64_CMD_BASE || lId >= DIANA_A64_CMD_BASE + pTables->groupsCount)
        return 0;
    return pTables->pGroups + (lId - DIANA_A64_CMD_BASE);
}

#endif
