#include "test_processor_m.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static void test_processor_movs()
{
    unsigned char code[1024] = 
    {
        0xF3, 0xA4                      //rep movs    byte ptr es:[edi],byte ptr [esi] 
    }; 

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ESI(100);
    SET_REG_EDI(200);
    SET_REG_ECX(7);

    memcpy(code+100, "hello!", 7);
    memset(code+200, '*', 7);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(memcmp(code+200, "hello!", 7) == 0);
}

static void test_processor_movsxd64()
{
    unsigned char code[9] = 
    {
        0x48, 0x63, 0x4C, 0x24, 0x04, 1, 2, 3, 0x80        //movsxd  rcx, [rsp+4]
    }; 

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RSP(1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RCX == 0xFFFFFFFF80030201ULL);
}


static void test_processor_movsxd64_2()
{
    unsigned char code[9] = 
    {
        0x63, 0x4C, 0x24, 0x04, 1, 2, 3, 0x80        //movsxd  rcx, [rsp+4]
    }; 

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RCX == 0xFFFFFFFF80030201ULL);
}


static void test_processor_mov64()
{
    unsigned char code[] = {0x48, 0xb8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; 
    // 48b8ffffffffffffffff mov rax,0FFFFFFFFFFFFFFFFh

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFFFFFFFFFFFFFFULL);
}


static void test_processor_mov64_2()
{
    unsigned char code[] = {0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff}; 
    // 48c7c0ffffffff      rax,0FFFFFFFFFFFFFFFFh

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFFFFFFFFFFFFFFULL);
}

static void test_processor_mov64_3()
{
    unsigned char code[] = {0xc7, 0xc0, 0x55, 0x55, 0x55, 0x55}; 
    // mov eax,-1

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000055555555ULL);
}


static void test_processor_mov64_4()
{
    unsigned char code[] = {0xb8, 0x55, 0x55, 0x55, 0x55}; 
    // mov eax,-1

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x0000000055555555ULL);
}


static void test_processor_mov()
{
    unsigned char code[] = {0xb8, 0x55, 0x55, 0x55, 0x55}; 
    // mov eax,-1

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFFFFFF55555555ULL);
}


static void test_processor_mul64()
{
    unsigned char code[] = {0x48, 0xf7, 0xe1};
    // mul rcx

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


static void test_processor_mul64_2()
{
    unsigned char code[] = {0x48, 0xf7, 0xe3};
    // mul rbx

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RDX(0);
    SET_REG_RAX(23);
    SET_REG_RBX(-45);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFFFFFFFFFFFBF5ULL);
    DIANA_TEST_ASSERT(GET_REG_RDX == 0x0000000000000016ULL);
}


