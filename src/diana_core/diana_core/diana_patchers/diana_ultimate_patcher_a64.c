#include "diana_ultimate_patcher.h"
#include "diana_ultimate_patcher_impl.h"
#include "diana_arm.h"

#ifndef DIANA_NO_ARM

/* AArch64 hooks. The allocation:

     +0x000  the stub: saves x0-x18, x29, x30 and NZCV, calls hook(patchContext, original sp, registers),
             restores them
     +0x07C  the moved instructions, then a jump back to the hooked function
     +0x200  the literals the stub loads: patchContext, hookFunction

   The hooked function starts with "b stub" (the stub within 128 MB) or with
   "ldr x16, #8; br x16; .quad stub". x16 and x17 (IP0, IP1) are free at a function entry, linker veneers
   use them the same way. Instructions are little endian. */

#define DIANA_HOOK_A64_ALLOCATION_SIZE      1024
#define DIANA_HOOK_A64_LITERALS_OFFSET      0x200
#define DIANA_HOOK_A64_NEAR_SIZE            4
#define DIANA_HOOK_A64_FAR_SIZE             16
#define DIANA_HOOK_A64_MAX_JUMPS_COUNT      20

/* the words of the stub that load the literals */
#define DIANA_HOOK_A64_LOAD_CONTEXT_INDEX   15
#define DIANA_HOOK_A64_LOAD_HOOK_INDEX      16

static const DI_UINT32 g_stubA64[] =
{
    0xD102C3FF,     /* sub  sp, sp, #0xb0 */
    0xA90007E0,     /* stp  x0, x1, [sp] */
    0xA9010FE2,     /* stp  x2, x3, [sp, #0x10] */
    0xA90217E4,     /* stp  x4, x5, [sp, #0x20] */
    0xA9031FE6,     /* stp  x6, x7, [sp, #0x30] */
    0xA90427E8,     /* stp  x8, x9, [sp, #0x40] */
    0xA9052FEA,     /* stp  x10, x11, [sp, #0x50] */
    0xA90637EC,     /* stp  x12, x13, [sp, #0x60] */
    0xA9073FEE,     /* stp  x14, x15, [sp, #0x70] */
    0xA90847F0,     /* stp  x16, x17, [sp, #0x80] */
    0xD53B4209,     /* mrs  x9, nzcv */
    0xA90927F2,     /* stp  x18, x9, [sp, #0x90] */
    0xA90A7BFD,     /* stp  x29, x30, [sp, #0xa0] */
    0x9102C3E1,     /* add  x1, sp, #0xb0           the original sp */
    0x910003E2,     /* mov  x2, sp                  the registers */
    0x58000000,     /* ldr  x0, <patchContext>      DIANA_HOOK_A64_LOAD_CONTEXT_INDEX */
    0x58000010,     /* ldr  x16, <hookFunction>     DIANA_HOOK_A64_LOAD_HOOK_INDEX */
    0xD63F0200,     /* blr  x16 */
    0xA94927F2,     /* ldp  x18, x9, [sp, #0x90] */
    0xD51B4209,     /* msr  nzcv, x9 */
    0xA94007E0,     /* ldp  x0, x1, [sp] */
    0xA9410FE2,     /* ldp  x2, x3, [sp, #0x10] */
    0xA94217E4,     /* ldp  x4, x5, [sp, #0x20] */
    0xA9431FE6,     /* ldp  x6, x7, [sp, #0x30] */
    0xA94427E8,     /* ldp  x8, x9, [sp, #0x40] */
    0xA9452FEA,     /* ldp  x10, x11, [sp, #0x50] */
    0xA94637EC,     /* ldp  x12, x13, [sp, #0x60] */
    0xA9473FEE,     /* ldp  x14, x15, [sp, #0x70] */
    0xA94847F0,     /* ldp  x16, x17, [sp, #0x80] */
    0xA94A7BFD,     /* ldp  x29, x30, [sp, #0xa0] */
    0x9102C3FF      /* add  sp, sp, #0xb0 */
};

