#include "test_processor_n.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static void test_processor_neg()
{
    unsigned char code[] = {0xf7, 0xdb}; // neg         ebx

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EBX(0xFF);

    int res = proc.ExecOnce();
    
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RBX == 0xFFFFFF01);
    DIANA_TEST_ASSERT(pCallContext->m_flags.impl.l.impl.l.value == 0x00000293);
}

static void test_processor_neg2()
{
    unsigned char code[] = {0xf7, 0xdb}; // neg         ebx

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EBX(0x80000000);

    int res = proc.ExecOnce();
    
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RBX == 0x80000000);
    DIANA_TEST_ASSERT(pCallContext->m_flags.impl.l.impl.l.value == 0x00000A87);
}

static void test_processor_nop()
{
    unsigned char code[] = {0x90}; // nop

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0x12345678);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(0x12345678 == GET_REG_RAX);
}

static void test_processor_nop64()
{
    unsigned char code[] = {0x90}; // nop

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0x1234567812345678ULL);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(0x1234567812345678ULL == GET_REG_RAX);
}

static void test_processor_nop64_2()
{
    unsigned char code[] = {0x40, 0x90}; // "nop"

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0x1111222233334444ULL);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(0x1111222233334444ULL == GET_REG_RAX);
}

void test_processor_n()
{
    DIANA_TEST(test_processor_neg());
    DIANA_TEST(test_processor_neg2());
    DIANA_TEST(test_processor_nop());
    DIANA_TEST(test_processor_nop64());
    //test_processor_nop64_2();
}