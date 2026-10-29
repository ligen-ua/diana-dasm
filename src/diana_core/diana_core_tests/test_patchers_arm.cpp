extern "C"
{
#include "diana_patchers/diana_ultimate_patcher.h"
#include "diana_streams.h"
#include "diana_arm.h"
#include "diana_arm32.h"
#include "diana_text_output_masm.h"
}

#include "test_common.h"
#include "test_patchers.h"
#include "diana_core_cpp.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <string>
#include <vector>

#ifndef DIANA_NO_ARM

/* AArch64 hooks: there is no ARM emulator, so the tests patch a memory of a few windows at chosen
   addresses (the function, the stub near or far from it) and check the disassembly of what was written */
namespace
{
    const OPERAND_SIZE c_codeBase = 0x100000;
    const OPERAND_SIZE c_context = 0xC0C0C0C0C0ULL;
    const OPERAND_SIZE c_hook = 0x7700001000ULL;
    const OPERAND_SIZE c_originalSlot = c_codeBase + 0x800;
    const unsigned int c_nop = 0xd503201f;

    struct Window
    {
        OPERAND_SIZE base;
        std::vector<unsigned char> bytes;
    };

    class CArmTestMemory;

    /* what the callbacks get: plain structs, so offsetof works */
    struct Callbacks
    {
        DianaReadWriteRandomStream stream;
        DianaHook_Allocator allocator;
        CArmTestMemory * pOwner;
    };

    class CArmTestMemory
    {
        CArmTestMemory(const CArmTestMemory&);
        CArmTestMemory & operator = (const CArmTestMemory&);
    public:
        Callbacks m_callbacks;
        DianaHook_TargetMemoryProvider m_provider;
        std::vector<Window> m_windows;
        OPERAND_SIZE m_stubAddress;
        int m_allocations;
        int m_frees;

        CArmTestMemory(const unsigned int * pWords, size_t count, OPERAND_SIZE stubAddress)
            : m_stubAddress(stubAddress), m_allocations(0), m_frees(0)
        {
            Window code;
            code.base = c_codeBase;
            code.bytes.assign(0x1000, 0);
            for (size_t i = 0; i < count; ++i)
            {
                PutWord(&code.bytes[i * 4], pWords[i]);
            }
            m_windows.push_back(code);
            m_callbacks.pOwner = this;
            DianaReadWriteRandomStream_Init(&m_callbacks.stream, Read, MoveTo, RandomRead, RandomWrite);
            DianaHook_Allocator_Init(&m_callbacks.allocator, Alloc, Free);
            DianaHook_TargetMemoryProvider_Init(&m_provider, &m_callbacks.stream, &m_callbacks.allocator);
        }
        static void PutWord(unsigned char * p, unsigned int word)
        {
            p[0] = (unsigned char)word;
            p[1] = (unsigned char)(word >> 8);
            p[2] = (unsigned char)(word >> 16);
            p[3] = (unsigned char)(word >> 24);
        }
        unsigned char * Find(OPERAND_SIZE address, int size)
        {
            for (size_t i = 0; i < m_windows.size(); ++i)
            {
                Window & window = m_windows[i];
                if (address >= window.base && address + size <= window.base + window.bytes.size())
                    return &window.bytes[(size_t)(address - window.base)];
            }
            return 0;
        }
        unsigned int Word(OPERAND_SIZE address)
        {
            unsigned char * p = Find(address, 4);
            DIANA_TEST_ASSERT(p != 0);
            if (!p)
                return 0;
            return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
        }
        OPERAND_SIZE Qword(OPERAND_SIZE address)
        {
            return (OPERAND_SIZE)Word(address) | ((OPERAND_SIZE)Word(address + 4) << 32);
        }
        unsigned int Half(OPERAND_SIZE address)
        {
            unsigned char * p = Find(address, 2);
            DIANA_TEST_ASSERT(p != 0);
            if (!p)
                return 0;
            return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
        }
        /* write raw code bytes at c_codeBase (for AArch32 instruction streams) */
        void SetBytes(const unsigned char * pBytes, size_t count)
        {
            size_t i = 0;
            for (; i < count; ++i)
                m_windows[0].bytes[i] = pBytes[i];
        }
        int Patch(int flags = 0)
        {
            DianaHook_CustomOptions options;
            memset(&options, 0, sizeof(options));
            options.flags = flags | DIANA_HOOK_CUSTOM_OPTION_SAVE_ORIGINAL_FP;
            options.originalFunctionPointer = c_originalSlot;
            options.patchContext = c_context;
            return DianaHook_PatchStreamEx(&m_provider, DIANA_MODE64, DIANA_ARCH_AARCH64, c_codeBase, c_hook, &options);
        }
        /* AArch32: context/hook/original fit 32 bits; address keeps the Thumb bit off */
        int PatchArch(int arch, int flags = 0)
        {
            DianaHook_CustomOptions options;
            memset(&options, 0, sizeof(options));
            options.flags = flags | DIANA_HOOK_CUSTOM_OPTION_SAVE_ORIGINAL_FP;
            options.originalFunctionPointer = c_originalSlot;
            options.patchContext = 0x11223344u;
            return DianaHook_PatchStreamEx(&m_provider, DIANA_MODE32, arch, c_codeBase, 0x55667788u, &options);
        }