#define DIANA_HOOK_A64_STUB_WORDS   ((int)(sizeof(g_stubA64) / sizeof(g_stubA64[0])))
#define DIANA_HOOK_A64_STUB_SIZE    (DIANA_HOOK_A64_STUB_WORDS * 4)

/* encodings */
#define A64_B                   0x14000000u
#define A64_LDR_X16_PLUS_8      0x58000050u     /* ldr x16, #8 */
#define A64_LDR_X16_PLUS_12     0x58000070u     /* ldr x16, #12 */
#define A64_LDR_X_PLUS_8        0x58000040u     /* ldr xN, #8 | N */
#define A64_BR_X16              0xD61F0200u
#define A64_BLR_X16             0xD63F0200u
#define A64_B_PLUS_12           0x14000003u     /* b #12: over a literal */
#define A64_IP1                 17

static void PutWord(unsigned char * pOut, DI_UINT32 word)
{
    pOut[0] = (unsigned char)word;
    pOut[1] = (unsigned char)(word >> 8);
    pOut[2] = (unsigned char)(word >> 16);
    pOut[3] = (unsigned char)(word >> 24);
}

static void PutQword(unsigned char * pOut, DI_UINT64 value)
{
    PutWord(pOut, (DI_UINT32)value);
    PutWord(pOut + 4, (DI_UINT32)(value >> 32));
}

static DI_INT64 SignExtend(DI_UINT32 value, int bits)
{
    DI_UINT64 sign = (DI_UINT64)1 << (bits - 1);
    DI_UINT64 wide = (DI_UINT64)value & ((sign << 1) - 1);
    return (DI_INT64)((wide ^ sign) - sign);
}

/* value is a signed number of that many bits */
static int FitsSigned(DI_INT64 value, int bits)
{
    DI_INT64 limit = (DI_INT64)1 << (bits - 1);
    return value >= -limit && value < limit;
}

static DI_INT64 Distance(OPERAND_SIZE from, OPERAND_SIZE to)
{
    return (DI_INT64)(DI_UINT64)(to - from);
}

static int ReadWord(DianaHook_InternalMessage * pMessage, OPERAND_SIZE address, DI_UINT32 * pWord)
{
    unsigned char bytes[4];
    OPERAND_SIZE readBytes = 0;
    DI_CHECK(pMessage->pReadWriteStream->parent.pRandomRead(&pMessage->pReadWriteStream->parent,
                                                            address,
                                                            bytes,
                                                            4,
                                                            &readBytes,
                                                            0));
    if (readBytes != 4)
    {
        return DI_ERROR;
    }
    *pWord = (DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8) | ((DI_UINT32)bytes[2] << 16) | ((DI_UINT32)bytes[3] << 24);
    return DI_SUCCESS;
}

static int Write(DianaHook_InternalMessage * pMessage, OPERAND_SIZE address, void * pData, int size)
{
    OPERAND_SIZE written = 0;
    DI_CHECK(pMessage->pReadWriteStream->pRandomWrite(pMessage->pReadWriteStream,
                                                      address,
                                                      pData,
                                                      size,
                                                      &written,
                                                      0));
    if (written != (OPERAND_SIZE)size)
    {
        return DI_ERROR;
    }
    return DI_SUCCESS;
}

/* the moved code goes to workBufferRaw */
static int EmitWord(DianaHook_InternalMessage * pMessage, DI_UINT32 word)
{
    void * pOut = 0;
    DI_CHECK(DianaHook_AllocateCmd(pMessage, 4, &pOut));
    PutWord((unsigned char *)pOut, word);
    return DI_SUCCESS;
}

static int EmitQword(DianaHook_InternalMessage * pMessage, DI_UINT64 value)
{
    void * pOut = 0;
    DI_CHECK(DianaHook_AllocateCmd(pMessage, 8, &pOut));
    PutQword((unsigned char *)pOut, value);
    return DI_SUCCESS;
}

