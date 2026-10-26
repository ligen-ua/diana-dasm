#include "diana_ultimate_patcher.h"
#include "diana_ultimate_patcher_impl.h"
#include "diana_arm32.h"

#ifndef DIANA_NO_ARM

/* AArch32 hooks, A32 (DIANA_ARCH_ARM32) and T32 (DIANA_ARCH_THUMB). The allocation is like the AArch64 one:

     +0x000  the stub: saves r0-r12, lr and the flags, calls hook(r0 = patchContext, r1 = the sp of the
             hooked function, r2 = the saved registers), restores them
     +stub   the moved instructions, then a jump back to the hooked function
     +0x200  the literals: patchContext, hookFunction

   The stub loads the literals with a pc-relative ldr and, after it restores the registers, falls into the
   moved instructions. T32 code is kept 4-aligned so the pc-relative words it loads are aligned.

   The moved instructions are the ones the hook overwrites. A branch among them (b, bl, b<cond>, cbz/cbnz)
   is turned into a short sequence that reaches the same absolute target from the new place; an instruction
   that reads pc in any other way, or an IT block, is refused (DI_ERROR_NOT_IMPLEMENTED). The target of a
   branch comes from the decoder: target = instructionAddress + size + rel.m_value.

   T32 targets keep bit 0 set (Thumb); A32 targets are even. The caller flushes the instruction cache. */

#define DIANA_HOOK_A32_ALLOCATION_SIZE      1024
#define DIANA_HOOK_A32_LITERALS_OFFSET      0x200
#define DIANA_HOOK_A32_MAX_JUMPS_COUNT      20

#define DIANA_HOOK_A32_COND_AL              14

/* the stub ends with the register restore; the moved code follows it */
static const DI_UINT32 g_stubA32[] =
{
    0xE92D5FFF,     /* push  {r0-r12, lr} */
    0xE10F0000,     /* mrs   r0, cpsr */
    0xE52D0004,     /* push  {r0} */
    0xE1A0200D,     /* mov   r2, sp               the saved registers */
    0xE28D103C,     /* add   r1, sp, #0x3c        the original sp (cpsr + 14 registers) */
    0xE59F01E4,     /* ldr   r0, [pc, #0x1e4]     patchContext at +0x200 */
    0xE59F31E4,     /* ldr   r3, [pc, #0x1e4]     hookFunction at +0x204 */
    0xE12FFF33,     /* blx   r3 */
    0xE49D0004,     /* pop   {r0} */
    0xE128F000,     /* msr   cpsr_f, r0 */
    0xE8BD5FFF      /* pop   {r0-r12, lr} */
};
#define DIANA_HOOK_A32_STUB_WORDS   ((int)(sizeof(g_stubA32) / sizeof(g_stubA32[0])))
#define DIANA_HOOK_A32_STUB_SIZE    (DIANA_HOOK_A32_STUB_WORDS * 4)

static const unsigned char g_stubT32[] =
{
    0x2D, 0xE9, 0xFF, 0x5F,     /* push  {r0-r12, lr} */
    0xEF, 0xF3, 0x00, 0x80,     /* mrs   r0, cpsr */
    0x01, 0xB4,                 /* push  {r0} */
    0x6A, 0x46,                 /* mov   r2, sp */
    0x0F, 0xA9,                 /* add   r1, sp, #0x3c */
    0xDF, 0xF8, 0xF0, 0x01,     /* ldr.w r0, [pc, #0x1f0]     patchContext at +0x200 */
    0xDF, 0xF8, 0xF0, 0x31,     /* ldr.w r3, [pc, #0x1f0]     hookFunction at +0x204 */
    0x98, 0x47,                 /* blx   r3 */
    0x01, 0xBC,                 /* pop   {r0} */
    0x80, 0xF3, 0x00, 0x88,     /* msr   cpsr_f, r0 */
    0xBD, 0xE8, 0xFF, 0x5F,     /* pop   {r0-r12, lr} */
    0x00, 0xBF                  /* nop                        pad: the moved code starts 4-aligned */
};
#define DIANA_HOOK_A32_STUB_T32_SIZE    ((int)sizeof(g_stubT32))

