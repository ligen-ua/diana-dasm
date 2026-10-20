// the code analyzer on AArch64: the same API and tables as test_analyze.cpp (x86).
// The words come from aarch64-linux-gnu-as; CMPBR words (no assembler support yet) are hand-encoded
#include "test_analyze_arm64.h"
#include "test_analyze_common.h"
#include <vector>

static std::vector<unsigned char> Code(std::initializer_list<DI_UINT32> words)
{
    std::vector<unsigned char> code;
    for (DI_UINT32 word : words)
    {
        code.push_back((unsigned char)word);
        code.push_back((unsigned char)(word >> 8));
        code.push_back((unsigned char)(word >> 16));
        code.push_back((unsigned char)(word >> 24));
    }
    return code;
}

static DianaAnalyzeParams Arm64(const char * pProfile = 0)
{
    DianaAnalyzeParams params;
    DianaAnalyzeParams_Init(&params, DIANA_ARCH_AARCH64);
    if (pProfile)
        params.pArmProfile = DianaArm_FindProfile(pProfile);
    return params;
}

static void VerifyInstructions(AnalyzedCode & analyzed, const IntructionInfo * pInstructions, int count)
{
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == count);
    for (int i = 0; i < count; ++i)
    {
        const IntructionInfo * pInfo = pInstructions + i;
        Diana_Instruction * pInstruction = analyzed.At(pInfo->offset);
        DIANA_TEST_ASSERT(pInstruction);
        if (!pInstruction)
            continue;
        DIANA_TEST_ASSERT(pInstruction->m_offset == pInfo->offset);
        VerifyREF(pInfo->pXrefsFrom, &pInstruction->m_referencesToThisInstruction, 0);
        VerifyREF(pInfo->pXrefsTo, &pInstruction->m_referencesFromThisInstruction, 1);
    }
}

// branches, calls, a call out of the buffer; the same checks as test_analyzer1
static void test_analyzer_arm64_flow()
{
    std::vector<unsigned char> code = Code({
        0xa9bf7bfd,     // stp x29, x30, [sp, #-16]!    :0x00
        0x94000009,     // bl f1                        :0x04
        0xb40000a0,     // cbz x0, l1                   :0x08
        0x371800a1,     // tbnz w1, #3, l2              :0x0c
        0x54000061,     // b.ne l1                      :0x10
        0x94000400,     // bl 0x1014, out of the buffer :0x14
        0x14000003,     // b l3                         :0x18
        0xd2800020,     // l1: mov x0, #1               :0x1c
        0xa8c17bfd,     // l2: ldp x29, x30, [sp], #16  :0x20
        0xd65f03c0,     // l3: ret                      :0x24
        0x0b010000,     // f1: add w0, w0, w1           :0x28
        0xd65f03c0,     // ret                          :0x2c
    });
    IntructionInfo instructions[] =
                        {{0x00,   0,          0},
                         {0x04,   "40",       0},
                         {0x08,   "28",       0},
                         {0x0c,   "32",       0},
                         {0x10,   "28",       0},
                         {0x14,   "$4116",    0},
                         {0x18,   "36",       0},
                         {0x1c,   0,          "8;16"},
                         {0x20,   0,          "12"},
                         {0x24,   0,          "24"},
                         {0x28,   0,          "4"},
                         {0x2c,   0,          0}};
    DianaAnalyzeParams params = Arm64();
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    VerifyInstructions(analyzed, instructions, sizeof(instructions)/sizeof(instructions[0]));
    DIANA_TEST_ASSERT(analyzed.At(0x24)->m_flags & DI_INSTRUCTION_IS_RET);
    DIANA_TEST_ASSERT(analyzed.At(0x00)->m_flags & DI_INSTRUCTION_ROOT);
}