/* ldr x16, #8; br x16; .quad target or, a call, ldr x16, #12; blr x16; b #12; .quad target */
static int EmitFarJump(DianaHook_InternalMessage * pMessage, OPERAND_SIZE target, int bLink)
{
    if (bLink)
    {
        DI_CHECK(EmitWord(pMessage, A64_LDR_X16_PLUS_12));
        DI_CHECK(EmitWord(pMessage, A64_BLR_X16));
        DI_CHECK(EmitWord(pMessage, A64_B_PLUS_12));
    }
    else
    {
        DI_CHECK(EmitWord(pMessage, A64_LDR_X16_PLUS_8));
        DI_CHECK(EmitWord(pMessage, A64_BR_X16));
    }
    return EmitQword(pMessage, target);
}

/* ldr xN, #8; b #12; .quad value: xN = value */
static int EmitLoadAddress(DianaHook_InternalMessage * pMessage, DI_UINT32 reg, OPERAND_SIZE value)
{
    DI_CHECK(EmitWord(pMessage, A64_LDR_X_PLUS_8 | reg));
    DI_CHECK(EmitWord(pMessage, A64_B_PLUS_12));
    return EmitQword(pMessage, value);
}

/* b.cond, cbz/cbnz, tbz/tbnz: the offset field at bit 5, fieldBits wide. Out of range: the opposite
   condition skips a far jump */
static int MoveConditional(DianaHook_InternalMessage * pMessage,
                           DI_UINT32 word,
                           OPERAND_SIZE src,
                           OPERAND_SIZE dst,
                           int fieldBits,
                           DI_UINT32 invertMask)
{
    DI_UINT32 fieldMask = (((DI_UINT32)1 << fieldBits) - 1) << 5;
    OPERAND_SIZE target = src + (OPERAND_SIZE)(SignExtend((word & fieldMask) >> 5, fieldBits) * 4);
    DI_INT64 delta = Distance(dst, target);

    if (FitsSigned(delta / 4, fieldBits))
    {
        return EmitWord(pMessage, (word & ~fieldMask) | (((DI_UINT32)(delta / 4) << 5) & fieldMask));
    }
    /* b.!cond #20, over ldr, br and the literal */
    DI_CHECK(EmitWord(pMessage, ((word ^ invertMask) & ~fieldMask) | (5u << 5)));
    return EmitFarJump(pMessage, target, 0);
}