static void test_processor_mul64_3()
{
    unsigned char code[] = {0x48, 0xf7, 0xe6};
    // mul rsi

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


static void test_processor_mul64_4()
{
    unsigned char code[] = {0x49, 0xf7, 0xe6};
    // mul r14

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

static void test_processor_mov64_5()
{
    // mov b,[10],-1
    // mov rbx,[10]
    unsigned char code[] = {0xc6, 0x05, 0x09, 0x00, 0x00, 0x00, 0xff, 0x48, 0x8b, 0x1d, 0x02, 0x00, 0x00, 0x00, 0xaa, 0xaa, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xaa, 0xaa };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.Exec( 2 );
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT( 0xFF == GET_REG_BL );
    DIANA_TEST_ASSERT( 0x55FF == GET_REG_BX );
    DIANA_TEST_ASSERT( 0x555555FF == GET_REG_EBX );
    DIANA_TEST_ASSERT( 0x55555555555555FFULL == GET_REG_RBX );
}

static void test_processor_mov64_6()
{
    // mov d,[rsi],12345678h
    unsigned char code[] = {0xc7, 0x06, 0x78, 0x56, 0x34, 0x12, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xaa};

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RSI( 8 );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT( 0xaa == code[ 0x07 ] );
    DIANA_TEST_ASSERT( 0x78 == code[ 0x08 ] );
    DIANA_TEST_ASSERT( 0x56 == code[ 0x09 ] );
    DIANA_TEST_ASSERT( 0x34 == code[ 0x0a ] );
    DIANA_TEST_ASSERT( 0x12 == code[ 0x0b ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0c ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0d ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0e ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0f ] );
    DIANA_TEST_ASSERT( 0xaa == code[ 0x10 ] );
}

static void test_processor_mov64_7()
{
    // mov q,[rsi],12345678h
    unsigned char code[] = {0x48, 0xc7, 0x06, 0x78, 0x56, 0x34, 0x12, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa};

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RSI( 8 );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT( 0xaa == code[ 0x07 ] );
    DIANA_TEST_ASSERT( 0x78 == code[ 0x08 ] );
    DIANA_TEST_ASSERT( 0x56 == code[ 0x09 ] );
    DIANA_TEST_ASSERT( 0x34 == code[ 0x0a ] );
    DIANA_TEST_ASSERT( 0x12 == code[ 0x0b ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0c ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0d ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0e ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0f ] );
    DIANA_TEST_ASSERT( 0xaa == code[ 0x10 ] );
}

static void test_processor_mov64_8()
{
    // mov d,[rsi],12345678h
    unsigned char code[] = {0xc7, 0x06, 0x78, 0x56, 0x34, 0x82, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0xaa};

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RSI( 8 );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT( 0xaa == code[ 0x07 ] );
    DIANA_TEST_ASSERT( 0x78 == code[ 0x08 ] );
    DIANA_TEST_ASSERT( 0x56 == code[ 0x09 ] );
    DIANA_TEST_ASSERT( 0x34 == code[ 0x0a ] );
    DIANA_TEST_ASSERT( 0x82 == code[ 0x0b ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0c ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0d ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0e ] );
    DIANA_TEST_ASSERT( 0x00 == code[ 0x0f ] );
    DIANA_TEST_ASSERT( 0xaa == code[ 0x10 ] );
}

static void test_processor_mov64_9()
{
    // mov q,[rsi],82345678h
    unsigned char code[] = {0x48, 0xc7, 0x06, 0x78, 0x56, 0x34, 0x82, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa};

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RSI( 8 );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT( 0xaa == code[ 0x07 ] );
    DIANA_TEST_ASSERT( 0x78 == code[ 0x08 ] );
    DIANA_TEST_ASSERT( 0x56 == code[ 0x09 ] );
    DIANA_TEST_ASSERT( 0x34 == code[ 0x0a ] );
    DIANA_TEST_ASSERT( 0x82 == code[ 0x0b ] );
    DIANA_TEST_ASSERT( 0xff == code[ 0x0c ] );
    DIANA_TEST_ASSERT( 0xff == code[ 0x0d ] );
    DIANA_TEST_ASSERT( 0xff == code[ 0x0e ] );
    DIANA_TEST_ASSERT( 0xff == code[ 0x0f ] );
    DIANA_TEST_ASSERT( 0xaa == code[ 0x10 ] );
}

static void test_processor_mov64_10()
{
    // mov b,[4],-1
    // mov rax,[4]
    unsigned char code[] = {
        0xaa, 0xaa, 0xaa, 0xaa,
        0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        0xaa, 0xaa, 0xaa, 0xaa,
        0xc6, 0x05, 0xed, 0xff, 0xff, 0xff, 0xff,
        0x48, 0x8b, 0x05, 0xe6, 0xff, 0xff, 0xff
    };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RIP( 0x10 );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT( 0xaa == code[ 0x03 ] );
    DIANA_TEST_ASSERT( 0xff == code[ 0x04 ] );
    DIANA_TEST_ASSERT( 0x55 == code[ 0x05 ] );
    int res2 = proc.ExecOnce();
    DIANA_TEST_ASSERT(res2 == DI_SUCCESS);
    DIANA_TEST_ASSERT( 0x55555555555555FFULL == GET_REG_RAX );
}

static void test_processor_movzx_64()
{
    unsigned char code[] = {
        0x41, 0x0f, 0xb7, 0x0c, 0x43      // movzx   ecx,word ptr [r11+rax*2]
    };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RCX(0x1020304050607080ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE valRcx = GET_REG_RCX;
    DIANA_TEST_ASSERT(valRcx == 0x0000000000000f41);
}

void test_processor_m()
{
    DIANA_TEST(test_processor_movzx_64());
    DIANA_TEST(test_processor_movs());
    DIANA_TEST(test_processor_movsxd64());
    DIANA_TEST(test_processor_movsxd64_2());
    DIANA_TEST(test_processor_mov64());
    DIANA_TEST(test_processor_mov64_2());
    DIANA_TEST(test_processor_mov64_3());
    DIANA_TEST(test_processor_mov64_4());
    DIANA_TEST(test_processor_mov());

    DIANA_TEST(test_processor_mul64());
    DIANA_TEST(test_processor_mul64_2());
    DIANA_TEST(test_processor_mul64_3());
    DIANA_TEST(test_processor_mul64_4());

    // "RIP-relative" addressing
    DIANA_TEST(test_processor_mov64_5());
    DIANA_TEST(test_processor_mov64_6());
    DIANA_TEST(test_processor_mov64_7());
    DIANA_TEST(test_processor_mov64_8());
    DIANA_TEST(test_processor_mov64_9());
    DIANA_TEST(test_processor_mov64_10());
}
