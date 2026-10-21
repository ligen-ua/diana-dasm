#include "test_common.h"
#include "orthia_shellcode.h"
#include "orthia_diana_arch.h"
#include "orthia_diana_module.h"
#include "orthia_memory_cache.h"
#include <set>
extern "C"
{
#include "diana_analyze.h"
}

// raw code: the bytes are the image, placed at the base the user chose
static void test_shellcode_layout()
{
    const std::vector<char> code = { '\x55', '\x48', '\x89', '\xe5', '\xc3' };
    orthia::CSimpleShellcodeFile file(code, 0x10000, DIANA_MODE64, DIANA_ARCH_X86);

    DIANA_TEST_ASSERT(file.GetImageBase() == 0x10000);
    DIANA_TEST_ASSERT(file.GetImageEnd() == 0x10004);
    DIANA_TEST_ASSERT(file.GetEntryPoint() == 0x10000);
    DIANA_TEST_ASSERT(file.GetDianaMode() == DIANA_MODE64);
    DIANA_TEST_ASSERT(file.GetMappedFile() == code);
}

static void test_shellcode_relocate()
{
    orthia::CSimpleShellcodeFile file(std::vector<char>(0x20, '\x90'), 0x10000, DIANA_MODE32, DIANA_ARCH_X86);
    file.Relocate(0x400000);

    DIANA_TEST_ASSERT(file.GetImageBase() == 0x400000);
    DIANA_TEST_ASSERT(file.GetImageEnd() == 0x40001f);
    DIANA_TEST_ASSERT(file.GetEntryPoint() == 0x400000);
}

static void test_shellcode_empty()
{
    orthia::CSimpleShellcodeFile file(std::vector<char>(), 0x10000, DIANA_MODE64, DIANA_ARCH_X86);

    DIANA_TEST_ASSERT(file.GetMappedFile().empty());
    DIANA_TEST_ASSERT(file.GetImageEnd() == 0x10000);
}

// no headers: nothing to export, import or link
static void test_shellcode_has_no_tables()
{
    orthia::CSimpleShellcodeFile file(std::vector<char>(0x10, '\x90'), 0x10000, DIANA_MODE64, DIANA_ARCH_X86);

    DIANA_TEST_ASSERT(file.QueryExports(nullptr) == 0);
    DIANA_TEST_ASSERT(file.QueryImports(nullptr) == 0);
    DIANA_TEST_ASSERT(file.DiGetProcAddress("anything") == 0);
    const char patch[2] = { 1, 2 };
    DIANA_TEST_ASSERT(!file.WriteImage(0x10000, patch, sizeof(patch)));
    DIANA_TEST_ASSERT(file.GetMappedFile()[0] == '\x90');
}

static void test_shellcode_arch()
{
    orthia::CSimpleShellcodeFile x64(std::vector<char>(4, '\x90'), 0x10000, DIANA_MODE64, DIANA_ARCH_X86);
    DIANA_TEST_ASSERT(x64.GetDianaArch() == DIANA_ARCH_X86);
    orthia::CSimpleShellcodeFile arm64(std::vector<char>(4, 0), 0x10000, DIANA_MODE64, DIANA_ARCH_AARCH64);
    DIANA_TEST_ASSERT(arm64.GetDianaMode() == DIANA_MODE64);
    DIANA_TEST_ASSERT(arm64.GetDianaArch() == DIANA_ARCH_AARCH64);
    orthia::CSimpleShellcodeFile thumb(std::vector<char>(4, 0), 0x10000, DIANA_MODE32, DIANA_ARCH_THUMB);
    DIANA_TEST_ASSERT(thumb.GetDianaArch() == DIANA_ARCH_THUMB);
}

// the names of --arch and the shellcode dialog
static void test_arch_choices()
{
    DIANA_TEST_ASSERT(orthia::GetDianaArchChoiceNames() == "x86, x64, arm64, arm, thumb");
    struct { const char* name; int mode; int arch; int alignment; } expected[] = {
        { "x86",   DIANA_MODE32, DIANA_ARCH_X86,     1 },
        { "x64",   DIANA_MODE64, DIANA_ARCH_X86,     1 },
        { "arm64", DIANA_MODE64, DIANA_ARCH_AARCH64, 4 },
        { "arm",   DIANA_MODE32, DIANA_ARCH_ARM32,   4 },
        { "thumb", DIANA_MODE32, DIANA_ARCH_THUMB,   2 },
    };
    DIANA_TEST_ASSERT(orthia::GetDianaArchChoices().size() == sizeof(expected) / sizeof(expected[0]));
    for (auto& e : expected)
    {
        const auto* byName = orthia::FindDianaArchChoice(std::string(e.name));
        DIANA_TEST_ASSERT(byName && byName->dianaMode == e.mode && byName->dianaArch == e.arch);
        DIANA_TEST_ASSERT(orthia::FindDianaArchChoice(e.mode, e.arch) == byName);
        DIANA_TEST_ASSERT(orthia::GetDianaInstructionAlignment(e.arch) == e.alignment);
    }
    DIANA_TEST_ASSERT(!orthia::FindDianaArchChoice(std::string("ARM64")));
    DIANA_TEST_ASSERT(!orthia::FindDianaArchChoice(std::string("32")));
    DIANA_TEST_ASSERT(!orthia::FindDianaArchChoice(DIANA_MODE16, DIANA_ARCH_X86));
    DIANA_TEST_ASSERT(!orthia::FindDianaArchChoice(DIANA_MODE32, DIANA_ARCH_AARCH64));
}