// pointer authentication, hinted and compare-and-branch: retaa ends a function like ret, braa is an unresolved jump.
// With the ARMv8.0 profile they are still analyzed, only flagged
static void test_analyzer_arm64_new_branches()
{
    std::vector<unsigned char> code = Code({
        0xd503233f,     // paciasp                      :0x00
        0x74010060,     // cbgt w0, w1, a1              :0x04
        0x540000b0,     // bc.eq a2                     :0x08
        0xd71f0a11,     // braa x16, x17                :0x0c
        0xd50323bf,     // a1: autiasp                  :0x10
        0xd65f0bff,     // retaa                        :0x14
        0x8b010000,     // add x0, x0, x1: not reached  :0x18
        0xd2800040,     // a2: mov x0, #2               :0x1c
        0xd65f0fff,     // retab                        :0x20
    });
    IntructionInfo instructions[] =
                        {{0x00,   0,          0},
                         {0x04,   "16",       0},
                         {0x08,   "28",       0},
                         {0x0c,   0,          0},
                         {0x10,   0,          "4"},
                         {0x14,   0,          0},
                         {0x1c,   0,          "8"},
                         {0x20,   0,          0}};
    DianaAnalyzeParams params = Arm64("armv8.0");
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    VerifyInstructions(analyzed, instructions, sizeof(instructions)/sizeof(instructions[0]));
    DIANA_TEST_ASSERT(!analyzed.At(0x18));
    DIANA_TEST_ASSERT(analyzed.At(0x14)->m_flags & DI_INSTRUCTION_IS_RET);
    DIANA_TEST_ASSERT(analyzed.At(0x04)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE);
    DIANA_TEST_ASSERT(analyzed.At(0x0c)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE);
    DIANA_TEST_ASSERT(analyzed.At(0x14)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE);
    DIANA_TEST_ASSERT(!(analyzed.At(0x1c)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE));
}

// M5: SVE and SME are instructions without control flow. On apple-m4 (SME, no SVE) streaming SVE is in the profile,
// SVE-only instructions are outside it, and neither ends a route
static void test_analyzer_arm64_sve_sme()
{
    std::vector<unsigned char> code = Code({
        0x0480184a,     // add z10.s, p6/m, z10.s, z2.s (SVE or SME)   :0x00
        0x65820c20,     // ftsmul z0.s, z1.s, z2.s (SVE only)          :0x04
        0xa5e0e000,     // ld1d {z0.d-z3.d}... ld4d (SVE or SME)       :0x08
        0x8085c081,     // fmopa za1.s, p0/m, p6/m, z4.s, z5.s (SME)   :0x0c
        0xc0480001,     // zero {zt0} (SME2)                           :0x10
        0xd65f03c0,     // ret                                         :0x14
        0x91000800,     // add x0, x0, #2: not reached                 :0x18
    });
    DianaAnalyzeParams params = Arm64("apple-m4");
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 6);
    for (int offset = 0; offset < 0x18; offset += 4)
    {
        DIANA_TEST_ASSERT(analyzed.At(offset) != 0);
        DIANA_TEST_ASSERT(analyzed.At(offset) && !(analyzed.At(offset)->m_flags & DI_INSTRUCTION_UNSUPPORTED));
    }
    DIANA_TEST_ASSERT(!(analyzed.At(0x00)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE));
    DIANA_TEST_ASSERT(analyzed.At(0x04)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE);
    DIANA_TEST_ASSERT(!(analyzed.At(0x0c)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE));
    DIANA_TEST_ASSERT(!(analyzed.At(0x10)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE));
    DIANA_TEST_ASSERT(!analyzed.At(0x18));
}

// unsupported but valid (SVE, CPA) and disabled by the profile (crc32b on ARMv8.0) never stop a function
static void test_analyzer_arm64_word_classes()
{
    std::vector<unsigned char> code = Code({
        0x91000400,     // add x0, x0, #1               :0x00
        0x04c40000,     // addpt z0.d, p0/m... (SVE CPA) :0x04
        0x9a002020,     // addpt x0, x1, x0 (CPA)       :0x08
        0x1ac24020,     // crc32b w0, w1, w2            :0x0c
        0xd65f03c0,     // ret                          :0x10
        0x91000800,     // add x0, x0, #2: not reached  :0x14
    });
    DianaAnalyzeParams params = Arm64("armv8.0");
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 5);
    DIANA_TEST_ASSERT(analyzed.At(0x04) && (analyzed.At(0x04)->m_flags & DI_INSTRUCTION_UNSUPPORTED));
    DIANA_TEST_ASSERT(analyzed.At(0x08) && (analyzed.At(0x08)->m_flags & DI_INSTRUCTION_UNSUPPORTED));
    DIANA_TEST_ASSERT(analyzed.At(0x0c) && (analyzed.At(0x0c)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE));
    DIANA_TEST_ASSERT(analyzed.At(0x0c) && !(analyzed.At(0x0c)->m_flags & DI_INSTRUCTION_UNSUPPORTED));
    DIANA_TEST_ASSERT(!(analyzed.At(0x00)->m_flags & (DI_INSTRUCTION_UNSUPPORTED|DI_INSTRUCTION_OUTSIDE_PROFILE)));
    DIANA_TEST_ASSERT(analyzed.At(0x10) && !analyzed.At(0x14));

    // without a profile nothing is outside it
    DianaAnalyzeParams all = Arm64();
    AnalyzedCode analyzedAll(&code[0], code.size(), DIANA_MODE64, &all);
    DIANA_TEST_ASSERT(analyzedAll.At(0x0c) && !(analyzedAll.At(0x0c)->m_flags & DI_INSTRUCTION_OUTSIDE_PROFILE));
}

