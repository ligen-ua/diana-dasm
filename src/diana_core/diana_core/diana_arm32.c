// AArch32 decoding: A32 words and T32 halfwords, through the generated tables of diana_gen_a32.c
#include "diana_arm32.h"

#ifndef DIANA_NO_ARM

// ------------------------------------------------------------------ fields
static DI_UINT32 FieldAt(const DianaA64OperandDesc * pDesc, int index, DI_UINT32 word)
{
    int width = pDesc->width[index];
    DI_UINT32 field = word >> pDesc->lsb[index];
    return width < 32 ? field & ((1u << width) - 1) : field;
}

// the fields first..count-1 concatenated, the first is the most significant
static DI_UINT32 FieldsFrom(const DianaA64OperandDesc * pDesc, int first, DI_UINT32 word, int * pWidth)
{
    DI_UINT32 value = 0;
    int total = 0;
    int i = first;
    for (; i < pDesc->fieldCount; ++i)
    {
        int width = pDesc->width[i];
        value = width < 32 ? (value << width) | FieldAt(pDesc, i, word) : FieldAt(pDesc, i, word);
        total += width;
    }
    *pWidth = total;
    return value;
}

static DI_UINT32 SignExtend32(DI_UINT32 value, int bits)
{
    DI_UINT32 sign = 0;
    if (bits <= 0 || bits >= 32)
        return value;
    sign = 1u << (bits - 1);
    value &= (sign << 1) - 1;
    return (value ^ sign) - sign;
}

static DI_UINT32 Ror32(DI_UINT32 value, int amount)
{
    amount &= 31;
    return amount ? (value >> amount) | (value << (32 - amount)) : value;
}

static DianaUnifiedRegister GetR(DI_UINT32 index)
{
    return (DianaUnifiedRegister)(reg_A32_R0 + (index & 15));
}

static int PopCount16(DI_UINT32 value)
{
    int count = 0;
    for (value &= 0xFFFF; value; value &= value - 1)
        ++count;
    return count;
}

// DecodeImmShift: type 0..3 is lsl, lsr, asr, ror; lsr/asr #0 mean #32, ror #0 is rrx; lsl #0 is no shift
static void SetImmShift(DianaLinkedOperand * pOp, DI_UINT32 type, DI_UINT32 amount)
{
    if (type == 0 && amount == 0)
        return;
    if (type == 3 && amount == 0)
    {
        pOp->arm.shiftType = DIANA_ARM_SHIFT_RRX;
        return;
    }
    pOp->arm.shiftType = (DI_UINT8)(DIANA_ARM_SHIFT_LSL + type);
    pOp->arm.shiftAmount = (DI_UINT8)(amount ? amount : 32);
    pOp->arm.hasAmount = 1;
}

// ThumbExpandImm without the carry
static DI_UINT32 ThumbExpandImm(DI_UINT32 imm12)
{
    DI_UINT32 imm8 = imm12 & 0xFF;
    if ((imm12 >> 10) == 0)
    {
        switch ((imm12 >> 8) & 3)
        {
        case 0: return imm8;
        case 1: return imm8 | (imm8 << 16);
        case 2: return (imm8 << 8) | (imm8 << 24);
        default: return imm8 | (imm8 << 8) | (imm8 << 16) | (imm8 << 24);
        }
    }
    return Ror32(0x80 | (imm12 & 0x7F), (int)(imm12 >> 7));
}

static void SetMemory(DianaLinkedOperand * pOp, DianaUnifiedRegister base, DI_UINT32 flags)
{
    pOp->type = diana_index;
    pOp->value.rmIndex.seg_reg = reg_none;
    pOp->value.rmIndex.reg = base;
    pOp->value.rmIndex.indexed_reg = reg_none;
    pOp->value.rmIndex.index = 0;
    pOp->value.rmIndex.dispSize = 4;
    pOp->value.rmIndex.dispValue = 0;
    pOp->usedAddressSize = 4;
    if (flags & DIANA_A32_OPF_PRE)
        pOp->arm.writeback = DIANA_ARM_WRITEBACK_PRE;
    else
    if (flags & DIANA_A32_OPF_POST)
        pOp->arm.writeback = DIANA_ARM_WRITEBACK_POST;
}

// VFPExpandImm of imm8 (a:b:cdefgh) for a float of bits 16, 32 or 64: sign a, exponent NOT(b):b...b:cd,
// fraction efgh then zeros
static DI_UINT64 VfpExpandImm(DI_UINT32 imm8, int bits)
{
    int e = bits == 16 ? 5 : bits == 32 ? 8 : 11;
    int f = bits - e - 1;
    DI_UINT64 sign = (imm8 >> 7) & 1;
    DI_UINT64 b = (imm8 >> 6) & 1;
    DI_UINT64 exponent = ((b ^ 1) << (e - 1)) | ((b ? ((1u << (e - 3)) - 1) : 0) << 2) | ((imm8 >> 4) & 3);
    DI_UINT64 fraction = (DI_UINT64)(imm8 & 15) << (f - 4);
    return (sign << (bits - 1)) | (exponent << f) | fraction;
}

