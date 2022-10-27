#include "test_processor_call.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static void test_processor_call_1()
{
    // test normal sub 
    unsigned char code[] = {0xFF, 0xD0}; // call        eax  

    unsigned char procedure[] = {0xBA, 0x01, 0x00, 0x00, 0x00, //   mov         edx,1 
                                 0xC3};                        //   ret   

    const int procOffset = 4;
    const int startESP = 0x1000;
    std::vector<unsigned char> memory(0x1000);
    memcpy(&memory.front(), code, sizeof(code));
    memcpy(&memory.front() + procOffset, procedure, sizeof(procedure));

    CTestProcessor proc(&memory.front(), memory.size());
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EAX(procOffset);
    SET_REG_RSP(startESP);

    // exec call
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RIP == procOffset);
    DIANA_TEST_ASSERT(GET_REG_ESP == startESP-4);

    // exec mov
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    // exec ret
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_ESP == startESP);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 2);

    DIANA_TEST_ASSERT(GET_REG_EDX == 1);
}
static void test_processor_call_2()
{
    // test normal sub 
    unsigned char code[] = {0x6A, 0x05,                     // push        5 
                            0xE8, 0x0B, 0x00, 0x00, 0x00};  // call        $+10  

    unsigned char procedure[] = {0xBA, 0x01, 0x00, 0x00, 0x00, //   mov         edx,1 
                                 0xC2, 0x04, 0x00};            //   ret   4

    const int procOffset = 0x12;
    const int startESP = 0x1000;
    std::vector<unsigned char> memory(0x1000);
    memcpy(&memory.front(), code, sizeof(code));
    memcpy(&memory.front() + procOffset, procedure, sizeof(procedure));

    CTestProcessor proc(&memory.front(), memory.size());
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EAX(procOffset);
    SET_REG_RSP(startESP);

    // exec call
    int res = proc.Exec(4);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_ESP == startESP);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == sizeof(code));

    DIANA_TEST_ASSERT(GET_REG_EDX == 1);
}


static void test_processor_call_3()
{
    // test normal sub 
    unsigned char code[] = {0x65, 0xFF, 0x15, 0x0A, 0x00, 0x00, 0x00};  // call        dword ptr gs:[0Ah] 

    unsigned char procedure[] = {0xBA, 0x11, 0x00, 0x00, 0x00, //   mov         edx, 11
                                 0xC3};                        //   ret 

    const int procOffset = 0x12;
    const int startESP = 0x1000;
    const int segSize = 0x1000;
    std::vector<unsigned char> memory(0x2000);
    memcpy(&memory.front(), code, sizeof(code));
    memcpy(&memory.front() + procOffset, procedure, sizeof(procedure));
    memcpy(&memory.front() + segSize + 0xA, &procOffset, sizeof(procOffset));

    CTestProcessor proc(&memory.front(), memory.size(), segSize);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_GS(1);
    SET_REG_EAX(procOffset);
    SET_REG_RSP(startESP);

    // exec call
    int res = proc.Exec(3);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_ESP == startESP);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == sizeof(code));

    DIANA_TEST_ASSERT(GET_REG_EDX == 0x11);
}



static void test_processor_call_4()
{
    // test normal sub 
    unsigned char code[] = {0x9A, 0x12, 0x00, 0x00, 0x00, 0x01, 0x00};  // call        01:0xA

    unsigned char procedure[] = {0xBA, 0x11, 0x00, 0x00, 0x00, //   mov         edx, 11
                                 0xC3};                        //   ret 

    const int segSize = 0x1000;
    const int procOffset = segSize+ 0x12;
    const int startESP = 0x1000;
    std::vector<unsigned char> memory(0x2000);
    memcpy(&memory.front(), code, sizeof(code));
    memcpy(&memory.front() + procOffset, procedure, sizeof(procedure));

    CTestProcessor proc(&memory.front(), memory.size(), segSize);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RSP(startESP);

    // exec call
    int res = proc.Exec(3);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_ESP == startESP);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == sizeof(code));

    DIANA_TEST_ASSERT(GET_REG_EDX == 0x11);
}

void test_processor_call()
{
    DIANA_TEST(test_processor_call_1());
    DIANA_TEST(test_processor_call_2());
    DIANA_TEST(test_processor_call_3());
    DIANA_TEST(test_processor_call_4());
}