/* the AArch64 instruction word at src, moved to dst. *pbEndsFlow: nothing after it runs (b, br, ret) */
static int MoveInstructionA64(DianaHook_InternalMessage * pMessage,
                              DI_UINT32 word,
                              OPERAND_SIZE src,
                              OPERAND_SIZE dst,
                              int * pbEndsFlow)
{
    *pbEndsFlow = 0;

    if ((word & 0x7C000000u) == 0x14000000u)
    {
        /* b, bl: imm26 */
        int bLink = (word >> 31) != 0;
        OPERAND_SIZE target = src + (OPERAND_SIZE)(SignExtend(word & 0x3FFFFFFu, 26) * 4);
        DI_INT64 delta = Distance(dst, target);
        *pbEndsFlow = !bLink;
        if (FitsSigned(delta / 4, 26))
        {
            return EmitWord(pMessage, (word & 0xFC000000u) | ((DI_UINT32)(delta / 4) & 0x3FFFFFFu));
        }
        return EmitFarJump(pMessage, target, bLink);
    }
    if ((word & 0xFF000000u) == 0x54000000u)
    {
        /* b.cond, bc.cond: imm19. Conditions al and nv always branch */
        if ((word & 0xE) == 0xE)
        {
            OPERAND_SIZE target = src + (OPERAND_SIZE)(SignExtend((word >> 5) & 0x7FFFF, 19) * 4);
            DI_INT64 delta = Distance(dst, target);
            *pbEndsFlow = 1;
            if (FitsSigned(delta / 4, 19))
            {
                return EmitWord(pMessage, (word & ~(0x7FFFFu << 5)) | (((DI_UINT32)(delta / 4) & 0x7FFFF) << 5));
            }
            return EmitFarJump(pMessage, target, 0);
        }
        return MoveConditional(pMessage, word, src, dst, 19, 1);
    }
    if ((word & 0x7E000000u) == 0x34000000u)
    {
        /* cbz, cbnz: imm19 */
        return MoveConditional(pMessage, word, src, dst, 19, 0x01000000u);
    }
    if ((word & 0x7E000000u) == 0x36000000u)
    {
        /* tbz, tbnz: imm14 */
        return MoveConditional(pMessage, word, src, dst, 14, 0x01000000u);
    }
    if ((word & 0x1F000000u) == 0x10000000u)
    {
        /* adr, adrp: immhi:immlo */
        DI_UINT32 reg = word & 31;
        DI_INT64 imm = SignExtend((((word >> 5) & 0x7FFFF) << 2) | ((word >> 29) & 3), 21);
        OPERAND_SIZE target = 0;
        DI_INT64 delta = 0;
        if (word >> 31)
        {
            target = (src & ~(OPERAND_SIZE)0xFFF) + (OPERAND_SIZE)(imm * 4096);
            delta = Distance(dst & ~(OPERAND_SIZE)0xFFF, target) / 4096;
        }
        else
        {
            target = src + (OPERAND_SIZE)imm;
            delta = Distance(dst, target);
        }
        if (FitsSigned(delta, 21))
        {
            return EmitWord(pMessage, (word & 0x9F00001Fu) | (((DI_UINT32)delta & 3) << 29) |
                                      ((((DI_UINT32)delta >> 2) & 0x7FFFF) << 5));
        }
        return EmitLoadAddress(pMessage, reg, target);
    }
    if ((word & 0x3B000000u) == 0x18000000u)
    {
        /* ldr (literal): w, x, ldrsw, prfm; s, d, q */
        DI_UINT32 opc = word >> 30;
        int bSimd = (word >> 26) & 1;
        DI_UINT32 rt = word & 31;
        OPERAND_SIZE address = src + (OPERAND_SIZE)(SignExtend((word >> 5) & 0x7FFFF, 19) * 4);
        DI_INT64 delta = Distance(dst, address);
        DI_UINT32 base = rt;
        DI_UINT32 load = 0;
        if (FitsSigned(delta / 4, 19))
        {
            return EmitWord(pMessage, (word & ~(0x7FFFFu << 5)) | (((DI_UINT32)(delta / 4) & 0x7FFFF) << 5));
        }
        if (bSimd)
        {
            static const DI_UINT32 simdLoads[3] = { 0xBD400000u /* ldr st, [xn] */,
                                                    0xFD400000u /* ldr dt, [xn] */,
                                                    0x3DC00000u /* ldr qt, [xn] */ };
            if (opc == 3)
            {
                return DI_ERROR;
            }
            base = A64_IP1;
            load = simdLoads[opc];
        }
        else
        {
            static const DI_UINT32 loads[4] = { 0xB9400000u /* ldr wt, [xn] */,
                                                0xF9400000u /* ldr xt, [xn] */,
                                                0xB9800000u /* ldrsw xt, [xn] */,
                                                0xF9800000u /* prfm op, [xn] */ };
            load = loads[opc];
            /* prfm has no register to load the address into, nor has xzr */
            if (opc == 3 || rt == 31)
            {
                base = A64_IP1;
            }
        }
        DI_CHECK(EmitLoadAddress(pMessage, base, address));
        return EmitWord(pMessage, load | (base << 5) | rt);
    }

    /* everything else is copied: no other operand may depend on the address */
    {
        DianaParserResult * pResult = &pMessage->result;
        int i = 0;
        DI_CHECK(DianaA64_DecodeWord(&pMessage->context, word, pResult));
        if (DianaA64_IsUndefined(pResult))
        {
            return DI_ERROR;
        }
        for (i = 0; i < pResult->iLinkedOpCount; ++i)
        {
            const DianaLinkedOperand * pOp = pResult->linkedOperands + i;
            if (pOp->type == diana_rel ||
                (pOp->type == diana_index && pOp->value.rmIndex.reg == reg_A64_PC))
            {
                return DI_ERROR_NOT_IMPLEMENTED;
            }
        }
        /* br, ret, eret, braa...: everything in the unconditional branch (register) class but blr, blraa */
        if ((word & 0xFE000000u) == 0xD6000000u && ((word >> 21) & 7) != 1)
        {
            *pbEndsFlow = 1;
        }
    }
    return EmitWord(pMessage, word);
}

