#include "test_common.h"
#include "orthia_memory_cache.h"
#include "orthia_files.h"
#include "orthia_streams.h"
#include "orthia_elf.h"
#include "orthia_diana_module.h"
#include <map>
#include <set>

extern "C"
{
#include "diana_elf.h"
}
#include "diana_pe_cpp.h"
#include "ot_common.h"

// data/elf/ is committed. data/private/elf/ is gitignored: third-party binaries that can't be
// published (dmesg, apt-mark, ls.bin); the tests that need them are skipped when they are absent.
orthia::PlatformString_type ElfTestFilePath(const orthia::PlatformString_type& name)
{
    auto moduleDir = orthia::GetCurrentProcessDir();
#ifdef DIANA_HAS_WIN32
    const auto dataDir = moduleDir + ORTHIA_TCSTR("../../../data/");
#else
    const auto dataDir = moduleDir + ORTHIA_TCSTR("../../../../data/");
#endif
    const auto committed = dataDir + ORTHIA_TCSTR("elf/") + name;
    const auto privateFile = dataDir + ORTHIA_TCSTR("private/elf/") + name;
    if (!orthia::IsFileExists(committed) && orthia::IsFileExists(privateFile))
    {
        return privateFile;
    }
    return committed;
}

bool ElfTestFilesPresent(const char* testName, std::initializer_list<orthia::PlatformString_type> names)
{
    for (const auto& name : names)
    {
        if (!orthia::IsFileExists(ElfTestFilePath(name)))
        {
            std::cout << "[SKIP: " << testName << "] private test file not present: data/private/elf/"
                      << orthia::PlatformStringToUtf8(name) << "\n";
            return false;
        }
    }
    return true;
}

std::vector<char> LoadElfTestFile(const orthia::PlatformString_type& name)
{
    std::vector<char> data;
    orthia::LoadFileToVector(ElfTestFilePath(name), data);
    return data;
}


class ImportsObserver:public diana::CBasePeLinkImportsObserver
{

public:
    ImportsObserver()
    {
    }
    void QueryFunctionByOrdinal(const char* pDllName,
        DI_UINT32 ordinal,
        OPERAND_SIZE* pAddress)
    {
    }
    void QueryFunctionByName(const char* pDllName,
        const char* pFunctionName,
        DI_UINT32 hint,
        OPERAND_SIZE* pAddress)
    {
    }
};

class CollectingImportsObserver : public diana::CBasePeLinkImportsObserver
{
public:
    std::vector<std::string> imports;

    void QueryFunctionByOrdinal(const char* pDllName, DI_UINT32 ordinal, OPERAND_SIZE* pAddress)
    {
    }
    void QueryFunctionByName(const char* pDllName, const char* pFunctionName, DI_UINT32 hint, OPERAND_SIZE* pAddress)
    {
        if (pFunctionName)
            imports.push_back(pFunctionName);
    }
};

static int TranslateAbsoluteAddress(struct _dianaMemoryStream* pThis, OPERAND_SIZE* address)
{
    return DI_SUCCESS;
}

static void test_elf1()
{
    std::vector<char> data = LoadElfTestFile(ORTHIA_TCSTR("ls.bin"));

    DianaMemoryStream dianaElfFileStream;
    Diana_InitMemoryStream(&dianaElfFileStream, &data.front(), data.size());
    dianaElfFileStream.translateAbsoluteAddress = TranslateAbsoluteAddress;

    Diana_ElfFile dianaElfFile;
    DI_CHECK_CPP(DianaElfFile_Init(&dianaElfFile,
        &dianaElfFileStream.parent.parent,
        data.size(),
        0));
    diana::Guard<diana::ElfFile> peFileGuard(&dianaElfFile);

    OPERAND_SIZE symbolAddress = 0;
    DI_CHECK_CPP(DianaElfFile_GetProcAddress(&dianaElfFile,
        &dianaElfFileStream.parent.parent,
        "_obstack_begin",
        &symbolAddress));

    ::DianaMemoryStream rwStream;
    Diana_InitMemoryStreamEx2(&rwStream, &data.front(), data.size(), 0, 0);
    rwStream.translateAbsoluteAddress = TranslateAbsoluteAddress;
    ImportsObserver observer;
    DI_CHECK_CPP(DianaElfFile_QueryImports(&dianaElfFile,
        0,
        &rwStream.parent,
        0,
        0,
        observer.GetParent(),
        0,
        0));


    DI_CHECK_CPP(DianaElfFile_QueryExports(&dianaElfFile,
        &rwStream.parent.parent,
        0,
        0,
        observer.GetParent(),
        0));
}