        static CArmTestMemory * FromStream(void * pThis)
        {
            return ((Callbacks *)((char *)pThis - offsetof(Callbacks, stream)))->pOwner;
        }
        static CArmTestMemory * FromAllocator(void * pThis)
        {
            return ((Callbacks *)((char *)pThis - offsetof(Callbacks, allocator)))->pOwner;
        }
        static int Read(void *, void *, int, int *)
        {
            return DI_ERROR;
        }
        static int MoveTo(void *, OPERAND_SIZE)
        {
            return DI_SUCCESS;
        }
        static int RandomRead(void * pThis, OPERAND_SIZE offset, void * pBuffer, int size, OPERAND_SIZE * readBytes, int)
        {
            unsigned char * p = FromStream(pThis)->Find(offset, size);
            *readBytes = 0;
            if (!p)
                return DI_ERROR;
            memcpy(pBuffer, p, (size_t)size);
            *readBytes = (OPERAND_SIZE)size;
            return DI_SUCCESS;
        }
        static int RandomWrite(void * pThis, OPERAND_SIZE offset, void * pBuffer, int size, OPERAND_SIZE * writeBytes, int)
        {
            unsigned char * p = FromStream(pThis)->Find(offset, size);
            *writeBytes = 0;
            if (!p)
                return DI_ERROR;
            memcpy(p, pBuffer, (size_t)size);
            *writeBytes = (OPERAND_SIZE)size;
            return DI_SUCCESS;
        }
        static int Alloc(void * pThis, OPERAND_SIZE size, OPERAND_SIZE * pAddress, const OPERAND_SIZE *, int)
        {
            CArmTestMemory * pMemory = FromAllocator(pThis);
            Window window;
            window.base = pMemory->m_stubAddress;
            window.bytes.assign((size_t)size, 0);
            pMemory->m_windows.push_back(window);
            ++pMemory->m_allocations;
            *pAddress = window.base;
            return DI_SUCCESS;
        }
        static void Free(void * pThis, const OPERAND_SIZE *)
        {
            ++FromAllocator(pThis)->m_frees;
        }
    };

    OPERAND_SIZE ParseHex(const char * pText)
    {
        OPERAND_SIZE value = 0;
        if (pText[0] == '0' && pText[1] == 'x')
            pText += 2;
        for (; *pText; ++pText)
        {
            int digit = *pText >= 'a' ? *pText - 'a' + 10 : *pText - '0';
            value = value * 16 + (OPERAND_SIZE)digit;
        }
        return value;
    }

    std::string Hex(OPERAND_SIZE value)
    {
        static const char digits[] = "0123456789abcdef";
        std::string text;
        do
        {
            text.insert(text.begin(), digits[(size_t)(value & 0xF)]);
            value >>= 4;
        } while (value);
        return "0x" + text;
    }

