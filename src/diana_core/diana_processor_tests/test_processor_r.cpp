#include "test_processor_r.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static void test_processor_rcl()
{
    // rcl eax, cl
    unsigned char code[] = {0xD3, 0xD0};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_RAX(0xA5A5A5A5);
    SET_REG_CL(5);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE eax = GET_REG_EAX;
    DIANA_TEST_ASSERT(eax == 0xb4b4b4aa);    
    DIANA_TEST_ASSERT(GET_REG_RIP == 2);

    DIANA_TEST_ASSERT(!GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_rcl2()
{
    // rcl eax, cl
    unsigned char code[] = {0xD3, 0xD0};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_RAX(0xA5A5A5A5);
    SET_REG_CL(1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE eax = GET_REG_EAX;
    DIANA_TEST_ASSERT(eax == 0x4B4B4B4A);
    DIANA_TEST_ASSERT(GET_REG_RIP == 2);

    DIANA_TEST_ASSERT(GET_FLAG_CF);
    DIANA_TEST_ASSERT(GET_FLAG_OF);
}

static void test_processor_rcl3()
{
    // rcl eax, cl
    unsigned char code[] = {0xD3, 0xD0};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_RAX(0xA5A5A5A5);
    SET_FLAG_CF;
    SET_REG_CL(2);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE eax = GET_REG_EAX;
    DIANA_TEST_ASSERT(eax == 0x96969697);
    DIANA_TEST_ASSERT(GET_REG_RIP == 2);

    DIANA_TEST_ASSERT(!GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_rcr()
{
    // rcr eax, 1
    unsigned char code[] = {0xD1, 0xD8};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_RAX(0xA5A5A5A5);
    SET_FLAG_CF;

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE eax = GET_REG_EAX;
    DIANA_TEST_ASSERT(eax == 0xD2D2D2D2);
    DIANA_TEST_ASSERT(GET_REG_RIP == 2);

    DIANA_TEST_ASSERT(GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_rol()
{
    // rol eax, 1
    unsigned char code[] = {0xD1, 0xC0};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_RAX(0xA5A5A5A4);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE eax = GET_REG_EAX;
    DIANA_TEST_ASSERT(eax == 0x4B4B4B49);
    DIANA_TEST_ASSERT(GET_REG_RIP == 2);

    DIANA_TEST_ASSERT(GET_FLAG_CF);
    DIANA_TEST_ASSERT(GET_FLAG_OF);
}

static void test_processor_ror()
{
    // ror eax, 1
    unsigned char code[] = {0xD1, 0xC8};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_RAX(0xA5A5A5A5);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE eax = GET_REG_EAX;
    DIANA_TEST_ASSERT(eax == 0xD2D2D2D2);
    DIANA_TEST_ASSERT(GET_REG_RIP == 2);

    DIANA_TEST_ASSERT(GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_ror2()
{
    // ror eax, 1
    unsigned char code[] = {0xD1, 0xC8};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_RAX(0x75A5A5A5);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE eax = GET_REG_EAX;
    DIANA_TEST_ASSERT(eax == 0xBAD2D2D2);
    DIANA_TEST_ASSERT(GET_REG_RIP == 2);

    DIANA_TEST_ASSERT(GET_FLAG_CF);
    DIANA_TEST_ASSERT(GET_FLAG_OF);
}

static void test_processor_ret64()
{
    // ret
    unsigned char code[] = {0xC3, 0x01, 0x01, 0x01, 
                            0x01, 0x01, 0x01, 0xFF};
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_RSP(0);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RIP == 0xff010101010101c3);
    DIANA_TEST_ASSERT(GET_REG_RSP == 8);
}

static void test_processor_retf()
{
    // retf 
    unsigned char code[] = {0xca, 0x22, 0x00, 0, 0x23, 0,0,1};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_RSP(0);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RIP == 0x22ca);
    DIANA_TEST_ASSERT(GET_REG_CS == 0x23);
    DIANA_TEST_ASSERT(GET_REG_RSP == 0x2a);
}

static void test_processor_rep_add()
{
    // rep add eax,eax
    unsigned char code[] = {0xF3, 0x03, 0xC0};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(1);
    SET_REG_ECX(3);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 2);
    DIANA_TEST_ASSERT(GET_REG_RCX == 3);
}

void test_processor_r()
{
    DIANA_TEST(test_processor_rcl());
    DIANA_TEST(test_processor_rcl2());
    DIANA_TEST(test_processor_rcl3());
    
    DIANA_TEST(test_processor_rcr());
    DIANA_TEST(test_processor_rol());

    DIANA_TEST(test_processor_ror());
    DIANA_TEST(test_processor_ror2());

    DIANA_TEST(test_processor_ret64());
    DIANA_TEST(test_processor_retf());
    
    DIANA_TEST(test_processor_rep_add());
}