// AdvSIMDExpandImm(op, cmode, imm8): the 64-bit pattern of VMOV, VMVN, VORR, VBIC (immediate)
static DI_UINT64 AdvSimdExpandImm(DI_UINT32 op, DI_UINT32 cmode, DI_UINT32 imm8)
{
    DI_UINT64 imm = imm8;
    DI_UINT64 imm32 = 0;
    switch (cmode >> 1)
    {
    case 0: case 1: case 2: case 3:
        imm32 = imm << (8 * (cmode >> 1));
        return imm32 | (imm32 << 32);
    case 4: case 5:
        imm32 = (imm << (8 * ((cmode >> 1) & 1))) * 0x00010001u;
        return imm32 | (imm32 << 32);
    case 6:
        imm32 = (cmode & 1) ? (imm << 16) | 0xFFFF : (imm << 8) | 0xFF;
        return imm32 | (imm32 << 32);
    default:
        break;
    }
    if (!(cmode & 1) && !op)
        return imm * 0x0101010101010101ULL;
    if (!(cmode & 1))
    {
        // each bit of imm8 is a byte of ones or zeros
        DI_UINT64 result = 0;
        int i = 0;
        for (; i < 8; ++i)
        {
            if ((imm8 >> i) & 1)
                result |= (DI_UINT64)0xFF << (8 * i);
        }
        return result;
    }
    if (!op)
    {
        // a float: imm8<7>:NOT(imm8<6>):Replicate(imm8<6>, 5):imm8<5:0>:Zeros(19)
        imm32 = ((imm & 0x80) << 24) | ((imm & 0x40) ? 0x3E000000 : 0x40000000) | ((imm & 0x3F) << 19);
        return imm32 | (imm32 << 32);
    }
    return 0;                   // cmode 1111, op 1: UNDEFINED, not decoded
}

// VLD/VST multiple structures (objdump's table): itype -> registers | (spacing - 1) << 4
static const DI_UINT8 g_vldTypes[16] =
{
    0x04, 0x14, 0x04, 0x04, 0x03, 0x13, 0x03, 0x01, 0x02, 0x12, 0x02, 0, 0, 0, 0, 0
};

// the lists of VLD/VST: multiple structures, one lane, all lanes
static void FillVList(const DianaA64OperandDesc * pDesc, DI_UINT32 word, DianaLinkedOperand * pOp)
{
    int mode = pDesc->param & 15;
    int count = pDesc->param >> 4;
    int stride = 1;
    DI_UINT32 rd = (FieldAt(pDesc, 0, word) << 4) | FieldAt(pDesc, 1, word);
    pOp->type = diana_arm_reglist;
    if (mode == 0)
    {
        DI_UINT8 type = g_vldTypes[FieldAt(pDesc, 2, word) & 15];
        count = type & 15;
        stride = (type >> 4) + 1;
    }
    else
    if (mode == 1)
    {
        // [D, Vd, index_align, size]: the index above the alignment bits, spacing 2 by bit size
        DI_UINT32 indexAlign = FieldAt(pDesc, 2, word);
        DI_UINT32 size = FieldAt(pDesc, 3, word);
        if (count > 1 && size > 0 && (indexAlign & (1u << size)))
            stride = 2;
        pOp->arm.arrangement = (DI_UINT8)DIANA_ARM_ARR_ELEMENT(size);
        pOp->arm.lane = (DI_UINT8)(indexAlign >> (size + 1));
    }
    else
    {
        // [D, Vd, T, size]: T is two registers for VLD1, the spacing for the others
        DI_UINT32 t = FieldAt(pDesc, 2, word);
        if (t && count == 1)
            count = 2;
        else
            stride = (int)t + 1;
        pOp->arm.arrangement = (DI_UINT8)DIANA_ARM_ARR_ELEMENT(FieldAt(pDesc, 3, word));
        pOp->arm.qualifier = DIANA_ARM_QUAL_ALL_LANES;
    }
    pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A32_D0 + (rd & 31));
    pOp->arm.regCount = (DI_UINT8)count;
    pOp->arm.regStride = (DI_UINT8)stride;
}

// the address of VLD/VST: [Rn :align], the alignment in bytes in arm.shiftAmount (objdump's rules)
static void FillVMem(const DianaA64OperandDesc * pDesc, DI_UINT32 word, DianaLinkedOperand * pOp)
{
    int mode = pDesc->param & 15;
    int count = pDesc->param >> 4;
    int bits = 0;
    int fields = pDesc->fieldCount - ((pDesc->flags & DIANA_A32_OPF_VMEM_POST_REG) ? 1 : 0);
    SetMemory(pOp, GetR(FieldAt(pDesc, 0, word)), 0);
    if (mode == 0)
    {
        DI_UINT32 align = FieldAt(pDesc, 1, word);
        bits = align ? 32 << align : 0;
    }
    else
    if (fields >= 3)
    {
        DI_UINT32 alignField = FieldAt(pDesc, 1, word);
        DI_UINT32 size = FieldAt(pDesc, 2, word);
        if (mode == 1)
        {
            switch (count)
            {
            case 1:
                if (size > 0 && (alignField & ((1u << size) - 1)) == ((1u << size) - 1))
                    bits = 8 << size;
                break;
            case 2:
                bits = (alignField & 1) ? 16 << size : 0;
                break;
            case 4:
                if (size == 2)
                    bits = (int)(alignField & 3) * 64;
                else
                    bits = (alignField & 1) ? 32 << size : 0;
                break;
            default:
                break;
            }
        }
        else
        if (alignField)
        {
            // all lanes: the a bit
            bits = (8 * count) << size;
            if (count == 4 && size > 1)
                bits >>= 1;
        }
    }
    pOp->arm.shiftAmount = (DI_UINT8)(bits / 8);
    if (pDesc->flags & DIANA_A32_OPF_VMEM_WRITEBACK)
        pOp->arm.writeback = DIANA_ARM_WRITEBACK_POST;
    if (pDesc->flags & DIANA_A32_OPF_VMEM_POST_REG)
    {
        pOp->arm.writeback = DIANA_ARM_WRITEBACK_POST;
        pOp->value.rmIndex.indexed_reg = GetR(FieldAt(pDesc, pDesc->fieldCount - 1, word));
    }
}