    /* the code at address: one line per instruction, ".quad 0x..." is a literal of 8 bytes */
    void ExpectCode(CArmTestMemory & memory, OPERAND_SIZE address, const std::vector<std::string> & lines)
    {
        diana::CMasmString text;
        DianaContext context;
        Diana_InitContextEx(&context, DIANA_MODE64, DIANA_ARCH_AARCH64);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            const std::string & expected = lines[i];
            if (expected.compare(0, 6, ".quad ") == 0)
            {
                OPERAND_SIZE value = ParseHex(expected.c_str() + 6);
                OPERAND_SIZE actual = memory.Qword(address);
                if (actual != value)
                {
                    printf("patcher a64: %llx: expected %s, got %s\n", (unsigned long long)address, expected.c_str(), Hex(actual).c_str());
                    DIANA_TEST_ASSERT(0);
                }
                address += 8;
                continue;
            }
            DianaParserResult result;
            const char * pText = "<no decode>";
            if (DianaA64_DecodeWord(&context, memory.Word(address), &result) == DI_SUCCESS)
                pText = text.Assign(&result, address);
            if (expected != pText)
            {
                printf("patcher a64: %llx: expected \"%s\", got \"%s\"\n", (unsigned long long)address, expected.c_str(), pText);
                DIANA_TEST_ASSERT(0);
            }
            address += 4;
        }
    }

    class Lines
    {
    public:
        std::vector<std::string> m_lines;
        Lines & operator ()(const std::string & line)
        {
            m_lines.push_back(line);
            return *this;
        }
    };

    const int c_stubSize = 31 * 4;

    void ExpectStub(CArmTestMemory & memory, OPERAND_SIZE stub)
    {
        Lines lines;
        lines("sub sp, sp, #0xb0")
             ("stp x0, x1, [sp]")
             ("stp x2, x3, [sp, #16]")
             ("stp x4, x5, [sp, #32]")
             ("stp x6, x7, [sp, #48]")
             ("stp x8, x9, [sp, #64]")
             ("stp x10, x11, [sp, #80]")
             ("stp x12, x13, [sp, #96]")
             ("stp x14, x15, [sp, #112]")
             ("stp x16, x17, [sp, #128]")
             ("mrs x9, nzcv")
             ("stp x18, x9, [sp, #144]")
             ("stp x29, x30, [sp, #160]")
             ("add x1, sp, #0xb0")
             ("mov x2, sp")
             ("ldr x0, " + Hex(stub + 0x200))
             ("ldr x16, " + Hex(stub + 0x208))
             ("blr x16")
             ("ldp x18, x9, [sp, #144]")
             ("msr nzcv, x9")
             ("ldp x0, x1, [sp]")
             ("ldp x2, x3, [sp, #16]")
             ("ldp x4, x5, [sp, #32]")
             ("ldp x6, x7, [sp, #48]")
             ("ldp x8, x9, [sp, #64]")
             ("ldp x10, x11, [sp, #80]")
             ("ldp x12, x13, [sp, #96]")
             ("ldp x14, x15, [sp, #112]")
             ("ldp x16, x17, [sp, #128]")
             ("ldp x29, x30, [sp, #160]")
             ("add sp, sp, #0xb0");
        ExpectCode(memory, stub, lines.m_lines);
        DIANA_TEST_ASSERT(memory.Qword(stub + 0x200) == c_context);
        DIANA_TEST_ASSERT(memory.Qword(stub + 0x208) == c_hook);
    }

    /* the function starts with b <stub>, the original pointer is the moved code */
    void ExpectNearHook(CArmTestMemory & memory, OPERAND_SIZE target, OPERAND_SIZE stub)
    {
        Lines lines;
        lines("b " + Hex(stub));
        ExpectCode(memory, target, lines.m_lines);
        DIANA_TEST_ASSERT(memory.Qword(c_originalSlot) == stub + c_stubSize);
    }

    void ExpectFarHook(CArmTestMemory & memory, OPERAND_SIZE stub)
    {
        Lines lines;
        lines("ldr x16, " + Hex(c_codeBase + 8))
             ("br x16")
             (".quad " + Hex(stub));
        ExpectCode(memory, c_codeBase, lines.m_lines);
        DIANA_TEST_ASSERT(memory.Qword(c_originalSlot) == stub + c_stubSize);
    }

    /* one instruction moved by a near hook: the moved code at the stub, then b back */
    void TestMovedOne(unsigned int word, OPERAND_SIZE stub, const Lines & moved)
    {
        unsigned int code[] = { word, c_nop };
        CArmTestMemory memory(code, 2, stub);
        DIANA_TEST_ASSERT(memory.Patch() == DI_SUCCESS);
        ExpectNearHook(memory, c_codeBase, stub);
        ExpectStub(memory, stub);
        ExpectCode(memory, stub + c_stubSize, moved.m_lines);
        DIANA_TEST_ASSERT(memory.m_frees == 0);
    }
}

/* a prologue, the stub 512 KB away */
static void test_patchers_a64_near()
{
    const OPERAND_SIZE stub = 0x180000;
    unsigned int code[] = {
        0xa9bf7bfd,     /* stp x29, x30, [sp, #-16]! */
        0x910003fd,     /* mov x29, sp */
        0xd65f03c0      /* ret */
    };
    CArmTestMemory memory(code, 3, stub);
    DIANA_TEST_ASSERT(memory.Patch() == DI_SUCCESS);
    ExpectNearHook(memory, c_codeBase, stub);
    ExpectStub(memory, stub);
    Lines moved;
    moved("stp x29, x30, [sp, #-16]!")
         ("b " + Hex(c_codeBase + 4));
    ExpectCode(memory, stub + c_stubSize, moved.m_lines);
    /* the rest of the function is as it was */
    DIANA_TEST_ASSERT(memory.Word(c_codeBase + 4) == 0x910003fd);
}

