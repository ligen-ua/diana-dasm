extern "C"
{
#include "diana_patchers/diana_ultimate_patcher.h"
#include "diana_streams.h"
#include "diana_arm.h"
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
        int Patch(int flags = 0)
        {
            DianaHook_CustomOptions options;
            memset(&options, 0, sizeof(options));
            options.flags = flags | DIANA_HOOK_CUSTOM_OPTION_SAVE_ORIGINAL_FP;
            options.originalFunctionPointer = c_originalSlot;
            options.patchContext = c_context;
            return DianaHook_PatchStreamEx(&m_provider, DIANA_MODE64, DIANA_ARCH_AARCH64, c_codeBase, c_hook, &options);
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

void test_patchers_arm()
{
    DIANA_TEST(test_patchers_a64_near());
    DIANA_TEST(test_patchers_a64_far());
    DIANA_TEST(test_patchers_a64_far_option());
    DIANA_TEST(test_patchers_a64_moved());
    DIANA_TEST(test_patchers_a64_adrp_far());
    DIANA_TEST(test_patchers_a64_follow());
    DIANA_TEST(test_patchers_a64_invalid());
}

#endif