// an unallocated word: a root route steps over it (4 bytes), with DI_ANALYSE_BREAK_ON_INVALID_CMD it is rolled back
static void test_analyzer_arm64_unallocated()
{
    std::vector<unsigned char> code = Code({
        0x91000400,     // add x0, x0, #1               :0x00
        0xffffffff,     // unallocated                  :0x04
        0x91000400,     // add x0, x0, #1               :0x08
        0xd65f03c0,     // ret                          :0x0c
    });
    DianaAnalyzeParams params = Arm64();
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 3);
        DIANA_TEST_ASSERT(analyzed.At(0x00) && !analyzed.At(0x04) && analyzed.At(0x08) && analyzed.At(0x0c));
    }
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params, DI_ANALYSE_BREAK_ON_INVALID_CMD);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 0);
    }
}

// brk/udf stop a route; adr takes an address (rolled back to a data xref when it is no code),
// ldr (literal) reads data: no code there even if it decodes
static void test_analyzer_arm64_references()
{
    std::vector<unsigned char> code = Code({
        0x100000e0,     // adr x0, d1                   :0x00
        0x580000e1,     // ldr x1, d2                   :0x04
        0x10000102,     // adr x2, d3                   :0x08
        0x94000800,     // bl 0x200c, out of the buffer :0x0c
        0xd4200020,     // brk #1                       :0x10
        0x8b010000,     // f2: add x0, x0, x1           :0x14
        0xd65f03c0,     // ret                          :0x18
        0xffffffff,     // d1: unallocated              :0x1c
        0x8b010000,     // d2: data that decodes        :0x20
        0xd65f03c0,     //                              :0x24
        0x00000000,     // d3: zeros, udf #0            :0x28
        0x00000000,     //                              :0x2c
    });
    IntructionInfo instructions[] =
                        {{0x00,   "28;#28",   0},
                         {0x04,   "#32",      0},
                         {0x08,   "40;#40",   0},
                         {0x0c,   "$8204",    0},
                         {0x10,   0,          0}};
    DianaAnalyzeParams params = Arm64();
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    VerifyInstructions(analyzed, instructions, sizeof(instructions)/sizeof(instructions[0]));
    DIANA_TEST_ASSERT(!analyzed.At(0x14) && !analyzed.At(0x1c) && !analyzed.At(0x20) && !analyzed.At(0x28));
    DIANA_TEST_ASSERT(analyzed.At(0x04)->m_flags & DI_INSTRUCTION_USES_RIP);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x00), 0x1c, DI_XREF_DATA|DI_XREF_ADDRESS_TAKEN) != 0);
    DIANA_TEST_ASSERT(analyzed.At(0x10)->m_flags & DI_INSTRUCTION_CAN_CHANGE_CSIP);
}

// adr to code: a questionable route that is kept; udf in a root route is an instruction that stops it
static void test_analyzer_arm64_adr_code()
{
    std::vector<unsigned char> code = Code({
        0x10000060,     // adr x0, f                    :0x00
        0x00000000,     // udf #0                       :0x04
        0x91000400,     // add x0, x0, #1: not reached  :0x08
        0x8b010000,     // f: add x0, x0, x1            :0x0c
        0xd65f03c0,     // ret                          :0x10
    });
    DianaAnalyzeParams params = Arm64();
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 4);
    DIANA_TEST_ASSERT(analyzed.At(0x04) && !analyzed.At(0x08) && analyzed.At(0x0c) && analyzed.At(0x10));
    Diana_XRef * pXRef = FindXRefFrom(analyzed.At(0x00), 0x0c, DI_XREF_ADDRESS_TAKEN);
    DIANA_TEST_ASSERT(pXRef && !(pXRef->m_flags & (DI_XREF_DATA|DI_XREF_INVALID)));
    DIANA_TEST_ASSERT(!(analyzed.At(0x0c)->m_flags & DI_INSTRUCTION_ROOT));
}

