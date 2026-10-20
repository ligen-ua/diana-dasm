#ifndef DIANA_ARM_H
#define DIANA_ARM_H

#include "diana_core.h"
#include "diana_gen_a64.h"

#ifdef __cplusplus
extern "C" {
#endif

// operand kinds, keep in sync with OperandKind in diana_gen_arm/arm_spec.h
#define DIANA_A64_OPK_NONE          0
#define DIANA_A64_OPK_REG_W         1
#define DIANA_A64_OPK_REG_X         2
#define DIANA_A64_OPK_REG_WSP       3
#define DIANA_A64_OPK_REG_XSP       4
#define DIANA_A64_OPK_AIMM          5
#define DIANA_A64_OPK_BIMM32        6
#define DIANA_A64_OPK_BIMM64        7
#define DIANA_A64_OPK_UIMM          8
#define DIANA_A64_OPK_MOVW          9
#define DIANA_A64_OPK_SHREG_W       10
#define DIANA_A64_OPK_SHREG_X       11
#define DIANA_A64_OPK_EXTREG_W      12
#define DIANA_A64_OPK_EXTREG_X      13
#define DIANA_A64_OPK_LABEL         14
#define DIANA_A64_OPK_ADR           15
#define DIANA_A64_OPK_ADRP          16
#define DIANA_A64_OPK_LITERAL       17
#define DIANA_A64_OPK_COND          18
#define DIANA_A64_OPK_TBIT          19
#define DIANA_A64_OPK_MEM_BASE      20
#define DIANA_A64_OPK_MEM_UOFF      21
#define DIANA_A64_OPK_MEM_SOFF      22
#define DIANA_A64_OPK_MEM_PRE       23
#define DIANA_A64_OPK_MEM_POST      24
#define DIANA_A64_OPK_MEM_REGOFF    25
#define DIANA_A64_OPK_SYSREG        26
#define DIANA_A64_OPK_CREG          27
#define DIANA_A64_OPK_BARRIER       28
#define DIANA_A64_OPK_PRFOP         29
#define DIANA_A64_OPK_SIMM          30
// FP and SIMD. Fields are read one by one in the order given, [] marks an optional field:
#define DIANA_A64_OPK_FPREG         31      // b0..q0: [reg, size source]
#define DIANA_A64_OPK_VREG          32      // v0.4s: [reg, size source, Q]
#define DIANA_A64_OPK_VELEM         33      // v0.s[1]: [reg, size source, index...], param = DIANA_A64_INDEX_*
#define DIANA_A64_OPK_VLIST         34      // {v0.4s-v1.4s}: [reg, size source, Q], param = register count
#define DIANA_A64_OPK_VLIST_ELEM    35      // {v0.s-v1.s}[1]: [reg, index...], param = count | shift << 3, index = fields >> shift
#define DIANA_A64_OPK_GPREG_IMM5    36      // w/x by imm5 (x for a D element): [reg, imm5]
#define DIANA_A64_OPK_FPIMM         37      // VFPExpandImm(imm8), fields concatenated
#define DIANA_A64_OPK_FZERO         38      // #0.0
#define DIANA_A64_OPK_MODIMM        39      // AdvSIMD modified immediate, fields concatenated, param = DIANA_A64_MODIMM_*
#define DIANA_A64_OPK_SHIFTIMM      40      // shift or fbits from immh:immb or scale, param = DIANA_A64_SHIFT_*
#define DIANA_A64_OPK_CONST         41      // param << [field]
#define DIANA_A64_OPK_MEM_POSTIMM   42      // [Xn|SP], #(param << [field]): [Rn, field]
#define DIANA_A64_OPK_MEM_POSTREG   43      // [Xn|SP], Xm: [Rn, Rm]
#define DIANA_A64_OPK_ROTATE        44      // FCMLA #0..#270: rot * 90 (2-bit rot); FCADD #90/#270: 90 + rot * 180 (1 bit)
#define DIANA_A64_OPK_MEM_PREIMM    45      // [Xn|SP, #-param]!: [Rn] (STILP, STLR with writeback)
// SVE and SME, the element size is read like DIANA_A64_ARR_* with the DIANA_A64_SARR_* layout:
#define DIANA_A64_OPK_ZREG          46      // z0.s, z0: [reg, size source...]
#define DIANA_A64_OPK_PREG          47      // p0.s, p0/m, p0/z: [reg, size source...], param = DIANA_ARM_QUAL_*,
                                            // 3: merging by a field [reg, size source..., M] (MOVPRFX),
                                            // 4: an index [reg, index fields...] (pn13[2]); flag SVE_PN: pn0..pn15
                                            // (a 3-bit field is pn8..pn15)
#define DIANA_A64_OPK_SVE_PATTERN   48      // vl64, mul #4: [pattern, imm4], no imm4 is mul #1 (PTRUE); all, mul #1 is left out
#define DIANA_A64_OPK_SVE_IMM8SH    49      // #-15616, #0, lsl #8: [sh, imm8], value imm8 << 8 * sh, param 1: imm8 is signed
#define DIANA_A64_OPK_GPREG_SIZE    50      // w or x: [reg, field], x when the field is all ones (size 11, sf 1)
#define DIANA_A64_OPK_ZELEM         51      // z1.b[3]: [reg, size source..., index fields...], param 1: index >> (element + 1)
#define DIANA_A64_OPK_ZLIST         52      // {z1.b}, {z0.s-z1.s}, {p2.h-p3.h}: [reg, size source...], param = register count,
                                            // bit 3: P registers; a field narrower than the register number counts
                                            // in steps of the count (Zn:4 is z0, z2...); bit 4: strided,
                                            // [T, Zt, size source...], the first is T:0:Zt, arm.regStride 16 / count
#define DIANA_A64_OPK_SVE_BIMM      53      // #0xffe0000f: [N:immr:imms], the bitmask immediate of the element size
#define DIANA_A64_OPK_SVE_FPCONST   54      // #0.5: [i1], value.imm = the IEEE double; param: 0 #0.5/#1.0, 1 #0.5/#2.0, 2 #0.0/#1.0
// SVE memory, diana_index operands: rmIndex.reg is X/SP or a Z register, indexed_reg X or Z, arm.arrangement the
// element size of the Z registers
#define DIANA_A64_OPK_SVE_MEM_IMM   55      // [x0, #-2, mul vl], [x4, #37]: [Rn, imm...], offset = imm * param
#define DIANA_A64_OPK_SVE_MEM_REG   56      // [x0, x1, lsl #3], [x0, z1.s, sxtw #2], [z1.d, z2.d, lsl #1]:
                                            // [base, index, size source..., extend field, shift field],
                                            // param = the shift, 0x80: the shift is the last field (ADR msz),
                                            // 0x40: an X index that is xzr is left out ([z1.d, x2], [z1.d])
#define DIANA_A64_OPK_SVE_MEM_ZIMM  57      // [z1.d, #8]: [Zn, size source..., imm], offset = imm * param
#define DIANA_A64_OPK_SVE_PRFOP     58      // pldl1keep..pstl3strm, #6: [prfop]
#define DIANA_A64_OPK_PREG_INDEX    59      // psel p1.h[w12, 3], diana_index: rmIndex.reg = P, indexed_reg = W(param & 0x7F + Rv),
                                            // dispValue = index: [reg, size source..., Rv, index fields], index >> (element + 1);
                                            // param 0x80: the size source fields end the index (i1:tszh:tszl)
#define DIANA_A64_OPK_SVE_VLX       60      // vlx2, vlx4: [vl], value.imm = 2 or 4
// SME
#define DIANA_A64_OPK_ZA_TILE       61      // za3.s: [tile, size source...], reg_A64_ZA0 + tile
#define DIANA_A64_OPK_ZA_SLICE      62      // za1h.s[w12, 3], {za0v.b[w13, 7]}, za[w12, 5], za.s[w8, 4:7, vgx2]: diana_index,
                                            // rmIndex.reg is the tile (reg_A64_ZA0 + n) or reg_A64_ZA, indexed_reg the W
                                            // register, dispValue the (first) offset, arm.qualifier
                                            // DIANA_ARM_QUAL_HORIZONTAL/VERTICAL, arm.lane the slices after the first
                                            // (1 for 2:3, 3 for 4:7), arm.regCount the vector group (0, 2, 4);
                                            // [tile, V, size source..., Rs, offset], param = DIANA_A64_ZA_*
#define DIANA_A64_OPK_ZA_MASK       63      // zero {za0.s, za2.d}: [imm8], one bit per 64-bit tile
#define DIANA_A64_OPK_ZT0           64      // zt0, {zt0}, zt0[56]: reg_A64_ZT0; param 0x01 braces, 0x02 an index field,
                                            // bits 4..6 log2 of its multiplier (arm.lane is the index)

// ZA_SLICE params: which fields there are
#define DIANA_A64_ZA_TILE           0x01    // a tile field (else za0)
#define DIANA_A64_ZA_HV             0x02    // a horizontal/vertical field
#define DIANA_A64_ZA_OFFSET         0x04    // an offset field (else 0)
#define DIANA_A64_ZA_BRACES         0x08    // printed in braces, the slice is a list of one (LD1B)
#define DIANA_A64_ZA_ARRAY          0x10    // the whole array: za[w12, 0] (LDR, STR)
#define DIANA_A64_ZA_W8             0x20    // the W register is w8 + field, else w12 + field

// DianaA64OperandDesc::flags
#define DIANA_A64_OPF_DECIMAL       0x01    // print the immediate in decimal
#define DIANA_A64_OPF_NEGATIVE      0x02    // LABEL: the offset is subtracted (RETAASPPC)
#define DIANA_A64_OPF_OPTIONAL_OFFSET 0x04  // MEM_PRE: a zero offset is not printed, [x1]! (LDRAA)
#define DIANA_A64_OPF_WRITEBACK     0x08    // MEM_BASE [x0]!, REG_X x1!: the register is updated (MOPS)
#define DIANA_A64_OPF_SVE_SIGNED    0x10    // SVE_MEM_IMM: the immediate is signed
#define DIANA_A64_OPF_SVE_MUL_VL    0x20    // SVE_MEM_IMM: the offset counts vector lengths, mul vl
#define DIANA_A64_OPF_SVE_INDEX_Z   0x10    // SVE_MEM_REG: the index is a Z register
#define DIANA_A64_OPF_SVE_BASE_Z    0x20    // SVE_MEM_REG: the base is a Z register (ADR)
#define DIANA_A64_OPF_SVE_UXTW      0x40    // SVE_MEM_REG: the index is extended, uxtw
#define DIANA_A64_OPF_SVE_SXTW      0x80    // SVE_MEM_REG: sxtw
#define DIANA_A64_OPF_SVE_EXTEND_FIELD 0xC0 // SVE_MEM_REG: uxtw or sxtw by a field (xs)
#define DIANA_A64_OPF_SVE_PN        0x10    // PREG: a predicate-as-counter register, pn8
#define DIANA_A64_OPF_ZA_RANGE2     0x10    // ZA_SLICE: two slices from offset * 2, za1h.d[w12, 2:3]
#define DIANA_A64_OPF_ZA_RANGE4     0x20    // ZA_SLICE: four slices from offset * 4, za.s[w8, 4:7]
#define DIANA_A64_OPF_ZA_VGX2       0x40    // ZA_SLICE: a group of two vectors, vgx2
#define DIANA_A64_OPF_ZA_VGX4       0x80    // ZA_SLICE: vgx4

// DianaA64OperandDesc::arr: element size and register width of FP/SIMD operands
#define DIANA_A64_ARR_SRC_MASK      0x0F    // where the element size comes from:
#define DIANA_A64_ARR_SRC_FIXED     1       //   1..5: fixed B, H, S, D, Q
#define DIANA_A64_ARR_SRC_SIZE      6       //   size: B, H, S, D
#define DIANA_A64_ARR_SRC_SZ        7       //   sz: S, D
#define DIANA_A64_ARR_SRC_IMMH      8       //   highest set bit of immh: B, H, S, D
#define DIANA_A64_ARR_SRC_IMM5      9       //   lowest set bit of imm5: B, H, S, D
#define DIANA_A64_ARR_DELTA_SHIFT   4       // bits 4..5: delta + 1 added to the element size (wide and narrow forms)
#define DIANA_A64_ARR_WIDTH_SHIFT   6       // bits 6..7:
#define DIANA_A64_ARR_WIDTH_NONE    0       //   scalar or one element; on a vector: 32 or 64 bits by Q (FMLAL v1.2h/v1.4h)
#define DIANA_A64_ARR_WIDTH_Q       1       //   64 or 128 bits by Q
#define DIANA_A64_ARR_WIDTH_128     2       //   128 bits
#define DIANA_A64_ARR_WIDTH_PAIR    3       //   two elements

// DianaA64OperandDesc::arr of the SVE and SME operands: the source as in DIANA_A64_ARR_SRC_MASK (0: no element size),
#define DIANA_A64_ARR_SRC_BIMM      10      //   the element size of a bitmask immediate: [N:immr:imms] (ORR, DUPM)
#define DIANA_A64_SARR_DELTA_SHIFT  4       // bits 4..6: delta + 3 added to the element size (size-2: SDOT .b by size)
#define DIANA_A64_SARR_TWO_FIELDS   0x80    // the source is two fields read as one (tszh:tszl)

// VELEM index rules
#define DIANA_A64_INDEX_CONST       0       // index = param >> 2
#define DIANA_A64_INDEX_IMM5        1       // index = imm5 >> (element + 1), the size source field
#define DIANA_A64_INDEX_IMM4        2       // index = imm4 >> element: [reg, imm5, imm4]
#define DIANA_A64_INDEX_HLM         3       // by element: [Rm, size source, H, L, M], H: v0-v15 and H:L:M, S: M:Rm and H:L, D: M:Rm and H
#define DIANA_A64_INDEX_UNIT_SHIFT  2       //   HLM, bits 2..3: the index counts units of 1 << n elements (FCMLA pairs, SDOT 4B)
#define DIANA_A64_INDEX_GROUP       0x10    //   HLM: the unit is printed as one element (v2.4b[1], v2.2h[3])
                                            // CONST with a field after the size source: index = param >> 2 + field (SM3TT1A)

// MODIMM kinds
#define DIANA_A64_MODIMM_LSL        0       // imm8:cmode, lsl by cmode<2:1>
#define DIANA_A64_MODIMM_MSL        1       // imm8:cmode, msl 8 or 16 by cmode<0>
#define DIANA_A64_MODIMM_BYTES      2       // each bit of imm8 is a byte of the 64-bit value
#define DIANA_A64_MODIMM_PLAIN      3       // imm8

// SHIFTIMM kinds, esize from the highest set bit of immh
#define DIANA_A64_SHIFT_RIGHT       0       // 2 * esize - immh:immb
#define DIANA_A64_SHIFT_LEFT        1       // immh:immb - esize
#define DIANA_A64_SHIFT_FBITS       2       // 64 - scale
#define DIANA_A64_SHIFT_RIGHT16     3       // 16 - imm4: SVE2.1 narrowing shifts of two S vectors (SQRSHRN)
#define DIANA_A64_SHIFT_RIGHT_WIDE  4       // 2 * esize - tsize:imm5, esize 32 << highest bit of tsize (SME2 SQRSHR of four)

#define DIANA_A64_MAX_OPERANDS      6
#define DIANA_A64_MAX_FIELDS        5

// generated tables, see diana_gen_a64.c
typedef struct _DianaA64OperandDesc
{
    DI_UINT8 kind;          // DIANA_A64_OPK_*
    DI_UINT8 param;         // scale (log2) or bias, depends on kind
    DI_UINT8 fieldCount;
    DI_UINT8 flags;
    DI_UINT8 arr;           // DIANA_A64_ARR_*, FP/SIMD operands
    DI_UINT8 lsb[DIANA_A64_MAX_FIELDS];     // fields are concatenated, the first is the most significant
    DI_UINT8 width[DIANA_A64_MAX_FIELDS];
}DianaA64OperandDesc;

// DianaA64CmdInfo::flags
#define DIANA_A64_CMDF_SUFFIX2      0x01    // the mnemonic gets "2" when Q (bit 30) is set: sqxtn2, ushll2

typedef struct _DianaA64CmdInfo
{
    DianaCmdInfo header;    // must be the first: DianaParserResult::pInfo points to it
    DI_UINT32 mask;
    DI_UINT32 value;
    DI_UINT64 features;     // DIANA_A64_FEATURE_*, all of them are needed
    DI_UINT16 unlessFirst;
    DI_UINT8 unlessCount;
    DI_UINT8 operandCount;
    // not this instruction if any pair of fields from pDistinct[distinctFirst...] is equal
    DI_UINT16 distinctFirst;
    DI_UINT8 distinctCount;
    DI_UINT8 flags;         // DIANA_A64_CMDF_*
    // and at least one feature of DianaA64Tables::pFeaturesAny[featuresAny] (SVE or SME), 0: none.
    // An index in what was padding, so the struct keeps its size
    DI_UINT8 featuresAny;
    DianaA64OperandDesc operands[DIANA_A64_MAX_OPERANDS];
}DianaA64CmdInfo;

typedef struct _DianaA64MaskValue
{
    DI_UINT32 mask;
    DI_UINT32 value;
}DianaA64MaskValue;

// DianaA64Distinct::flags: the first field is a base register, 31 is SP and never equals XZR
#define DIANA_A64_DISTINCT_SP_IS_31     0x01

typedef struct _DianaA64Distinct
{
    DI_UINT8 lsb1;
    DI_UINT8 lsb2;
    DI_UINT8 width;
    DI_UINT8 flags;
}DianaA64Distinct;

// switches on (word >> shift) & ((1 << bits) - 1), the slot index is first + that value
typedef struct _DianaA64Node
{
    DI_UINT8 shift;
    DI_UINT8 bits;
    DI_UINT16 reserved;
    DI_UINT32 first;
}DianaA64Node;

typedef struct _DianaA64Leaf
{
    DI_UINT32 mask;
    DI_UINT32 value;
    DI_UINT32 cmd;
}DianaA64Leaf;

// slot values: 0 - undefined, 0x80000000|n - node n, (leaf << 4)|count - leaves
#define DIANA_A64_SLOT_NODE_FLAG    0x80000000u
#define DIANA_A64_SLOT_COUNT_BITS   4

typedef struct _DianaA64Tables
{
    const DianaA64Node * pNodes;
    const DI_UINT32 * pSlots;
    const DianaA64Leaf * pLeaves;
    DianaA64CmdInfo * pCmds;        // ".inst" commands: pCmds[0] (unallocated), unsupportedCmd, disabledCmd
    long cmdsCount;
    const DianaA64MaskValue * pUnless;
    const DianaA64Distinct * pDistinct;
    DianaGroupInfo * pGroups;       // pGroups[i].m_commandId == DIANA_A64_CMD_BASE + i
    long groupsCount;
    // words Arm allocates that the spec doesn't decode, walked only when the main tree has no instruction
    const DianaA64Node * pUnsupportedNodes;
    const DI_UINT32 * pUnsupportedSlots;
    const DianaA64Leaf * pUnsupportedLeaves;
    long unsupportedCmd;            // ".inst": Arm allocates the word, the spec doesn't decode it
    long disabledCmd;               // ".inst": the profile leaves the instruction out
    const DI_UINT64 * pFeaturesAny; // the any-of feature sets of DianaA64CmdInfo::featuresAny, [0] = 0
}DianaA64Tables;

const DianaA64Tables * DianaA64_GetTables(void);

// the instruction is in a profile's feature set: all of pCmd->features and one of its featuresAny set
int DianaA64_IsCmdEnabled(const DianaA64CmdInfo * pCmd, DI_UINT64 profileFeatures);

// CPU profiles: the set of allowed features. Instructions outside the profile decode as ".inst"
typedef struct _DianaArmProfile
{
    const char * pName;
    const char * pDescription;
    DI_UINT64 features;         // DIANA_A64_FEATURE_*
    DI_UINT64 a32Features;      // DIANA_A32_FEATURE_*: AArch32 (ARM32 and THUMB), 0 if the CPU has no AArch32
}DianaArmProfile;

const DianaArmProfile * DianaArm_GetProfiles(int * pCount);
const DianaArmProfile * DianaArm_FindProfile(const char * pName);
// pProfile == 0 allows everything. The feature set is the one of pContext->iArch: features for AArch64,
// a32Features for ARM32 and THUMB, so the architecture is set first (Diana_InitContextEx)
void Diana_SetArmProfile(DianaContext * pContext, const DianaArmProfile * pProfile);

// decoding, used by Diana_ParseCmd when pContext->iArch is DIANA_ARCH_AARCH64
int DianaA64_ParseCmd(DianaContext * pContext,
                      DianaReadStream * readStream,
                      DianaParserResult * pResult);

int DianaA64_DecodeWord(DianaContext * pContext,
                        DI_UINT32 word,
                        DianaParserResult * pResult);

// returns the ARM command info if the result is an ARM instruction, 0 otherwise
const DianaA64CmdInfo * DianaA64_GetCmdInfo(const DianaParserResult * pResult);

// the result is ".inst": no instruction was decoded, see DianaA64_GetWordClass for why
int DianaA64_IsUndefined(const DianaParserResult * pResult);

// why a word decoded the way it did
#define DIANA_A64_WORD_SUPPORTED        0   // an instruction
#define DIANA_A64_WORD_DISABLED         1   // ".inst": an instruction whose feature the profile leaves out
#define DIANA_A64_WORD_UNSUPPORTED      2   // ".inst": a valid instruction Arm defines and diana_core doesn't decode yet
#define DIANA_A64_WORD_UNALLOCATED      3   // ".inst": no instruction in Arm's spec, not code
// DIANA_A64_WORD_*, -1 if the result is not AArch64
int DianaA64_GetWordClass(const DianaParserResult * pResult);

// the ARM part of Diana_GetGroupInfo
DianaGroupInfo * DianaA64_GetGroupInfo(long lId);

// decodes a logical immediate, returns 0 if the encoding is reserved
int DianaA64_DecodeBitMasks(int n, int imms, int immr, int regSize, DI_UINT64 * pValue);

#ifdef __cplusplus
}
#endif

#endif