static void test_simple_elf_map()
{
    std::vector<char> data = LoadElfTestFile(ORTHIA_TCSTR("dmesg"));

    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    elf.MapFile(data, params);

    DIANA_TEST_ASSERT(!elf.GetMappedFile().empty());
    DIANA_TEST_ASSERT(elf.GetImageEnd() > elf.GetImageBase());
    DIANA_TEST_ASSERT(elf.GetDianaMode() != 0);
    DIANA_TEST_ASSERT(elf.GetEntryPoint() != 0);
}

static void test_simple_elf_needed_libs()
{
    std::vector<char> data = LoadElfTestFile(ORTHIA_TCSTR("dmesg"));

    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    elf.MapFile(data, params);

    auto libs = elf.GetNeededLibraries();
    DIANA_TEST_ASSERT(!libs.empty());
}

static void test_simple_elf_imports()
{
    std::vector<char> data = LoadElfTestFile(ORTHIA_TCSTR("dmesg"));

    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    elf.MapFile(data, params);

    CollectingImportsObserver observer;
    elf.QueryImports(&observer);
    DIANA_TEST_ASSERT(!observer.imports.empty());
}

static void test_simple_elf_relocate()
{
    std::vector<char> data = LoadElfTestFile(ORTHIA_TCSTR("dmesg"));

    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    elf.MapFile(data, params);

    const DI_UINT64 newBase = 0x500000;
    elf.Relocate(newBase);
    DIANA_TEST_ASSERT(elf.GetImageBase() == newBase);
    DIANA_TEST_ASSERT(elf.GetImageEnd() > newBase);
}

static void test_elf_build_id()
{
    std::vector<char> data = LoadElfTestFile(ORTHIA_TCSTR("ls.bin"));

    DianaMemoryStream dianaElfFileStream;
    Diana_InitMemoryStream(&dianaElfFileStream, &data.front(), data.size());
    dianaElfFileStream.translateAbsoluteAddress = TranslateAbsoluteAddress;

    Diana_ElfFile dianaElfFile;
    DI_CHECK_CPP(DianaElfFile_Init(&dianaElfFile,
        &dianaElfFileStream.parent.parent,
        data.size(),
        0));
    diana::Guard<diana::ElfFile> elfFileGuard(&dianaElfFile);

    DI_UINT8 buildId[256] = { 0, };
    DI_UINT32 buildIdSize = 0;
    DI_CHECK_CPP(DianaElfFile_QueryBuildId(&dianaElfFile,
        &dianaElfFileStream.parent.parent,
        0,
        buildId,
        sizeof(buildId),
        &buildIdSize));

    const DI_UINT8 expected[] = {
        0x05, 0xda, 0xd2, 0xc2, 0x79, 0xf7, 0x65, 0x17, 0x22, 0x80,
        0x9f, 0xa7, 0x5a, 0xdb, 0xa6, 0xbf, 0x9a, 0xb1, 0xc2, 0x09
    };
    DIANA_TEST_ASSERT(buildIdSize == sizeof(expected));
    DIANA_TEST_ASSERT(memcmp(buildId, expected, sizeof(expected)) == 0);
}

class ExportsMapObserver : public diana::CBasePeLinkImportsObserver
{
public:
    std::map<std::string, OPERAND_SIZE> exports;
    int reports = 0;

    void QueryFunctionByOrdinal(const char*, DI_UINT32, OPERAND_SIZE*)
    {
    }
    void QueryFunctionByName(const char*, const char* pFunctionName, DI_UINT32, OPERAND_SIZE* pAddress)
    {
        ++reports;
        exports[pFunctionName] = *pAddress;
    }
};

