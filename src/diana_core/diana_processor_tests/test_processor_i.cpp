#include "test_processor_i.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static void test_processor_idiv()
{
    unsigned char code[] = {0xF7, 0xF9};      
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(2);
    SET_REG_EAX(1);
    SET_REG_ECX(-10);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0xCCCCCCCD);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x00000003);
    DIANA_TEST_ASSERT(GET_REG_RCX == 0xFFFFFFF6);
}


static void test_processor_idiv2()
{
    unsigned char code[] = {0xF6, 0xFA}; // idiv dl
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(0x127E);
    SET_REG_EAX(0x12341678);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    OPERAND_SIZE rax = GET_REG_RAX;
    DIANA_TEST_ASSERT(rax == 0x1234522D);
}


static void test_processor_imul()
{
    unsigned char code[] = {0xF6, 0xEB}; //imul        bl      
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_BL((DI_CHAR)-2);
    SET_REG_AL((DI_CHAR)-128);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0x100);
    DIANA_TEST_ASSERT(GET_FLAG_CF);
    DIANA_TEST_ASSERT(GET_FLAG_OF);
}

static void test_processor_imul2()
{
    unsigned char code[] = {0xF6, 0xEB}; //imul        bl      
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_BL((DI_CHAR)2);
    SET_REG_AL((DI_CHAR)-128);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFF00);
    DIANA_TEST_ASSERT(GET_FLAG_CF);
    DIANA_TEST_ASSERT(GET_FLAG_OF);
}

static void test_processor_imul3()
{
    unsigned char code[] = {0xF6, 0xEB}; //imul        bl      
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_BL((DI_CHAR)1);
    SET_REG_AL((DI_CHAR)-128);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFF80);
    DIANA_TEST_ASSERT(!GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_imul4()
{
    unsigned char code[] = {0xF6, 0xEB}; //imul        bl      
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_BL((DI_CHAR)-1);
    SET_REG_AL((DI_CHAR)127);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFF81);
    DIANA_TEST_ASSERT(!GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_imul5()
{
    unsigned char code[] = {0x6B,0xC2,0x02}; // imul        eax,edx,2 
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(0x7FFFFFFF);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFFFFFe);
    DIANA_TEST_ASSERT(GET_FLAG_CF);
    DIANA_TEST_ASSERT(GET_FLAG_OF);
}

static void test_processor_imul6()
{
    unsigned char code[] = {0x6B,0xC2,0x02}; // imul        eax,edx,2 
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(0xFFFFFFFF);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFFFFFe);
    DIANA_TEST_ASSERT(!GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_imul7()
{
    unsigned char code[] = {0x0F, 0xAF, 0xC2}; // imul        eax,edx
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EAX(-23455);
    SET_REG_EDX(5636);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0xF81EE784);
    DIANA_TEST_ASSERT(!GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_imul8()
{
    unsigned char code[] = {0xf7, 0xea}; // imul        edx
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EAX(0x40000000);
    SET_REG_EDX(0x00000130);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0x00000000);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x0000004c);
}

static void test_processor_imul9()
{
    unsigned char code[] = {0x66, 0x6B, 0xC2, 0xFF}; // imul        ax,dx,-1
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(0x2);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
        
    DIANA_TEST_ASSERT(GET_REG_RAX == 65534);
    DIANA_TEST_ASSERT(!GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_imul10()
{
    unsigned char code[] = {0x66, 0x6B, 0xC2, 0xD3}; // imul        ax,dx,0FFD3h
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(0x17);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
        
    DIANA_TEST_ASSERT(GET_REG_RAX == 0xfbf5);
    DIANA_TEST_ASSERT(!GET_FLAG_CF);
    DIANA_TEST_ASSERT(!GET_FLAG_OF);
}

static void test_processor_inc()
{
    unsigned char code[] = {0xF0, 0xFF, 0x42, 0x04, 0xff,0xff,0xff,0xff}; // lock inc    dword ptr [edx+4] 
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EDX(0);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_FLAG_ZF);
}

static void test_processor_imul64()
{
    unsigned char code[] = {0x48, 0xf7, 0xe9};
    // imul rcx

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(0x1234001d1234001dULL);
    SET_REG_RCX(45);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x3324051C33240519ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x0000000000000003ULL);
}

static void test_processor_imul64_2()
{
    unsigned char code[] = {0x48, 0xf7, 0xeb};
    // imul rbx

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(23);
    SET_REG_RBX(-45);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFFFFFFFFFFFBF5ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0xFFFFFFFFFFFFFFFFULL);
}