// ------------------------------------------------------------------ operands
static int FillOperand(const DianaA32CmdInfo * pCmd,
                       const DianaA64OperandDesc * pDesc,
                       DI_UINT32 word,
                       DianaLinkedOperand * pOp)
{
    int width = 0;
    int next = 0;
    DI_UINT32 value = 0;

    DIANA_MEMSET(pOp, 0, sizeof(*pOp));
    pOp->arm.opKind = pDesc->kind;

    switch (pDesc->kind)
    {
    case DIANA_A32_OPK_REG:
        {
            int count = pDesc->fieldCount;
            if (pDesc->flags & DIANA_A32_OPF_WRITEBACK_FIELD)
            {
                // Rn{!}: the last field is W
                --count;
                if (FieldAt(pDesc, count, word))
                    pOp->arm.writeback = DIANA_ARM_WRITEBACK_PRE;
            }
            if (pDesc->flags & DIANA_A32_OPF_WRITEBACK)
                pOp->arm.writeback = DIANA_ARM_WRITEBACK_PRE;
            if (pDesc->flags & DIANA_A32_OPF_WRITEBACK_LIST)
                --count;
            if (pDesc->arr)
            {
                value = (DI_UINT32)pDesc->arr - 1;
            }
            else
            {
                int i = 0;
                for (; i < count; ++i)
                    value = (value << pDesc->width[i]) | FieldAt(pDesc, i, word);
            }
            if ((pDesc->flags & DIANA_A32_OPF_WRITEBACK_LIST) && !((FieldAt(pDesc, count, word) >> value) & 1))
                pOp->arm.writeback = DIANA_ARM_WRITEBACK_PRE;
            // param 1: the second register of a pair (LDRD Rt2)
            pOp->type = diana_register;
            pOp->value.recognizedRegister = GetR(value + pDesc->param);
            pOp->usedSize = 4;
        }
        break;

    case DIANA_A32_OPK_IMM:
        pOp->type = diana_imm;
        pOp->value.imm = (DI_UINT64)FieldsFrom(pDesc, 0, word, &width) << pDesc->param;
        if (pDesc->flags & DIANA_A32_OPF_PLUS1)
            ++pOp->value.imm;
        pOp->usedSize = 4;
        break;

    case DIANA_A32_OPK_BFWIDTH:
        pOp->type = diana_imm;
        // [msb, lsb...]: the lsb fields concatenated (T32 imm3:imm2)
        pOp->value.imm = (DI_UINT64)((DI_INT64)FieldAt(pDesc, 0, word) - (DI_INT64)FieldsFrom(pDesc, 1, word, &width) + 1);
        pOp->usedSize = 4;
        break;

    case DIANA_A32_OPK_COPROC:
    case DIANA_A32_OPK_CREG:
        // [field] + param, or the fixed number param (p14, c5)
        pOp->type = diana_arm_special;
        pOp->value.imm = (pDesc->fieldCount ? FieldsFrom(pDesc, 0, word, &width) : 0) + pDesc->param;
        break;

    case DIANA_A32_OPK_PSRMASK:
    case DIANA_A32_OPK_BARRIER:
    case DIANA_A32_OPK_IFLAGS:
    case DIANA_A32_OPK_ENDIAN:
        pOp->type = diana_arm_special;
        pOp->value.imm = FieldsFrom(pDesc, 0, word, &width);
        break;

    case DIANA_A32_OPK_SYSM:
        // objdump's SYSm: R << 6 | B << 5 | M << 4 | M1; MRS/MSR of CPSR or SPSR has B = 0, M = 0, M1 = 1111
        pOp->type = diana_arm_special;
        if (pDesc->param)
            pOp->value.imm = (FieldAt(pDesc, 0, word) << 6) | 0x20 | (FieldAt(pDesc, 1, word) << 4) | FieldAt(pDesc, 2, word);
        else
            pOp->value.imm = (FieldAt(pDesc, 0, word) << 6) | 0x0F;
        break;

    case DIANA_A32_OPK_MODIMM:
        // imm12: an 8-bit value rotated right by twice the top 4 bits
        value = FieldsFrom(pDesc, 0, word, &width);
        pOp->type = diana_imm;
        pOp->arm.shiftAmount = (DI_UINT8)((value >> 8) * 2);
        pOp->value.imm = Ror32(value & 0xFF, pOp->arm.shiftAmount);
        pOp->usedSize = 4;
        break;

    case DIANA_A32_OPK_TMODIMM:
        pOp->type = diana_imm;
        pOp->value.imm = ThumbExpandImm(FieldsFrom(pDesc, 0, word, &width));
        pOp->usedSize = 4;
        break;

    case DIANA_A32_OPK_SHREG:
        // Rm, type, amount...; with param: Rm, amount..., the type fixed (ROR #rotate*8, LSL #imm5)
        pOp->type = diana_arm_shifted_reg;
        pOp->value.recognizedRegister = GetR(FieldAt(pDesc, 0, word));
        pOp->usedSize = 4;
        if (pDesc->param)
        {
            DI_UINT32 type = (DI_UINT32)(pDesc->param & 7) - 1;
            DI_UINT32 amount = FieldsFrom(pDesc, 1, word, &width) << (pDesc->param >> 4);
            // a rotation of 0 is no rotation, not rrx
            if (amount || type == 1 || type == 2)
                SetImmShift(pOp, type, amount);
            break;
        }
        SetImmShift(pOp, FieldAt(pDesc, 1, word), FieldsFrom(pDesc, 2, word, &width));
        break;

    case DIANA_A32_OPK_SHREG_REG:
        // Rm, type, Rs: the amount is the register r<shiftAmount>
        pOp->type = diana_arm_shifted_reg;
        pOp->value.recognizedRegister = GetR(FieldAt(pDesc, 0, word));
        pOp->usedSize = 4;
        if (pDesc->param)
        {
            // a fixed type: [Rm, Rs]
            pOp->arm.shiftType = (DI_UINT8)(DIANA_ARM_SHIFT_LSL + (pDesc->param & 7) - 1);
            pOp->arm.shiftAmount = (DI_UINT8)FieldAt(pDesc, 1, word);
        }
        else
        {
            pOp->arm.shiftType = (DI_UINT8)(DIANA_ARM_SHIFT_LSL + FieldAt(pDesc, 1, word));
            pOp->arm.shiftAmount = (DI_UINT8)FieldAt(pDesc, 2, word);
        }
        pOp->arm.qualifier = DIANA_ARM_QUAL_SHIFT_REG;
        break;

    case DIANA_A32_OPK_LABEL:
        {
            DI_UINT32 offset = 0;
            DI_UINT32 pc = (pDesc->flags & DIANA_A32_OPF_THUMB) ? 4 : 8;
            if (pDesc->flags & DIANA_A32_OPF_MODIMM)
            {
                // ADR: the rotated imm12, added or subtracted
                value = FieldAt(pDesc, 0, word);
                offset = Ror32(value & 0xFF, (int)((value >> 8) * 2));
                if (pDesc->flags & DIANA_A32_OPF_SUBTRACT)
                    offset = 0 - offset;
                pOp->type = diana_rel;
                pOp->value.rel.m_size = 4;
                pOp->value.rel.m_value = (DI_UINT64)(DI_INT64)(DI_INT32)(pc + offset - pCmd->size);
                pOp->usedAddressSize = 4;
                break;
            }
            if (pDesc->flags & DIANA_A32_OPF_J_BITS)
            {
                // S:J1:J2:rest, I1 = NOT(J1 EOR S), I2 = NOT(J2 EOR S)
                DI_UINT32 s = FieldAt(pDesc, 0, word);
                DI_UINT32 i1 = !(FieldAt(pDesc, 1, word) ^ s);
                DI_UINT32 i2 = !(FieldAt(pDesc, 2, word) ^ s);
                value = FieldsFrom(pDesc, 3, word, &width);
                value |= ((s << 2) | (i1 << 1) | i2) << width;
                width += 3;
            }
            else
            {
                value = FieldsFrom(pDesc, 0, word, &width);
            }
            offset = (pDesc->flags & DIANA_A32_OPF_UNSIGNED) ? value : SignExtend32(value, width);
            offset <<= pDesc->param;
            if (pDesc->flags & DIANA_A32_OPF_SUBTRACT)
                offset = 0 - offset;        // T32 ADR T2
            // relative to the next instruction; an aligned base takes the address rounded down to 4
            pOp->type = diana_rel;
            pOp->value.rel.m_size = 4;
            pOp->value.rel.m_value = (DI_UINT64)(DI_INT64)(DI_INT32)(pc + offset - pCmd->size);
            pOp->usedAddressSize = 4;
            if (pDesc->flags & DIANA_A32_OPF_ALIGN)
                pOp->arm.qualifier = DIANA_ARM_QUAL_ALIGN_PC;
        }
        break;

    case DIANA_A32_OPK_MEM_IMM:
    case DIANA_A32_OPK_MEM_REG:
        {
            int add = 1;
            DianaUnifiedRegister base;
            if (pDesc->arr)
            {
                base = GetR((DI_UINT32)pDesc->arr - 1);
            }
            else
            {
                base = GetR(FieldAt(pDesc, 0, word));
                next = 1;
            }
            if (pDesc->flags & DIANA_A32_OPF_HAS_U)
                add = (int)FieldAt(pDesc, next++, word);
            SetMemory(pOp, base, pDesc->flags);
            if (pDesc->flags & DIANA_A32_OPF_SUBTRACTED)
                add = 0;
            if (!add)
                pOp->arm.qualifier = DIANA_ARM_QUAL_NEGATIVE;
            if (pDesc->kind == DIANA_A32_OPK_MEM_IMM)
            {
                DI_UINT32 imm = FieldsFrom(pDesc, next, word, &width) << pDesc->param;
                pOp->value.rmIndex.dispValue = add ? (DI_UINT64)imm : (DI_UINT64)0 - imm;
                break;
            }
            // Rm, then the optional type and amount
            pOp->value.rmIndex.indexed_reg = GetR(FieldAt(pDesc, next, word));
            pOp->value.rmIndex.index = 1;
            pOp->value.rmIndex.dispSize = 0;
            if (pDesc->param)
            {
                // T32: lsl by the fields after Rm, or by the constant param >> 4 (TBH)
                DI_UINT32 amount = next + 1 < pDesc->fieldCount ? FieldsFrom(pDesc, next + 1, word, &width)
                                                                : (DI_UINT32)(pDesc->param >> 4);
                if (amount)
                    SetImmShift(pOp, 0, amount);
            }
            else
            if (next + 1 < pDesc->fieldCount)
                SetImmShift(pOp, FieldAt(pDesc, next + 1, word), FieldsFrom(pDesc, next + 2, word, &width));
        }
        break;

    case DIANA_A32_OPK_SREG:
    case DIANA_A32_OPK_DREG:
        value = FieldsFrom(pDesc, 0, word, &width) + pDesc->param;
        pOp->type = diana_register;
        pOp->value.recognizedRegister = (DianaUnifiedRegister)((pDesc->kind == DIANA_A32_OPK_SREG ? reg_A32_S0 : reg_A32_D0) + (value & 31));
        pOp->usedSize = pDesc->kind == DIANA_A32_OPK_SREG ? 4 : 8;
        break;

    case DIANA_A32_OPK_FPIMM:
        pOp->type = diana_imm;
        pOp->value.imm = pDesc->fieldCount ? VfpExpandImm(FieldsFrom(pDesc, 0, word, &width), pDesc->param) : 0;
        pOp->usedSize = pDesc->param ? pDesc->param / 8 : 4;
        break;

    case DIANA_A32_OPK_FBITS:
        // param - fields (VCVT fixed point, a shift to the right), fields - param (to the left), param (no fields)
        pOp->type = diana_imm;
        value = pDesc->fieldCount ? FieldsFrom(pDesc, 0, word, &width) : 0;
        if (pDesc->flags & DIANA_A32_OPF_REVERSE)
            pOp->value.imm = (DI_UINT64)((int)value - pDesc->param);
        else
            pOp->value.imm = (DI_UINT64)(pDesc->param - (int)value);
        pOp->usedSize = 4;
        break;

    case DIANA_A32_OPK_QREG:
        value = FieldsFrom(pDesc, 0, word, &width);
        pOp->type = diana_register;
        pOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_A32_Q0 + ((value >> 1) & 15));
        pOp->usedSize = 16;
        break;

    case DIANA_A32_OPK_DSCALAR:
        {
            int regFields = pDesc->param & 15;
            DI_UINT32 index = 0;
            int i = 0;
            for (; i < regFields; ++i)
                value = (value << pDesc->width[i]) | FieldAt(pDesc, i, word);
            for (; i < pDesc->fieldCount; ++i)
                index = (index << pDesc->width[i]) | FieldAt(pDesc, i, word);
            pOp->type = diana_register;
            pOp->value.recognizedRegister = (DianaUnifiedRegister)(((pDesc->flags & DIANA_A32_OPF_SINGLE) ? reg_A32_S0 : reg_A32_D0) + (value & 31));
            pOp->arm.arrangement = (DI_UINT8)DIANA_ARM_ARR_ELEMENT(pDesc->param >> 4);
            pOp->arm.lane = (DI_UINT8)index;
            pOp->usedSize = 1 << (pDesc->param >> 4);
        }
        break;

    case DIANA_A32_OPK_NEONIMM:
        {
            // [op, cmode, i, imm3, imm4]; without op it is bit 5 (VMVN, VORR, VBIC)
            int first = pDesc->fieldCount == 5 ? 1 : 0;
            DI_UINT32 op = first ? FieldAt(pDesc, 0, word) : (word >> 5) & 1;
            DI_UINT32 cmode = FieldAt(pDesc, first, word);
            DI_UINT32 imm8 = FieldsFrom(pDesc, first + 1, word, &width);
            pOp->type = diana_imm;
            pOp->value.imm = AdvSimdExpandImm(op, cmode, imm8);
            pOp->usedSize = 8;
        }
        break;

    case DIANA_A32_OPK_ROTATE:
        pOp->type = diana_imm;
        value = FieldsFrom(pDesc, 0, word, &width);
        pOp->value.imm = pDesc->param ? 90 + value * 180 : value * 90;
        pOp->usedSize = 4;
        break;

    case DIANA_A32_OPK_VLIST:
        FillVList(pDesc, word, pOp);
        break;

    case DIANA_A32_OPK_VMEM:
        FillVMem(pDesc, word, pOp);
        break;

    case DIANA_A32_OPK_SREGLIST:
    case DIANA_A32_OPK_DREGLIST:
        {
            // [first register fields..., count]: the count field is imm8, registers for S, twice the registers for D
            int count = 0;
            DI_UINT32 first = 0;
            int i = 0;
            for (; i + 1 < pDesc->fieldCount; ++i)
                first = (first << pDesc->width[i]) | FieldAt(pDesc, i, word);
            count = (int)FieldAt(pDesc, pDesc->fieldCount - 1, word);
            if (pDesc->flags & DIANA_A32_OPF_COUNT_PLUS1)
                ++count;                // VTBL: len + 1
            else
            if (pDesc->kind == DIANA_A32_OPK_DREGLIST)
                count >>= 1;
            pOp->type = diana_arm_reglist;
            pOp->value.recognizedRegister = (DianaUnifiedRegister)((pDesc->kind == DIANA_A32_OPK_SREGLIST ? reg_A32_S0 : reg_A32_D0) + (first & 31));
            pOp->arm.regCount = (DI_UINT8)count;
        }
        break;

    case DIANA_A32_OPK_FPSYSREG:
        pOp->type = diana_arm_special;
        pOp->value.imm = FieldsFrom(pDesc, 0, word, &width);
        break;

    case DIANA_A32_OPK_REGLIST:
        // the list, bit n is rn; an extra bit adds register param
        // extra bits: [list, extra1, extra2] for the registers param & 15 and param >> 4 (T32 M lr, P pc)
        value = FieldAt(pDesc, 0, word);
        if ((pDesc->fieldCount > 1 && FieldAt(pDesc, 1, word)) || (pDesc->flags & DIANA_A32_OPF_ALWAYS))
            value |= 1u << (pDesc->param & 15);
        if (pDesc->fieldCount > 2 && FieldAt(pDesc, 2, word))
            value |= 1u << ((pDesc->param >> 4) & 15);
        if (pDesc->flags & DIANA_A32_OPF_USER)
            pOp->arm.qualifier = DIANA_ARM_QUAL_USER;
        pOp->type = diana_arm_regmask;
        pOp->value.imm = value;
        pOp->arm.regCount = (DI_UINT8)PopCount16(value);
        break;

    default:
        return DI_ERROR;
    }
    return DI_SUCCESS;
}

