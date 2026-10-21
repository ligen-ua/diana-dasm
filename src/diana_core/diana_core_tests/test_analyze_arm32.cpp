// the code analyzer on AArch32 (A32 and T32): the same API and tables as test_analyze.cpp (x86).
// The bytes come from arm-linux-gnueabihf-as, linked at 0x10000: the literals hold absolute addresses
#include "test_analyze_arm32.h"
#include "test_analyze_common.h"
extern "C"
{
#include "diana_arm32.h"
}
#include <vector>

static const OPERAND_SIZE g_base = 0x10000;

static std::vector<unsigned char> Words(const DI_UINT32 * pWords, size_t count)
{
    std::vector<unsigned char> code;
    for (size_t i = 0; i < count; ++i)
    {
        for (int b = 0; b < 4; ++b)
            code.push_back((unsigned char)(pWords[i] >> (8 * b)));
    }
    return code;
}

static DianaAnalyzeParams Params(int arch)
{
    DianaAnalyzeParams params;
    DianaAnalyzeParams_Init(&params, arch);
    params.imageBase = g_base;
    return params;
}

// data the analyzer found: a literal or a table, never code, also not for the scan
static bool IsData(AnalyzedCode & analyzed, OPERAND_SIZE offset)
{
    Diana_Instruction * pInstruction = analyzed.owner.m_ppPresenceVec[offset];
    return pInstruction && (pInstruction->m_flags & DI_INSTRUCTION_INVALID) && (pInstruction->m_flags & DI_INSTRUCTION_DATA);
}

static bool IsThumb(Diana_Instruction * pInstruction)
{
    return pInstruction && (pInstruction->m_flags & DI_INSTRUCTION_THUMB);
}

static bool IsRet(Diana_Instruction * pInstruction)
{
    return pInstruction && (pInstruction->m_flags & DI_INSTRUCTION_IS_RET);
}

// A32: the tables of a switch (ldrls pc and addls pc), a veneer (ldr pc, =target), PIC (add r3, pc, r3),
// returns that are conditional or not, blx to T32 code, a function only the scan finds
static const DI_UINT32 g_armCode[] = {
    0xe92d4010,     // f0: push {r4, lr}                :0x00
    0xe3500002,     // cmp r0, #2                       :0x04
    0x979ff100,     // ldrls pc, [pc, r0, lsl #2]       :0x08
    0xea00000a,     // b def                            :0x0c
    0x0001001c,     // .word c0                         :0x10
    0x00010024,     // .word c1                         :0x14
    0x0001002c,     // .word c2                         :0x18
    0xe3a00001,     // c0: mov r0, #1                   :0x1c
    0xea00000d,     // b done                           :0x20
    0xeb00000d,     // c1: bl f1                        :0x24
    0xea00000b,     // b done                           :0x28
    0xe59f3048,     // c2: ldr r3, [pc, #72] (0x7c)     :0x2c
    0xe08f3003,     // add r3, pc, r3                   :0x30
    0xe12fff33,     // blx r3                           :0x34
    0xea000007,     // b done                           :0x38
    0xe3510001,     // def: cmp r1, #1                  :0x3c
    0x908ff101,     // addls pc, pc, r1, lsl #2         :0x40
    0xea000004,     // b done                           :0x44
    0xea000000,     // b sw0                            :0x48
    0xea000001,     // b sw1                            :0x4c
    0xe3a00003,     // sw0: mov r0, #3                  :0x50
    0xea000000,     // b done                           :0x54
    0xe59ff028,     // sw1: ldr pc, [pc, #40] (0x88)    :0x58
    0xe8bd8010,     // done: pop {r4, pc}               :0x5c
    0xe3500000,     // f1: cmp r0, #0                   :0x60
    0x01a0f00e,     // moveq pc, lr                     :0x64
    0xe2800001,     // add r0, r0, #1                   :0x68
    0xe12fff1e,     // bx lr                            :0x6c
    0xe3a00007,     // veneer_target: mov r0, #7        :0x70
    0xfa000004,     // blx th                           :0x74
    0xe12fff1e,     // bx lr                            :0x78
    0x00000048,     // .word pic_target - (.Lp + 8)     :0x7c
    0xe3a00009,     // pic_target: mov r0, #9           :0x80
    0xe12fff1e,     // bx lr                            :0x84
    0x00010070,     // .word veneer_target              :0x88
    0x47702001,     // th: movs r0, #1; bx lr (T32)     :0x8c
    0xe52de004,     // unref: push {lr}                 :0x90
    0xebfffff1,     // bl f1                            :0x94
    0xe49df004,     // pop {pc}                         :0x98
    0x00000000,     // .word 0                          :0x9c
};

