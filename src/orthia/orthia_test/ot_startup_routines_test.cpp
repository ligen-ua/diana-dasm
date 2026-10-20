// Code the loader runs before the entry point: PE TLS callbacks, ELF IFUNC resolvers and
// initializers. The analyzer must start at them, as nothing in the code calls them.
#include "test_common.h"
#include "orthia_memory_cache.h"
#include "orthia_files.h"
#include "orthia_pe.h"
#include "orthia_elf.h"
#include "orthia_diana_module.h"
#include <set>

extern "C"
{
#include "diana_pe_analyzer.h"
#include "diana_streams.h"
}
#include "diana_core_cpp.h"

std::vector<char> LoadElfTestFile(const orthia::PlatformString_type& name);

// data/pe/, next to data/elf/
static std::vector<char> LoadPeTestFile(const orthia::PlatformString_type& name)
{
    auto moduleDir = orthia::GetCurrentProcessDir();
#ifdef DIANA_HAS_WIN32
    const auto dataDir = moduleDir + ORTHIA_TCSTR("../../../data/");
#else
    const auto dataDir = moduleDir + ORTHIA_TCSTR("../../../../data/");
#endif
    std::vector<char> data;
    orthia::LoadFileToVector(dataDir + ORTHIA_TCSTR("pe/") + name, data);
    return data;
}

// the instruction offsets the analyzer found in an image loaded at base
static std::set<orthia::Address_type> AnalyzeImage(DI_UINT64 base, const std::vector<char>& image)
{
    orthia::CReaderOverVector reader(base, image);
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
    return instructions;
}

// ------------------------------------------------------------------ PE: TLS callbacks
// data/pe/tls_{x86_64,aarch64}.dll, built from data/pe/tls_dll: only the TLS directory reaches the callback
static const DI_UINT64 c_tlsCallback = 0x1000;
static const DI_UINT64 c_tlsEntryPoint = 0x1020;

// relocated, as in a process or a file mapped at another base
static void test_pe_tls_callback_analyzed(const orthia::PlatformString_type& fileName)
{
    const DI_UINT64 base = 0x10000000;
    orthia::CSimplePeFile pe;
    orthia::MapFileParameters params;
    params.imageBase = base;
    pe.MapFile(LoadPeTestFile(fileName), params);
    DIANA_TEST_ASSERT(pe.GetImageBase() == base);

    // the $tls_ names come from here
    std::vector<OPERAND_SIZE> callbacks;
    DIANA_TEST_ASSERT(pe.QueryTLSCallbacks(callbacks) == DI_SUCCESS);
    DIANA_TEST_ASSERT(callbacks.size() == 1 && callbacks[0] == base + c_tlsCallback);

    auto instructions = AnalyzeImage(base, pe.GetMappedFile());
    DIANA_TEST_ASSERT(instructions.count(c_tlsCallback) == 1);
    DIANA_TEST_ASSERT(instructions.count(c_tlsEntryPoint) == 1);
}

// at the preferred base and analyzed in a buffer: the pointer scan can't take the callback array's
// pointers for module addresses, only the TLS directory leads to the callback
static void test_pe_tls_callback_is_root_in_memory(const orthia::PlatformString_type& fileName)
{
    orthia::CSimplePeFile pe;
    orthia::MapFileParameters params;
    pe.MapFile(LoadPeTestFile(fileName), params);
    std::vector<char> image = pe.GetMappedFile();

    Diana_InstructionsOwner owner;
    DIANA_TEST_ASSERT(Diana_PE_AnalyzePEInMemory(image.data(), image.size(), &owner, 0) == DI_SUCCESS);
    diana::Guard<diana::InstructionsOwner> ownerGuard(&owner);
    for (DI_UINT64 root : { c_tlsCallback, c_tlsEntryPoint })
    {
        Diana_Instruction* pInstruction = owner.m_ppPresenceVec[root];
        DIANA_TEST_ASSERT(pInstruction && (pInstruction->m_flags & DI_INSTRUCTION_ROOT) &&
                          !(pInstruction->m_flags & DI_INSTRUCTION_INVALID));
    }
}