// ------------------------------------------------------------------ decoding
static int MatchesUnless(const DianaA32Tables * pTables, const DianaA32CmdInfo * pCmd, DI_UINT32 word)
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

static int HasEqualFields(const DianaA32Tables * pTables, const DianaA32CmdInfo * pCmd, DI_UINT32 word)
{
    const DianaA64Distinct * pDistinct = pTables->pDistinct + pCmd->distinctFirst;
    int i = 0;
    for (; i < pCmd->distinctCount; ++i)
    {
        DI_UINT32 mask = (1u << pDistinct[i].width) - 1;
        if (((word >> pDistinct[i].lsb1) & mask) == ((word >> pDistinct[i].lsb2) & mask))
            return 1;
    }
    return 0;
}

static int IsCmdEnabled(const DianaA32Tables * pTables, const DianaA32CmdInfo * pCmd, DI_UINT64 profileFeatures)
{
    if (pCmd->features & ~profileFeatures)
        return 0;
    return !pCmd->featuresAny || (pTables->pFeaturesAny[pCmd->featuresAny] & profileFeatures) != 0;
}

int DianaA32_IsCmdEnabled(const DianaA32CmdInfo * pCmd, DI_UINT64 profileFeatures)
{
    return IsCmdEnabled(DianaA32_GetTables(), pCmd, profileFeatures);
}

