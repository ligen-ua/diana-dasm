#ifndef DIANA_CORE_H
#define DIANA_CORE_H

#include "diana_config.h"
#include "diana_commands.h"
#include "memory.h"

// <gen
#include "diana_gen.h"
// >gen

typedef enum
{
 reg_none,
 reg_AL,    reg_CL,    reg_DL,    reg_BL,    reg_AH,    reg_CH,    reg_DH,    reg_BH,
 reg_AX,    reg_CX,    reg_DX,    reg_BX,    reg_SP,    reg_BP,    reg_SI,    reg_DI,
 reg_EAX,   reg_ECX,   reg_EDX,   reg_EBX,   reg_ESP,   reg_EBP,   reg_ESI,   reg_EDI,
 reg_ES,    reg_CS,    reg_SS,    reg_DS,    reg_FS,    reg_GS,
 reg_CR0,   reg_CR1,   reg_CR2,   reg_CR3,   reg_CR4,   reg_CR5,   reg_CR6,   reg_CR7,
 reg_DR0,   reg_DR1,   reg_DR2,   reg_DR3,   reg_DR4,   reg_DR5,   reg_DR6,   reg_DR7,
 reg_TR0,   reg_TR1,   reg_TR2,   reg_TR3,   reg_TR4,   reg_TR5,   reg_TR6,   reg_TR7,
 reg_IP,

 // x64 part
 reg_RAX,   reg_RCX,   reg_RDX,   reg_RBX,   reg_RSP,   reg_RBP,   reg_RSI,   reg_RDI,
 reg_SIL,   reg_DIL,   reg_BPL,   reg_SPL,
 reg_R8,    reg_R9,    reg_R10,   reg_R11,   reg_R12,   reg_R13,   reg_R14,   reg_R15,
 reg_R8D,   reg_R9D,   reg_R10D,  reg_R11D,  reg_R12D,  reg_R13D,  reg_R14D,  reg_R15D,
 reg_R8W,   reg_R9W,   reg_R10W,  reg_R11W,  reg_R12W,  reg_R13W,  reg_R14W,  reg_R15W,
 reg_R8B,   reg_R9B,   reg_R10B,  reg_R11B,  reg_R12B,  reg_R13B,  reg_R14B,  reg_R15B,
 reg_RIP,

 reg_MM0,   reg_MM1,   reg_MM2,   reg_MM3,   reg_MM4,   reg_MM5,   reg_MM6,   reg_MM7,
 reg_MM_Internal0,   reg_MM_Internal1,   reg_MM_Internal2,  reg_MM_Internal3,  reg_MM_Internal4,  reg_MM_Internal5,  reg_MM_Internal6,  reg_MM_Internal7,

 reg_XMM0,  reg_XMM1,  reg_XMM2,  reg_XMM3,  reg_XMM4,  reg_XMM5,  reg_XMM6,  reg_XMM7,
 reg_XMM8,  reg_XMM9,  reg_XMM10, reg_XMM11, reg_XMM12, reg_XMM13, reg_XMM14, reg_XMM15,

 reg_YMM0,  reg_YMM1,  reg_YMM2,  reg_YMM3,  reg_YMM4,  reg_YMM5,  reg_YMM6,  reg_YMM7,
 reg_YMM8,  reg_YMM9,  reg_YMM10, reg_YMM11, reg_YMM12, reg_YMM13, reg_YMM14, reg_YMM15,

 reg_fpu_ST0, reg_fpu_ST1, reg_fpu_ST2, reg_fpu_ST3, reg_fpu_ST4, reg_fpu_ST5, reg_fpu_ST6, reg_fpu_ST7,

 reg_CR8,   reg_CR9,   reg_CR10,  reg_CR11,  reg_CR12,  reg_CR13,  reg_CR14,  reg_CR15,
 reg_DR8,   reg_DR9,   reg_DR10,  reg_DR11,  reg_DR12,  reg_DR13,  reg_DR14,  reg_DR15,

 // AVX-512: zmm0..31; xmm16..31 and ymm16..31 (EVEX only, the low parts of zmm16..31); the opmask registers
 // (Diana_VectorRegister: the register of a number and a size)
 reg_ZMM0,  reg_ZMM1,  reg_ZMM2,  reg_ZMM3,  reg_ZMM4,  reg_ZMM5,  reg_ZMM6,  reg_ZMM7,
 reg_ZMM8,  reg_ZMM9,  reg_ZMM10, reg_ZMM11, reg_ZMM12, reg_ZMM13, reg_ZMM14, reg_ZMM15,
 reg_ZMM16, reg_ZMM17, reg_ZMM18, reg_ZMM19, reg_ZMM20, reg_ZMM21, reg_ZMM22, reg_ZMM23,
 reg_ZMM24, reg_ZMM25, reg_ZMM26, reg_ZMM27, reg_ZMM28, reg_ZMM29, reg_ZMM30, reg_ZMM31,
 reg_XMM16, reg_XMM17, reg_XMM18, reg_XMM19, reg_XMM20, reg_XMM21, reg_XMM22, reg_XMM23,
 reg_XMM24, reg_XMM25, reg_XMM26, reg_XMM27, reg_XMM28, reg_XMM29, reg_XMM30, reg_XMM31,
 reg_YMM16, reg_YMM17, reg_YMM18, reg_YMM19, reg_YMM20, reg_YMM21, reg_YMM22, reg_YMM23,
 reg_YMM24, reg_YMM25, reg_YMM26, reg_YMM27, reg_YMM28, reg_YMM29, reg_YMM30, reg_YMM31,
 reg_K0,    reg_K1,    reg_K2,    reg_K3,    reg_K4,    reg_K5,    reg_K6,    reg_K7,

 count_of_DianaUnifiedRegister,

 // ARM part: a separate block, not included into count_of_DianaUnifiedRegister
 // registers are laid out in blocks of 32: reg_A64_X0 + n, reg_A64_W0 + n, ...
 reg_A64_first = 0x400,
 reg_A64_X0    = reg_A64_first,          // X0..X30
 reg_A64_XZR   = reg_A64_X0 + 31,
 reg_A64_W0    = reg_A64_first + 0x20,   // W0..W30
 reg_A64_WZR   = reg_A64_W0 + 31,
 reg_A64_SP    = reg_A64_first + 0x40,
 reg_A64_WSP,
 reg_A64_PC,
 reg_A64_B0    = reg_A64_first + 0x80,   // B0..B31
 reg_A64_H0    = reg_A64_first + 0xA0,   // H0..H31
 reg_A64_S0    = reg_A64_first + 0xC0,   // S0..S31
 reg_A64_D0    = reg_A64_first + 0xE0,   // D0..D31
 reg_A64_Q0    = reg_A64_first + 0x100,  // Q0..Q31
 reg_A64_V0    = reg_A64_first + 0x120,  // V0..V31
 // SVE and SME
 reg_A64_Z0    = reg_A64_first + 0x140,  // Z0..Z31, scalable vectors
 reg_A64_P0    = reg_A64_first + 0x160,  // P0..P15, predicates
 reg_A64_PN0   = reg_A64_first + 0x170,  // PN0..PN15: the P registers used as predicate-as-counter (pn8)
 reg_A64_ZA    = reg_A64_first + 0x180,  // the whole ZA array
 reg_A64_ZA0   = reg_A64_first + 0x190,  // ZA0..ZA15, ZA tiles; the element size is in DianaArmOperandExt::arrangement
 reg_A64_ZT0   = reg_A64_first + 0x1A0,  // the SME2 lookup table
 reg_A64_last  = reg_A64_first + 0x1A1,

 // AArch32 (A32 and T32), blocks like the A64 ones
 reg_A32_first = 0x600,
 reg_A32_R0    = reg_A32_first,          // R0..R15
 reg_A32_SP    = reg_A32_R0 + 13,
 reg_A32_LR    = reg_A32_R0 + 14,
 reg_A32_PC    = reg_A32_R0 + 15,        // as the instruction reads it: the address + 8 (A32) or + 4 (T32)
 reg_A32_S0    = reg_A32_first + 0x20,   // S0..S31
 reg_A32_D0    = reg_A32_first + 0x40,   // D0..D31
 reg_A32_Q0    = reg_A32_first + 0x60,   // Q0..Q15
 reg_A32_APSR  = reg_A32_first + 0x70,
 reg_A32_CPSR,
 reg_A32_SPSR,
 reg_A32_FPSCR,
 reg_A32_last
}DianaUnifiedRegister;

