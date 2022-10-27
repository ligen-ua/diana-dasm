#include "test_processor_a.h"
#include "test_common.h"
#include "test_processor_impl.h"

static void test_processor_and()
{
    unsigned char buff[] = {0x24, 0x01}; //and         al,1 
    
    CTestProcessor proc(buff, sizeof(buff));
    DianaProcessor * pCallContext = proc.GetSelf();

    pCallContext->m_flags.impl.l.value = 0x256;

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(pCallContext->m_flags.impl.l.value == 0x246);
}


static void test_processor_and2()
{
    unsigned char buff[] = {0x83, 0x66, 0x00, 0x00,//and  [esi], 0
                            0,0,0,0
    }; 
    
    CTestProcessor proc(buff, sizeof(buff));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ESI(4);
    pCallContext->m_flags.impl.l.value = 0x206;

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(pCallContext->m_flags.impl.l.value == 0x246);
}


static void test_processor_adc()
{
    unsigned char buff[] = {0x83, 0xd3, 0xff};           //adc     ebx,0xffffffff
    
    CTestProcessor proc(buff, sizeof(buff));
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_EBX == 0xffffffff);
}


static void test_processor_and64()
{
    unsigned char buff[] = {0x25, 0xf0, 0xf0, 0xf0, 0xf0};
    //and eax,0f0f0f0f0h

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0x00000000F0F0F0F0ULL);
}


static void test_processor_and64_2()
{
    unsigned char buff[] = {0x66, 0x25, 0xf0, 0xf0, 0xf0, 0xf0};
    //and ax,0f0f0h

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0xFFFFFFFFFFFFFFFFULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RIP == 0x0004);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFFFFFFFFFFF0F0ULL);
}

static void test_processor_and64_3()
{
    // and rsp, 0xfffffffffffffff0
    unsigned char buff[] = {0x48, 0x83, 0xe4, 0xf0};

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RSP(0x000000000006FF58ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RSP == 0x000000000006FF50ULL);
}

static void test_processor_aad()
{
    // aad 7
    unsigned char buff[] = {0xd5, 0x07};

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0x325ULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RAX == 0x3AULL);
}

static void test_processor_aam()
{
    // aam 3
    unsigned char buff[] = {0xd4, 0x03};

    CTestProcessor proc(buff, sizeof(buff), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RAX(0x3AULL);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_RAX == 0x1301ULL);
}

void test_processor_a()
{
    DIANA_TEST(test_processor_and());
    DIANA_TEST(test_processor_and2());
    DIANA_TEST(test_processor_adc());
    DIANA_TEST(test_processor_and64());
    DIANA_TEST(test_processor_and64_2());
    DIANA_TEST(test_processor_and64_3());
    DIANA_TEST(test_processor_aad());
    DIANA_TEST(test_processor_aam());
}