// instructions are 4 bytes at 4-aligned addresses
static void test_analyzer_arm64_alignment()
{
    std::vector<unsigned char> code = Code({
        0x50000020,     // adr x0, .+6: not an instruction address :0x00
        0xd65f03c0,     // ret                                     :0x04
    });
    DianaAnalyzeParams params = Arm64();
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 2);
        DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x00), 6, DI_XREF_DATA|DI_XREF_ADDRESS_TAKEN) != 0);
    }
    {
        // a seed in the middle of a word
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params, 0, 2);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 0);
    }
    {
        // offset 0 is at an address that is not 4-aligned
        DianaAnalyzeParams shifted = params;
        shifted.imageBase = 0x1002;
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &shifted);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 0);
    }
    {
        // the last word is cut
        std::vector<unsigned char> cut = Code({0x91000400, 0x91000400});
        cut.resize(6);
        AnalyzedCode analyzed(&cut[0], cut.size(), DIANA_MODE64, &params);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 1);
    }
}

// a zeroed buffer of the size with words put at their offsets
static std::vector<unsigned char> Buffer(size_t size, std::initializer_list<std::pair<size_t, DI_UINT32> > words)
{
    std::vector<unsigned char> code(size);
    for (const auto & item : words)
    {
        for (int i = 0; i < 4; ++i)
            code[item.first + i] = (unsigned char)(item.second >> (i * 8));
    }
    return code;
}

// the function every pointer below leads to
static const std::pair<size_t, DI_UINT32> g_function[] = {{0x40, 0x91000400}, {0x44, 0xd65f03c0}};   // add x0, x0, #1; ret

// adrp + add takes an address. ADRP pages come from the real address: the image base matters
static void test_analyzer_arm64_adrp_add()
{
    std::vector<unsigned char> code = Buffer(0x900, {
        {0x000, 0xb0000000},    // adrp x0, <page + 0x1000>
        {0x004, 0x91004000},    // add x0, x0, #0x10
        {0x008, 0xd65f03c0},    // ret
        {0x810, 0x91000400},    // add x0, x0, #1
        {0x814, 0xd65f03c0},    // ret
    });
    {
        // offset 0 is at 0x800: page 0, x0 = 0x1010, offset 0x810
        DianaAnalyzeParams params = Arm64();
        params.imageBase = 0x800;
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
        Diana_XRef * pXRef = FindXRefFrom(analyzed.At(0x004), 0x810, DI_XREF_ADDRESS_TAKEN);
        DIANA_TEST_ASSERT(pXRef && !(pXRef->m_flags & (DI_XREF_DATA|DI_XREF_INVALID)));
        DIANA_TEST_ASSERT(analyzed.At(0x810) && analyzed.At(0x814));
        DIANA_TEST_ASSERT(!FindXRefFrom(analyzed.At(0x000), 0x810, 0));     // adrp alone makes no xref
    }
    {
        // offset 0 is at 0: x0 = 0x1010, out of the buffer
        DianaAnalyzeParams params = Arm64();
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
        DIANA_TEST_ASSERT(!analyzed.At(0x810));
    }
    {
        // the base is not known: no ADRP references
        DianaAnalyzeParams params = Arm64();
        params.imageBase = 0x800;
        params.flags = DIANA_ANALYZE_IMAGE_BASE_UNKNOWN;
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
        DIANA_TEST_ASSERT(!analyzed.At(0x810));
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 3);
    }
}

// an import thunk: adrp x16; ldr x16, [x16, #off]; br x16 jumps to the pointer in the slot
static void test_analyzer_arm64_thunk()
{
    std::vector<unsigned char> code = Buffer(0x50, {
        {0x00, 0x90000010},     // adrp x16, 0
        {0x04, 0xf9401210},     // ldr x16, [x16, #0x20]
        {0x08, 0xd61f0200},     // br x16
        {0x20, 0x00010040},     // .quad 0x10040: the image is at 0x10000
        g_function[0], g_function[1],
    });
    DianaAnalyzeParams params = Arm64();
    params.imageBase = 0x10000;
    // ADRP values as addresses, and as offsets (PE/ELF analyzers); the slot holds an absolute address either way
    for (int flags : {0, DIANA_ANALYZE_ADDRESSES_RELATIVE})
    {
        params.flags = flags;
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
        // the slot is data, the value in it an address taken, the jump goes there
        Diana_XRef * pSlot = FindXRefFrom(analyzed.At(0x04), 0x20, DI_XREF_DATA);
        DIANA_TEST_ASSERT(pSlot && !(pSlot->m_flags & DI_XREF_ADDRESS_TAKEN));
        DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x04), 0x40, DI_XREF_ADDRESS_TAKEN) != 0);
        Diana_XRef * pJump = FindXRefFrom(analyzed.At(0x08), 0x40, 0);
        DIANA_TEST_ASSERT(pJump && !(pJump->m_flags & (DI_XREF_DATA|DI_XREF_ADDRESS_TAKEN|DI_XREF_INVALID)));
        DIANA_TEST_ASSERT(analyzed.At(0x40) && analyzed.At(0x44) && !analyzed.At(0x20));
    }

    // a slot pointing out of the module: an import
    params.flags = 0;
    code[0x21] = 0x50;      // .quad 0x15040
    AnalyzedCode external(&code[0], code.size(), DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(FindXRefFrom(external.At(0x08), 0x5040, DI_XREF_EXTERNAL) != 0);
}

