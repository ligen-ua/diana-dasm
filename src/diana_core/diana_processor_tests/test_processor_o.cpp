#include "test_processor_o.h"
#include "test_common.h"
#include "test_processor_impl.h"

static void test_processor_or()
{
// or          dword ptr [ebp-4],0FFFFFFFFh
    unsigned char code[] = {0x83, 0x4D, 0xFC, 0xFF,
                            0,0,0,0};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EBP(8);

    pCallContext->m_flags.impl.l.value = 0x202;
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(pCallContext->m_flags.impl.l.value == 0x286);
}


static void test_processor_or64()
{
    unsigned char code[] = {0x48, 0x09, 0xc0}; 
    // or rax,rax

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0x1234567812345678ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x1234567812345678ULL);
}


static void test_processor_or64_2()
{
    unsigned char code[] = {0x09, 0xc0}; 
    // or eax,eax

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0x1234567812345678ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000012345678ULL);
}

void test_processor_o()
{
    DIANA_TEST(test_processor_or());
    DIANA_TEST(test_processor_or64());
    DIANA_TEST(test_processor_or64_2());
}