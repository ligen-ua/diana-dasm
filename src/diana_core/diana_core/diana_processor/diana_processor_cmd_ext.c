#include "diana_processor_cmd_ext.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

// General-register extensions: POPCNT, LZCNT, TZCNT, BMI1 (andn, bextr, blsi, blsmsk, blsr), BMI2 (bzhi, mulx, pdep,
// pext, rorx, sarx, shlx, shrx), CRC32, MOVBE, ADX (adcx, adox), the CET hints endbr32/endbr64. The flags follow the
// SDM; the undefined ones are left as they are. The operands are in the order of the decoder (the Intel order).

#define DI_EXT_ARITH_FLAGS_CLEAR \
    CLEAR_FLAG_CF; CLEAR_FLAG_PF; CLEAR_FLAG_AF; CLEAR_FLAG_ZF; CLEAR_FLAG_SF; CLEAR_FLAG_OF;

static OPERAND_SIZE Di_SizeMask(int size)
{
    return size >= 8 ? ~(OPERAND_SIZE)0 : (((OPERAND_SIZE)1 << (size * 8)) - 1);
}

static void Di_SetZFSF(DianaProcessor * pCallContext, OPERAND_SIZE value, int size)
{
    CLEAR_FLAG_ZF;
    CLEAR_FLAG_SF;
    value &= Di_SizeMask(size);
    if (!value)
        SET_FLAG_ZF;
    if (value & DianaProcessor_GetSignMask(size))
        SET_FLAG_SF;
}

int Diana_Call_endbr32(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}

int Diana_Call_endbr64(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    pDianaContext; pCallContext;
    DI_PROC_END
}

// popcnt: ZF of the source, the other flags cleared
int Diana_Call_popcnt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    OPERAND_SIZE count = 0, value = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);
    value = src & Di_SizeMask(src_size);
    for (; value; value &= value - 1)
        ++count;
    DI_EXT_ARITH_FLAGS_CLEAR
    if (!(src & Di_SizeMask(src_size)))
        SET_FLAG_ZF;
    dest = count;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// lzcnt, tzcnt: the count, the operand size for 0; CF for a source of 0, ZF for a count of 0
static int Di_CountZeros(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int leading)
{
    OPERAND_SIZE count = 0, value = 0;
    int bits = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);
    bits = src_size * 8;
    value = src & Di_SizeMask(src_size);
    if (leading)
    {
        OPERAND_SIZE bit = (OPERAND_SIZE)1 << (bits - 1);
        for (; count < (OPERAND_SIZE)bits && !(value & bit); ++count, bit >>= 1)
        {
        }
    }
    else
    {
        for (; count < (OPERAND_SIZE)bits && !((value >> count) & 1); ++count)
        {
        }
    }
    CLEAR_FLAG_CF;
    CLEAR_FLAG_ZF;
    if (!value)
        SET_FLAG_CF;
    if (!count)
        SET_FLAG_ZF;
    dest = count;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_lzcnt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_CountZeros(pDianaContext, pCallContext, 1);
}

int Diana_Call_tzcnt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_CountZeros(pDianaContext, pCallContext, 0);
}