static void test_analyzer_arm32_flow()
{
    std::vector<unsigned char> code = Words(g_armCode, sizeof(g_armCode) / sizeof(g_armCode[0]));
    DianaAnalyzeParams params = Params(DIANA_ARCH_ARM32);
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE32, &params);

    // ldrls pc, [pc, r0, lsl #2] after cmp r0, #2: three addresses, the table is data
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x08), 0x1c, 0) != 0);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x08), 0x24, 0) != 0);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x08), 0x2c, 0) != 0);
    DIANA_TEST_ASSERT(IsData(analyzed, 0x10) && IsData(analyzed, 0x14) && IsData(analyzed, 0x18));
    // ldrls is conditional: the next instruction is code too
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x0c), 0x3c, 0) != 0);

    // addls pc, pc, r1, lsl #2 after cmp r1, #1: two branches after it
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x40), 0x48, 0) != 0);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x40), 0x4c, 0) != 0);
    DIANA_TEST_ASSERT(analyzed.At(0x50) && analyzed.At(0x58));

    // ldr pc, =veneer_target: the literal is data, its value the target
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x58), 0x70, 0) != 0);
    DIANA_TEST_ASSERT(IsData(analyzed, 0x88));

    // ldr r3, [pc, #72]; add r3, pc, r3; blx r3: the offset plus PC
    DIANA_TEST_ASSERT(IsData(analyzed, 0x7c));
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x34), 0x80, 0) != 0);
    DIANA_TEST_ASSERT(analyzed.At(0x84) && IsRet(analyzed.At(0x84)));

    // returns: pop {r4, pc}, bx lr; moveq pc, lr goes on
    DIANA_TEST_ASSERT(IsRet(analyzed.At(0x5c)) && IsRet(analyzed.At(0x6c)));
    DIANA_TEST_ASSERT(analyzed.At(0x64) && !IsRet(analyzed.At(0x64)));
    DIANA_TEST_ASSERT(analyzed.At(0x68));
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x24), 0x60, 0) != 0);

    // blx th: T32 code
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x74), 0x8c, 0) != 0);
    DIANA_TEST_ASSERT(IsThumb(analyzed.At(0x8c)) && IsThumb(analyzed.At(0x8e)) && IsRet(analyzed.At(0x8e)));
    DIANA_TEST_ASSERT(!IsThumb(analyzed.At(0x1c)) && !IsThumb(analyzed.At(0x70)));

    // nothing refers to unref
    DIANA_TEST_ASSERT(!analyzed.At(0x90));

    // the scan finds it; the data stays data, zeros are no code
    analyzed.AnalyzeScanThrough(DIANA_MODE32, &params);
    DIANA_TEST_ASSERT(analyzed.At(0x90) && analyzed.At(0x94) && IsRet(analyzed.At(0x98)));
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x94), 0x60, 0) != 0);
    DIANA_TEST_ASSERT(!analyzed.At(0x9c));
    DIANA_TEST_ASSERT(IsData(analyzed, 0x10) && IsData(analyzed, 0x7c) && IsData(analyzed, 0x88));
    // T32 at 0x8c: no A32 instruction there or in its middle
    DIANA_TEST_ASSERT(IsThumb(analyzed.At(0x8c)));
}

