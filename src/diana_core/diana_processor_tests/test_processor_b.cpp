#include "test_processor_b.h"
#include "test_common.h"
#include "test_processor_impl.h"



static void test_processor_bswap()
{
    // bswap ebx
    unsigned char buff[] = {0x0f, 0xcb};

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RBX(0x1000000012345678ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RIP == 0x0002);

    DIANA_TEST_ASSERT(GET_REG_RBX == 0x1000000078563412ULL);
}

static void test_processor_bswap_2()
{
    // undocumented
    // bswap ax
    unsigned char buff[] = {0x66, 0x0f, 0xc8};

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0x1000000012345678ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RIP == 0x0003);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x1000000012340000ULL);
}

static void test_processor_bswap64()
{
    // bswap ebx
    unsigned char buff[] = {0x0f, 0xcb};

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RBX(0x1234fdb512345678ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RIP == 0x0002);

    DIANA_TEST_ASSERT(GET_REG_RBX == 0x0000000078563412ULL);
}

static void test_processor_bswap64_2()
{
    // bswap rbx
    unsigned char buff[] = {0x48, 0x0f, 0xcb};

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RBX(0x1234fdb512345678ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RIP == 0x0003);

    DIANA_TEST_ASSERT(GET_REG_RBX == 0x78563412b5fd3412ULL);
}

void test_processor_b()
{
    DIANA_TEST(test_processor_bswap());
    DIANA_TEST(test_processor_bswap_2());
    DIANA_TEST(test_processor_bswap64());
    DIANA_TEST(test_processor_bswap64_2());
}
