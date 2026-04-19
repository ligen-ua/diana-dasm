#include "test_common.h"
#include "orthia_memory_cache.h"
#include "orthia_files.h"
#include "orthia_streams.h"
#include "orthia_elf.h"

extern "C"
{
#include "diana_elf.h"
}
#include "diana_pe_cpp.h"
#include "ot_common.h"

std::vector<char> LoadElfTestFile(const orthia::PlatformString_type& name)
{
    auto moduleDir = orthia::GetCurrentProcessDir();

#ifdef DIANA_HAS_WIN32
    auto fileName = moduleDir + ORTHIA_TCSTR("../../../data/elf/") + name;
#else
    auto fileName = moduleDir + ORTHIA_TCSTR("../../../../data/elf/") + name;
#endif

    std::vector<char> data;
    orthia::LoadFileToVector(fileName, data);
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

void test_elf()
{
    DIANA_TEST(test_elf1());
    DIANA_TEST(test_simple_elf_map());
    DIANA_TEST(test_simple_elf_needed_libs());
    DIANA_TEST(test_simple_elf_imports());
    DIANA_TEST(test_simple_elf_relocate());
}