// T32: tbb after bhi, an IT block, a literal with a T32 address (bit 0) for blx r3, movw/movt, blx to A32
static const unsigned char g_thumbCode[] = {
    0x10, 0xb5,                 // f0: push {r4, lr}                    :0x00
    0x00, 0xf0, 0x0e, 0xf8,     // bl f1                                :0x02
    0x02, 0x28,                 // cmp r0, #2                           :0x06
    0x09, 0xd8,                 // bhi def                              :0x08
    0xdf, 0xe8, 0x00, 0xf0,     // tbb [pc, r0]                         :0x0a
    0x02, 0x04, 0x06, 0x00,     // the table: (c0, c1, c2 - tbl) / 2    :0x0e
    0x01, 0x20,                 // c0: movs r0, #1                      :0x12
    0x04, 0xe0,                 // b done                               :0x14
    0x02, 0x20,                 // c1: movs r0, #2                      :0x16
    0x02, 0xe0,                 // b done                               :0x18
    0x0a, 0x4b,                 // c2: ldr r3, [pc, #40] (0x44)         :0x1a
    0x98, 0x47,                 // blx r3                               :0x1c
    0x00, 0x20,                 // def: movs r0, #0                     :0x1e
    0x10, 0xbd,                 // done: pop {r4, pc}                   :0x20
    0x00, 0x28,                 // f1: cmp r0, #0                       :0x22
    0x08, 0xbf,                 // it eq                                :0x24
    0x70, 0x47,                 // bxeq lr                              :0x26
    0x01, 0x30,                 // adds r0, #1                          :0x28
    0x70, 0x47,                 // bx lr                                :0x2a
    0x40, 0xf2, 0x00, 0x00,     // f2: movw r0, #0                      :0x2c
    0xc0, 0xf2, 0x01, 0x00,     // movt r0, #1                          :0x30
    0x41, 0x68,                 // ldr r1, [r0, #4]                     :0x34
    0x00, 0xf0, 0x02, 0xe8,     // blx f3                               :0x36
    0x70, 0x47,                 // bx lr                                :0x3a
    0x05, 0x00, 0xa0, 0xe3,     // f3: mov r0, #5 (A32)                 :0x3c
    0x1e, 0xff, 0x2f, 0xe1,     // bx lr                                :0x40
    0x2d, 0x00, 0x01, 0x00,     // .word f2 + 1                         :0x44
};

static void test_analyzer_thumb_flow()
{
    std::vector<unsigned char> code(g_thumbCode, g_thumbCode + sizeof(g_thumbCode));
    DianaAnalyzeParams params = Params(DIANA_ARCH_THUMB);
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE32, &params);

    DIANA_TEST_ASSERT(IsThumb(analyzed.At(0x00)) && IsThumb(analyzed.At(0x02)));
    // cmp r0, #2; bhi def: tbb has three entries, the table is data
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x08), 0x1e, 0) != 0);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x0a), 0x12, 0) != 0);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x0a), 0x16, 0) != 0);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x0a), 0x1a, 0) != 0);
    DIANA_TEST_ASSERT(IsData(analyzed, 0x0e) && IsData(analyzed, 0x10));
    // of byte entries; the literal below is a word
    DIANA_TEST_ASSERT(analyzed.owner.m_ppPresenceVec[0x0e]->m_flags & DI_INSTRUCTION_DATA_BYTES);
    DIANA_TEST_ASSERT(!(analyzed.owner.m_ppPresenceVec[0x44]->m_flags & (DI_INSTRUCTION_DATA_BYTES | DI_INSTRUCTION_DATA_HALFWORDS)));

    // ldr r3, =f2 + 1; blx r3: T32 code at f2, the literal is data
    DIANA_TEST_ASSERT(IsData(analyzed, 0x44) && IsData(analyzed, 0x46));
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x1c), 0x2c, 0) != 0);
    DIANA_TEST_ASSERT(IsThumb(analyzed.At(0x2c)));

    // it eq; bxeq lr goes on, bx lr returns
    DIANA_TEST_ASSERT(analyzed.At(0x26) && !IsRet(analyzed.At(0x26)));
    DIANA_TEST_ASSERT(analyzed.At(0x28) && IsRet(analyzed.At(0x2a)));

    // movw/movt r0 = 0x10000: an even address in T32 code is data; ldr r1, [r0, #4] reads 0x10004
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x30), 0x00, DI_XREF_DATA | DI_XREF_ADDRESS_TAKEN) != 0);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x34), 0x04, DI_XREF_DATA) != 0);

    // blx f3: A32 code, counted from the address rounded down to 4
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x36), 0x3c, 0) != 0);
    DIANA_TEST_ASSERT(analyzed.At(0x3c) && !IsThumb(analyzed.At(0x3c)) && IsRet(analyzed.At(0x40)));

    // the scan starts nothing inside an instruction: bl, tbb, movw (T32), mov (A32)
    analyzed.AnalyzeScanThrough(DIANA_MODE32, &params);
    DIANA_TEST_ASSERT(!analyzed.owner.m_ppPresenceVec[0x04] && !analyzed.owner.m_ppPresenceVec[0x0c]);
    DIANA_TEST_ASSERT(!analyzed.owner.m_ppPresenceVec[0x2e] && !analyzed.owner.m_ppPresenceVec[0x3e]);
    DIANA_TEST_ASSERT(!analyzed.owner.m_ppPresenceVec[0x42]);
    DIANA_TEST_ASSERT(IsData(analyzed, 0x0e) && IsData(analyzed, 0x44));
}