// architectures, see Diana_InitContextEx
#define DIANA_ARCH_X86       0
#define DIANA_ARCH_AARCH64   1
#define DIANA_ARCH_ARM32     2
#define DIANA_ARCH_THUMB     3

#define DI_VALUE_FLAG_CMD_REVERSE           ((DI_CHAR)0x80)
#define DI_VALUE_FLAG_CMD_MASK              ((DI_CHAR)0x7F)

// flags -> DI_UINT32 m_flags
#define DI_FLAG_CMD_SUPPORTS_IMM64          0x00000001
#define DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64   0x00000002
#define DI_FLAG_CMD_AMD_INVALID             0x00000004
#define DI_FLAG_CMD_FPU_I                   0x00000008
#define DI_FLAG_CMD_IGNORE_REX_PREFIX       0x00000010
#define DI_FLAG_CMD_I386                    0x00000020
#define DI_FLAG_CMD_AMD64                   0x00000040
#define DI_FLAG_CMD_AMD64_SIGN_EXTENDS      0x00000080
#define DI_FLAG_CMD_PUSH_SEG                0x00000100
#define DI_FLAG_CMD_UNDOCUMENTED            0x00000200
#define DI_FLAG_CMD_PRIVILEGED              0x00000400
#define DI_FLAG_CMD_PREFIX_LOCK             0x00000800

#define DI_FLAG_CMD_POSTBYTE_USED           0x00001000
#define DI_FLAG_CMD_REGISTER_AS_OPCODE      0x00002000
#define DI_FLAG_CMD_HAS32BIT_ANALOG         0x00004000
#define DI_FLAG_CMD_IS_TRUE_PREFIX          0x00008000
#define DI_FLAG_CMD_TEST_MODE_ONLY          0x00010000

#define DI_FLAG_CMD_MUST_BE_ALIGNED         0x00020000

// VEX encoding flags (bits 18-26)
#define DI_FLAG_CMD_VEX_ENCODED             0x00040000  // is a VEX-encoded instruction
#define DI_FLAG_CMD_VEX_L1                  0x00080000  // L=1 (256-bit, YMM operands)
// pp field (bits 20-21): implied mandatory prefix
#define DI_FLAG_CMD_VEX_PP_66               0x00100000  // pp=01 (implied 66h)
#define DI_FLAG_CMD_VEX_PP_F3               0x00200000  // pp=10 (implied F3h)
#define DI_FLAG_CMD_VEX_PP_F2               0x00300000  // pp=11 (implied F2h)
// map_select (bits 22-23): implied leading opcode escape bytes
#define DI_FLAG_CMD_VEX_MAP_0F              0x00400000  // map=01 (0F)
#define DI_FLAG_CMD_VEX_MAP_0F38            0x00800000  // map=10 (0F 38)
#define DI_FLAG_CMD_VEX_MAP_0F3A            0x00C00000  // map=11 (0F 3A)
#define DI_FLAG_CMD_VEX_W1                  0x01000000  // W=1
// operand encoding kind (bits 25-26)
#define DI_FLAG_CMD_VEX_NDS                 0x02000000  // vvvv = non-destructive source
#define DI_FLAG_CMD_VEX_NDD                 0x04000000  // vvvv = destination
#define DI_FLAG_CMD_VEX_DDS                 0x06000000  // vvvv = second source, dest implicit