// a register written with an unknown value is forgotten: by mov from an unknown register,
// and by any instruction the tracker doesn't follow
static void test_analyzer_arm64_tracking_lost()
{
    static const DI_UINT32 writers[] = {0xaa0003f0,     // mov x16, x0
                                        0x8b110210};    // add x16, x16, x17
    for (size_t i = 0; i < sizeof(writers)/sizeof(writers[0]); ++i)
    {
        std::vector<unsigned char> code = Buffer(0x50, {
            {0x00, 0x90000010},     // adrp x16, 0
            {0x04, 0xf9401210},     // ldr x16, [x16, #0x20]
            {0x08, writers[i]},
            {0x0c, 0xd61f0200},     // br x16: unknown
            {0x20, 0x00000040},     // .quad 0x40
            g_function[0], g_function[1],
        });
        DianaAnalyzeParams params = Arm64();
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
        DIANA_TEST_ASSERT(analyzed.At(0x0c));
        DIANA_TEST_ASSERT(Diana_FList_First(&analyzed.At(0x0c)->m_referencesFromThisInstruction) == 0);
    }
}

// a call may change x0..x18 and x30: the second blr x8 is unknown
static void test_analyzer_arm64_call_clobbers()
{
    std::vector<unsigned char> code = Buffer(0x50, {
        {0x00, 0x90000008},     // adrp x8, 0
        {0x04, 0xf9401108},     // ldr x8, [x8, #0x20]
        {0x08, 0xd63f0100},     // blr x8
        {0x0c, 0xd63f0100},     // blr x8
        {0x10, 0xd65f03c0},     // ret
        {0x20, 0x00000040},     // .quad 0x40
        g_function[0], g_function[1],
    });
    DianaAnalyzeParams params = Arm64();
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x08), 0x40, 0) != 0);
    DIANA_TEST_ASSERT(analyzed.At(0x0c));
    DIANA_TEST_ASSERT(Diana_FList_First(&analyzed.At(0x0c)->m_referencesFromThisInstruction) == 0);
}

// ldr x1, =f: the literal is data, the value in it an address taken
static void test_analyzer_arm64_literal_value()
{
    std::vector<unsigned char> code = Buffer(0x50, {
        {0x00, 0x58000081},     // ldr x1, 0x10
        {0x04, 0xd65f03c0},     // ret
        {0x10, 0x00000040},     // .quad 0x40
        g_function[0], g_function[1],
    });
    DianaAnalyzeParams params = Arm64();
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x00), 0x10, DI_XREF_DATA) != 0);
    Diana_XRef * pXRef = FindXRefFrom(analyzed.At(0x00), 0x40, DI_XREF_ADDRESS_TAKEN);
    DIANA_TEST_ASSERT(pXRef && !(pXRef->m_flags & DI_XREF_DATA));
    DIANA_TEST_ASSERT(analyzed.At(0x40) && !analyzed.At(0x10));
}

// switch jump tables, linked at 0 by aarch64-linux-gnu-ld. The br gets an xref to every case, the cases are code,
// the table is data
struct JumpTableCase
{
    const char * pName;
    std::vector<unsigned char> code;
    size_t seed, load, br, table;
    std::vector<size_t> cases;
};