/* the stub 1 GB away: four instructions move, bl and the way back go through literals, adrp reaches */
static void test_patchers_a64_far()
{
    const OPERAND_SIZE stub = 0x40000000;
    const OPERAND_SIZE moved = stub + c_stubSize;
    unsigned int code[] = {
        0xa9bf7bfd,     /* stp x29, x30, [sp, #-16]! */
        0x910003fd,     /* mov x29, sp */
        0x9400003e,     /* bl 0x100100 */
        0xb0000000,     /* adrp x0, 0x101000 */
        0xd65f03c0      /* ret */
    };
    CArmTestMemory memory(code, 5, stub);
    DIANA_TEST_ASSERT(memory.Patch() == DI_SUCCESS);
    ExpectFarHook(memory, stub);
    ExpectStub(memory, stub);
    Lines lines;
    lines("stp x29, x30, [sp, #-16]!")
         ("mov x29, sp")
         ("ldr x16, " + Hex(moved + 0x14))
         ("blr x16")
         ("b " + Hex(moved + 0x1c))
         (".quad 0x100100")
         ("adrp x0, 0x101000")
         ("ldr x16, " + Hex(moved + 0x28))
         ("br x16")
         (".quad " + Hex(c_codeBase + 16));
    ExpectCode(memory, moved, lines.m_lines);
    DIANA_TEST_ASSERT(memory.Word(c_codeBase + 16) == 0xd65f03c0);
}

/* DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP: the far hook with a near stub, the way back is near */
static void test_patchers_a64_far_option()
{
    const OPERAND_SIZE stub = 0x180000;
    unsigned int code[] = { c_nop, c_nop, c_nop, c_nop, 0xd65f03c0 };
    CArmTestMemory memory(code, 5, stub);
    DIANA_TEST_ASSERT(memory.Patch(DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP) == DI_SUCCESS);
    ExpectFarHook(memory, stub);
    Lines lines;
    lines("nop")("nop")("nop")("nop")("b " + Hex(c_codeBase + 16));
    ExpectCode(memory, stub + c_stubSize, lines.m_lines);
}

/* b.cond, cbz, tbz, adr, ldr (literal) and bl: in range they are encoded again, out of range they
   become a sequence */
static void test_patchers_a64_moved()
{
    const OPERAND_SIZE near = 0x180000;          /* 512 KB: b.cond, cbz, adr and ldr reach */
    const OPERAND_SIZE close = 0x101000;         /* 4 KB: tbz reaches */
    const OPERAND_SIZE mid = 0x4100000;          /* 64 MB: only b reaches */
    const OPERAND_SIZE movedNear = near + c_stubSize;
    const OPERAND_SIZE movedMid = mid + c_stubSize;
    const std::string back = "b " + Hex(c_codeBase + 4);

    /* b.ne 0x100040 */
    TestMovedOne(0x54000201, near, Lines()("b.ne 0x100040")(back));
    TestMovedOne(0x54000201, mid, Lines()("b.eq " + Hex(movedMid + 20))
                                         ("ldr x16, " + Hex(movedMid + 12))
                                         ("br x16")
                                         (".quad 0x100040")
                                         (back));
    /* b.al 0x100040 always branches: a far jump without a condition, nothing after it */
    TestMovedOne(0x5400020e, mid, Lines()("ldr x16, " + Hex(movedMid + 8))
                                         ("br x16")
                                         (".quad 0x100040")
                                         (back));
    /* cbz x0, 0x100040 */
    TestMovedOne(0xb4000200, near, Lines()("cbz x0, 0x100040")(back));
    TestMovedOne(0xb4000200, mid, Lines()("cbnz x0, " + Hex(movedMid + 20))
                                         ("ldr x16, " + Hex(movedMid + 12))
                                         ("br x16")
                                         (".quad 0x100040")
                                         (back));
    /* tbz w1, #3, 0x100040: 32 KB */
    TestMovedOne(0x36180201, close, Lines()("tbz w1, #3, 0x100040")(back));
    TestMovedOne(0x36180201, near, Lines()("tbnz w1, #3, " + Hex(movedNear + 20))
                                          ("ldr x16, " + Hex(movedNear + 12))
                                          ("br x16")
                                          (".quad 0x100040")
                                          (back));
    /* adr x0, 0x100040 */
    TestMovedOne(0x10000200, near, Lines()("adr x0, 0x100040")(back));
    TestMovedOne(0x10000200, mid, Lines()("ldr x0, " + Hex(movedMid + 8))
                                         ("b " + Hex(movedMid + 16))
                                         (".quad 0x100040")
                                         (back));
    /* ldr x1, 0x100040 */
    TestMovedOne(0x58000201, near, Lines()("ldr x1, 0x100040")(back));
    TestMovedOne(0x58000201, mid, Lines()("ldr x1, " + Hex(movedMid + 8))
                                         ("b " + Hex(movedMid + 16))
                                         (".quad 0x100040")
                                         ("ldr x1, [x1]")
                                         (back));
    /* ldr w3, ldrsw x2: the address goes to the register loaded */
    TestMovedOne(0x18000203, mid, Lines()("ldr x3, " + Hex(movedMid + 8))
                                         ("b " + Hex(movedMid + 16))
                                         (".quad 0x100040")
                                         ("ldr w3, [x3]")
                                         (back));
    TestMovedOne(0x98000202, mid, Lines()("ldr x2, " + Hex(movedMid + 8))
                                         ("b " + Hex(movedMid + 16))
                                         (".quad 0x100040")
                                         ("ldrsw x2, [x2]")
                                         (back));
    /* ldr q0 and prfm have no general register of their own: x17 */
    TestMovedOne(0x9c000200, mid, Lines()("ldr x17, " + Hex(movedMid + 8))
                                         ("b " + Hex(movedMid + 16))
                                         (".quad 0x100040")
                                         ("ldr q0, [x17]")
                                         (back));
    TestMovedOne(0xd8000200, mid, Lines()("ldr x17, " + Hex(movedMid + 8))
                                         ("b " + Hex(movedMid + 16))
                                         (".quad 0x100040")
                                         ("prfm pldl1keep, [x17]")
                                         (back));
    /* bl 0x100100 returns into the stub, after the call */
    TestMovedOne(0x94000040, mid, Lines()("bl 0x100100")(back));
}

