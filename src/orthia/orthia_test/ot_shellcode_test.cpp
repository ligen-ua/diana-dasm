#include "test_common.h"
#include "orthia_shellcode.h"

// raw code: the bytes are the image, placed at the base the user chose
static void test_shellcode_layout()
{
    const std::vector<char> code = { '\x55', '\x48', '\x89', '\xe5', '\xc3' };
    orthia::CSimpleShellcodeFile file(code, 0x10000, DIANA_MODE64);

    DIANA_TEST_ASSERT(file.GetImageBase() == 0x10000);
    DIANA_TEST_ASSERT(file.GetImageEnd() == 0x10004);
    DIANA_TEST_ASSERT(file.GetEntryPoint() == 0x10000);
    DIANA_TEST_ASSERT(file.GetDianaMode() == DIANA_MODE64);
    DIANA_TEST_ASSERT(file.GetMappedFile() == code);
}

static void test_shellcode_relocate()
{
    orthia::CSimpleShellcodeFile file(std::vector<char>(0x20, '\x90'), 0x10000, DIANA_MODE32);
    file.Relocate(0x400000);

    DIANA_TEST_ASSERT(file.GetImageBase() == 0x400000);
    DIANA_TEST_ASSERT(file.GetImageEnd() == 0x40001f);
    DIANA_TEST_ASSERT(file.GetEntryPoint() == 0x400000);
}

static void test_shellcode_empty()
{
    orthia::CSimpleShellcodeFile file(std::vector<char>(), 0x10000, DIANA_MODE64);

    DIANA_TEST_ASSERT(file.GetMappedFile().empty());
    DIANA_TEST_ASSERT(file.GetImageEnd() == 0x10000);
}

// no headers: nothing to export, import or link
static void test_shellcode_has_no_tables()
{
    orthia::CSimpleShellcodeFile file(std::vector<char>(0x10, '\x90'), 0x10000, DIANA_MODE64);

    DIANA_TEST_ASSERT(file.QueryExports(nullptr) == 0);
    DIANA_TEST_ASSERT(file.QueryImports(nullptr) == 0);
    DIANA_TEST_ASSERT(file.DiGetProcAddress("anything") == 0);
    const char patch[2] = { 1, 2 };
    DIANA_TEST_ASSERT(!file.WriteImage(0x10000, patch, sizeof(patch)));
    DIANA_TEST_ASSERT(file.GetMappedFile()[0] == '\x90');
}

void test_shellcode()
{
    DIANA_TEST(test_shellcode_layout());
    DIANA_TEST(test_shellcode_relocate());
    DIANA_TEST(test_shellcode_empty());
    DIANA_TEST(test_shellcode_has_no_tables());
}
