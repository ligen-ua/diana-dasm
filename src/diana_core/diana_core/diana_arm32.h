#ifndef DIANA_ARM32_H
#define DIANA_ARM32_H

// AArch32: A32 (DIANA_ARCH_ARM32) and T32 (DIANA_ARCH_THUMB), one command set (DIANA_CMD_SET_A32)
#include "diana_arm.h"
#include "diana_gen_a32.h"

#ifdef __cplusplus
extern "C" {
#endif

// operand kinds (DianaArmOperandExt::opKind), keep in sync with A32OperandKind in diana_gen_arm/arm_spec.h.
// The tables use DianaA64OperandDesc: kind, param, fields as there; arr is a fixed register + 1 (0: the register is
// the first field)
#define DIANA_A32_OPK_NONE          0
#define DIANA_A32_OPK_REG           1   // r0..r15, diana_register: [reg...] or the fixed register arr - 1; [reg, W] for Rn{!}
#define DIANA_A32_OPK_IMM           2   // diana_imm: the fields concatenated, << param
#define DIANA_A32_OPK_MODIMM        3   // diana_imm: A32 modified immediate [imm12], arm.shiftAmount is the rotation
#define DIANA_A32_OPK_TMODIMM       4   // diana_imm: T32 modified immediate [i, imm3, imm8] (ThumbExpandImm)
#define DIANA_A32_OPK_SHREG         5   // diana_arm_shifted_reg: [Rm, type, amount...], DecodeImmShift (lsr #32, rrx)
#define DIANA_A32_OPK_SHREG_REG     6   // diana_arm_shifted_reg: [Rm, type, Rs], arm.qualifier DIANA_ARM_QUAL_SHIFT_REG
#define DIANA_A32_OPK_LABEL         7   // diana_rel: [offset fields...], relative to the next instruction like x86;
                                        // DIANA_ARM_QUAL_ALIGN_PC when the base is Align(PC, 4)
#define DIANA_A32_OPK_MEM_IMM       8   // diana_index: [Rn, U, imm...], dispValue is the signed offset
#define DIANA_A32_OPK_MEM_REG       9   // diana_index: [Rn, U, Rm, type, amount...], indexed_reg Rm, arm.shiftType
#define DIANA_A32_OPK_REGLIST       10  // diana_arm_regmask: [list, extra1, extra2], the extra bits are the registers
                                        // param & 15 and param >> 4 (T32 PUSH lr, LDM pc and lr)
// diana_arm_special, value.imm:
#define DIANA_A32_OPK_COPROC        11  // the coprocessor number: [field] + param (p14, p15), no field: param
#define DIANA_A32_OPK_CREG          12  // c0..c15 of a coprocessor: [field], no field: param (c5)
#define DIANA_A32_OPK_SYSM          13  // MRS/MSR register as objdump's SYSm: R << 6 | B << 5 | M << 4 | M1,
                                        // param 0 [R]: CPSR (15) or SPSR (79); param 1 [R, M, M1]: a banked register
#define DIANA_A32_OPK_PSRMASK       14  // MSR CPSR_fsxc: R << 4 | mask
#define DIANA_A32_OPK_BARRIER       15  // DMB, DSB, ISB option
#define DIANA_A32_OPK_IFLAGS        16  // CPS: A << 2 | I << 1 | F
#define DIANA_A32_OPK_ENDIAN        17  // SETEND: 1 is be
// diana_imm:
#define DIANA_A32_OPK_BFWIDTH       18  // BFI/BFC width: [msb, lsb...], msb - lsb + 1 (can be 0 or less, UNPREDICTABLE)
// VFP (diana_register reg_A32_S0 + n, reg_A32_D0 + n):
#define DIANA_A32_OPK_SREG          19  // s0..s31: [fields...] concatenated (Vd:D), + param (VMOV Sm1)
#define DIANA_A32_OPK_DREG          20  // d0..d31: [fields...] concatenated (D:Vd)
// diana_imm:
#define DIANA_A32_OPK_FPIMM         21  // [imm4H, imm4L]: VFPExpandImm, the bits of a param-bit float (16, 32, 64);
                                        // no fields: #0.0
#define DIANA_A32_OPK_FBITS         22  // VCVT fixed point: param (16 or 32) minus the fields (imm4:i)
// diana_arm_reglist, value.recognizedRegister the first register, arm.regCount registers:
#define DIANA_A32_OPK_SREGLIST      23  // {s0-s3}: [first register fields..., count]
#define DIANA_A32_OPK_DREGLIST      24  // {d8-d15}: [first register fields..., count * 2] (FLDMX: + 1)
// diana_arm_special, value.imm:
#define DIANA_A32_OPK_FPSYSREG      25  // VMRS/VMSR register number: 0 fpsid, 1 fpscr, 5..7 mvfr2..mvfr0, 8 fpexc...
// Advanced SIMD:
#define DIANA_A32_OPK_QREG          26  // diana_register q0..q15: [fields...] concatenated (D:Vd) / 2
#define DIANA_A32_OPK_DSCALAR       27  // diana_register d2[1] (s2[1] with DIANA_A32_OPF_SINGLE): [register fields...,
                                        // index fields...], param: register field count | log2(esize / 8) << 4;
                                        // arm.arrangement DIANA_ARM_ARR_ELEMENT, arm.lane the index
#define DIANA_A32_OPK_NEONIMM       28  // diana_imm: [op, cmode, i, imm3, imm4] (no op: bit 5), AdvSIMDExpandImm, 64 bits
#define DIANA_A32_OPK_ROTATE        29  // diana_imm: VCMLA rot * 90 (param 0), VCADD 90 + rot * 180 (param 1)
#define DIANA_A32_OPK_VLIST         30  // diana_arm_reglist of D registers, arm.regStride; param: mode | structures << 4,
                                        // mode 0 [D, Vd, itype] (multiple structures), 1 [D, Vd, index_align, size]
                                        // (one lane: arm.arrangement and arm.lane), 2 [D, Vd, T, size] (all lanes:
                                        // DIANA_ARM_QUAL_ALL_LANES)
#define DIANA_A32_OPK_VMEM          31  // diana_index [Rn :align]: [Rn, the alignment fields of the mode, Rm], param as
                                        // VLIST; arm.shiftAmount the alignment in bytes (0: none); writeback POST with
                                        // DIANA_A32_OPF_VMEM_WRITEBACK (Rn += the size) or _POST_REG (Rn += Rm, indexed_reg)
// SHREG with param: a fixed type (param & 7) - 1 + DIANA_ARM_SHIFT_LSL, the amount is [field] << (param >> 4);
// SHREG_REG with param: [Rm, Rs], the type fixed the same way; MEM_REG with param: lsl, the amount from the fields
// after Rm or, without them, param >> 4 (TBH [Rn, Rm, lsl #1])
// memory: rmIndex.reg is the base, PC reads as the address + 8 (A32) or Align(address + 4, 4) (T32);
// arm.writeback DIANA_ARM_WRITEBACK_PRE/POST; DIANA_ARM_QUAL_NEGATIVE when the offset is subtracted (U = 0)

// DianaA64OperandDesc::flags of AArch32 operands
#define DIANA_A32_OPF_WRITEBACK     0x01    // REG: Rn!, the register is updated
#define DIANA_A32_OPF_WRITEBACK_FIELD 0x02  // REG: Rn{!}, by the last field (W)
#define DIANA_A32_OPF_WRITEBACK_LIST 0x04   // REG: Rn{!list}, updated when Rn is not in the list field (T32 16-bit LDM)
#define DIANA_A32_OPF_THUMB         0x04    // LABEL, MEM: a T32 instruction, the PC reads as the address + 4
#define DIANA_A32_OPF_ALIGN         0x08    // LABEL: the base is Align(PC, 4) (BLX to A32, ADR, literals)
#define DIANA_A32_OPF_UNSIGNED      0x10    // LABEL: the offset is unsigned (CBZ)
#define DIANA_A32_OPF_J_BITS        0x20    // LABEL: [S, J1, J2, ...], I1 = NOT(J1 EOR S), I2 = NOT(J2 EOR S) (BL)
#define DIANA_A32_OPF_MODIMM        0x40    // LABEL: [imm12], the offset is an A32 modified immediate (ADR)
#define DIANA_A32_OPF_SUBTRACT      0x80    // LABEL: the offset is subtracted (ADR A2, T2)
#define DIANA_A32_OPF_SUBTRACTED    0x08    // MEM_IMM: the offset is always subtracted (T32 [r1, #-4])
#define DIANA_A32_OPF_HAS_U         0x10    // MEM: the field after the base is U, 0 subtracts
#define DIANA_A32_OPF_OPTIONAL      0x20    // MEM_IMM: a zero offset that adds is not printed: [r1]
#define DIANA_A32_OPF_PRE           0x40    // MEM: pre-index with writeback, [r1, #4]!
#define DIANA_A32_OPF_POST          0x80    // MEM: post-index, [r1], #4
#define DIANA_A32_OPF_PLUS1         0x01    // IMM: the value is the field + 1 (SSAT, SBFX width)
#define DIANA_A32_OPF_ALWAYS        0x01    // REGLIST: register param is always in the list (LDM exception return)
#define DIANA_A32_OPF_USER          0x02    // REGLIST: ^ (user registers, exception return), arm.qualifier
                                            // DIANA_ARM_QUAL_USER
#define DIANA_A32_OPF_REVERSE       0x01    // FBITS: the fields minus param (a shift to the left, VSHL #imm6-8)
#define DIANA_A32_OPF_COUNT_PLUS1   0x01    // DREGLIST: the count is the field + 1 (VTBL len)
#define DIANA_A32_OPF_SINGLE        0x01    // DSCALAR: an S register (VFMAL s2[1])
#define DIANA_A32_OPF_VMEM_WRITEBACK 0x01   // VMEM: [Rn]!
#define DIANA_A32_OPF_VMEM_POST_REG 0x02    // VMEM: [Rn], Rm (the last field)

// DianaA32CmdInfo::flags
#define DIANA_A32_CMDF_S_OUTSIDE_IT 0x01    // T32 16-bit: sets the flags outside an IT block (adds), not inside (addeq);
                                            // the group is Arm's name without the s (add)
#define DIANA_A32_CMDF_WIDE         0x02    // printed with .w after the condition (add.w)
#define DIANA_A32_CMDF_NARROW       0x04    // printed with .n (b.n)
#define DIANA_A32_CMDF_THUMB        0x08    // a T32 instruction

#define DIANA_A32_NO_COND           0xFF    // DianaA32CmdInfo::condLsb: no condition field

typedef struct _DianaA32CmdInfo
{
    DianaCmdInfo header;    // must be the first: DianaParserResult::pInfo points to it
    DI_UINT32 mask;         // T32: hw1 in bits 31..16, hw2 (or nothing for a 16-bit instruction) in bits 15..0
    DI_UINT32 value;
    DI_UINT64 features;     // DIANA_A32_FEATURE_*, all of them are needed
    DI_UINT16 unlessFirst;
    DI_UINT8 unlessCount;
    DI_UINT8 operandCount;
    DI_UINT16 distinctFirst;
    DI_UINT8 distinctCount;
    DI_UINT8 flags;         // DIANA_A32_CMDF_*
    DI_UINT8 featuresAny;   // an index into DianaA32Tables::pFeaturesAny, 0: none
    DI_UINT8 condLsb;       // the condition field, DIANA_A32_NO_COND if none
    DI_UINT8 size;          // 2 or 4 bytes
    DI_UINT8 suffix;        // the data type printed after the condition (.f32): DianaA32Tables::pSuffixes, 0: none
    DianaA64OperandDesc operands[DIANA_A64_MAX_OPERANDS];
}DianaA32CmdInfo;

typedef struct _DianaA32Tree
{
    const DianaA64Node * pNodes;
    const DI_UINT32 * pSlots;
    const DianaA64Leaf * pLeaves;
}DianaA32Tree;

// what an undefined word is: the index into DianaA32Tables::undefinedCmds
#define DIANA_A32_FORM_A32          0
#define DIANA_A32_FORM_T16          1
#define DIANA_A32_FORM_T32          2

typedef struct _DianaA32Tables
{
    DianaA32Tree a32;
    DianaA32Tree t32;               // the T32 word is hw1 << 16 | hw2, a 16-bit instruction is hw1 << 16
    DianaA32Tree a32Unsupported;    // words Arm allocates that the spec doesn't decode
    DianaA32Tree t32Unsupported;
    DianaA32CmdInfo * pCmds;
    long cmdsCount;
    const DianaA64MaskValue * pUnless;
    const DianaA64Distinct * pDistinct;
    DianaGroupInfo * pGroups;       // pGroups[i].m_commandId == DIANA_A32_CMD_BASE + i
    long groupsCount;
    const DI_UINT64 * pFeaturesAny;
    // ".inst" commands by DIANA_A32_FORM_*: unallocated, unsupported, disabled by the profile
    long undefinedCmds[3];
    long unsupportedCmds[3];
    long disabledCmds[3];
    const char * const * pSuffixes; // DianaA32CmdInfo::suffix -> ".f32", ".f64.s32"; 0 is ""
    long suffixesCount;
}DianaA32Tables;

const DianaA32Tables * DianaA32_GetTables(void);

int DianaA32_IsCmdEnabled(const DianaA32CmdInfo * pCmd, DI_UINT64 profileFeatures);

// DianaParserResult::iPrefix of an AArch32 instruction: the condition it executes under
#define DIANA_A32_PREFIX_COND_MASK  0x0F    // DIANA_A32_COND_*, AL if the instruction is not conditional
#define DIANA_A32_PREFIX_IN_IT      0x10    // T32: in an IT block; the condition is the IT one unless the
                                            // instruction has its own (B<c>, then it is UNPREDICTABLE)
#define DIANA_A32_PREFIX_NOT_LAST   0x20    // T32: in an IT block and not its last instruction
#define DIANA_A32_PREFIX_IT_SHIFT   8       // T32 in an IT block: bits 8..11 are the IT condition
#define DIANA_A32_COND_AL           14

int DianaA32_GetCondition(const DianaParserResult * pResult);

// 2 or 4: the size of the T32 instruction that starts with this halfword
int DianaA32_GetT32Size(DI_UINT32 hw1);

// decoding, used by Diana_ParseCmd when pContext->iArch is DIANA_ARCH_ARM32 or DIANA_ARCH_THUMB.
// THUMB: the IT state in pContext->iArmFlags gives the condition and moves to the next instruction
int DianaA32_ParseCmd(DianaContext * pContext,
                      DianaReadStream * readStream,
                      DianaParserResult * pResult);

int DianaA32_DecodeA32(DianaContext * pContext,
                       DI_UINT32 word,
                       DianaParserResult * pResult);

// hw2 is not used by a 16-bit instruction; the IT state is used and advanced like in DianaA32_ParseCmd
int DianaA32_DecodeT32(DianaContext * pContext,
                       DI_UINT32 hw1,
                       DI_UINT32 hw2,
                       DianaParserResult * pResult);

// starts a new instruction stream: no IT block
void DianaA32_ResetItState(DianaContext * pContext);

// returns the AArch32 command info if the result is an AArch32 instruction, 0 otherwise
const DianaA32CmdInfo * DianaA32_GetCmdInfo(const DianaParserResult * pResult);

// DIANA_A64_WORD_*, -1 if the result is not AArch32
int DianaA32_GetWordClass(const DianaParserResult * pResult);

// the AArch32 part of Diana_GetGroupInfo
DianaGroupInfo * DianaA32_GetGroupInfo(long lId);

#ifdef __cplusplus
}
#endif

#endif