static void test_processor_imul64_3()
{
    unsigned char code[] = {0x48, 0xf7, 0xee};
    // imul rsi

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(0x8000000000000000ULL);
    SET_REG_RSI(0x8000000000000000ULL);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000000000000ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x4000000000000000ULL);
}

static void test_processor_imul64_4()
{
    unsigned char code[] = {0x49, 0xf7, 0xee};
    // imul r14

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(0x100000000);
    SET_REG_R14(0x100000000);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000000000000ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x0000000000000001ULL);
}

static void test_processor_div64()
{
    unsigned char code[] = {0x48, 0xf7, 0xf1};
    // div rcx

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(0x1234001d1234001dULL);
    SET_REG_RCX(12347);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000609E930653F8ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x00000000000025F5ULL);
}

static void test_processor_div64_2()
{
    unsigned char code[] = {0x49, 0xf7, 0xf1};
    // div r9

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(-233223);
    SET_REG_R9(-45);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000000000000ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0xFFFFFFFFFFFC70F9ULL);
}

static void test_processor_div64_3()
{
    unsigned char code[] = {0x48, 0xf7, 0xf3};
    // div rbx

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(0x8000000000000000);
    SET_REG_RBX(-1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000000000000ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x8000000000000000ULL);
}

static void test_processor_div64_4()
{
    unsigned char code[] = {0x48, 0xf7, 0xf6};
    // div rsi

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0x12343);
    SET_REG_RAX(0x12345678);
    SET_REG_RSI(0x81234567);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x000241641D54BAFFULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x000000005ED95EDFULL);
}

static void test_processor_idiv64()
{
    unsigned char code[] = {0x48, 0xf7, 0xf9};
    // idiv rcx

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(0x1234001d1234001dULL);
    SET_REG_RCX(12347);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000609E930653F8ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x00000000000025F5ULL);
}

static void test_processor_idiv64_2()
{
    unsigned char code[] = {0x49, 0xf7, 0xf9};
    // idiv r9

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(-233223);//FFFFFFFFFFFC70F9
    SET_REG_R9(-45);//FFFFFFFFFFFFFFD3

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFA4FA4FA4FA50E8FULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x000000000000001CULL);
}

static void test_processor_idiv64_3()
{
    unsigned char code[] = {0x48, 0xf7, 0xfb};
    // idiv rbx

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(0x8000000000000000);
    SET_REG_RBX(-1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x8000000000000000ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x0000000000000000ULL);
}

static void test_processor_idiv64_4()
{
    unsigned char code[] = {0x48, 0xf7, 0xfe};
    // idiv rsi

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0x12343);
    SET_REG_RAX(0x12345678);
    SET_REG_RSI(0x81234567);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x000241641D54BAFFULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x000000005ED95EDFULL);
}

static void test_processor_invalid_lock()
{
    unsigned char code[] = {0xF0, 0x41}; // lock inc ecx
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_INVALID_OPCODE);
}

void test_processor_i()
{
    DIANA_TEST(test_processor_idiv());
    DIANA_TEST(test_processor_idiv2());
    DIANA_TEST(test_processor_imul());
    DIANA_TEST(test_processor_imul2());
    DIANA_TEST(test_processor_imul3());
    DIANA_TEST(test_processor_imul4());
    DIANA_TEST(test_processor_imul5());
    DIANA_TEST(test_processor_imul6());
    DIANA_TEST(test_processor_imul7());
    DIANA_TEST(test_processor_imul8());
    DIANA_TEST(test_processor_imul9());
    DIANA_TEST(test_processor_imul10());
    DIANA_TEST(test_processor_inc());

    DIANA_TEST(test_processor_imul64());
    DIANA_TEST(test_processor_imul64_2());
    DIANA_TEST(test_processor_imul64_3());
    DIANA_TEST(test_processor_imul64_4());
    DIANA_TEST(test_processor_div64());
    DIANA_TEST(test_processor_div64_2());
    DIANA_TEST(test_processor_div64_3());
    DIANA_TEST(test_processor_div64_4());
    DIANA_TEST(test_processor_idiv64());
    DIANA_TEST(test_processor_idiv64_2());
    DIANA_TEST(test_processor_idiv64_3());
    DIANA_TEST(test_processor_idiv64_4());
    DIANA_TEST(test_processor_invalid_lock());
}