/* adrp of a page more than 4 GB away: the page through a literal */
static void test_patchers_a64_adrp_far()
{
    const OPERAND_SIZE stub = 0x2000000000ULL;
    const OPERAND_SIZE moved = stub + c_stubSize;
    unsigned int code[] = { 0xb0000000 /* adrp x0, 0x101000 */, c_nop, c_nop, c_nop, 0xd65f03c0 };
    CArmTestMemory memory(code, 5, stub);
    DIANA_TEST_ASSERT(memory.Patch() == DI_SUCCESS);
    ExpectFarHook(memory, stub);
    Lines lines;
    lines("ldr x0, " + Hex(moved + 8))
         ("b " + Hex(moved + 16))
         (".quad 0x101000")
         ("nop")("nop")("nop")
         ("ldr x16, " + Hex(moved + 0x24))
         ("br x16")
         (".quad " + Hex(c_codeBase + 16));
    ExpectCode(memory, moved, lines.m_lines);
}

/* b at the entry: the function it leads to is hooked, unless DIANA_HOOK_CUSTOM_OPTION_DONT_FOLLOW_JUMPS */
static void test_patchers_a64_follow()
{
    const OPERAND_SIZE stub = 0x180000;
    unsigned int code[] = {
        0x14000004,     /* b 0x100010 */
        c_nop, c_nop, c_nop,
        0xa9bf7bfd,     /* stp x29, x30, [sp, #-16]! */
        0xd65f03c0      /* ret */
    };
    {
        CArmTestMemory memory(code, 6, stub);
        DIANA_TEST_ASSERT(memory.Patch() == DI_SUCCESS);
        DIANA_TEST_ASSERT(memory.Word(c_codeBase) == 0x14000004);
        ExpectNearHook(memory, c_codeBase + 16, stub);
        ExpectCode(memory, stub + c_stubSize, Lines()("stp x29, x30, [sp, #-16]!")("b " + Hex(c_codeBase + 20)).m_lines);
    }
    {
        CArmTestMemory memory(code, 6, stub);
        DIANA_TEST_ASSERT(memory.Patch(DIANA_HOOK_CUSTOM_OPTION_DONT_FOLLOW_JUMPS) == DI_SUCCESS);
        ExpectNearHook(memory, c_codeBase, stub);
        ExpectCode(memory, stub + c_stubSize, Lines()("b 0x100010")("b " + Hex(c_codeBase + 4)).m_lines);
    }
}