static void PutWord(unsigned char * pOut, DI_UINT32 word)
{
    pOut[0] = (unsigned char)word;
    pOut[1] = (unsigned char)(word >> 8);
    pOut[2] = (unsigned char)(word >> 16);
    pOut[3] = (unsigned char)(word >> 24);
}

static DI_INT64 SignExtend(DI_UINT32 value, int bits)
{
    DI_UINT64 sign = (DI_UINT64)1 << (bits - 1);
    DI_UINT64 wide = (DI_UINT64)value & ((sign << 1) - 1);
    return (DI_INT64)((wide ^ sign) - sign);
}

static int FitsSigned(DI_INT64 value, int bits)
{
    DI_INT64 limit = (DI_INT64)1 << (bits - 1);
    return value >= -limit && value < limit;
}

static int ReadMemory(DianaHook_InternalMessage * pMessage, OPERAND_SIZE address, void * pBuffer, int size)
{
    OPERAND_SIZE readBytes = 0;
    DI_CHECK(pMessage->pReadWriteStream->parent.pRandomRead(&pMessage->pReadWriteStream->parent,
                                                            address, pBuffer, size, &readBytes, 0));
    if (readBytes != (OPERAND_SIZE)size)
    {
        return DI_ERROR;
    }
    return DI_SUCCESS;
}

static int Write(DianaHook_InternalMessage * pMessage, OPERAND_SIZE address, void * pData, int size)
{
    OPERAND_SIZE written = 0;
    DI_CHECK(pMessage->pReadWriteStream->pRandomWrite(pMessage->pReadWriteStream, address, pData, size, &written, 0));
    if (written != (OPERAND_SIZE)size)
    {
        return DI_ERROR;
    }
    return DI_SUCCESS;
}

/* the moved code goes to workBufferRaw */
static int EmitHalf(DianaHook_InternalMessage * pMessage, DI_UINT32 half)
{
    void * pOut = 0;
    DI_CHECK(DianaHook_AllocateCmd(pMessage, 2, &pOut));
    ((unsigned char *)pOut)[0] = (unsigned char)half;
    ((unsigned char *)pOut)[1] = (unsigned char)(half >> 8);
    return DI_SUCCESS;
}

static int EmitWord(DianaHook_InternalMessage * pMessage, DI_UINT32 word)
{
    void * pOut = 0;
    DI_CHECK(DianaHook_AllocateCmd(pMessage, 4, &pOut));
    PutWord((unsigned char *)pOut, word);
    return DI_SUCCESS;
}

/* a T32 32-bit instruction, hw1 first in memory */
static int EmitThumb2(DianaHook_InternalMessage * pMessage, DI_UINT32 hw1, DI_UINT32 hw2)
{
    DI_CHECK(EmitHalf(pMessage, hw1));
    return EmitHalf(pMessage, hw2);
}

/* 0xbf00 (nop) until the next emit is 4-aligned: before a T32 pc-relative word */
static int AlignT32(DianaHook_InternalMessage * pMessage, OPERAND_SIZE movedAddress)
{
    while ((movedAddress + (OPERAND_SIZE)pMessage->workBufferRawSize) & 3)
    {
        DI_CHECK(EmitHalf(pMessage, 0xBF00));
    }
    return DI_SUCCESS;
}

/* an unconditional jump to an absolute target, no register touched */
static int EmitJump(DianaHook_InternalMessage * pMessage, OPERAND_SIZE movedAddress, int isThumb, OPERAND_SIZE target)
{
    if (!isThumb)
    {
        DI_CHECK(EmitWord(pMessage, 0xE51FF004));   /* ldr pc, [pc, #-4] */
        return EmitWord(pMessage, (DI_UINT32)target);
    }
    DI_CHECK(AlignT32(pMessage, movedAddress));
    DI_CHECK(EmitThumb2(pMessage, 0xF8DF, 0xF000)); /* ldr.w pc, [pc, #0] */
    return EmitWord(pMessage, (DI_UINT32)target | 1);
}