// data/elf/libexports_{gnu,sysv}.so, built from data/elf/exports_lib: the same exports
// behind a GNU and a SysV hash table, with a symbol in two versions (vfunc@VER_1 hidden,
// vfunc@@VER_2 default) and undefined imports in front of the hashed symbols
static void test_elf_exports(const orthia::PlatformString_type& fileName)
{
    std::vector<char> data = LoadElfTestFile(fileName);

    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    elf.MapFile(data, params);

    ExportsMapObserver observer;
    DI_CHECK_CPP(elf.QueryExports(&observer));

    std::set<std::string> expected = { "exp_data_object", "exp_weak_func", "exp_ifunc",
        "vfunc", "vfunc_v1", "vfunc_v2" };
    for (int i = 0; i < 40; ++i)
    {
        char name[32];
        snprintf(name, sizeof(name), "exp_func_%02d", i);
        expected.insert(name);
    }
    std::set<std::string> actual;
    for (auto& entry : observer.exports)
    {
        actual.insert(entry.first);
    }
    // no static, hidden-visibility, undefined or version-definition symbols; each name once
    DIANA_TEST_ASSERT(actual == expected);
    DIANA_TEST_ASSERT(observer.reports == (int)expected.size());

    // the default version wins, as with dlsym
    DIANA_TEST_ASSERT(observer.exports["vfunc"] == observer.exports["vfunc_v2"]);
    DIANA_TEST_ASSERT(observer.exports["vfunc"] != observer.exports["vfunc_v1"]);

    // the hash-table lookup finds every export at the enumerated value, moved to where the image is
    const DI_UINT64 bias = elf.GetImageBase() - elf.GetImpl()->mappedElf.pImpl->linkBase;
    for (auto& entry : observer.exports)
    {
        DIANA_TEST_ASSERT(elf.DiGetProcAddress(entry.first.c_str()) == bias + entry.second);
    }
    DIANA_TEST_ASSERT(elf.DiGetProcAddress("hidden_func") == 0);
    DIANA_TEST_ASSERT(elf.DiGetProcAddress("ext_import_1") == 0);
    DIANA_TEST_ASSERT(elf.DiGetProcAddress("no_such_export") == 0);
}

// resolves every import from one library, as the file-mode dependency loader does
class LibraryResolver : public diana::CBasePeLinkImportsObserver
{
    orthia::CSimpleElfFile& m_library;
public:
    std::map<std::string, OPERAND_SIZE> resolved;

    explicit LibraryResolver(orthia::CSimpleElfFile& library)
        : m_library(library)
    {
    }
    void QueryFunctionByOrdinal(const char*, DI_UINT32, OPERAND_SIZE*)
    {
    }
    void QueryFunctionByName(const char*, const char* pFunctionName, DI_UINT32, OPERAND_SIZE* pAddress)
    {
        *pAddress = m_library.DiGetProcAddress(pFunctionName);
        resolved[pFunctionName] = *pAddress;
    }
};

static DI_UINT64 ReadMappedPointer(const orthia::CSimpleElfFile& elf, DI_UINT64 address)
{
    const auto& image = elf.GetMappedFile();
    const auto offset = address - elf.GetImageBase();
    DIANA_TEST_ASSERT(offset + 8 <= image.size());
    DI_UINT64 value = 0;
    memcpy(&value, image.data() + offset, 8);
    return value;
}