// ISA extension flags (bits 27-31)
#define DI_FLAG_CMD_SSE41      0x08000000  // SSE4.1 instruction
#define DI_FLAG_CMD_SSE42      0x10000000  // SSE4.2 instruction
#define DI_FLAG_CMD_BMI1       0x20000000  // BMI1 instruction
#define DI_FLAG_CMD_BMI2       0x40000000  // BMI2 instruction
#define DI_FLAG_CMD_SHA_EXT    0x80000000  // SHA extension instruction

// iVexPrefix of DianaContext: 1 | the raw W bit (W is not in the REX state outside 64-bit mode)
#define DI_VEX_PREFIX_PRESENT  1
#define DI_VEX_PREFIX_W        2

// VEX field accessors
#define DI_VEX_GET_PP(flags)   (((flags) >> 20) & 0x3)
#define DI_VEX_GET_MAP(flags)  (((flags) >> 22) & 0x3)
#define DI_VEX_GET_NDS(flags)  (((flags) >> 25) & 0x3)

// index fields
#define DI_MAX_INT             (0x7FFFFFFF)
#define DI_MAX_OPERANDS_COUNT  (6)
#define DI_MAX_OPCODE_COUNT    (4)

#define DI_CACHE_RESERVED      2
#define DI_CACHE_SIZE (DI_MAX_OPCODE_COUNT + 1 + DI_CACHE_RESERVED)

#define DI_MAX_INSTRUCTION_SIZE 15
#define DI_MAX_PREFIXES_COUNT DI_MAX_INSTRUCTION_SIZE-1
#define DI_MAX_INSTRUCTION_SIZE_ALIGNED  16

#define DI_CONST64(x)  (x##LL)
// common
void Diana_DispatchSIB(unsigned char sib, int * pSS, int *pIndex, int *pBase);
DI_CHAR Diana_GetReg(unsigned char postbyte);
DI_CHAR Diana_GetRm(unsigned char postbyte);
DI_CHAR Diana_GetMod(unsigned char postbyte);

typedef struct _dianaBaseGenObject
{
    DI_UINT16 m_type;
    DI_UINT16 m_flags;
}DianaBaseGenObject_type;

// types:
#define DIANA_BASE_GEN_OBJECT_CMD                0x01
#define DIANA_BASE_GEN_OBJECT_LINE               0x02
#define DIANA_BASE_GEN_OBJECT_INDEX              0x03
// flags:
#define DIANA_BASE_GEN_OBJECT_THE_SAME_OPCODE    0x01
// flags of DIANA_BASE_GEN_OBJECT_CMD, the prefix rule: an F2, F3 or 66 that is not the mandatory prefix of the entry
// makes another instruction, #UD here (66 0f 77, f3 0f c7 /7 rdpid); without the flag the CPU ignores it (repz imul).
// The generator sets them for MMX/SSE (with an F2 or F3 as the mandatory prefix 66 has no effect) and from the spec
// options /pprefix_ud, /prep_ud, /pf3_ud
#define DIANA_BASE_GEN_OBJECT_PREFIX_NO_66       0x10
#define DIANA_BASE_GEN_OBJECT_PREFIX_NO_F2       0x20
#define DIANA_BASE_GEN_OBJECT_PREFIX_NO_F3       0x40
// VEX.W0: the table walk skips the entry when VEX.W is 1 (the CPU: #UD in every mode; a W1 entry on the same
// opcode, BMI r64, is found next)
#define DIANA_BASE_GEN_OBJECT_VEX_W0             0x80
// VEX.W1: skipped when VEX.W is 0 (the W0 instruction of the opcode is found next: vpsllvd, vpsllvq)
#define DIANA_BASE_GEN_OBJECT_VEX_W1             0x08
// an EVEX entry (AVX-512): DianaEvexCmdAttributes are before it, Diana_GetEvexAttributes
#define DIANA_BASE_GEN_OBJECT_EVEX               0x100

// parser structures:
// 1) line
typedef struct _dianaCmdKey
{
    void * keyLineOrCmdInfo;
    DI_CHAR opcode;
}DianaCmdKey;

typedef struct _dianaCmdKeyLine
{
    DianaBaseGenObject_type parent;
    int iKeysCount;
    DianaCmdKey key[1];
}DianaCmdKeyLine;
//-------
// 2) index line
typedef struct _dianaIndexKey
{
    void * keyLineOrCmdInfo;
}DianaIndexKey;

typedef struct _dianaIndexKeyLine
{
    DianaBaseGenObject_type parent;
    DianaIndexKey key[256];
}DianaIndexKeyLine;

// result
typedef struct _dianaOperandInfo
{
    DianaOperands_type m_type;
    DI_CHAR m_size;
    DI_CHAR m_size2;
    DI_CHAR m_value;
    DianaSreg_type m_sreg_type;
}DianaOperandInfo;