static DI_UINT32 WalkTree(const DianaA32Tree * pTree, DI_UINT32 word)
{
    const DianaA64Node * pNode = pTree->pNodes;
    DI_UINT32 slot = 0;
    for (;;)
    {
        slot = pTree->pSlots[pNode->first + ((word >> pNode->shift) & ((1u << pNode->bits) - 1))];
        if (!(slot & DIANA_A64_SLOT_NODE_FLAG))
            return slot;
        pNode = pTree->pNodes + (slot & ~DIANA_A64_SLOT_NODE_FLAG);
    }
}

static int FillResult(DianaA32CmdInfo * pCmd, DI_UINT32 word, DianaParserResult * pResult)
{
    int i = 0;
    for (; i < pCmd->operandCount; ++i)
    {
        DI_CHECK(FillOperand(pCmd, pCmd->operands + i, word, pResult->linkedOperands + i));
    }
    pResult->pInfo = &pCmd->header;
    pResult->iLinkedOpCount = pCmd->operandCount;
    return DI_SUCCESS;
}

#if defined(_MSC_VER)
#define DIANA_A32_COLD __declspec(noinline)
#elif defined(__GNUC__)
#define DIANA_A32_COLD __attribute__((noinline, cold))
#else
#define DIANA_A32_COLD
#endif