static std::vector<char> Words(std::initializer_list<DI_UINT32> words)
{
    std::vector<char> code;
    for (DI_UINT32 word : words)
    {
        for (int b = 0; b < 4; ++b)
            code.push_back((char)(word >> (8 * b)));
    }
    return code;
}

// the instructions and their xrefs the range analyzer found, relative offsets
struct RangeAnalysis
{
    std::set<orthia::Address_type> instructions;
    std::set<std::pair<orthia::Address_type, orthia::Address_type>> calls;     // from, to
};

static RangeAnalysis AnalyzeRange(DI_UINT64 base, const std::vector<char>& code, int mode, int arch, int flags)
{
    orthia::CReaderOverVector reader(base, code);
    orthia::CDianaModule module;
    module.InitRaw(base, code.size(), &reader, mode, arch);
    module.Analyze(flags);

    RangeAnalysis result;
    orthia::CDianaInstructionIterator it;
    module.QueryInstructionIterator(&it);
    for (it.MoveToFirst(); !it.IsEmpty(); it.MoveToNext())
    {
        const auto offset = it.GetInstructionOffset();
        result.instructions.insert(offset);
        std::vector<orthia::CommonReferenceInfo> refs;
        it.QueryRefsToCurrentInstuction(&refs);
        for (auto& ref : refs)
        {
            result.calls.insert({ ref.address, offset });
        }
    }
    return result;
}

// AArch64 code placed where the user says: decoded as A64, words only, and with the scan the
// function nothing calls (and its callee) too; zeros stay data
static void test_range_arm64()
{
    const auto code = Words({
        0x94000002,     // bl f                 :0x00
        0xd65f03c0,     // ret                  :0x04
        0xd2800000,     // f: mov x0, #0        :0x08
        0xd65f03c0,     // ret                  :0x0c
        0x00000000,     // zeros                :0x10
        0x94000002,     // g: bl h              :0x14
        0xd65f03c0,     // ret                  :0x18
        0xd2800020,     // h: mov x0, #1        :0x1c
        0xd65f03c0,     // ret                  :0x20
    });
    const DI_UINT64 base = 0x10000;
    {
        auto analysis = AnalyzeRange(base, code, DIANA_MODE64, DIANA_ARCH_AARCH64, 0);
        DIANA_TEST_ASSERT(analysis.instructions == std::set<orthia::Address_type>({ 0x00, 0x04, 0x08, 0x0c }));
        DIANA_TEST_ASSERT(analysis.calls.count({ 0x00, 0x08 }) == 1);
    }
    {
        auto analysis = AnalyzeRange(base, code, DIANA_MODE64, DIANA_ARCH_AARCH64, DI_ANALYSE_PE_FILE_SCAN_THROUGH);
        DIANA_TEST_ASSERT(analysis.instructions ==
            std::set<orthia::Address_type>({ 0x00, 0x04, 0x08, 0x0c, 0x14, 0x18, 0x1c, 0x20 }));
        DIANA_TEST_ASSERT(analysis.calls.count({ 0x14, 0x1c }) == 1);
    }
}

// a route the scan rolls back stays in the analyzer's vector: it is no code, and the instructions
// after it in the vector are still there
static void test_range_rollback_is_no_code()
{
    const auto code = Words({
        0x94000002,     // bl f                 :0x00
        0xd65f03c0,     // ret                  :0x04
        0xd65f03c0,     // f: ret               :0x08
        0x94000003,     // bl g, then no code   :0x0c
        0xffffffff,     // not an instruction   :0x10
        0xd2800040,     // mov x0, #2           :0x14
        0xd65f03c0,     // ret                  :0x18
        0xd65f03c0,     // g: ret               :0x1c
    });
    auto analysis = AnalyzeRange(0x10000, code, DIANA_MODE64, DIANA_ARCH_AARCH64, DI_ANALYSE_PE_FILE_SCAN_THROUGH);
    DIANA_TEST_ASSERT(analysis.instructions.count(0x0c) == 0);
    DIANA_TEST_ASSERT(analysis.instructions.count(0x14) == 1 && analysis.instructions.count(0x18) == 1);
    DIANA_TEST_ASSERT(analysis.instructions.count(0x1c) == 1);
    // the call from the rolled back route is gone
    DIANA_TEST_ASSERT(analysis.calls.count({ 0x0c, 0x1c }) == 0);
}