#define DIANA_GT_CAN_CHANGE_RIP                      0x1
#define DIANA_GT_IS_CALL                             0x2
#define DIANA_GT_IS_JUMP                             0x4
#define DIANA_GT_CAN_GO_TO_THE_NEXT_INSTRUCTION      0x8
#define DIANA_GT_RET                                 0x10
// the instruction never goes to the next one and has no target: AArch64 BRK, HLT, UDF
#define DIANA_GT_STOP                                0x20

typedef struct _Diana_LinkedAdditionalGroupInfo
{
    int flags;
    int relArgrumentNumber;
}Diana_LinkedAdditionalGroupInfo;

typedef struct _dianaGroupInfo
{
    long m_lGroupId;
    const char * m_pName;
    Diana_LinkedAdditionalGroupInfo * m_pLinkedInfo;
    void * m_pTag;
    DianaCommandId m_commandId;
}DianaGroupInfo;

// forward declaration
struct _dianaContext;

typedef void (*Diana_PrefixFnc)(struct _dianaContext * pContext);

typedef void (*Diana_CallFnc)(struct _dianaContext * pDianaContext,
                              void * pCallContext);

// WARNING! This structure SHOULD EXACT MATCH DianaCmdInfo1 from diana_gen.c
typedef struct _dianaCmdInfo
{
    DianaBaseGenObject_type m_parent;   // m_flags: DIANA_BASE_GEN_OBJECT_PREFIX_NO_*
    DI_CHAR m_extension;
    DI_CHAR m_extension_mask;
    DI_CHAR m_extension_deny_mask;

    DI_INT32 m_lGroupId;
    DI_UINT32 m_flags;  // DI_FLAG_CMD_*
    DI_CHAR m_iCSIPExtensionSizeInBytes;
    DI_CHAR m_iImmediateOperandSizeInBytes;
    DI_CHAR m_iImmediateOperandSizeInBytes2;

    DI_CHAR m_operandCount;
    Diana_PrefixFnc m_linkedPrefixFnc;
    DianaGroupInfo * m_pGroupInfo;
    DianaOperandInfo m_operands[1];
}DianaCmdInfo;

// EVEX (AVX-512) attributes of an entry, DianaEvexCmdAttributes::m_flags
#define DI_EVEX_VL_128           0           // the vector length of the entry (EVEX.L'L)
#define DI_EVEX_VL_256           1
#define DI_EVEX_VL_512           2
#define DI_EVEX_VL_LIG           3           // a scalar: L'L is ignored
#define DI_EVEX_VL_MASK          3
#define DI_EVEX_MASK             0x4         // {k}: an opmask on operand 0 (merging)
#define DI_EVEX_ZEROING          0x8         // {z}: zeroing too
#define DI_EVEX_MASK_REQUIRED    0x10        // {k!}: k0 is #UD (gathers, scatters)
#define DI_EVEX_BCST_SHIFT       5           // the broadcast element: 0 none, 1 16 bits, 2 32 bits, 3 64 bits
#define DI_EVEX_BCST_MASK        0x60
#define DI_EVEX_ER               0x80        // {er}: EVEX.b on the register form is the rounding in L'L
#define DI_EVEX_SAE              0x100       // {sae}: EVEX.b on the register form suppresses all exceptions
#define DI_EVEX_N_SHIFT          9           // the disp8 scale 1 << (value - 1); 0: the size of the memory operand
#define DI_EVEX_N_MASK           0xE00
#define DI_EVEX_B_IGNORED        0x1000      // {b}: EVEX.b on the register form is ignored (vcvtdq2pd zmm, ymm)
#define DI_EVEX_DISTINCT         0x2000      // the destination is none of the source registers (vfmulcph), else #UD

typedef struct _dianaEvexCmdAttributes
{
    DI_UINT32 m_flags;  // DI_EVEX_*
}DianaEvexCmdAttributes;

// an EVEX entry of diana_gen.c: the attributes, then the entry
typedef struct _dianaEvexCmdInfo
{
    DianaEvexCmdAttributes m_evex;
    DianaCmdInfo m_cmd;
}DianaEvexCmdInfo;

// the attributes of an EVEX entry, 0 for any other
const DianaEvexCmdAttributes * Diana_GetEvexAttributes(const DianaCmdInfo * pInfo);

typedef struct _dianaRmIndex
{
    DianaUnifiedRegister seg_reg;
    DianaUnifiedRegister reg;
    DianaUnifiedRegister indexed_reg;
    DI_CHAR index;
    DI_CHAR dispSize;
    DI_UINT64 dispValue;
}DianaRmIndex;

typedef struct _dianaCallPtr
{
    DI_UINT64 m_segment;
    DI_UINT64 m_address;
    unsigned long m_segment_size;
    unsigned long m_address_size;
}DianaCallPtr;

typedef struct _dianaRel
{
    int       m_size;
    DI_UINT64 m_value;
}DianaRel;

typedef struct _dianaMemory
{
    DianaRmIndex m_index;
}DianaMemory;

typedef enum {diana_none, diana_register, diana_index, diana_imm, diana_call_ptr, diana_rel, diana_memory, diana_reserved_reg,
              // ARM part, details are in DianaLinkedOperand::arm
              diana_arm_shifted_reg,    // value.recognizedRegister + arm.shiftType/shiftAmount
              diana_arm_cond,           // arm.cond
              diana_arm_sysreg,         // value.imm = o0:op1:CRn:CRm:op2
              diana_arm_special,        // value.imm, meaning depends on arm.opKind (barrier, prefetch op, Cn, adrp page)
              diana_arm_reglist,        // value.recognizedRegister is the first V register, arm.regCount registers (wrap after V31),
                                        // arm.arrangement, arm.lane if the arrangement is an element
              diana_arm_regmask         // AArch32 core register list {r4-r11, lr}: value.imm bit n is rn, arm.regCount registers
}DianaValueType;