// data/elf/linked_x64, built from data/elf/linked_exec: a PIE that imports exp_func_07 and
// exp_func_21 from libexports_gnu.so through the PLT slots at 0x4008 and 0x4000.
// Linking writes the address of each export where the library is mapped into its slot.
static void test_elf_link_imports()
{
    const DI_UINT64 libraryBase = 0x10000;
    orthia::CSimpleElfFile library;
    orthia::MapFileParameters libraryParams;
    libraryParams.imageBase = libraryBase;
    library.MapFile(LoadElfTestFile(ORTHIA_TCSTR("libexports_gnu.so")), libraryParams);
    DIANA_TEST_ASSERT(library.GetImageBase() == libraryBase);

    // an export is where the library is: its base plus the symbol value
    DIANA_TEST_ASSERT(library.DiGetProcAddress("exp_func_07") == libraryBase + 0x10d8);
    DIANA_TEST_ASSERT(library.DiGetProcAddress("exp_func_21") == libraryBase + 0x1164);

    orthia::CSimpleElfFile exe;
    orthia::MapFileParameters exeParams;
    exe.MapFile(LoadElfTestFile(ORTHIA_TCSTR("linked_x64")), exeParams);
    const DI_UINT64 exeBias = exe.GetImageBase();

    LibraryResolver resolver(library);
    DIANA_TEST_ASSERT(exe.QueryImports(&resolver) == DI_SUCCESS);
    DIANA_TEST_ASSERT(resolver.resolved.size() == 2);
    DIANA_TEST_ASSERT(ReadMappedPointer(exe, exeBias + 0x4008) == libraryBase + 0x10d8);
    DIANA_TEST_ASSERT(ReadMappedPointer(exe, exeBias + 0x4000) == libraryBase + 0x1164);
}

// data/elf/libstartup_{x64,arm64,thumb,arm32}.so, built from data/elf/startup_lib: module offsets (link-time
// addresses); T32 code has bit 0 set
struct StartupFixture
{
    const orthia::PlatformString_type::value_type* fileName;
    DI_UINT64 globalResolver;   // exp_ifunc, a STT_GNU_IFUNC symbol
    DI_UINT64 localResolver;    // local_ifunc, only R_*_IRELATIVE in .rela.plt
    DI_UINT64 localSlot;        // the GOT slot of that IRELATIVE
    DI_UINT64 init;             // DT_INIT: startup_init
    DI_UINT64 ctor;             // DT_INIT_ARRAY[0]: startup_ctor
    DI_UINT64 ctorSlot;
};

static const StartupFixture g_startupX64 = { ORTHIA_TCSTR("libstartup_x64.so"), 0x1044, 0x1050, 0x4000, 0x107a, 0x106e, 0x3e90 };
static const StartupFixture g_startupArm64 = { ORTHIA_TCSTR("libstartup_arm64.so"), 0x2f0, 0x2fc, 0x20000, 0x330, 0x31c, 0x1fe88 };
static const StartupFixture g_startupThumb = { ORTHIA_TCSTR("libstartup_thumb.so"), 0x1b1, 0x1bd, 0x200c, 0x1e1, 0x1d1, 0x1f74 };
static const StartupFixture g_startupArm32 = { ORTHIA_TCSTR("libstartup_arm32.so"), 0x1b8, 0x1c8, 0x200c, 0x200, 0x1e4, 0x1f74 };


static void VerifyStartupRoutines(const StartupFixture& fixture, const orthia::CSimpleElfFile& elf)
{
    const DI_UINT64 base = elf.GetImageBase();
    std::vector<orthia::ElfStartupRoutine> routines;
    DIANA_TEST_ASSERT(elf.QueryStartupRoutines(routines) == DI_SUCCESS);
    DIANA_TEST_ASSERT(routines.size() == 4);
    if (routines.size() != 4)
        return;

    // IFUNC resolvers first: the named one, then the one only an IRELATIVE knows
    DIANA_TEST_ASSERT(routines[0].kind == DIANA_ELF_STARTUP_IFUNC);
    DIANA_TEST_ASSERT(routines[0].address == base + fixture.globalResolver);
    DIANA_TEST_ASSERT(routines[0].name == "exp_ifunc");
    DIANA_TEST_ASSERT(routines[0].slotAddress == 0);
    DIANA_TEST_ASSERT(orthia::GetElfStartupRoutineName(routines[0]) == "$ifunc_exp_ifunc");

    DIANA_TEST_ASSERT(routines[1].kind == DIANA_ELF_STARTUP_IFUNC);
    DIANA_TEST_ASSERT(routines[1].address == base + fixture.localResolver);
    DIANA_TEST_ASSERT(routines[1].name.empty() && routines[1].index == 0);
    DIANA_TEST_ASSERT(routines[1].slotAddress == base + fixture.localSlot);
    DIANA_TEST_ASSERT(orthia::GetElfStartupRoutineName(routines[1]).rfind("$ifunc_", 0) == 0);

    // then the initializers in the order the loader calls them
    DIANA_TEST_ASSERT(routines[2].kind == DIANA_ELF_STARTUP_INIT);
    DIANA_TEST_ASSERT(routines[2].address == base + fixture.init);
    DIANA_TEST_ASSERT(orthia::GetElfStartupRoutineName(routines[2]) == "$init");

    DIANA_TEST_ASSERT(routines[3].kind == DIANA_ELF_STARTUP_INIT_ARRAY);
    DIANA_TEST_ASSERT(routines[3].index == 0);
    DIANA_TEST_ASSERT(routines[3].address == base + fixture.ctor);
    DIANA_TEST_ASSERT(routines[3].slotAddress == base + fixture.ctorSlot);
    DIANA_TEST_ASSERT(orthia::GetElfStartupRoutineName(routines[3]).rfind("$init_array_", 0) == 0);

    // the IRELATIVE slot holds the resolver: the mapping can't call it
    DIANA_TEST_ASSERT(ReadMappedPointer(elf, base + fixture.localSlot) == base + fixture.localResolver);
}