// AArch32: the A32/T32/data map the disassembly reads from the database: a switch table, a literal,
// T32 code reached by blx, A32 code found by the scan
static void test_range_arm32_code_map()
{
    const auto code = Words({
        0xe92d4010,     // push {r4, lr}                :0x00
        0xe3500001,     // cmp r0, #1                   :0x04
        0x979ff100,     // ldrls pc, [pc, r0, lsl #2]   :0x08
        0xe12fff1e,     // bx lr                        :0x0c
        0x00010018,     // .word c0                     :0x10
        0x0001001c,     // .word c1                     :0x14
        0xe8bd8010,     // c0: pop {r4, pc}             :0x18
        0xe59f0004,     // c1: ldr r0, [pc, #4] (0x28)  :0x1c
        0xfa000001,     // blx th                       :0x20
        0xe8bd8010,     // pop {r4, pc}                 :0x24
        0x12345678,     // the literal                  :0x28
        0x47702001,     // th: movs r0, #1; bx lr (T32) :0x2c
        0xe12fff1e,     // unref: bx lr                 :0x30
    });
    const DI_UINT64 base = 0x10000;
    orthia::CReaderOverVector reader(base, code);
    orthia::CDianaModule module;
    module.InitRaw(base, code.size(), &reader, DIANA_MODE32, DIANA_ARCH_ARM32);
    module.Analyze(DI_ANALYSE_PE_FILE_SCAN_THROUGH);
    DIANA_TEST_ASSERT(module.GetDianaArch() == DIANA_ARCH_ARM32);

    std::vector<orthia::ArmCodeChange> changes;
    module.QueryArmCodeMap(changes);
    const orthia::ArmCodeChange expected[] = {
        { 0x10, orthia::ArmCodeKind::Data },
        { 0x18, orthia::ArmCodeKind::A32 },
        { 0x28, orthia::ArmCodeKind::Data },
        { 0x2c, orthia::ArmCodeKind::T32 },
        { 0x30, orthia::ArmCodeKind::A32 },
    };
    DIANA_TEST_ASSERT(changes.size() == sizeof(expected) / sizeof(expected[0]));
    for (size_t i = 0; i < changes.size() && i < sizeof(expected) / sizeof(expected[0]); ++i)
    {
        DIANA_TEST_ASSERT(changes[i].offset == expected[i].offset && changes[i].kind == expected[i].kind);
    }

    // a T32 module: T32 by default, A32 where blx goes
    const std::vector<char> thumb = {
        '\x00', '\xf0', '\x02', '\xe8',     // blx a32              :0x00
        '\x70', '\x47',                     // bx lr                :0x04
        '\x00', '\xbf',                     // nop                  :0x06
        '\x1e', '\xff', '\x2f', '\xe1',     // a32: bx lr           :0x08
    };
    orthia::CReaderOverVector thumbReader(base, thumb);
    orthia::CDianaModule thumbModule;
    thumbModule.InitRaw(base, thumb.size(), &thumbReader, DIANA_MODE32, DIANA_ARCH_THUMB);
    thumbModule.Analyze(0);
    thumbModule.QueryArmCodeMap(changes);
    DIANA_TEST_ASSERT(changes.size() == 1);
    DIANA_TEST_ASSERT(changes.size() == 1 && changes[0].offset == 0x08 && changes[0].kind == orthia::ArmCodeKind::A32);

    // other architectures: no map
    orthia::CDianaModule arm64;
    orthia::CReaderOverVector arm64Reader(base, Words({ 0xd65f03c0 }));
    arm64.InitRaw(base, 4, &arm64Reader, DIANA_MODE64, DIANA_ARCH_AARCH64);
    arm64.Analyze(0);
    arm64.QueryArmCodeMap(changes);
    DIANA_TEST_ASSERT(changes.empty());
}

// x86 ranges are analyzed as before: from the start only, without the scan flag
static void test_range_x86_unchanged()
{
    const std::vector<char> code = { '\xc3', '\x31', '\xc0', '\xc3' };    // ret; xor eax, eax; ret
    auto analysis = AnalyzeRange(0x10000, code, DIANA_MODE64, DIANA_ARCH_X86, 0);
    DIANA_TEST_ASSERT(analysis.instructions == std::set<orthia::Address_type>({ 0 }));
}

void test_shellcode()
{
    DIANA_TEST(test_shellcode_layout());
    DIANA_TEST(test_shellcode_relocate());
    DIANA_TEST(test_shellcode_empty());
    DIANA_TEST(test_shellcode_has_no_tables());
    DIANA_TEST(test_shellcode_arch());
    DIANA_TEST(test_arch_choices());
    DIANA_TEST(test_range_arm64());
    DIANA_TEST(test_range_rollback_is_no_code());
    DIANA_TEST(test_range_arm32_code_map());
    DIANA_TEST(test_range_x86_unchanged());
}