typedef union _dianaOperandValue
{
    // register
    DianaUnifiedRegister recognizedRegister;
    // r/m
    DianaRmIndex rmIndex;
    // immediate
    DI_UINT64 imm;
    // call ptr
    DianaCallPtr ptr;
    // rel
    DianaRel rel;
    // memory
    DianaMemory memory;
}DianaOperandValue;

// ARM-only operand details, zero for x86
#define DIANA_ARM_SHIFT_NONE    0
#define DIANA_ARM_SHIFT_LSL     1
#define DIANA_ARM_SHIFT_LSR     2
#define DIANA_ARM_SHIFT_ASR     3
#define DIANA_ARM_SHIFT_ROR     4
#define DIANA_ARM_SHIFT_MSL     5
#define DIANA_ARM_SHIFT_MUL     6   // SVE: a multiplier, mul #4 (shiftAmount is the factor)
#define DIANA_ARM_SHIFT_MUL_VL  7   // SVE memory: the displacement counts vector lengths, [x0, #2, mul vl]
#define DIANA_ARM_EXTEND_UXTB   8
#define DIANA_ARM_EXTEND_UXTH   9
#define DIANA_ARM_EXTEND_UXTW   10
#define DIANA_ARM_EXTEND_UXTX   11
#define DIANA_ARM_EXTEND_SXTB   12
#define DIANA_ARM_EXTEND_SXTH   13
#define DIANA_ARM_EXTEND_SXTW   14
#define DIANA_ARM_EXTEND_SXTX   15
#define DIANA_ARM_SHIFT_RRX     16  // AArch32: rotate right by one through the carry, rrx (no amount)

#define DIANA_ARM_WRITEBACK_NONE  0
#define DIANA_ARM_WRITEBACK_PRE   1
#define DIANA_ARM_WRITEBACK_POST  2

// SIMD arrangement, 0 = none (general and scalar registers):
//   bits 0..2  element size, log2 of bytes (0 = B ... 4 = Q)
//   bits 3..5  log2(element count) + 1 for a vector (v0.4s)
//   bit 6      one element, DianaArmOperandExt::lane selects it (v0.s[1]);
//              with a count: a group of elements selected as one (v2.4b[1], SDOT)
#define DIANA_ARM_ARR(elementLog2, countLog2)  ((elementLog2) | (((countLog2) + 1) << 3))
#define DIANA_ARM_ARR_ELEMENT(elementLog2)     ((elementLog2) | 0x40)
#define DIANA_ARM_ARR_GROUP(elementLog2, countLog2) (DIANA_ARM_ARR(elementLog2, countLog2) | 0x40)
// SVE and SME: bit 7 marks a scalable vector, printed without a count (z0.s, p0.b), an element of one (z1.s[3])
#define DIANA_ARM_ARR_SCALABLE(elementLog2)    ((elementLog2) | 0x80)
#define DIANA_ARM_ARR_IS_SCALABLE(arr)         (((arr) & 0x80) != 0)
#define DIANA_ARM_ARR_ELEMENT_LOG2(arr)        ((arr) & 7)
#define DIANA_ARM_ARR_IS_ELEMENT(arr)          (((arr) & 0x40) != 0)
#define DIANA_ARM_ARR_HAS_COUNT(arr)           (((arr) & 0x38) != 0)
#define DIANA_ARM_ARR_COUNT(arr)               (((1 << (((arr) >> 3) & 7)) >> 1) | !DIANA_ARM_ARR_HAS_COUNT(arr))

#define DIANA_ARM_ARR_8B    DIANA_ARM_ARR(0, 3)
#define DIANA_ARM_ARR_16B   DIANA_ARM_ARR(0, 4)
#define DIANA_ARM_ARR_2H    DIANA_ARM_ARR(1, 1)
#define DIANA_ARM_ARR_4H    DIANA_ARM_ARR(1, 2)
#define DIANA_ARM_ARR_8H    DIANA_ARM_ARR(1, 3)
#define DIANA_ARM_ARR_2S    DIANA_ARM_ARR(2, 1)
#define DIANA_ARM_ARR_4S    DIANA_ARM_ARR(2, 2)
#define DIANA_ARM_ARR_1D    DIANA_ARM_ARR(3, 0)
#define DIANA_ARM_ARR_2D    DIANA_ARM_ARR(3, 1)
#define DIANA_ARM_ARR_1Q    DIANA_ARM_ARR(4, 0)
#define DIANA_ARM_ARR_B     DIANA_ARM_ARR_ELEMENT(0)
#define DIANA_ARM_ARR_H     DIANA_ARM_ARR_ELEMENT(1)
#define DIANA_ARM_ARR_S     DIANA_ARM_ARR_ELEMENT(2)
#define DIANA_ARM_ARR_D     DIANA_ARM_ARR_ELEMENT(3)

// DianaArmOperandExt::qualifier
#define DIANA_ARM_QUAL_NONE     0
#define DIANA_ARM_QUAL_MERGE    1   // a governing predicate that merges: p0/m
#define DIANA_ARM_QUAL_ZERO     2   // a governing predicate that zeroes: p0/z
#define DIANA_ARM_QUAL_HORIZONTAL 3 // a horizontal ZA tile slice: za1h.s
#define DIANA_ARM_QUAL_VERTICAL 4   // a vertical ZA tile slice: za1v.s
// AArch32
#define DIANA_ARM_QUAL_NEGATIVE 5   // memory: the offset is subtracted, [r0, -r1], [r0, #-0] (dispValue is the signed offset)
#define DIANA_ARM_QUAL_SHIFT_REG 6  // shifted register: the amount is a register, r<shiftAmount> (lsl r3)
#define DIANA_ARM_QUAL_ALIGN_PC 7   // diana_rel: the base is the instruction address rounded down to 4 (T32 BLX, ADR, literals),
                                    // target = (address & ~3) + instruction size + rel
