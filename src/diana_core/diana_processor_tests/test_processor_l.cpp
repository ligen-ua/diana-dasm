#include "test_processor_l.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static void test_processor_lea()
{
    // lea         ebx,[esp+ebx*2] 
    unsigned char code[] = {0x8D, 0x1C, 0x5C};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ESP(1);
    SET_REG_EBX(6);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_EBX == 13);
}

static void test_processor_lea2()
{
    // lea rax,[rax+05b8c110h] 
    unsigned char code[] = {0x48, 0x8d, 0x80, 0x10, 0xc1, 0xb8, 0x05 };

    CTestProcessor proc(code, sizeof(code),0,DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0);

    int res = proc.ExecOnce();

    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(0x05b8c110ULL == GET_REG_RAX);
}

static void test_processor_lea_64()
{
    DI_CHAR code[] = 
    {
        0x49, 0x83, 0xC8, 0xFF,      // or          r8,0FFFFFFFFFFFFFFFFh 
        0x33, 0xD2,                  // xor         edx,edx 
        0x44, 0x8D, 0x42, 0x30       // lea         r8d,[rdx+30h] 
    };            

    CTestProcessor proc(code, sizeof(code),0,DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0);

    // LEA should zero extend
    int res = proc.Exec(3);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE val_r8 = GET_REG_R8;
    DIANA_TEST_ASSERT(val_r8 == 0x30);
}

static void test_processor_lods()
{
    // lods    byte ptr [esi] 
    unsigned char code[] = {0xAC};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_ESI == 1);
    DIANA_TEST_ASSERT(GET_REG_EAX == 0xAC);
}

static void test_processor_lods2()
{
    // lods        word ptr [esi] 
    unsigned char code[] = {0x66, 0xAD};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_ESI == 2);
    DIANA_TEST_ASSERT(GET_REG_EAX == 0xAD66);
}

static void test_processor_lfs()
{
    // lfs ebx, [eax] ; fword
    unsigned char code[] = {0x0f, 0xb4, 0x18, 0x12, 0x34, 0x56};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_DS( 0x23ULL );
    SET_REG_RAX( 0x0ULL );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
}

static void test_processor_lfs2()
{
    // lfs bx, [eax] ; word
    unsigned char code[] = {0x66, 0x0f, 0xb4, 0x18};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_DS( 0x23ULL );
    SET_REG_RAX( 0x0ULL );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
}

void test_processor_l()
{
    DIANA_TEST(test_processor_lea());
    DIANA_TEST(test_processor_lea2());
    DIANA_TEST(test_processor_lods());
    DIANA_TEST(test_processor_lods2());
    DIANA_TEST(test_processor_lea_64());
    //test_processor_lfs();
    //test_processor_lfs2();
}