/* what can't be hooked: the stub is freed */
static void test_patchers_a64_invalid()
{
    const OPERAND_SIZE far = 0x40000000;
    {
        /* the function ends before the 16 bytes of a far hook */
        unsigned int code[] = { 0xa9bf7bfd, 0xd65f03c0, c_nop, c_nop };
        CArmTestMemory memory(code, 4, far);
        DIANA_TEST_ASSERT(memory.Patch() == DI_END);
        DIANA_TEST_ASSERT(memory.m_frees == 1);
        DIANA_TEST_ASSERT(memory.Word(c_codeBase) == 0xa9bf7bfd);
    }
    {
        /* not an instruction */
        unsigned int code[] = { 0xffffffff, c_nop };
        CArmTestMemory memory(code, 2, 0x180000);
        DIANA_TEST_ASSERT(memory.Patch() == DI_ERROR);
        DIANA_TEST_ASSERT(memory.m_frees == 1);
    }
    {
        /* a misaligned address, a 32-bit mode */
        unsigned int code[] = { c_nop, c_nop };
        CArmTestMemory memory(code, 2, 0x180000);
        DIANA_TEST_ASSERT(DianaHook_PatchStreamEx(&memory.m_provider, DIANA_MODE64, DIANA_ARCH_AARCH64, c_codeBase + 2, c_hook, 0) == DI_ERROR);
        DIANA_TEST_ASSERT(DianaHook_PatchStreamEx(&memory.m_provider, DIANA_MODE32, DIANA_ARCH_AARCH64, c_codeBase, c_hook, 0) == DI_ERROR);
        DIANA_TEST_ASSERT(memory.m_allocations == 0);
    }
}

/* ------------------------------------------------------------------ AArch32 (A32 and T32)
   These check the installed hook, the stub literals, the original pointer and the moved relocations by
   decoding what was written. Real execution is covered under qemu (scratchpad driver). */
namespace
{
    const OPERAND_SIZE c_a32Context = 0x11223344u;
    const OPERAND_SIZE c_a32Hook = 0x55667788u;
    const int c_a32StubSize = 44;    /* 11 words */
    const int c_t32StubSize = 36;

    /* the absolute target of the branch at address, decoded fresh */
    OPERAND_SIZE DecodedTarget(const unsigned char * bytes, OPERAND_SIZE address, int arch)
    {
        DianaContext ctx;
        DianaParserResult res;
        int i = 0;
        Diana_InitContextEx(&ctx, DIANA_MODE32, arch);
        if (arch == DIANA_ARCH_ARM32)
        {
            DianaA32_DecodeA32(&ctx, (DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8) |
                               ((DI_UINT32)bytes[2] << 16) | ((DI_UINT32)bytes[3] << 24), &res);
        }
        else
        {
            DI_UINT32 hw1 = (DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8);
            DI_UINT32 hw2 = DianaA32_GetT32Size(hw1) == 4 ? ((DI_UINT32)bytes[2] | ((DI_UINT32)bytes[3] << 8)) : 0;
            DianaA32_DecodeT32(&ctx, hw1, hw2, &res);
        }
        for (i = 0; i < res.iLinkedOpCount; ++i)
        {
            if (res.linkedOperands[i].type == diana_rel)
            {
                OPERAND_SIZE t = address + (OPERAND_SIZE)res.iFullCmdSize + (OPERAND_SIZE)res.linkedOperands[i].value.rel.m_value;
                return arch == DIANA_ARCH_THUMB ? (t | 1) : t;
            }
        }
        DIANA_TEST_ASSERT(0);
        return 0;
    }

    void SetAndPatchA32(CArmTestMemory & memory, const unsigned char * bytes, size_t count, int arch, int flags, int expect)
    {
        int status = 0;
        memory.SetBytes(bytes, count);
        status = memory.PatchArch(arch, flags);
        if (status != expect)
            printf("patcher a32/t32: status %d, expected %d\n", status, expect);
        DIANA_TEST_ASSERT(status == expect);
    }

    void ExpectLiteralsAndOriginal(CArmTestMemory & memory, OPERAND_SIZE stub, int stubSize, int isThumb)
    {
        DIANA_TEST_ASSERT(memory.Word(stub + 0x200) == c_a32Context);
        DIANA_TEST_ASSERT(memory.Word(stub + 0x204) == c_a32Hook);
        DIANA_TEST_ASSERT(memory.Word(c_originalSlot) == ((DI_UINT32)(stub + stubSize) | (isThumb ? 1u : 0u)));
    }
}

