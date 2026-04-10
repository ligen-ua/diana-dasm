#include "test_common.h"
#include "orthia_memory_cache.h"
#include "orthia_files.h"
#include "orthia_streams.h"

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


static void test_elf1()
{
    std::vector<char> data = LoadElfTestFile(ORTHIA_TCSTR("ls.bin"));

    DianaMovableReadStreamOverMemory dianaElfFileStream;
    DianaMovableReadStreamOverMemory_Init(&dianaElfFileStream, &data.front(), data.size());

    Diana_ElfFile dianaElfFile;
    DI_CHECK_CPP(DianaElfFile_Init(&dianaElfFile,
        &dianaElfFileStream.stream,
        data.size(),
        0));
    diana::Guard<diana::ElfFile> peFileGuard(&dianaElfFile);

    OPERAND_SIZE symbolAddress = 0;
    DI_CHECK_CPP(DianaElfFile_GetProcAddress(&dianaElfFile,
        &dianaElfFileStream.stream,
        "_obstack_begin",
        &symbolAddress));

    ::DianaMemoryStream rwStream;
    Diana_InitMemoryStreamEx2(&rwStream, &data.front(), data.size(), 0, 0);

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

void test_elf()
{
    DIANA_TEST(test_elf1());
}