static int FollowJumpsA64(DianaHook_InternalMessage * pMessage)
{
    DI_UINT32 word = 0;
    if (pMessage->pCustomOptions && (pMessage->pCustomOptions->flags & DIANA_HOOK_CUSTOM_OPTION_DONT_FOLLOW_JUMPS))
    {
        return DI_SUCCESS;
    }
    for (;;)
    {
        DI_CHECK(ReadWord(pMessage, pMessage->addressToHook, &word));
        if ((word & 0xFC000000u) != A64_B)
        {
            return DI_SUCCESS;
        }
        /* b <target>: a thunk (incremental linking), hook the function it leads to */
        if (pMessage->jumpsCounter >= DIANA_HOOK_A64_MAX_JUMPS_COUNT)
        {
            return DI_ERROR_NOT_IMPLEMENTED;
        }
        pMessage->addressToHook += (OPERAND_SIZE)(SignExtend(word & 0x3FFFFFFu, 26) * 4);
        ++pMessage->jumpsCounter;
    }
}

static int WriteHookA64(DianaHook_InternalMessage * pMessage, OPERAND_SIZE stubAddress, int bNear)
{
    unsigned char hook[DIANA_HOOK_A64_FAR_SIZE];
    if (bNear)
    {
        DI_INT64 delta = Distance(pMessage->addressToHook, stubAddress);
        PutWord(hook, A64_B | ((DI_UINT32)(delta / 4) & 0x3FFFFFFu));
        return Write(pMessage, pMessage->addressToHook, hook, DIANA_HOOK_A64_NEAR_SIZE);
    }
    PutWord(hook, A64_LDR_X16_PLUS_8);
    PutWord(hook + 4, A64_BR_X16);
    PutQword(hook + 8, stubAddress);
    return Write(pMessage, pMessage->addressToHook, hook, DIANA_HOOK_A64_FAR_SIZE);
}