/* A32: near b hook, far ldr-pc hook, moved b/bl, conditional b, and the refused forms */
static void test_patchers_a32()
{
    const OPERAND_SIZE stub = 0x180000;
    const OPERAND_SIZE moved = stub + c_a32StubSize;

    /* uncond b, near: the function jumps to the stub, the moved b jumps to the same absolute target */
    {
        unsigned char code[] = { 0x0E, 0x00, 0x00, 0xEA, 0,0,0,0 };     /* b codeBase+0x40 */
        OPERAND_SIZE target;
        CArmTestMemory memory(0, 0, stub);
        target = DecodedTarget(code, c_codeBase, DIANA_ARCH_ARM32);
        /* don't follow it as an entry thunk: move it */
        SetAndPatchA32(memory, code, sizeof(code), DIANA_ARCH_ARM32, DIANA_HOOK_CUSTOM_OPTION_DONT_FOLLOW_JUMPS, DI_SUCCESS);
        DIANA_TEST_ASSERT(memory.Word(c_codeBase) ==
            (0xEA000000u | (((DI_UINT32)(stub - (c_codeBase + 8)) >> 2) & 0xFFFFFFu)));
        ExpectLiteralsAndOriginal(memory, stub, c_a32StubSize, 0);
        DIANA_TEST_ASSERT(memory.Word(moved) == 0xE51FF004);            /* ldr pc, [pc, #-4] */
        DIANA_TEST_ASSERT(memory.Word(moved + 4) == (DI_UINT32)target);
    }
    /* bl, near: lr set to the word after the jump, then jump to the target */
    {
        unsigned char code[] = { 0x0E, 0x00, 0x00, 0xEB, 0,0,0,0 };     /* bl codeBase+0x40 */
        OPERAND_SIZE target;
        CArmTestMemory memory(0, 0, stub);
        target = DecodedTarget(code, c_codeBase, DIANA_ARCH_ARM32);
        SetAndPatchA32(memory, code, sizeof(code), DIANA_ARCH_ARM32, 0, DI_SUCCESS);
        DIANA_TEST_ASSERT(memory.Word(moved) == 0xE28FE004);            /* add lr, pc, #4 */
        DIANA_TEST_ASSERT(memory.Word(moved + 4) == 0xE51FF004);
        DIANA_TEST_ASSERT(memory.Word(moved + 8) == (DI_UINT32)target);
    }
    /* b<cond>, far hook (two instructions moved): the opposite condition skips a jump to the target */
    {
        unsigned char code[] = { 0x00, 0x00, 0x50, 0xE3,               /* cmp r0, #0 */
                                 0x0E, 0x00, 0x00, 0x0A };              /* beq codeBase+0x44 */
        OPERAND_SIZE target;
        CArmTestMemory memory(0, 0, stub);
        target = DecodedTarget(code + 4, c_codeBase + 4, DIANA_ARCH_ARM32);
        SetAndPatchA32(memory, code, sizeof(code), DIANA_ARCH_ARM32, DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP, DI_SUCCESS);
        DIANA_TEST_ASSERT(memory.Word(c_codeBase) == 0xE51FF004);       /* far hook */
        DIANA_TEST_ASSERT(memory.Word(c_codeBase + 4) == (DI_UINT32)stub);
        DIANA_TEST_ASSERT(memory.Word(moved) == 0xE3500000);           /* cmp copied */
        DIANA_TEST_ASSERT(memory.Word(moved + 4) == 0x1A000001);       /* bne #+12 */
        DIANA_TEST_ASSERT(memory.Word(moved + 8) == 0xE51FF004);
        DIANA_TEST_ASSERT(memory.Word(moved + 12) == (DI_UINT32)target);
    }
    /* a pc-relative load is refused and the allocation is freed */
    {
        unsigned char code[] = { 0x00, 0x00, 0x9F, 0xE5, 0,0,0,0 };    /* ldr r0, [pc, #0] */
        CArmTestMemory memory(0, 0, stub);
        SetAndPatchA32(memory, code, sizeof(code), DIANA_ARCH_ARM32, 0, DI_ERROR_NOT_IMPLEMENTED);
        DIANA_TEST_ASSERT(memory.m_frees == 1);
    }
}