// andn dest, src1, src2: NOT src1 AND src2; SF ZF, CF OF cleared
int Diana_Call_andn(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_DEF_LOCALS2(src1, src2, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET(src1, 1);
    DI_MEM_GET(src2, 2);
    dest = ~src1 & src2 & Di_SizeMask(dest_size);
    Di_SetZFSF(pCallContext, dest, dest_size);
    CLEAR_FLAG_CF;
    CLEAR_FLAG_OF;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// bextr dest, src, control: control[7:0] the start, control[15:8] the length; ZF, CF OF cleared
int Diana_Call_bextr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    unsigned start = 0, length = 0, bits = 0;
    DI_DEF_LOCALS2(src, control, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET(src, 1);
    DI_MEM_GET(control, 2);
    bits = dest_size * 8;
    start = (unsigned)(control & 0xff);
    length = (unsigned)((control >> 8) & 0xff);
    src &= Di_SizeMask(dest_size);
    dest = start < bits ? src >> start : 0;
    if (length < bits)
        dest &= ((OPERAND_SIZE)1 << length) - 1;
    Di_SetZFSF(pCallContext, dest, dest_size);
    CLEAR_FLAG_CF;
    CLEAR_FLAG_OF;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// blsi, blsmsk, blsr: the lowest set bit isolated, the mask up to it, the lowest set bit reset; CF, OF cleared
typedef enum { DI_BLSI, DI_BLSMSK, DI_BLSR } DiBlsOperation;

static int Di_Bls(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiBlsOperation operation)
{
    OPERAND_SIZE mask = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET(src, 1);
    mask = Di_SizeMask(dest_size);
    src &= mask;
    switch (operation)
    {
    case DI_BLSI:
        dest = (0 - src) & src;
        break;
    case DI_BLSMSK:
        dest = (src - 1) ^ src;
        break;
    default:
        dest = (src - 1) & src;
        break;
    }
    dest &= mask;
    Di_SetZFSF(pCallContext, dest, dest_size);
    CLEAR_FLAG_OF;
    CLEAR_FLAG_CF;
    // blsi: CF for a source that isn't 0; blsmsk, blsr: CF for a source of 0
    if (operation == DI_BLSI ? src != 0 : src == 0)
        SET_FLAG_CF;
    // blsmsk: ZF cleared
    if (operation == DI_BLSMSK)
        CLEAR_FLAG_ZF;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_blsi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Bls(pDianaContext, pCallContext, DI_BLSI);
}

int Diana_Call_blsmsk(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Bls(pDianaContext, pCallContext, DI_BLSMSK);
}

int Diana_Call_blsr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_Bls(pDianaContext, pCallContext, DI_BLSR);
}

// bzhi dest, src, index: the bits from index[7:0] up cleared; CF for an index past the operand, ZF SF, OF cleared
int Diana_Call_bzhi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    unsigned index = 0, bits = 0;
    DI_DEF_LOCALS2(src, control, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET(src, 1);
    DI_MEM_GET(control, 2);
    bits = dest_size * 8;
    index = (unsigned)(control & 0xff);
    dest = src & Di_SizeMask(dest_size);
    if (index < bits)
        dest &= ((OPERAND_SIZE)1 << index) - 1;
    Di_SetZFSF(pCallContext, dest, dest_size);
    CLEAR_FLAG_OF;
    CLEAR_FLAG_CF;
    if (index > bits - 1)
        SET_FLAG_CF;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// mulx high, low, src: EDX/RDX * src unsigned, no flags; the high half wins when high and low are one register
int Diana_Call_mulx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    OPERAND_SIZE high = 0, low = 0;
    DI_DEF_LOCALS2(src, high_dest, low_dest);
    oldDestValue;
    DI_MEM_GET(high_dest, 0);
    DI_MEM_GET(low_dest, 1);
    DI_MEM_GET(src, 2);
    if (high_dest_size == 8)
    {
        // 64 x 64 -> 128 in 32-bit halves
        OPERAND_SIZE a = GET_REG_RDX, b = src;
        OPERAND_SIZE aLow = a & 0xffffffffULL, aHigh = a >> 32, bLow = b & 0xffffffffULL, bHigh = b >> 32;
        OPERAND_SIZE ll = aLow * bLow, lh = aLow * bHigh, hl = aHigh * bLow, hh = aHigh * bHigh;
        OPERAND_SIZE middle = (ll >> 32) + (lh & 0xffffffffULL) + (hl & 0xffffffffULL);
        low = (middle << 32) | (ll & 0xffffffffULL);
        high = hh + (lh >> 32) + (hl >> 32) + (middle >> 32);
    }
    else
    {
        OPERAND_SIZE product = (OPERAND_SIZE)(DI_UINT32)GET_REG_EDX * (OPERAND_SIZE)(DI_UINT32)src;
        low = product & 0xffffffffULL;
        high = product >> 32;
    }
    low_dest = low;
    high_dest = high;
    DI_MEM_SET(low_dest, 1);
    DI_MEM_SET(high_dest, 0);
    DI_PROC_END
}

// pdep dest, src, mask: the low bits of src to the set bits of mask; pext: the bits of src at the set bits of mask
// to the low bits; no flags
static int Di_BitsDepositExtract(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int deposit)
{
    OPERAND_SIZE result = 0, bit = 1, k = 1;
    unsigned i = 0;
    DI_DEF_LOCALS2(src, mask, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET(src, 1);
    DI_MEM_GET(mask, 2);
    mask &= Di_SizeMask(dest_size);
    for (i = 0; i < (unsigned)dest_size * 8; ++i, bit <<= 1)
    {
        if (!(mask & bit))
            continue;
        if (deposit)
        {
            if (src & k)
                result |= bit;
        }
        else
        {
            if (src & bit)
                result |= k;
        }
        k <<= 1;
    }
    dest = result;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_pdep(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_BitsDepositExtract(pDianaContext, pCallContext, 1);
}

int Diana_Call_pext(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_BitsDepositExtract(pDianaContext, pCallContext, 0);
}

// rorx dest, src, imm8: rotate right, no flags
int Diana_Call_rorx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    unsigned count = 0, bits = 0;
    DI_DEF_LOCALS2(src, imm, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET(src, 1);
    DI_MEM_GET(imm, 2);
    bits = dest_size * 8;
    count = (unsigned)imm & (bits - 1);
    src &= Di_SizeMask(dest_size);
    dest = count ? ((src >> count) | (src << (bits - count))) & Di_SizeMask(dest_size) : src;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// sarx, shlx, shrx dest, src, count: the count masked to 5 or 6 bits, no flags
typedef enum { DI_SARX, DI_SHLX, DI_SHRX } DiShiftOperation;

static int Di_ShiftX(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiShiftOperation operation)
{
    unsigned count = 0, bits = 0;
    DI_DEF_LOCALS2(src, shift, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET(src, 1);
    DI_MEM_GET(shift, 2);
    bits = dest_size * 8;
    count = (unsigned)shift & (bits - 1);
    src &= Di_SizeMask(dest_size);
    switch (operation)
    {
    case DI_SHLX:
        dest = src << count;
        break;
    case DI_SHRX:
        dest = src >> count;
        break;
    default:
        dest = src >> count;
        if (count && (src & DianaProcessor_GetSignMask(dest_size)))
            dest |= Di_SizeMask(dest_size) & ~(Di_SizeMask(dest_size) >> count);
        break;
    }
    dest &= Di_SizeMask(dest_size);
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_sarx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_ShiftX(pDianaContext, pCallContext, DI_SARX);
}

int Diana_Call_shlx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_ShiftX(pDianaContext, pCallContext, DI_SHLX);
}

int Diana_Call_shrx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_ShiftX(pDianaContext, pCallContext, DI_SHRX);
}

// crc32 dest, src: CRC-32C (Castagnoli, reflected 0x82F63B78) of the source bytes, from the low 32 bits of dest;
// the result zero-extended to dest, no flags
int Diana_Call_crc32(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DI_UINT32 crc = 0;
    int i = 0, bit = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);
    crc = (DI_UINT32)dest;
    for (i = 0; i < src_size; ++i)
    {
        crc ^= (DI_UINT32)((src >> (i * 8)) & 0xff);
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0x82F63B78UL & (0 - (crc & 1)));
    }
    dest = crc;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// movbe: a load or a store with the bytes reversed, no flags
int Diana_Call_movbe(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    int i = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);
    dest = 0;
    for (i = 0; i < dest_size; ++i)
        dest |= ((src >> (i * 8)) & 0xff) << ((dest_size - 1 - i) * 8);
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

// adcx, adox: dest + src + CF (OF), the carry out in CF (OF); the other flags unchanged
static int Di_AddCarry(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int overflowFlag)
{
    OPERAND_SIZE mask = 0, sum = 0, carry = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);
    mask = Di_SizeMask(dest_size);
    dest &= mask;
    src &= mask;
    carry = overflowFlag ? (GET_FLAG_OF != 0) : (GET_FLAG_CF != 0);
    sum = (dest + src + carry) & mask;
    // the carry out: the sum wrapped, or it equals dest with a carry in and src all ones
    carry = sum < dest || (carry && sum == dest);
    if (overflowFlag)
    {
        CLEAR_FLAG_OF;
        if (carry)
            SET_FLAG_OF;
    }
    else
    {
        CLEAR_FLAG_CF;
        if (carry)
            SET_FLAG_CF;
    }
    dest = sum;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_adcx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AddCarry(pDianaContext, pCallContext, 0);
}

int Diana_Call_adox(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AddCarry(pDianaContext, pCallContext, 1);
}