/* the AArch32 instruction at src moved to dst (= movedAddress + what is already emitted).
   *pbEndsFlow: nothing after it runs. The decoded instruction is in pMessage->result */
static int MoveInstructionA32(DianaHook_InternalMessage * pMessage,
                              OPERAND_SIZE src,
                              OPERAND_SIZE movedAddress,
                              int isThumb,
                              int * pbEndsFlow)
{
    DianaParserResult * pResult = &pMessage->result;
    long id = (long)pResult->pInfo->m_pGroupInfo->m_commandId;
    int size = pResult->iFullCmdSize;
    int cond = DianaA32_GetCondition(pResult);
    int i = 0;
    int relIndex = -1;
    OPERAND_SIZE target = 0;

    *pbEndsFlow = 0;

    /* an IT block is not moved: the condition of the following instructions is in the IT state */
    if (id == (long)diana_a32_cmd_it || (pResult->iPrefix & DIANA_A32_PREFIX_IN_IT))
    {
        return DI_ERROR_NOT_IMPLEMENTED;
    }

    for (i = 0; i < pResult->iLinkedOpCount; ++i)
    {
        const DianaLinkedOperand * pOp = pResult->linkedOperands + i;
        if (pOp->type == diana_rel)
        {
            relIndex = i;
        }
        else if (pOp->type == diana_index && pOp->value.rmIndex.reg == reg_A32_PC)
        {
            return DI_ERROR_NOT_IMPLEMENTED;        /* a literal load: not handled yet */
        }
        else if (pOp->type == diana_register && pOp->value.recognizedRegister == reg_A32_PC)
        {
            return DI_ERROR_NOT_IMPLEMENTED;        /* reads or writes pc through a register operand */
        }
    }

    if (relIndex >= 0)
    {
        target = src + (OPERAND_SIZE)size + (OPERAND_SIZE)pResult->linkedOperands[relIndex].value.rel.m_value;
        if (isThumb)
        {
            target |= 1;
        }
    }

    if (id == (long)diana_a32_cmd_b)
    {
        if (cond == DIANA_HOOK_A32_COND_AL)
        {
            *pbEndsFlow = 1;
            return EmitJump(pMessage, movedAddress, isThumb, target);
        }
        /* b<cond> <target>: the opposite condition skips a jump to the target */
        if (!isThumb)
        {
            /* b<!cond> #+12 (over ldr pc, .word) */
            DI_CHECK(EmitWord(pMessage, ((DI_UINT32)(cond ^ 1) << 28) | 0x0A000000u | 1));
        }
        else
        {
            DI_CHECK(AlignT32(pMessage, movedAddress));
            /* b<!cond>.n #+8 (over the 8-byte ldr.w pc, .word), imm8 = 4 */
            DI_CHECK(EmitHalf(pMessage, 0xD000u | ((DI_UINT32)(cond ^ 1) << 8) | 4));
            DI_CHECK(EmitHalf(pMessage, 0xBF00));       /* pad so ldr.w is 4-aligned */
        }
        return EmitJump(pMessage, movedAddress, isThumb, target);
    }
    if (id == (long)diana_a32_cmd_bl)
    {
        /* a call: set lr to the instruction after the jump, then jump to the target */
        if (!isThumb)
        {
            DI_CHECK(EmitWord(pMessage, 0xE28FE004));   /* add lr, pc, #4 -> the word after the jump */
            DI_CHECK(EmitWord(pMessage, 0xE51FF004));   /* ldr pc, [pc, #-4] */
            return EmitWord(pMessage, (DI_UINT32)target);
        }
        DI_CHECK(AlignT32(pMessage, movedAddress));
        DI_CHECK(EmitThumb2(pMessage, 0xF8DF, 0xE008)); /* ldr.w lr, [pc, #8]  -> return address */
        DI_CHECK(EmitThumb2(pMessage, 0xF8DF, 0xF000)); /* ldr.w pc, [pc, #0]  -> the target */
        DI_CHECK(EmitWord(pMessage, (DI_UINT32)target));
        /* the return address: the next instruction, Thumb */
        return EmitWord(pMessage, (DI_UINT32)(movedAddress + (OPERAND_SIZE)pMessage->workBufferRawSize + 4) | 1);
    }
    if (id == (long)diana_a32_cmd_cbz || id == (long)diana_a32_cmd_cbnz)
    {
        /* T32 only. cb(n)z Rn, <target>: the opposite test skips a jump to the target */
        DI_UINT32 rn = (DI_UINT32)(pResult->linkedOperands[0].value.recognizedRegister - reg_A32_R0);
        DI_UINT32 op = id == (long)diana_a32_cmd_cbz ? 0x0800u : 0; /* flip cbz<->cbnz */
        DI_CHECK(AlignT32(pMessage, movedAddress));
        /* cb(n)z Rn, #+8 (over the 8-byte ldr.w pc, .word): i:imm5 = 4 -> imm5 field = 4 */
        DI_CHECK(EmitHalf(pMessage, 0xB100u | op | (4u << 3) | rn));
        DI_CHECK(EmitHalf(pMessage, 0xBF00));
        return EmitJump(pMessage, movedAddress, isThumb, target);
    }
    if (relIndex >= 0)
    {
        /* adr, blx <label> and the rest with a target: not handled yet */
        return DI_ERROR_NOT_IMPLEMENTED;
    }

    /* no pc reference: copy the bytes. bx, bxj and a pop/mov that writes pc end the flow */
    if (id == (long)diana_a32_cmd_bx || id == (long)diana_a32_cmd_bxj)
    {
        *pbEndsFlow = 1;
    }
    else if (pResult->iLinkedOpCount >= 1 &&
             pResult->linkedOperands[0].type == diana_register &&
             pResult->linkedOperands[0].value.recognizedRegister == reg_A32_PC)
    {
        *pbEndsFlow = 1;
    }
    if (size == 2)
    {
        return EmitHalf(pMessage, (DI_UINT32)pResult->bytes[0] | ((DI_UINT32)pResult->bytes[1] << 8));
    }
    if (isThumb)
    {
        DI_CHECK(EmitHalf(pMessage, (DI_UINT32)pResult->bytes[0] | ((DI_UINT32)pResult->bytes[1] << 8)));
        return EmitHalf(pMessage, (DI_UINT32)pResult->bytes[2] | ((DI_UINT32)pResult->bytes[3] << 8));
    }
    return EmitWord(pMessage, (DI_UINT32)pResult->bytes[0] | ((DI_UINT32)pResult->bytes[1] << 8) |
                              ((DI_UINT32)pResult->bytes[2] << 16) | ((DI_UINT32)pResult->bytes[3] << 24));
}