// ------------------------------------------------------------------ ELF in a process
// data/elf/libstartup_{x64,arm64}.so (see ot_elf_test.cpp): module offsets
struct ElfStartupOffsets
{
    const orthia::PlatformString_type::value_type* fileName;
    DI_UINT64 globalResolver, localResolver, init, ctor;
};
static const ElfStartupOffsets g_elfX64 = { ORTHIA_TCSTR("libstartup_x64.so"), 0x1044, 0x1050, 0x107a, 0x106e };
static const ElfStartupOffsets g_elfArm64 = { ORTHIA_TCSTR("libstartup_arm64.so"), 0x2f0, 0x2fc, 0x330, 0x31c };

// glibc relocates the pointer tags of the dynamic section, but not DT_INIT, DT_FINI and the
// init/fini arrays: it adds the load bias when it calls them
static void UndoRelocationAsGlibc(const orthia::CSimpleElfFile& elf, std::vector<char>& image, DI_UINT64 base)
{
    const auto* pImpl = elf.GetImpl()->mappedElf.pImpl;
    DIANA_TEST_ASSERT(pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_64BIT);
    int patched = 0;
    for (DI_UINT64 pos = 0; pos + 16 <= pImpl->dynamicSize; pos += 16)
    {
        DI_UINT64 tag = 0, value = 0;
        memcpy(&tag, &image[(size_t)(pImpl->dynamicAddress + pos)], 8);
        if (tag == DIANA_DT_NULL)
            break;
        if (tag != DIANA_DT_INIT && tag != DIANA_DT_FINI && tag != DIANA_DT_INIT_ARRAY && tag != DIANA_DT_FINI_ARRAY)
            continue;
        memcpy(&value, &image[(size_t)(pImpl->dynamicAddress + pos + 8)], 8);
        DIANA_TEST_ASSERT(value >= base);
        value -= base;
        memcpy(&image[(size_t)(pImpl->dynamicAddress + pos + 8)], &value, 8);
        ++patched;
    }
    DIANA_TEST_ASSERT(patched >= 2);    // DT_INIT, DT_INIT_ARRAY
}

static void test_elf_startup_routines_in_process(const ElfStartupOffsets& fixture)
{
    const DI_UINT64 base = 0x7f1234560000ULL;
    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    params.imageBase = base;
    elf.MapFile(LoadElfTestFile(fixture.fileName), params);
    std::vector<char> image = elf.GetMappedFile();
    UndoRelocationAsGlibc(elf, image, base);

    // the names: DeliverExtraExports reads the module like this
    DianaMovableReadStreamOverMemory stream;
    DianaMovableReadStreamOverMemory_Init(&stream, image.data(), image.size());
    std::vector<orthia::ElfStartupRoutine> routines;
    DIANA_TEST_ASSERT(orthia::QueryElfStartupRoutines(&elf.GetImpl()->mappedElf, &stream.stream, base, routines) == DI_SUCCESS);
    std::set<DI_UINT64> addresses;
    for (const auto& routine : routines)
    {
        addresses.insert(routine.address);
    }
    DIANA_TEST_ASSERT(routines.size() == 4);
    for (DI_UINT64 offset : { fixture.globalResolver, fixture.localResolver, fixture.init, fixture.ctor })
    {
        DIANA_TEST_ASSERT(addresses.count(base + offset) == 1);
    }

    // the analysis
    auto instructions = AnalyzeImage(base, image);
    for (DI_UINT64 offset : { fixture.globalResolver, fixture.localResolver, fixture.init, fixture.ctor })
    {
        DIANA_TEST_ASSERT(instructions.count(offset) == 1);
    }
}

void test_startup_routines()
{
    DIANA_TEST(test_pe_tls_callback_analyzed(ORTHIA_TCSTR("tls_x86_64.dll")));
    DIANA_TEST(test_pe_tls_callback_analyzed(ORTHIA_TCSTR("tls_aarch64.dll")));
    DIANA_TEST(test_pe_tls_callback_is_root_in_memory(ORTHIA_TCSTR("tls_x86_64.dll")));
    DIANA_TEST(test_pe_tls_callback_is_root_in_memory(ORTHIA_TCSTR("tls_aarch64.dll")));
    DIANA_TEST(test_elf_startup_routines_in_process(g_elfX64));
    DIANA_TEST(test_elf_startup_routines_in_process(g_elfArm64));
}