#define DIANA_ARM_QUAL_USER     8   // diana_arm_regmask: ^, the user mode registers or an exception return (LDM)
#define DIANA_ARM_QUAL_ALL_LANES 9  // diana_arm_reglist: all lanes of each register, {d0[], d1[]} (VLD2 all lanes)

typedef struct _dianaArmOperandExt
{
    DI_UINT8 opKind;        // DIANA_A64_OPK_*, or DIANA_A32_OPK_* for an AArch32 instruction
    DI_UINT8 shiftType;     // DIANA_ARM_SHIFT_* or DIANA_ARM_EXTEND_*
    DI_UINT8 shiftAmount;
    DI_UINT8 hasAmount;     // amount is explicitly encoded (register offset S bit)
    DI_UINT8 writeback;     // DIANA_ARM_WRITEBACK_*
    DI_UINT8 cond;          // condition code 0..15
    DI_UINT8 arrangement;   // DIANA_ARM_ARR_* for V registers and lists, 0 otherwise
    DI_UINT8 lane;          // element index when the arrangement is an element (v1.s[3])
    DI_UINT8 regCount;      // registers in a diana_arm_reglist
    DI_UINT8 qualifier;     // DIANA_ARM_QUAL_*
    DI_UINT8 regStride;     // diana_arm_reglist: registers between the entries, 0 or 1: consecutive ({z0.s, z8.s}: 8)
    DI_UINT8 reserved[1];
}DianaArmOperandExt;

typedef struct _dianaLinkedOperand
{
    DianaOperandInfo * pInfo;

    DianaValueType type;

    DianaOperandValue value;
    // offset
    int iOffset;

    int usedSize;
    int usedAddressSize;

#ifndef DIANA_NO_ARM
    DianaArmOperandExt arm;
#endif
}DianaLinkedOperand;

// DianaEvexInfo::rounding
#define DIANA_EVEX_ROUND_NONE    0
#define DIANA_EVEX_ROUND_SAE     1           // {sae}
#define DIANA_EVEX_ROUND_RN      2           // {rn-sae}, the rounding modes in the order of MXCSR.RC (RC + 2)
#define DIANA_EVEX_ROUND_RD      3           // {rd-sae}
#define DIANA_EVEX_ROUND_RU      4           // {ru-sae}
#define DIANA_EVEX_ROUND_RZ      5           // {rz-sae}

typedef struct _dianaEvexInfo
{
    DI_UINT8 isEvex;        // 1: EVEX encoded
    DI_UINT8 mask;          // the opmask of operand 0: 1..7 is k1..k7, 0 none ({k1})
    DI_UINT8 zeroing;       // {z}: the masked-off elements are zeroed, else they keep their value
    DI_UINT8 broadcast;     // the memory operand is one element broadcast to this many ({1to16}), 0 none
    DI_UINT8 rounding;      // DIANA_EVEX_ROUND_*
    DI_UINT8 vectorSize;    // the vector length in bytes: 16, 32, 64 (64 with a rounding, the scalars 16)
    DI_UINT8 disp8Scale;    // N of disp8*N: an 8-bit displacement is multiplied by it (dispValue has the product)
    DI_UINT8 reserved;
}DianaEvexInfo;

typedef struct _dianaParserResult
{
    DianaCmdInfo * pInfo;
    DianaLinkedOperand linkedOperands[DI_MAX_OPERANDS_COUNT];
    int iLinkedOpCount;
    int iPrefix;
    DI_CHAR iRexPrefix;
    int iFullCmdSize;
    unsigned char bytes[DI_MAX_INSTRUCTION_SIZE_ALIGNED];
    int bytesCount;

    int iOperandSize;           // x86: the operand size in bytes, 2, 4 or 8 (prefixes 66, REX.W, VEX.W applied)

    DianaEvexInfo evex;         // x86 EVEX (AVX-512): the opmask and the decorations, zero for other instructions
}DianaParserResult;

// Callback
#define DI_END                      ((int)1)
#define DI_ERROR                    ((int)-1)
#define DI_OUT_OF_MEMORY            ((int)-2)
#define DI_DIVISION_BY_ZERO         ((int)-3)
#define DI_UNSUPPORTED_COMMAND      ((int)-4)
#define DI_DIVISION_OVERFLOW        ((int)-5)
#define DI_INVALID_CONFIGURATION    ((int)-6)
#define DI_WIN32_ERROR              ((int)-7)
#define DI_ERROR_NOT_USED_BY_CORE   ((int)-8)
#define DI_INVALID_INPUT            ((int)-9)
#define DI_INVALID_OPCODE           ((int)-10)
#define DI_END_OF_STREAM            ((int)-11)
#define DI_GP                       ((int)-12)
#define DI_INTERRUPT                ((int)-13)
#define DI_PARTIAL_READ_WRITE       ((int)-14)
#define DI_ERROR_NOT_IMPLEMENTED    ((int)-15)
#define DI_OVERFLOW                 ((int)-16)
#define DI_NOT_FOUND                ((int)-17)
#define DI_UNSUPPORTED              ((int)-18)

#define DI_SUCCESS ((int)0)

#define DI_CHECK_ALLOC(x) { if(!(x)) { Diana_OnError(DI_OUT_OF_MEMORY); return DI_OUT_OF_MEMORY; } }
#define DI_CHECK_ALLOC_GOTO(x) { if(!(x)) { Diana_OnError(DI_OUT_OF_MEMORY); DI_CHECK_GOTO(DI_OUT_OF_MEMORY); } }