/* decode one instruction at address into pMessage->result, return its size */
static int DecodeAt(DianaHook_InternalMessage * pMessage, OPERAND_SIZE address, int isThumb, int * pSize)
{
    unsigned char bytes[4];
    if (!isThumb)
    {
        DI_CHECK(ReadMemory(pMessage, address, bytes, 4));
        DI_CHECK(DianaA32_DecodeA32(&pMessage->context,
                                    (DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8) |
                                    ((DI_UINT32)bytes[2] << 16) | ((DI_UINT32)bytes[3] << 24),
                                    &pMessage->result));
    }
    else
    {
        DI_UINT32 hw1 = 0, hw2 = 0;
        DI_CHECK(ReadMemory(pMessage, address, bytes, 2));
        hw1 = (DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8);
        if (DianaA32_GetT32Size(hw1) == 4)
        {
            DI_CHECK(ReadMemory(pMessage, address + 2, bytes + 2, 2));
            hw2 = (DI_UINT32)bytes[2] | ((DI_UINT32)bytes[3] << 8);
        }
        DI_CHECK(DianaA32_DecodeT32(&pMessage->context, hw1, hw2, &pMessage->result));
    }
    *pSize = pMessage->result.iFullCmdSize;
    return DI_SUCCESS;
}

static int FollowJumpsA32(DianaHook_InternalMessage * pMessage, int isThumb)
{
    int size = 0;
    if (pMessage->pCustomOptions && (pMessage->pCustomOptions->flags & DIANA_HOOK_CUSTOM_OPTION_DONT_FOLLOW_JUMPS))
    {
        return DI_SUCCESS;
    }
    for (;;)
    {
        DI_CHECK(DecodeAt(pMessage, pMessage->addressToHook, isThumb, &size));
        if ((long)pMessage->result.pInfo->m_pGroupInfo->m_commandId != (long)diana_a32_cmd_b ||
            DianaA32_GetCondition(&pMessage->result) != DIANA_HOOK_A32_COND_AL ||
            pMessage->result.iLinkedOpCount < 1 ||
            pMessage->result.linkedOperands[0].type != diana_rel)
        {
            return DI_SUCCESS;
        }
        if (pMessage->jumpsCounter >= DIANA_HOOK_A32_MAX_JUMPS_COUNT)
        {
            return DI_ERROR_NOT_IMPLEMENTED;
        }
        pMessage->addressToHook += (OPERAND_SIZE)size +
            (OPERAND_SIZE)pMessage->result.linkedOperands[0].value.rel.m_value;
        ++pMessage->jumpsCounter;
    }
}

