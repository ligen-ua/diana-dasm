#include "test_common.h"
#include "orthia_memory_cache.h"
#include "orthia_files.h"
#include "orthia_streams.h"
#include "orthia_elf.h"
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

    // the hash-table lookup finds every export at the enumerated address
    for (auto& entry : observer.exports)
    {
        DIANA_TEST_ASSERT(elf.DiGetProcAddress(entry.first.c_str()) == entry.second);
    }
    DIANA_TEST_ASSERT(elf.DiGetProcAddress("hidden_func") == 0);
    DIANA_TEST_ASSERT(elf.DiGetProcAddress("ext_import_1") == 0);
    DIANA_TEST_ASSERT(elf.DiGetProcAddress("no_such_export") == 0);
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
}