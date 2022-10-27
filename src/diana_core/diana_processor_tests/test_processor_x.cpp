#include "test_processor_x.h"
#include "test_common.h"
#include "test_processor_impl.h"

static void test_processor_xor()
{
    unsigned char buff[2] = {0x33, 0xc0}; // xor eax, eax

    CTestProcessor proc(buff, 2);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EAX(0x1234);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 2);

    DIANA_TEST_ASSERT(GET_REG_EAX == 0);
}


static void test_processor_xor64()
{
    unsigned char code[] = {0x48, 0x31, 0xc0}; 
    // xor rax,rax

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000000000000ULL);
}


static void test_processor_xor64_2()
{
    unsigned char code[] = {0x48, 0x33, 0xc0}; 
    // xor rax,rax

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000000000000ULL);
}


static void test_processor_xor64_3()
{
    unsigned char code[] = {0x33, 0xc0}; 
    // xor eax,eax

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000000000000ULL);
}


static void test_processor_xor64_4()
{
    unsigned char code[] = {0x31, 0xc0}; 
    // xor eax,eax

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000000000000ULL);
}

static void test_processor_xor64_5()
{
    unsigned char code[] = {0x31, 0xd8}; 
    // xor eax,ebx

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0x1111111111111111ULL);
    SET_REG_RBX(0x2222222222222222ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000033333333ULL);
}

void test_processor_x()
{
    DIANA_TEST(test_processor_xor());
    DIANA_TEST(test_processor_xor64());
    DIANA_TEST(test_processor_xor64_2());
    DIANA_TEST(test_processor_xor64_3());
    DIANA_TEST(test_processor_xor64_4());
    DIANA_TEST(test_processor_xor64_5());
}