static void test_elf_startup_routines(const StartupFixture& fixture)
{
    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    elf.MapFile(LoadElfTestFile(fixture.fileName), params);
    VerifyStartupRoutines(fixture, elf);

    // relocated: the dynamic tags and the init array point to the new base
    const DI_UINT64 newBase = 0x10000000;
    elf.Relocate(newBase);
    DIANA_TEST_ASSERT(elf.GetImageBase() == newBase);
    VerifyStartupRoutines(fixture, elf);
}

// nothing calls the resolvers and the initializers: the analyzer starts at them
static void test_elf_startup_routines_analyzed(const StartupFixture& fixture)
{
    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    params.imageBase = 0x10000000;
    elf.MapFile(LoadElfTestFile(fixture.fileName), params);
    const DI_UINT64 base = elf.GetImageBase();

    orthia::CReaderOverVector reader(base, elf.GetMappedFile());
    orthia::CDianaModule module;
    module.Init(base, &reader);
    module.Analyze(0);

    std::set<orthia::Address_type> instructions;
    orthia::CDianaInstructionIterator it;
    module.QueryInstructionIterator(&it);
    for (it.MoveToFirst(); !it.IsEmpty(); it.MoveToNext())
    {
        instructions.insert(it.GetInstructionOffset());
    }
    for (DI_UINT64 offset : { fixture.globalResolver, fixture.localResolver, fixture.init, fixture.ctor })
    {
        // the instruction of a T32 address is at the even one
        DIANA_TEST_ASSERT(instructions.count(orthia::GetDianaInstructionAddress(offset, elf.GetDianaArch())) == 1);
    }
}



// what the code map says the bytes at a module offset are
static orthia::ArmCodeKind ArmCodeKindAt(const std::vector<orthia::ArmCodeChange>& changes, int arch, DI_UINT64 offset)
{
    orthia::ArmCodeKind kind = orthia::GetArmCodeKind(arch);
    for (const auto& change : changes)
    {
        if (change.offset > offset)
            break;
        kind = change.kind;
    }
    return kind;
}