// no instruction for the word: the ".inst" command that says why
DIANA_A32_COLD
static long GetUndefinedCmd(const DianaContext * pContext, const DianaA32Tree * pTree, const DianaA32Tree * pUnsupported,
                            int form, DI_UINT32 word)
{
    const DianaA32Tables * pTables = DianaA32_GetTables();
    DI_UINT32 slot = WalkTree(pTree, word);
    const DianaA64Leaf * pLeaf = pTree->pLeaves + (slot >> DIANA_A64_SLOT_COUNT_BITS);
    const DianaA64Leaf * pEnd = pLeaf + (slot & ((1u << DIANA_A64_SLOT_COUNT_BITS) - 1));
    for (; slot && pLeaf < pEnd; ++pLeaf)
    {
        const DianaA32CmdInfo * pCmd = pTables->pCmds + pLeaf->cmd;
        if ((word & pLeaf->mask) != pLeaf->value || IsCmdEnabled(pTables, pCmd, pContext->armFeatures))
            continue;
        if (pCmd->unlessCount && MatchesUnless(pTables, pCmd, word))
            continue;
        if (pCmd->distinctCount && HasEqualFields(pTables, pCmd, word))
            continue;
        return pTables->disabledCmds[form];
    }
    slot = WalkTree(pUnsupported, word);
    pLeaf = pUnsupported->pLeaves + (slot >> DIANA_A64_SLOT_COUNT_BITS);
    pEnd = pLeaf + (slot & ((1u << DIANA_A64_SLOT_COUNT_BITS) - 1));
    for (; pLeaf < pEnd; ++pLeaf)
    {
        if ((word & pLeaf->mask) == pLeaf->value)
            return pTables->unsupportedCmds[form];
    }
    return pTables->undefinedCmds[form];
}