int DianaHook_PatchStreamA64(DianaHook_InternalMessage * pMessage)
{
    int status = 0;
    OPERAND_SIZE allocatedAddress = 0;
    int allocationSucceeded = 0;
    int bNear = 0;
    int hookSize = 0;
    int srcSize = 0;
    int bEndsFlow = 0;
    OPERAND_SIZE movedAddress = 0;
    unsigned char stub[sizeof(g_stubA64) / sizeof(g_stubA64[0]) * 4];
    unsigned char literals[16];
    unsigned char originalPointer[8];
    int i = 0;

    if (pMessage->processorMode != DIANA_MODE64 || (pMessage->addressToHook & 3))
    {
        return DI_ERROR;
    }
    Diana_InitContextEx(&pMessage->context, DIANA_MODE64, DIANA_ARCH_AARCH64);

    DI_CHECK_GOTO(FollowJumpsA64(pMessage));

    DI_CHECK_GOTO(pMessage->pAllocator->alloc(pMessage->pAllocator,
                                              DIANA_HOOK_A64_ALLOCATION_SIZE,
                                              &allocatedAddress,
                                              &pMessage->addressToHook,
                                              DIANA_HOOK_FLASG_ALLOCATE_EXECUTABLE));
    allocationSucceeded = 1;
    if (allocatedAddress & 3)
    {
        DI_CHECK_GOTO(DI_ERROR);
    }
    movedAddress = allocatedAddress + DIANA_HOOK_A64_STUB_SIZE;

    bNear = FitsSigned(Distance(pMessage->addressToHook, allocatedAddress) / 4, 26);
    if (pMessage->pCustomOptions && (pMessage->pCustomOptions->flags & DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP))
    {
        bNear = 0;
    }
    hookSize = bNear ? DIANA_HOOK_A64_NEAR_SIZE : DIANA_HOOK_A64_FAR_SIZE;

    /* the instructions the hook overwrites */
    DianaHook_ClearBuffers(pMessage);
    while (srcSize < hookSize)
    {
        DI_UINT32 word = 0;
        if (bEndsFlow)
        {
            /* the function ends before the hook fits */
            DI_CHECK_GOTO(DI_END);
        }
        DI_CHECK_GOTO(ReadWord(pMessage, pMessage->addressToHook + srcSize, &word));
        DI_CHECK_GOTO(MoveInstructionA64(pMessage,
                                         word,
                                         pMessage->addressToHook + srcSize,
                                         movedAddress + pMessage->workBufferRawSize,
                                         &bEndsFlow));
        srcSize += 4;
    }

    /* back to the rest of the function */
    {
        OPERAND_SIZE from = movedAddress + pMessage->workBufferRawSize;
        OPERAND_SIZE back = pMessage->addressToHook + srcSize;
        DI_INT64 delta = Distance(from, back);
        if (FitsSigned(delta / 4, 26))
        {
            DI_CHECK_GOTO(EmitWord(pMessage, A64_B | ((DI_UINT32)(delta / 4) & 0x3FFFFFFu)));
        }
        else
        {
            DI_CHECK_GOTO(EmitFarJump(pMessage, back, 0));
        }
    }
    if (DIANA_HOOK_A64_STUB_SIZE + (OPERAND_SIZE)pMessage->workBufferRawSize > DIANA_HOOK_A64_LITERALS_OFFSET)
    {
        DI_CHECK_GOTO(DI_ERROR);
    }

    /* the stub with its literal loads */
    for (i = 0; i < DIANA_HOOK_A64_STUB_WORDS; ++i)
    {
        DI_UINT32 word = g_stubA64[i];
        if (i == DIANA_HOOK_A64_LOAD_CONTEXT_INDEX || i == DIANA_HOOK_A64_LOAD_HOOK_INDEX)
        {
            DI_UINT32 literal = DIANA_HOOK_A64_LITERALS_OFFSET + (i == DIANA_HOOK_A64_LOAD_HOOK_INDEX ? 8 : 0);
            word |= ((literal - (DI_UINT32)i * 4) / 4) << 5;
        }
        PutWord(stub + i * 4, word);
    }
    PutQword(literals, pMessage->patchContext);
    PutQword(literals + 8, pMessage->hookFunction);

    DI_CHECK_GOTO(Write(pMessage, allocatedAddress + DIANA_HOOK_A64_LITERALS_OFFSET, literals, (int)sizeof(literals)));
    DI_CHECK_GOTO(Write(pMessage, allocatedAddress, stub, DIANA_HOOK_A64_STUB_SIZE));
    DI_CHECK_GOTO(Write(pMessage, movedAddress, pMessage->workBufferRaw, (int)pMessage->workBufferRawSize));
    if (pMessage->originalFunctionPointer != (DI_OPERAND_SIZE)-1)
    {
        PutQword(originalPointer, movedAddress);
        DI_CHECK_GOTO(Write(pMessage, pMessage->originalFunctionPointer, originalPointer, (int)sizeof(originalPointer)));
    }
    /* the last: the function goes to the stub */
    DI_CHECK_GOTO(WriteHookA64(pMessage, allocatedAddress, bNear));

cleanup:
    if (status && allocationSucceeded)
    {
        pMessage->pAllocator->free(pMessage->pAllocator, &allocatedAddress);
    }
    return status;
}

#endif