#ifdef _DEBUG
#define DI_CHECK(x) { int di____code = (x); if (di____code != DI_SUCCESS) { Diana_OnError(di____code); return di____code; } }
#else
#define DI_CHECK(x) { int di____code = (x); if (di____code != DI_SUCCESS) { return di____code; } }
#endif

#ifdef _DEBUG
#define DI_CHECK_GOTO(x) { int di____code = (x); if (di____code != DI_SUCCESS) { Diana_OnError(di____code); status = di____code; goto cleanup; } }
#else
#define DI_CHECK_GOTO(x) { int di____code = (x); if (di____code != DI_SUCCESS) { status = di____code;  goto cleanup; } }
#endif

#define DI_CHECK_CONDITION(X, code) if (!(X)) { DI_CHECK(code); }
#define DI_CHECK_CONDITION_GOTO(X, code) if (!(X)) { DI_CHECK_GOTO(code); }

// serial streams
typedef int (* DianaRead_fnc)(void * pThis,
                              void * pBuffer,
                              int iBufferSize,
                              int * readBytes);
typedef int (* DianaWrite_fnc)(void * pThis,
                               void * pBuffer,
                               int iBufferSize,
                               int * wrote);

typedef struct _dianaReadStream
{
    DianaRead_fnc pReadFnc;
}DianaReadStream;

int DianaExactRead(DianaReadStream * pThis,
                   void * pBuffer,
                   int iBufferSize);

int DianaExactReadSafe(DianaReadStream* pThis,
                        void* pBuffer,
                        OPERAND_SIZE iBufferSize);
// pseudo random streams
typedef int (* DianaAnalyzeMoveTo_fnc)(void * pThis, OPERAND_SIZE offset);

#define DIANA_ANALYZE_RANDOM_READ_ABSOLUTE  1
typedef int (* DianaAnalyzeRandomRead_fnc)(void * pThis, 
                                           OPERAND_SIZE offset,
                                           void * pBuffer, 
                                           int iBufferSize, 
                                           OPERAND_SIZE * readBytes,
                                           int flags);

typedef int (* DianaAnalyzeRandomWrite_fnc)(void * pThis, 
                                           OPERAND_SIZE offset,
                                           void * pBuffer, 
                                           int iBufferSize, 
                                           OPERAND_SIZE * writeBytes,
                                           int flags);

typedef struct _dianaMovableReadStream
{
    DianaReadStream parent;
    DianaAnalyzeMoveTo_fnc pMoveTo;
    DianaAnalyzeRandomRead_fnc pRandomRead;
}DianaMovableReadStream;

void DianaMovableReadStream_Init(DianaMovableReadStream * pStream,
                                 DianaRead_fnc pReadFnc, 
                                 DianaAnalyzeMoveTo_fnc pMoveTo,
                                 DianaAnalyzeRandomRead_fnc pRandomRead);


typedef struct _dianaReadWriteRandomStream
{
    DianaMovableReadStream parent;
    DianaAnalyzeRandomWrite_fnc pRandomWrite;
}DianaReadWriteRandomStream;

void DianaReadWriteRandomStream_Init(DianaReadWriteRandomStream * pStream,
                                     DianaRead_fnc pReadFnc, 
                                     DianaAnalyzeMoveTo_fnc pMoveTo,
                                     DianaAnalyzeRandomRead_fnc pRandomRead,
                                     DianaAnalyzeRandomWrite_fnc pRandomWrite);
// Allocators
#define DI_REX_PREFIX_START       0x40
#define DI_REX_PREFIX_END         0x4F
#define DI_REX_HAS_FLAG_W(X)          (X&0x8)
#define DI_REX_HAS_FLAG_R(X)          (X&0x4)
#define DI_REX_HAS_FLAG_X(X)          (X&0x2)
#define DI_REX_HAS_FLAG_B(X)          (X&0x1)

#define DI_NO_PREFIX     0
#define DI_PREFIX_NONE   0
#define DI_PREFIX_LOCK   1
#define DI_PREFIX_REP    2
#define DI_PREFIX_REPN   3

#define DIANA_MODE64    8
#define DIANA_MODE32    4
#define DIANA_MODE16    2

#define DIANA_HAS_ADDRESS_SIZE_PREFIX   1
#define DIANA_HAS_OPERAND_SIZE_PREFIX   2
#define DIANA_INVALID_STATE             4

#define DIANA_FIELD_OFFSET(type, field)    ((DI_UINT32)(DI_UINT64)&(((type *)0)->field))

typedef struct _dianaPrefixInfo
{
    Diana_PrefixFnc linkedPrefixFnc;
    DI_CHAR prefix;
}DianaPrefixInfo;