// the instruction of the word in the tree, 0 if none
static DianaA32CmdInfo * Lookup(const DianaContext * pContext, const DianaA32Tree * pTree, DI_UINT32 word,
                                DianaParserResult * pResult)
{
    const DianaA32Tables * pTables = DianaA32_GetTables();
    DI_UINT32 slot = WalkTree(pTree, word);
    const DianaA64Leaf * pLeaf = pTree->pLeaves + (slot >> DIANA_A64_SLOT_COUNT_BITS);
    const DianaA64Leaf * pEnd = pLeaf + (slot & ((1u << DIANA_A64_SLOT_COUNT_BITS) - 1));
    for (; slot && pLeaf < pEnd; ++pLeaf)
    {
        DianaA32CmdInfo * pCmd = 0;
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
            return pCmd;
    }
    return 0;
}

static void InitResult(DianaParserResult * pResult, int size)
{
    pResult->pInfo = 0;
    pResult->iLinkedOpCount = 0;
    pResult->iPrefix = DIANA_A32_COND_AL;
    pResult->iRexPrefix = 0;
    pResult->iFullCmdSize = size;
    pResult->bytesCount = size;
}

int DianaA32_DecodeA32(DianaContext * pContext,
                       DI_UINT32 word,
                       DianaParserResult * pResult)
{
    const DianaA32Tables * pTables = DianaA32_GetTables();
    DianaA32CmdInfo * pCmd = 0;

    InitResult(pResult, 4);
    pResult->bytes[0] = (unsigned char)word;
    pResult->bytes[1] = (unsigned char)(word >> 8);
    pResult->bytes[2] = (unsigned char)(word >> 16);
    pResult->bytes[3] = (unsigned char)(word >> 24);

    pCmd = Lookup(pContext, &pTables->a32, word, pResult);
    if (!pCmd)
    {
        pCmd = pTables->pCmds + GetUndefinedCmd(pContext, &pTables->a32, &pTables->a32Unsupported, DIANA_A32_FORM_A32, word);
        return FillResult(pCmd, word, pResult);
    }
    if (pCmd->condLsb != DIANA_A32_NO_COND)
        pResult->iPrefix = (int)((word >> pCmd->condLsb) & 15);
    return DI_SUCCESS;
}

int DianaA32_GetT32Size(DI_UINT32 hw1)
{
    // 0b11101, 0b11110, 0b11111 start a 32-bit instruction
    return ((hw1 >> 11) & 0x1F) >= 0x1D ? 4 : 2;
}

// ITAdvance: the condition of the next instruction in the block
static int AdvanceIt(int state)
{
    if ((state & 7) == 0)
        return 0;
    return (state & 0xE0) | ((state << 1) & 0x1F);
}

