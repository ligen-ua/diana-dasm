#include "test_processor_m.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"
#include <stdio.h>
#include <string.h>

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
        0x63, 0x4C, 0x24, 0x04, 1, 2, 3, 0x80        //movsxd  ecx, [rsp+4]
    }; 

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    // no REX.W: the destination is ecx, a 32-bit result clears the upper half of rcx
    DIANA_TEST_ASSERT(GET_REG_RCX == 0x0000000080030201ULL);
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

// the results of these bytes on an Intel Core i7-11800H (tests/x86 cpu checks, 2026-10-17)
static void test_processor_movsxd64_3()
{
    unsigned char code[9] = 
    {
        0x63, 0x4C, 0x24, 0x04, 1, 2, 3, 0x80        //movsxd  ecx, [rsp+4]
    }; 

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RCX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    // a 32-bit destination: the upper half of rcx is cleared, not filled with the sign
    DIANA_TEST_ASSERT(GET_REG_RCX == 0x0000000080030201ULL);
}

static void test_processor_movsxd64_4()
{
    unsigned char code[10] = 
    {
        0x66, 0x63, 0x4C, 0x24, 0x05, 1, 2, 3, 0x80        //movsxd  cx, [rsp+5]
    }; 

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RCX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    // 66: a 16-bit destination, the rest of rcx stays
    DIANA_TEST_ASSERT(GET_REG_RCX == 0xFFFFFFFFFFFF0201ULL);
}

// 64-bit mode: a write to a 32-bit register clears the upper half of the 64-bit one. rcx = all ones before, rbx = 64
// (the memory there: 0x80030201, 0x80030201), rdx = 0x40; the results of an Intel Core i7-11800H
struct ZeroExtendCase
{
    const char * pName;
    unsigned char bytes[8];
    int size;
    unsigned long long rcx;
};

static void test_processor_zero_extends()
{
    static const ZeroExtendCase cases[] =
    {
        {"movsxd ecx, [rbx]", {0x63, 0x0b}, 2, 0x0000000080030201ULL},
        {"movsxd cx, [rbx]", {0x66, 0x63, 0x0b}, 3, 0xffffffffffff0201ULL},
        {"inc ecx", {0xff, 0xc1}, 2, 0x0000000000000000ULL},
        {"dec ecx", {0xff, 0xc9}, 2, 0x00000000fffffffeULL},
        {"neg ecx", {0xf7, 0xd9}, 2, 0x0000000000000001ULL},
        {"not ecx", {0xf7, 0xd1}, 2, 0x0000000000000000ULL},
        {"imul ecx, ecx, 3", {0x6b, 0xc9, 0x03}, 3, 0x00000000fffffffdULL},
        {"imul ecx, [rbx]", {0x0f, 0xaf, 0x0b}, 3, 0x000000007ffcfdffULL},
        {"bsf ecx, [rbx]", {0x0f, 0xbc, 0x0b}, 3, 0x0000000000000000ULL},
        {"bsr ecx, [rbx]", {0x0f, 0xbd, 0x0b}, 3, 0x000000000000001fULL},
        {"movsx ecx, byte [rbx]", {0x0f, 0xbe, 0x0b}, 3, 0x0000000000000001ULL},
        {"xchg ecx, [rbx]", {0x87, 0x0b}, 2, 0x0000000080030201ULL},
        {"cmovz ecx, [rbx] (zf 0: no move)", {0x0f, 0x44, 0x0b}, 3, 0x00000000ffffffffULL},
        {"cmovnz ecx, [rbx]", {0x0f, 0x45, 0x0b}, 3, 0x0000000080030201ULL},
        {"movd ecx, xmm0", {0x66, 0x0f, 0x7e, 0xc1}, 4, 0x0000000000000000ULL},
        {"cvttsd2si ecx, [rbx+8]", {0xf2, 0x0f, 0x2c, 0x4b, 0x08}, 5, 0x0000000000000001ULL},
        {"mov cx, 1", {0x66, 0xb9, 0x01, 0x00}, 4, 0xffffffffffff0001ULL},
        {"mov cl, 1", {0xb1, 0x01}, 2, 0xffffffffffffff01ULL},
    };
    int failed = 0;
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i)
    {
        unsigned char code[128] = {0};
        memcpy(code, cases[i].bytes, cases[i].size);
        static const unsigned int data[4] = {0x80030201, 0x80030201, 0x8f5c28f6, 0x3ff4f5c2};   // [rbx+8]: 1.31
        memcpy(code + 64, data, sizeof(data));

        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
        DianaProcessor * pCallContext = proc.GetSelf();
        SET_REG_RCX(0xFFFFFFFFFFFFFFFFULL);
        SET_REG_RBX(64);
        SET_REG_RDX(0x40);
        int res = proc.ExecOnce();
        if (res != DI_SUCCESS || GET_REG_RCX != cases[i].rcx)
        {
            printf("%s: status %d, rcx %016llx, the cpu: %016llx\n", cases[i].pName, res,
                   (unsigned long long)GET_REG_RCX, cases[i].rcx);
            ++failed;
        }
    }
    DIANA_TEST_ASSERT(!failed);
}

void test_processor_m()
{
    DIANA_TEST(test_processor_movzx_64());
    DIANA_TEST(test_processor_movs());
    DIANA_TEST(test_processor_movsxd64());
    DIANA_TEST(test_processor_movsxd64_2());
    DIANA_TEST(test_processor_movsxd64_3());
    DIANA_TEST(test_processor_movsxd64_4());
    DIANA_TEST(test_processor_zero_extends());
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