/* T32: the hook is always a pc-relative load; moved bl and conditional branches */
static void test_patchers_t32()
{
    const OPERAND_SIZE stub = 0x180000;
    const OPERAND_SIZE moved = stub + c_t32StubSize;

    /* install + bl relocation */
    {
        unsigned char code[] = { 0x00, 0xF0, 0x1E, 0xF8,   /* bl codeBase+0x40 */
                                 0x63, 0x20,               /* movs r0, #99 */
                                 0x70, 0x47 };             /* bx lr */
        OPERAND_SIZE target;
        CArmTestMemory memory(0, 0, stub);
        target = DecodedTarget(code, c_codeBase, DIANA_ARCH_THUMB);
        SetAndPatchA32(memory, code, sizeof(code), DIANA_ARCH_THUMB, 0, DI_SUCCESS);
        DIANA_TEST_ASSERT(memory.Half(c_codeBase) == 0xF8DF && memory.Half(c_codeBase + 2) == 0xF000);
        DIANA_TEST_ASSERT(memory.Word(c_codeBase + 4) == ((DI_UINT32)stub | 1));
        ExpectLiteralsAndOriginal(memory, stub, c_t32StubSize, 1);
        DIANA_TEST_ASSERT(memory.Half(moved) == 0xF8DF && memory.Half(moved + 2) == 0xE008);    /* ldr.w lr,[pc,#8] */
        DIANA_TEST_ASSERT(memory.Half(moved + 4) == 0xF8DF && memory.Half(moved + 6) == 0xF000);/* ldr.w pc,[pc,#0] */
        DIANA_TEST_ASSERT(memory.Word(moved + 8) == (DI_UINT32)target);
        DIANA_TEST_ASSERT(memory.Word(moved + 12) == ((DI_UINT32)(moved + 16) | 1));            /* return address */
    }
    /* beq.w relocation: inverted narrow branch over a jump to the target */
    {
        unsigned char code[] = { 0x00, 0x28,               /* cmp r0, #0 */
                                 0x00, 0xF0, 0x1E, 0x80,   /* beq.w codeBase+0x42 */
                                 0x63, 0x20 };             /* movs r0, #99 */
        OPERAND_SIZE target;
        CArmTestMemory memory(0, 0, stub);
        target = DecodedTarget(code + 2, c_codeBase + 2, DIANA_ARCH_THUMB);
        SetAndPatchA32(memory, code, sizeof(code), DIANA_ARCH_THUMB, 0, DI_SUCCESS);
        DIANA_TEST_ASSERT(memory.Half(moved) == 0x2800);                /* cmp copied */
        DIANA_TEST_ASSERT(memory.Half(moved + 2) == 0xBF00);           /* align */
        DIANA_TEST_ASSERT(memory.Half(moved + 4) == 0xD104);           /* bne.n #+8 */
        DIANA_TEST_ASSERT(memory.Half(moved + 8) == 0xF8DF && memory.Half(moved + 10) == 0xF000);
        DIANA_TEST_ASSERT(memory.Word(moved + 12) == (DI_UINT32)target);
    }
    /* cbz relocation: cbnz over a jump to the target */
    {
        unsigned char code[] = { 0x70, 0xB1,               /* cbz r0, codeBase+0x20 */
                                 0x00, 0xBF,               /* nop */
                                 0x63, 0x20,               /* movs r0, #99 */
                                 0x70, 0x47 };             /* bx lr */
        OPERAND_SIZE target;
        CArmTestMemory memory(0, 0, stub);
        target = DecodedTarget(code, c_codeBase, DIANA_ARCH_THUMB);
        SetAndPatchA32(memory, code, sizeof(code), DIANA_ARCH_THUMB, 0, DI_SUCCESS);
        DIANA_TEST_ASSERT(memory.Half(moved) == 0xB920);               /* cbnz r0, #+8 */
        DIANA_TEST_ASSERT(memory.Half(moved + 2) == 0xBF00);
        DIANA_TEST_ASSERT(memory.Half(moved + 4) == 0xF8DF && memory.Half(moved + 6) == 0xF000);
        DIANA_TEST_ASSERT(memory.Word(moved + 8) == (DI_UINT32)target);
    }
    /* an uncond branch alone is too short for the 8-byte Thumb hook (not followed here) */
    {
        unsigned char code[] = { 0x00, 0xE0, 0,0,0,0,0,0 };            /* b.n */
        CArmTestMemory memory(0, 0, stub);
        SetAndPatchA32(memory, code, sizeof(code), DIANA_ARCH_THUMB, DIANA_HOOK_CUSTOM_OPTION_DONT_FOLLOW_JUMPS, DI_END);
    }
    /* a pc-relative load is refused */
    {
        unsigned char code[] = { 0x00, 0x48, 0,0,0,0,0,0 };            /* ldr r0, [pc, #0] */
        CArmTestMemory memory(0, 0, stub);
        SetAndPatchA32(memory, code, sizeof(code), DIANA_ARCH_THUMB, 0, DI_ERROR_NOT_IMPLEMENTED);
    }
}

void test_patchers_arm()
{
    DIANA_TEST(test_patchers_a64_near());
    DIANA_TEST(test_patchers_a64_far());
    DIANA_TEST(test_patchers_a64_far_option());
    DIANA_TEST(test_patchers_a64_moved());
    DIANA_TEST(test_patchers_a64_adrp_far());
    DIANA_TEST(test_patchers_a64_follow());
    DIANA_TEST(test_patchers_a64_invalid());
    DIANA_TEST(test_patchers_a32());
    DIANA_TEST(test_patchers_t32());
}

#endif