// flags: DianaAnalyzeParams::flags, DIANA_ANALYZE_ADDRESSES_RELATIVE as PE/ELF analyzers use it
static void VerifyJumpTable(const JumpTableCase & test, int flags)
{
    std::vector<unsigned char> code = test.code;
    DianaAnalyzeParams params = Arm64();
    params.flags = flags;
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params, 0, test.seed);
    Diana_Instruction * pBr = analyzed.At(test.br);
    DIANA_TEST_ASSERT(pBr);
    for (size_t target : test.cases)
    {
        Diana_XRef * pXRef = FindXRefFrom(pBr, target, 0);
        if (!pXRef || (pXRef->m_flags & (DI_XREF_DATA|DI_XREF_INVALID)) || !analyzed.At(target))
            std::cout << test.pName << ": case " << target << " is not found\n";
        DIANA_TEST_ASSERT(pXRef && !(pXRef->m_flags & (DI_XREF_DATA|DI_XREF_INVALID)));
        DIANA_TEST_ASSERT(analyzed.At(target));
    }
    DIANA_TEST_ASSERT(!analyzed.At(test.table));
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(test.load), test.table, DI_XREF_DATA) != 0);
}

static void test_analyzer_arm64_jump_tables()
{
    const JumpTableCase tests[] = {
        // gcc: cmp w0, #3; b.ls 1f; ...; 1: adrp/add x2, tbl; ldrb w2, [x2, w0, uxtw]; adr x0, base;
        // add x2, x0, w2, sxtb #2; br x2. The last entry is negative: the case before the function
        {"gcc, signed bytes", {
            0x20, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x1f, 0x0c, 0x00, 0x71, 0x69, 0x00, 0x00, 0x54,
            0x00, 0x00, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x02, 0x00, 0x00, 0x90, 0x42, 0x20, 0x01, 0x91,
            0x42, 0x48, 0x60, 0x38, 0x60, 0x00, 0x00, 0x10, 0x02, 0x88, 0x22, 0x8b, 0x40, 0x00, 0x1f, 0xd6,
            0x40, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x60, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6,
            0x80, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x00, 0x02, 0x04, 0xf4},
         0x08, 0x20, 0x2c, 0x48, {0x30, 0x38, 0x40, 0x00}},
        // mov w8, w0; cmp w8, #2; b.hi def; adrp/add x9, tbl; adr x10, base; ldrh w11, [x9, x8, lsl #1];
        // add x10, x10, x11, lsl #2; br x10
        {"unsigned halfwords", {
            0xe8, 0x03, 0x00, 0x2a, 0x1f, 0x09, 0x00, 0x71, 0xe8, 0x00, 0x00, 0x54, 0x09, 0x00, 0x00, 0x90,
            0x29, 0x11, 0x01, 0x91, 0xca, 0x00, 0x00, 0x10, 0x2b, 0x79, 0x68, 0x78, 0x4a, 0x09, 0x0b, 0x8b,
            0x40, 0x01, 0x1f, 0xd6, 0x00, 0x00, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x40, 0x01, 0x80, 0x52,
            0xc0, 0x03, 0x5f, 0xd6, 0x60, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x80, 0x01, 0x80, 0x52,
            0xc0, 0x03, 0x5f, 0xd6, 0x00, 0x00, 0x02, 0x00, 0x04, 0x00},
         0x00, 0x18, 0x20, 0x44, {0x2c, 0x34, 0x3c}},
        // cmp w0, #2; b.hi def; adrp/add x9, tbl; ldrsw x10, [x9, w0, uxtw #2]; add x10, x9, x10; br x10
        {"table-relative words", {
            0x1f, 0x08, 0x00, 0x71, 0xc8, 0x00, 0x00, 0x54, 0x09, 0x00, 0x00, 0x90, 0x29, 0xf1, 0x00, 0x91,
            0x2a, 0x59, 0xa0, 0xb8, 0x2a, 0x01, 0x0a, 0x8b, 0x40, 0x01, 0x1f, 0xd6, 0x00, 0x00, 0x80, 0x52,
            0xc0, 0x03, 0x5f, 0xd6, 0x40, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x60, 0x01, 0x80, 0x52,
            0xc0, 0x03, 0x5f, 0xd6, 0x80, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0xe8, 0xff, 0xff, 0xff,
            0xf0, 0xff, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff},
         0x00, 0x10, 0x18, 0x3c, {0x24, 0x2c, 0x34}},
        // cmp w0, #2; b.hi def; adrp/add x9, tbl; ldr x10, [x9, w0, uxtw #3]; br x10: absolute addresses
        {"absolute", {
            0x1f, 0x08, 0x00, 0x71, 0xa8, 0x00, 0x00, 0x54, 0x09, 0x00, 0x00, 0x90, 0x29, 0xe1, 0x00, 0x91,
            0x2a, 0x59, 0x60, 0xf8, 0x40, 0x01, 0x1f, 0xd6, 0x00, 0x00, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6,
            0x40, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x60, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6,
            0x80, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
         0x00, 0x10, 0x14, 0x38, {0x20, 0x28, 0x30}},
        // ldr w0, [x1, w0, uxtw #2] overwrites the bounded index (unoptimized libstdc++); adr x1, base;
        // add x0, x1, w0, sxtw #2
        {"the index overwritten", {
            0x1f, 0x08, 0x00, 0x71, 0xe8, 0x00, 0x00, 0x54, 0x01, 0x00, 0x00, 0x90, 0x21, 0x00, 0x01, 0x91,
            0x20, 0x58, 0x60, 0xb8, 0xa1, 0x00, 0x00, 0x10, 0x20, 0xc8, 0x20, 0x8b, 0x00, 0x00, 0x1f, 0xd6,
            0x00, 0x00, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x40, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6,
            0x60, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x80, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6,
            0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00},
         0x00, 0x10, 0x1c, 0x40, {0x28, 0x30, 0x38}},
        // memset of a Windows driver: a signed bound (cmp x2, #2; b.gt), the table inline after br, adr x8, tbl;
        // ldrb w9, [x8, x2]; add x8, x8, x9, lsl #2 (relative to the table)
        {"signed bound, inline table", {
            0x5f, 0x08, 0x00, 0xf1, 0xcc, 0x00, 0x00, 0x54, 0x88, 0x00, 0x00, 0x10, 0x09, 0x69, 0x62, 0x38,
            0x08, 0x09, 0x09, 0x8b, 0x00, 0x01, 0x1f, 0xd6, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, 0x80, 0x52,
            0xc0, 0x03, 0x5f, 0xd6, 0x40, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x60, 0x01, 0x80, 0x52,
            0xc0, 0x03, 0x5f, 0xd6, 0x80, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6},
         0x00, 0x0c, 0x14, 0x18, {0x24, 0x2c, 0x34}},
        // MSVC: cmp w19, #2; mov x21, x3; mov x20, x4; b.hi def: the compare holds until the flags change
        {"instructions between cmp and b.hi", {
            0x7f, 0x0a, 0x00, 0x71, 0xf5, 0x03, 0x03, 0xaa, 0xf4, 0x03, 0x04, 0xaa, 0xc8, 0x00, 0x00, 0x54,
            0xaa, 0x01, 0x00, 0x10, 0x49, 0x59, 0xb3, 0xb8, 0xaa, 0x00, 0x00, 0x10, 0x49, 0x09, 0x09, 0x8b,
            0x20, 0x01, 0x1f, 0xd6, 0x00, 0x00, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x40, 0x01, 0x80, 0x52,
            0xc0, 0x03, 0x5f, 0xd6, 0x60, 0x01, 0x80, 0x52, 0xc0, 0x03, 0x5f, 0xd6, 0x80, 0x01, 0x80, 0x52,
            0xc0, 0x03, 0x5f, 0xd6, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00},
         0x00, 0x14, 0x20, 0x44, {0x2c, 0x34, 0x3c}},
    };
    for (int flags : {0, DIANA_ANALYZE_ADDRESSES_RELATIVE})
    {
        for (const JumpTableCase & test : tests)
            VerifyJumpTable(test, flags);
    }

    // the table decodes (cbgt) and a ret follows it: only the load from it says it is data
    JumpTableCase followed = tests[0];
    followed.pName = "gcc, a ret after the table";
    const unsigned char ret[] = {0xc0, 0x03, 0x5f, 0xd6};
    followed.code.insert(followed.code.end(), ret, ret + 4);
    VerifyJumpTable(followed, 0);

    // without the bound nothing is guessed: cmp w0, #2 becomes nop
    std::vector<unsigned char> code = tests[2].code;
    code[0] = 0x1f; code[1] = 0x20; code[2] = 0x03; code[3] = 0xd5;
    DianaAnalyzeParams params = Arm64();
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(analyzed.At(0x18));
    DIANA_TEST_ASSERT(Diana_FList_First(&analyzed.At(0x18)->m_referencesFromThisInstruction) == 0);
    DIANA_TEST_ASSERT(!analyzed.At(0x24) && !analyzed.At(0x2c) && !analyzed.At(0x34));
}

// a route that has to prove it is code (DI_ANALYSE_BREAK_ON_INVALID_CMD: a pointer found in data) doesn't take
// zeros (udf #0) for code; a root route does: udf ends it
static void test_analyzer_arm64_strict_udf()
{
    std::vector<unsigned char> code = Code({0x00000000, 0x00000000});
    DianaAnalyzeParams params = Arm64();
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params, DI_ANALYSE_BREAK_ON_INVALID_CMD);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 0);
    }
    {
        AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
        DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 1);
    }
}