// AArch32 ELF: armhf code is T32 by default (gcc), -marm builds A32. The analysis follows the T32 bit of
// symbols and pointers, blx to the A32 .iplt stub and the PIC address of an IFUNC resolver
static void test_elf_arm32(const orthia::PlatformString_type& fileName, int expectedArch)
{
    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    params.imageBase = 0x10000000;
    elf.MapFile(LoadElfTestFile(fileName), params);
    DIANA_TEST_ASSERT(elf.GetDianaMode() == DIANA_MODE32);
    DIANA_TEST_ASSERT(elf.GetDianaArch() == expectedArch);

    // ELF32: the dynamic section has 8-byte entries
    ExportsMapObserver observer;
    DI_CHECK_CPP(elf.QueryExports(&observer));
    DIANA_TEST_ASSERT(observer.exports.count("call_local_ifunc") == 1);
    DIANA_TEST_ASSERT(observer.exports.count("startup_init") == 1);
    DIANA_TEST_ASSERT(observer.exports.count("exp_ifunc") == 1);

    const DI_UINT64 base = elf.GetImageBase();
    orthia::CReaderOverVector reader(base, elf.GetMappedFile());
    orthia::CDianaModule module;
    module.Init(base, &reader);
    module.Analyze(0);
    DIANA_TEST_ASSERT(module.GetDianaArch() == expectedArch);
    std::set<orthia::Address_type> instructions;
    orthia::CDianaInstructionIterator it;
    module.QueryInstructionIterator(&it);
    for (it.MoveToFirst(); !it.IsEmpty(); it.MoveToNext())
    {
        instructions.insert(it.GetInstructionOffset());
    }
    std::vector<orthia::ArmCodeChange> changes;
    module.QueryArmCodeMap(changes);

    const bool thumb = expectedArch == DIANA_ARCH_THUMB;
    const orthia::ArmCodeKind code = thumb ? orthia::ArmCodeKind::T32 : orthia::ArmCodeKind::A32;
    // call_local_ifunc: blx (T32) or bl (A32) to the .iplt stub, which is A32 code
    const DI_UINT64 caller = thumb ? 0x1c8 : 0x1d8;
    DIANA_TEST_ASSERT(instructions.count(caller) == 1);
    DIANA_TEST_ASSERT(ArmCodeKindAt(changes, expectedArch, caller) == code);
    DIANA_TEST_ASSERT(instructions.count(0x19c) == 1 && instructions.count(0x1a4) == 1);
    DIANA_TEST_ASSERT(ArmCodeKindAt(changes, expectedArch, 0x19c) == orthia::ArmCodeKind::A32);
    // global_impl: only the address the resolver returns (ldr r0, [pc]; add r0, pc) leads there
    DIANA_TEST_ASSERT(instructions.count(0x1a8) == 1);
    DIANA_TEST_ASSERT(ArmCodeKindAt(changes, expectedArch, 0x1a8) == code);
    // the resolver's literal is data
    const DI_UINT64 literal = thumb ? 0x1b8 : 0x1c4;
    DIANA_TEST_ASSERT(ArmCodeKindAt(changes, expectedArch, literal) == orthia::ArmCodeKind::Data);
    DIANA_TEST_ASSERT(instructions.count(literal) == 0);
}

void test_elf()
{
    if (ElfTestFilesPresent("test_elf", { ORTHIA_TCSTR("ls.bin"), ORTHIA_TCSTR("dmesg") }))
    {
        DIANA_TEST(test_elf1());
        DIANA_TEST(test_simple_elf_map());
        DIANA_TEST(test_simple_elf_needed_libs());
        DIANA_TEST(test_simple_elf_imports());
        DIANA_TEST(test_simple_elf_relocate());
        DIANA_TEST(test_elf_build_id());
    }
    DIANA_TEST(test_elf_exports(ORTHIA_TCSTR("libexports_gnu.so")));
    DIANA_TEST(test_elf_exports(ORTHIA_TCSTR("libexports_sysv.so")));
    DIANA_TEST(test_elf_link_imports());
    DIANA_TEST(test_elf_startup_routines(g_startupX64));
    DIANA_TEST(test_elf_startup_routines(g_startupArm64));
    DIANA_TEST(test_elf_startup_routines_analyzed(g_startupX64));
    DIANA_TEST(test_elf_startup_routines_analyzed(g_startupArm64));
    DIANA_TEST(test_elf_startup_routines(g_startupThumb));
    DIANA_TEST(test_elf_startup_routines(g_startupArm32));
    DIANA_TEST(test_elf_startup_routines_analyzed(g_startupThumb));
    DIANA_TEST(test_elf_startup_routines_analyzed(g_startupArm32));
    DIANA_TEST(test_elf_arm32(ORTHIA_TCSTR("libstartup_thumb.so"), DIANA_ARCH_THUMB));
    DIANA_TEST(test_elf_arm32(ORTHIA_TCSTR("libstartup_arm32.so"), DIANA_ARCH_ARM32));
}