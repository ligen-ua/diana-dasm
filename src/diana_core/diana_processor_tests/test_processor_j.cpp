#include "test_processor_j.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static void test_processor_jmp()
{
    // jmp $+5
    unsigned char code[] = {0xE9, 0x00, 0x00, 0x00, 0x00};      
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RIP == 5);
}

static void test_processor_jmp2()
{
    // jmp eax
    unsigned char code[] = {0xFF, 0xE0};
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    
    SET_REG_EAX(1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RIP == 1)
}


static void test_processor_jmp3()
{
    // jmp         dword ptr es:[0Ah] 
    unsigned char code[] = {0x26, 0xFF, 0x25, 0x0A, 0x00, 0x00, 0x00};  

    const int segSize = 0x1000;
    std::vector<unsigned char> memory(0x2000);
    memcpy(&memory.front(), code, sizeof(code));

    CTestProcessor proc(&memory.front(), memory.size(), segSize);
    DianaProcessor * pCallContext = proc.GetSelf();

    OPERAND_SIZE newIP = 111;
    int res = DianaProcessor_SetMemValue(pCallContext, 
                               0,
                               0x100A,
                               4,
                               &newIP,
                               0,
                               reg_DS);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    SET_REG_ES(1);

    // exec call
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == newIP);
}


static void test_processor_jmp4()
{
    // jle         cc (10CBD4Fh) 
    unsigned char code[] = {0x7E, 0x2F};

    const OPERAND_SIZE newIP = 0x31;

    const int segSize = 0x1000;
    std::vector<unsigned char> memory(0x2000);
    memcpy(&memory.front(), code, sizeof(code));

    CTestProcessor proc(&memory.front(), memory.size(), segSize);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_FLAG_SF;

    SET_REG_ES(1);

    // exec call
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == newIP);
}

static void test_processor_je()
{
    // je 5
    unsigned char code[] = {0x66, 0x0F, 0x84, 0x00, 0x00};

    const int segSize = 0x1000;
    std::vector<unsigned char> memory(0x2000);
    memcpy(&memory.front(), code, sizeof(code));
    CTestProcessor proc(&memory.front(), memory.size(),segSize);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_FLAG_ZF;
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT( 0x0005 == GET_REG_RIP );
}

static void test_processor_jmp_middle()
{
    // jumping to the middle of an instruction.
    unsigned char buff[] = {
        0xeb, 0xff,            // JMP 1
        0xc0, 0xc0, 0x01    // INC EAX, not ROL AL,1
    };

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EAX( 0x0011 );
    int res = proc.Exec( 2 );
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT( 0x0012 == GET_REG_EAX );
    DIANA_TEST_ASSERT( 0x0003 == GET_REG_RIP );
}

static void test_processor_jecxz()
{
    // jecxz 0
    unsigned char buff[] = { 0xe3, 0xfe };

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ECX( 0x0ULL );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT( 0x0ULL == GET_REG_RIP );
}

static void test_processor_jecxz2()
{
    // jecxz 0
    unsigned char buff[] = { 0xe3, 0xfe };

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ECX( 0x1ULL );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT( 0x2ULL == GET_REG_RIP );
}

static void test_processor_jecxz3()
{
    // jcxz 0
    unsigned char buff[] = { 0x67, 0xe3, 0xfd };

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ECX( 0x0ULL );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT( 0x0ULL == GET_REG_RIP );
}

static void test_processor_jecxz4()
{
    // jcxz 0
    unsigned char buff[] = { 0x67, 0xe3, 0xfd };

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ECX( 0x1ULL );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT( 0x3ULL == GET_REG_RIP );
}

static void test_processor_jecxz5()
{
    // jcxz 0
    unsigned char buff[] = { 0x67, 0xe3, 0xfd };

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ECX( 0x00010000ULL );
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT( 0x3ULL == GET_REG_RIP );
}

void test_processor_j()
{
    DIANA_TEST(test_processor_jmp());
    DIANA_TEST(test_processor_jmp2());
    DIANA_TEST(test_processor_jmp3());
    DIANA_TEST(test_processor_jmp4());
    DIANA_TEST(test_processor_je());
    DIANA_TEST(test_processor_jmp_middle());
    DIANA_TEST(test_processor_jecxz());
    DIANA_TEST(test_processor_jecxz2());
    DIANA_TEST(test_processor_jecxz3());
    DIANA_TEST(test_processor_jecxz4());
    DIANA_TEST(test_processor_jecxz5());
}
