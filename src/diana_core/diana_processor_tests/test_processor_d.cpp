#include "test_processor_d.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static void test_processor_div()
{
    // div         edx:eax,ecx 
    unsigned char code[] = {0xF7, 0xF1};      
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(2);
    SET_REG_EAX(1);
    SET_REG_ECX(0x10);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0x20000000);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x1);
    DIANA_TEST_ASSERT(GET_REG_RCX == 0x10);
}

static void test_processor_div2()
{
    // div         dx:ax, cx 
    unsigned char code[] = {0x66, 0xF7, 0xF1};      
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(2);
    SET_REG_EAX(1);
    SET_REG_ECX(0x10);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0x00002000);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x1);
    DIANA_TEST_ASSERT(GET_REG_RCX == 0x10);
}



static void test_processor_div3()
{
    // div         ax, cl 
    unsigned char code[] = {0xF6, 0xF1}; 
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EAX(0x123);
    SET_REG_ECX(0x10);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0x312);

}


static void test_processor_div_fail()
{
    // div         edx:eax,ecx 
    unsigned char code[] = {0xF7, 0xF1};      
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(0xFF);
    SET_REG_EAX(1);
    SET_REG_ECX(0x10);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_DIVISION_OVERFLOW);
    
}


static void test_processor_dec()
{
    unsigned char code[] = {0xf0, 0xff, 0x4a, 0x04, 1,1,1,1, 1,0,0,0};//         lock    dec dword ptr [edx+0x4]
    unsigned char etalon[] = {0xf0, 0xff, 0x4a, 0x04, 1,1,1,1, 0,0,0,0};
    unsigned char etalon2[] = {0xf0, 0xff, 0x4a, 0x04, 1,1,1,1, 0xFF,0xFF,0xFF,0xFF};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(4);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(memcmp(code, etalon, sizeof(code))==0);

    DI_JUMP_TO_RIP(0);

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(memcmp(code, etalon2, sizeof(code))==0);

}
void test_processor_d()
{
    DIANA_TEST(test_processor_div());
    DIANA_TEST(test_processor_div2());
    DIANA_TEST(test_processor_div3());
    DIANA_TEST(test_processor_div_fail());
    DIANA_TEST(test_processor_dec());
}