typedef struct _dianaContext
{
    int iMainMode_opsize;    // 4 or 2
    int iCurrentCmd_opsize;  // 4 or 2

    int iMainMode_addressSize;    // 4 or 2
    int iCurrentCmd_addressSize;  // 4 or 2

    DianaUnifiedRegister mainMode_sreg;
    DianaUnifiedRegister currentCmd_sreg;

    int iPrefix;
    int iSizePrefixes;

    int iAMD64Mode;
    DI_CHAR iRexPrefix;
    DI_CHAR iRexPrefixValue;

    unsigned char cache[DI_CACHE_SIZE];
    int cacheSize;
    int cacheIt;

    DI_FULL_CHAR lastPrefixBeforeRex;
    DianaPrefixInfo prefixes[DI_MAX_PREFIXES_COUNT];
    DI_FULL_CHAR prefixesCount;
    int testMode;

    // VEX prefix state (valid when iVexPrefix != 0)
    DI_CHAR iVexPrefix;    // nonzero if current instruction uses VEX encoding, DI_VEX_PREFIX_*
    DI_CHAR iVexVVVV;      // vvvv register index (0-15, already un-inverted)

    // architecture, DIANA_ARCH_*; x86 by default
    int iArch;
#ifndef DIANA_NO_ARM
    // ARM only: allowed features, see Diana_SetArmProfile: DIANA_A64_FEATURE_* for AArch64,
    // DIANA_A32_FEATURE_* for ARM32 and THUMB
    DI_UINT64 armFeatures;
    // THUMB: the IT block state (ITSTATE<7:0>, firstcond:mask), see DianaA32_ParseCmd
    int iArmFlags;
#endif

    // EVEX prefix state (valid when iEvexPrefix != 0; iVexPrefix, iVexVVVV and iRexPrefix are set too)
    DI_CHAR iEvexPrefix;   // DI_EVEX_PREFIX_*
    DI_CHAR iEvexAAA;      // the opmask, 0..7
    DI_CHAR iEvexLL;       // the raw L'L (the rounding with EVEX.b on a register form)
}DianaContext;

// iEvexPrefix of DianaContext
#define DI_EVEX_PREFIX_PRESENT  1
#define DI_EVEX_PREFIX_R1       2   // EVEX.R' (un-inverted): bit 4 of the ModRM.reg register
#define DI_EVEX_PREFIX_V1       4   // EVEX.V' (un-inverted): bit 4 of vvvv, of a VSIB index
#define DI_EVEX_PREFIX_X        8   // EVEX.X (un-inverted): bit 4 of a ModRM.rm register (mod 11)
#define DI_EVEX_PREFIX_B        16  // EVEX.b: broadcast, rounding, SAE
#define DI_EVEX_PREFIX_Z        32  // EVEX.z: zeroing

void Diana_FatalBreak();

DianaCmdInfo * Diana_GetNopInfo();

void Diana_InitContext(DianaContext * pThis, int Mode);
// Mode is still the pointer size (DIANA_MODE*), iArch is DIANA_ARCH_*
void Diana_InitContextEx(DianaContext * pThis, int Mode, int iArch);
// only in tests
void Diana_InitContextWithTestMode(DianaContext * pThis, int Mode);

void Diana_ClearCache(DianaContext * pThis);

int Diana_ParseCmd(DianaContext * pContext, // IN
                   DianaBaseGenObject_type * pInitialLine,  // IN
                   DianaReadStream * readStream,    // IN
                   DianaParserResult * pResult);    // OUT

int DianaRecognizeCommonReg(DI_CHAR iOpSize,
                            DI_CHAR regId,
                            DianaUnifiedRegister * pOut,
                            int isRexPrefix);

int DianaRecognizeMMX(DI_CHAR regId,
                      DianaUnifiedRegister * pOut);
int DianaRecognizeXMM(DI_CHAR regId,
                      DianaUnifiedRegister * pOut);
int DianaRecognizeYMM(DI_CHAR regId,
                      DianaUnifiedRegister * pOut);
// the vector register number (0..31) of a size in bytes (16 xmm, 32 ymm, 64 zmm), reg_none for another size
DianaUnifiedRegister Diana_VectorRegister(int size, int number);
// the number (0..31) and the size of a vector register, -1 (size 0) for another register
int Diana_VectorRegisterNumber(DianaUnifiedRegister reg, int * pSize);

typedef int (*Diana_ReadIndexStructure_type)(DianaContext * pContext,
                                             DianaLinkedOperand * pInfo,
                                             DI_CHAR iOpSize,
                                             unsigned char postbyte,
                                             DianaReadStream * pStream,
                                             DianaOperandValue * pValue,
                                             DianaValueType * pType);

DianaGroupInfo * Diana_GetGroupInfo(long lId);

void Diana_Init();
void Diana_ResetPrefixes(DianaContext * pContext);

typedef void * ( *Diana_Alloc_type)(void * pThis, DIANA_SIZE_T size);
typedef void ( *Diana_Free_type)(void * pThis, void * memory);
typedef int ( *Diana_Patcher_type)(void * pThis, void * pDest, const void * pSource, DIANA_SIZE_T size);

typedef struct _Diana_Allocator
{
    Diana_Alloc_type m_alloc;
    Diana_Free_type m_free;
    Diana_Patcher_type m_patch;
}Diana_Allocator;

void Diana_AllocatorInit(Diana_Allocator * pAllocator,
                         Diana_Alloc_type alloc,
                         Diana_Free_type free,
                         Diana_Patcher_type patch);


void Diana_CacheEatOneSafe(DianaContext * pContext);

int Diana_OnError(int code);
#define DIANA_CONTAINING_RECORD(address, type, field) ((type *)( \
                                                  (char*)(address) - \
                                                  (DIANA_SIZE_T)(&((type *)0)->field)))


int Diana_ConvertOpSizeToSizeT(const OPERAND_SIZE * pOpSize, DIANA_SIZE_T * pSizeT);
int Diana_SafeAdd(OPERAND_SIZE * pResult, OPERAND_SIZE arg);

/* Ceiling for any single allocation derived from file-supplied sizes (512 MB) */
#define DIANA_MAX_SAFE_ALLOC_SIZE  0x20000000ULL

// returns 0 if code is unknown
const char * Diana_QueryErrorText_Silent(int value);

OPERAND_SIZE Diana_ReadValue(const void* buffer,
    int size);

#endif