int DianaA32_DecodeT32(DianaContext * pContext,
                       DI_UINT32 hw1,
                       DI_UINT32 hw2,
                       DianaParserResult * pResult)
{
    const DianaA32Tables * pTables = DianaA32_GetTables();
    DianaA32CmdInfo * pCmd = 0;
    int size = DianaA32_GetT32Size(hw1);
    int state = pContext->iArmFlags & 0xFF;
    DI_UINT32 word = (hw1 & 0xFFFF) << 16;

    InitResult(pResult, size);
    pResult->bytes[0] = (unsigned char)hw1;
    pResult->bytes[1] = (unsigned char)(hw1 >> 8);
    if (size == 4)
    {
        word |= hw2 & 0xFFFF;
        pResult->bytes[2] = (unsigned char)hw2;
        pResult->bytes[3] = (unsigned char)(hw2 >> 8);
    }

    pCmd = Lookup(pContext, &pTables->t32, word, pResult);
    if (!pCmd)
    {
        int form = size == 2 ? DIANA_A32_FORM_T16 : DIANA_A32_FORM_T32;
        pCmd = pTables->pCmds + GetUndefinedCmd(pContext, &pTables->t32, &pTables->t32Unsupported, form, word);
        DI_CHECK(FillResult(pCmd, word, pResult));
    }
    else
    if (pCmd->condLsb != DIANA_A32_NO_COND)
    {
        pResult->iPrefix = (int)((word >> pCmd->condLsb) & 15);
    }
    else
    if (state & 0x0F)
    {
        // in an IT block: ITSTATE<7:4> is the condition
        pResult->iPrefix = state >> 4;
    }
    if (state & 0x0F)
    {
        pResult->iPrefix |= DIANA_A32_PREFIX_IN_IT | ((state >> 4) << DIANA_A32_PREFIX_IT_SHIFT);
        if (AdvanceIt(state))
            pResult->iPrefix |= DIANA_A32_PREFIX_NOT_LAST;
    }

    if (pCmd->header.m_pGroupInfo->m_commandId == (DianaCommandId)diana_a32_cmd_it)
        pContext->iArmFlags = (pContext->iArmFlags & ~0xFF) | (int)(hw1 & 0xFF);
    else
        pContext->iArmFlags = (pContext->iArmFlags & ~0xFF) | AdvanceIt(state);
    return DI_SUCCESS;
}

void DianaA32_ResetItState(DianaContext * pContext)
{
    pContext->iArmFlags &= ~0xFF;
}

int DianaA32_ParseCmd(DianaContext * pContext,
                      DianaReadStream * readStream,
                      DianaParserResult * pResult)
{
    unsigned char bytes[4];
    int read = 0;

    pResult->pInfo = 0;
    pResult->iLinkedOpCount = 0;
    pResult->iFullCmdSize = 0;
    pResult->bytesCount = 0;

    if (pContext->iArch == DIANA_ARCH_ARM32)
    {
        DI_CHECK(readStream->pReadFnc(readStream, bytes, 4, &read));
        if (read == 0)
            return DI_END;
        if (read != 4)
        {
            pResult->iFullCmdSize = read;
            return DI_END_OF_STREAM;
        }
        return DianaA32_DecodeA32(pContext,
                                  (DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8) | ((DI_UINT32)bytes[2] << 16) |
                                  ((DI_UINT32)bytes[3] << 24),
                                  pResult);
    }

    DI_CHECK(readStream->pReadFnc(readStream, bytes, 2, &read));
    if (read == 0)
        return DI_END;
    if (read != 2)
    {
        pResult->iFullCmdSize = read;
        return DI_END_OF_STREAM;
    }
    if (DianaA32_GetT32Size((DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8)) == 4)
    {
        DI_CHECK(readStream->pReadFnc(readStream, bytes + 2, 2, &read));
        if (read != 2)
        {
            pResult->iFullCmdSize = 2 + read;
            return DI_END_OF_STREAM;
        }
    }
    else
    {
        bytes[2] = bytes[3] = 0;
    }
    return DianaA32_DecodeT32(pContext,
                              (DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8),
                              (DI_UINT32)bytes[2] | ((DI_UINT32)bytes[3] << 8),
                              pResult);
}

// ------------------------------------------------------------------ results
const DianaA32CmdInfo * DianaA32_GetCmdInfo(const DianaParserResult * pResult)
{
    long id = 0;
    if (!pResult->pInfo || !pResult->pInfo->m_pGroupInfo)
        return 0;
    id = (long)pResult->pInfo->m_pGroupInfo->m_commandId;
    if (id < DIANA_A32_CMD_BASE || id >= diana_a32_cmd_end)
        return 0;
    return (const DianaA32CmdInfo *)pResult->pInfo;
}

int DianaA32_GetCondition(const DianaParserResult * pResult)
{
    if (!DianaA32_GetCmdInfo(pResult))
        return DIANA_A32_COND_AL;
    return pResult->iPrefix & DIANA_A32_PREFIX_COND_MASK;
}

int DianaA32_GetWordClass(const DianaParserResult * pResult)
{
    const DianaA32CmdInfo * pCmd = DianaA32_GetCmdInfo(pResult);
    if (!pCmd)
        return -1;
    switch ((int)pCmd->header.m_pGroupInfo->m_commandId)
    {
    case diana_a32_cmd_inst:                return DIANA_A64_WORD_UNALLOCATED;
    case diana_a32_cmd_inst_unsupported:    return DIANA_A64_WORD_UNSUPPORTED;
    case diana_a32_cmd_inst_disabled:       return DIANA_A64_WORD_DISABLED;
    }
    return DIANA_A64_WORD_SUPPORTED;
}

DianaGroupInfo * DianaA32_GetGroupInfo(long lId)
{
    const DianaA32Tables * pTables = DianaA32_GetTables();
    if (lId < DIANA_A32_CMD_BASE || lId >= DIANA_A32_CMD_BASE + pTables->groupsCount)
        return 0;
    return pTables->pGroups + (lId - DIANA_A32_CMD_BASE);
}

#endif