// a route that has to prove it is code doesn't take zeros for code: andeq r0, r0, r0 (A32),
// movs r0, r0 (T32); a root route does
static void test_analyzer_arm32_strict_zeros()
{
    std::vector<unsigned char> code(8, 0);
    DianaAnalyzeParams arm = Params(DIANA_ARCH_ARM32);
    DianaAnalyzeParams thumb = Params(DIANA_ARCH_THUMB);
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE32, &arm, DI_ANALYSE_BREAK_ON_INVALID_CMD);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 0);
    }
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE32, &thumb, DI_ANALYSE_BREAK_ON_INVALID_CMD);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 0);
    }
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE32, &arm);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 2);
    }
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE32, &thumb);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 4);
    }
}

// routes start at aligned addresses: 4 for A32, 2 for T32
static void test_analyzer_arm32_alignment()
{
    static const DI_UINT32 words[] = {
        0xe12fff1e,     // bx lr
        0xe12fff1e,     // bx lr
    };
    std::vector<unsigned char> code = Words(words, 2);
    DianaAnalyzeParams arm = Params(DIANA_ARCH_ARM32);
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE32, &arm, 0, 2);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 0);
    }
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE32, &arm, 0, 4);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 1 && IsRet(analyzed.At(4)));
    }
    // 0x4770 0x4770: bx lr twice in T32
    unsigned char thumbCode[] = {0x70, 0x47, 0x70, 0x47};
    DianaAnalyzeParams thumb = Params(DIANA_ARCH_THUMB);
    {
        AnalyzedCode analyzed(thumbCode, sizeof(thumbCode), DIANA_MODE32, &thumb, 0, 1);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 0);
    }
    {
        AnalyzedCode analyzed(thumbCode, sizeof(thumbCode), DIANA_MODE32, &thumb, 0, 2);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 1 && IsRet(analyzed.At(2)));
    }
}

// Diana_AnalyzeJumps, what the disassembly follows: T32 blx counts from the address rounded down to 4,
// tbb has no target of its own
static void test_analyzer_arm32_jumps()
{
    DianaContext context;
    DianaParserResult result;
    OPERAND_SIZE target = 0;
    int bAbsolute = 0;
    int bLinksToData = 0;
    Diana_InitContextEx(&context, DIANA_MODE32, DIANA_ARCH_THUMB);

    // blx f3 at 0x10036 (g_thumbCode): 0x1003c
    DIANA_TEST_ASSERT(DianaA32_DecodeT32(&context, 0xf000, 0xe802, &result) == DI_SUCCESS);
    DIANA_TEST_ASSERT(Diana_AnalyzeJumps(&result, 0x1003a, &target, &bAbsolute, &bLinksToData) == DI_SUCCESS);
    DIANA_TEST_ASSERT(target == 0x1003c);
    // bl f1 at 0x10002: 0x10022, nothing rounded
    DIANA_TEST_ASSERT(DianaA32_DecodeT32(&context, 0xf000, 0xf80e, &result) == DI_SUCCESS);
    DIANA_TEST_ASSERT(Diana_AnalyzeJumps(&result, 0x10006, &target, &bAbsolute, &bLinksToData) == DI_SUCCESS);
    DIANA_TEST_ASSERT(target == 0x10022);
    // tbb [pc, r0]
    DIANA_TEST_ASSERT(DianaA32_DecodeT32(&context, 0xe8df, 0xf000, &result) == DI_SUCCESS);
    DIANA_TEST_ASSERT(Diana_AnalyzeJumps(&result, 0x1000e, &target, &bAbsolute, &bLinksToData) == DI_NOT_FOUND);
}

void test_analyze_arm32()
{
    DIANA_TEST(test_analyzer_arm32_flow());
    DIANA_TEST(test_analyzer_thumb_flow());
    DIANA_TEST(test_analyzer_arm32_strict_zeros());
    DIANA_TEST(test_analyzer_arm32_alignment());
    DIANA_TEST(test_analyzer_arm32_jumps());
}