/* the hook written into the function: b <stub> (A32, near) or a pc-relative load of the stub */
static int WriteHookA32(DianaHook_InternalMessage * pMessage, OPERAND_SIZE stubAddress, int isThumb, int bNear)
{
    unsigned char hook[12];
    OPERAND_SIZE address = pMessage->addressToHook;

    if (!isThumb)
    {
        if (bNear)
        {
            DI_INT64 delta = (DI_INT64)(DI_UINT64)(stubAddress - (address + 8));
            PutWord(hook, 0xEA000000u | ((DI_UINT32)(delta >> 2) & 0xFFFFFFu));  /* b <stub> */
            return Write(pMessage, address, hook, 4);
        }
        PutWord(hook, 0xE51FF004);                          /* ldr pc, [pc, #-4] */
        PutWord(hook + 4, (DI_UINT32)stubAddress);
        return Write(pMessage, address, hook, 8);
    }
    /* T32: ldr.w pc, [pc, #0]; .word stub|1, kept 4-aligned with a leading nop if needed */
    {
        int offset = 0;
        if (address & 3)
        {
            hook[0] = 0x00;
            hook[1] = 0xBF;                                 /* nop */
            offset = 2;
        }
        hook[offset + 0] = 0xDF;
        hook[offset + 1] = 0xF8;
        hook[offset + 2] = 0x00;
        hook[offset + 3] = 0xF0;                            /* ldr.w pc, [pc, #0] */
        PutWord(hook + offset + 4, (DI_UINT32)stubAddress | 1);
        return Write(pMessage, address, hook, offset + 8);
    }
}