// a questionable route that reaches the end of the buffer without a terminator is no code
static void test_analyzer_arm64_route_to_the_end()
{
    std::vector<unsigned char> code = Code({
        0x10000040,     // adr x0, d                    :0x00
        0xd65f03c0,     // ret                          :0x04
        0x91000400,     // d: add x0, x0, #1            :0x08
        0x91000400,     // add x0, x0, #1, then the end :0x0c
    });
    DianaAnalyzeParams params = Arm64();
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 2);
    DIANA_TEST_ASSERT(!analyzed.At(0x08) && !analyzed.At(0x0c));
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x00), 0x08, DI_XREF_DATA|DI_XREF_ADDRESS_TAKEN) != 0);
}

// BTI landing pads that no route reaches start strict routes: the pad after an unconditional branch (a C++ exception
// landing pad) is found, a bti in front of data is rolled back, a bti already reached is left alone
static void test_analyzer_arm64_landing_pads()
{
    std::vector<unsigned char> code = Code({
        0xd503233f,     // f: paciasp                   :0x00
        0x94000009,     // bl g                         :0x04
        0x14000006,     // b done                       :0x08
        0xd503249f,     // pad: bti j                   :0x0c
        0xd2800020,     // mov x0, #1                   :0x10
        0x14000003,     // b done                       :0x14
        0xd503245f,     // bti c                        :0x18
        0xffffffff,     // not an instruction           :0x1c
        0xd50323bf,     // done: autiasp                :0x20
        0xd65f03c0,     // ret                          :0x24
        0xd503245f,     // g: bti c                     :0x28
        0xd65f03c0,     // ret                          :0x2c
    });
    DianaAnalyzeParams params = Arm64();
    AnalyzedCode analyzed(&code[0], code.size(), DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 7);
    DIANA_TEST_ASSERT(!analyzed.At(0x0c) && !analyzed.At(0x18));

    analyzed.AnalyzeLandingPads(DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 10);
    DIANA_TEST_ASSERT(analyzed.At(0x0c) && analyzed.At(0x10) && analyzed.At(0x14));
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0x14), 0x20, 0) != 0);
    DIANA_TEST_ASSERT(!analyzed.At(0x18) && !analyzed.At(0x1c));
    DIANA_TEST_ASSERT(analyzed.At(0x28) && (analyzed.At(0x28)->m_flags & DI_INSTRUCTION_ROOT));

    // x86: nothing to do
    {
        AnalyzedCode x86(&code[0], code.size(), DIANA_MODE64);
        OPERAND_SIZE before = x86.owner.m_actualSize;
        DianaAnalyzeParams x86Params;
        DianaAnalyzeParams_Init(&x86Params, DIANA_ARCH_X86);
        x86.AnalyzeLandingPads(DIANA_MODE64, &x86Params);
        DIANA_TEST_ASSERT(x86.owner.m_actualSize == before);
    }
}

void test_analyze_arm64()
{
    DIANA_TEST(test_analyzer_arm64_flow());
    DIANA_TEST(test_analyzer_arm64_new_branches());
    DIANA_TEST(test_analyzer_arm64_word_classes());
    DIANA_TEST(test_analyzer_arm64_sve_sme());
    DIANA_TEST(test_analyzer_arm64_unallocated());
    DIANA_TEST(test_analyzer_arm64_references());
    DIANA_TEST(test_analyzer_arm64_adr_code());
    DIANA_TEST(test_analyzer_arm64_alignment());
    DIANA_TEST(test_analyzer_arm64_adrp_add());
    DIANA_TEST(test_analyzer_arm64_thunk());
    DIANA_TEST(test_analyzer_arm64_tracking_lost());
    DIANA_TEST(test_analyzer_arm64_call_clobbers());
    DIANA_TEST(test_analyzer_arm64_literal_value());
    DIANA_TEST(test_analyzer_arm64_jump_tables());
    DIANA_TEST(test_analyzer_arm64_route_to_the_end());
    DIANA_TEST(test_analyzer_arm64_strict_udf());
    DIANA_TEST(test_analyzer_arm64_landing_pads());
}