int DianaHook_PatchStreamA32(DianaHook_InternalMessage * pMessage)
{
    int status = 0;
    int isThumb = pMessage->arch == DIANA_ARCH_THUMB;
    OPERAND_SIZE allocatedAddress = 0;
    int allocationSucceeded = 0;
    int stubSize = 0;
    int hookSize = 0;
    int bNear = 0;
    int srcSize = 0;
    int bEndsFlow = 0;
    OPERAND_SIZE movedAddress = 0;
    unsigned char stub[DIANA_HOOK_A32_STUB_SIZE > DIANA_HOOK_A32_STUB_T32_SIZE ?
                       DIANA_HOOK_A32_STUB_SIZE : DIANA_HOOK_A32_STUB_T32_SIZE];
    unsigned char literals[8];
    unsigned char originalPointer[4];
    int i = 0;

    if (pMessage->processorMode != DIANA_MODE32)
    {
        return DI_ERROR;
    }
    /* the hooked address: A32 is 4-aligned, T32 is 2-aligned; bit 0 is not part of it */
    pMessage->addressToHook &= ~(OPERAND_SIZE)(isThumb ? 1 : 3);
    Diana_InitContextEx(&pMessage->context, DIANA_MODE32, pMessage->arch);

    DI_CHECK_GOTO(FollowJumpsA32(pMessage, isThumb));

    DI_CHECK_GOTO(pMessage->pAllocator->alloc(pMessage->pAllocator,
                                              DIANA_HOOK_A32_ALLOCATION_SIZE,
                                              &allocatedAddress,
                                              &pMessage->addressToHook,
                                              DIANA_HOOK_FLASG_ALLOCATE_EXECUTABLE));
    allocationSucceeded = 1;
    if (allocatedAddress & 3)
    {
        DI_CHECK_GOTO(DI_ERROR);
    }
    stubSize = isThumb ? DIANA_HOOK_A32_STUB_T32_SIZE : DIANA_HOOK_A32_STUB_SIZE;
    movedAddress = allocatedAddress + stubSize;

    /* A32 can reach a near stub with b (+-32 MB); T32 always uses the pc-relative load */
    if (!isThumb)
    {
        bNear = FitsSigned((DI_INT64)(DI_UINT64)(allocatedAddress - (pMessage->addressToHook + 8)) >> 2, 24);
        if (pMessage->pCustomOptions && (pMessage->pCustomOptions->flags & DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP))
        {
            bNear = 0;
        }
        hookSize = bNear ? 4 : 8;
    }
    else
    {
        hookSize = (pMessage->addressToHook & 3) ? 10 : 8;
    }

    /* the instructions the hook overwrites, moved to the stub */
    DianaHook_ClearBuffers(pMessage);
    while (srcSize < hookSize)
    {
        int size = 0;
        if (bEndsFlow)
        {
            DI_CHECK_GOTO(DI_END);
        }
        DI_CHECK_GOTO(DecodeAt(pMessage, pMessage->addressToHook + srcSize, isThumb, &size));
        DI_CHECK_GOTO(MoveInstructionA32(pMessage,
                                         pMessage->addressToHook + srcSize,
                                         movedAddress,
                                         isThumb,
                                         &bEndsFlow));
        srcSize += size;
    }

    /* back to the rest of the function */
    if (isThumb)
    {
        DI_CHECK_GOTO(AlignT32(pMessage, movedAddress));
    }
    DI_CHECK_GOTO(EmitJump(pMessage, movedAddress, isThumb, pMessage->addressToHook + (OPERAND_SIZE)srcSize));

    if (stubSize + (OPERAND_SIZE)pMessage->workBufferRawSize > DIANA_HOOK_A32_LITERALS_OFFSET)
    {
        DI_CHECK_GOTO(DI_ERROR);
    }

    /* the stub, then the literals it loads */
    if (!isThumb)
    {
        for (i = 0; i < DIANA_HOOK_A32_STUB_WORDS; ++i)
        {
            PutWord(stub + i * 4, g_stubA32[i]);
        }
    }
    else
    {
        DIANA_MEMCPY(stub, g_stubT32, DIANA_HOOK_A32_STUB_T32_SIZE);
    }
    PutWord(literals, (DI_UINT32)pMessage->patchContext);
    PutWord(literals + 4, (DI_UINT32)pMessage->hookFunction);

    DI_CHECK_GOTO(Write(pMessage, allocatedAddress + DIANA_HOOK_A32_LITERALS_OFFSET, literals, (int)sizeof(literals)));
    DI_CHECK_GOTO(Write(pMessage, allocatedAddress, stub, stubSize));
    DI_CHECK_GOTO(Write(pMessage, movedAddress, pMessage->workBufferRaw, (int)pMessage->workBufferRawSize));
    if (pMessage->originalFunctionPointer != (DI_OPERAND_SIZE)-1)
    {
        /* Thumb: the entry keeps bit 0 set */
        PutWord(originalPointer, (DI_UINT32)movedAddress | (isThumb ? 1 : 0));
        DI_CHECK_GOTO(Write(pMessage, pMessage->originalFunctionPointer, originalPointer, (int)sizeof(originalPointer)));
    }
    DI_CHECK_GOTO(WriteHookA32(pMessage, allocatedAddress, isThumb, bNear));

cleanup:
    if (status && allocationSucceeded)
    {
        pMessage->pAllocator->free(pMessage->pAllocator, &allocatedAddress);
    }
    return status;
}

#endif
