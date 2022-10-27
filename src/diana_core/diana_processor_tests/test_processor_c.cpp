#include "test_processor_c.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "vector"

static void test_processor_c_1()
{
    // test normal cwde 
    unsigned char code[] = {0x66, 0x98, // cbw
                            0x98};      // cwde

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_AX(0xccFF);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFF);

    SET_REG_EAX(0xccccFFFF);
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xFFFFFFFF);
}

void test_processor_cmp()
{
    unsigned char code[] = {0x3B, 0xD9};//            cmp         ebx,ecx

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    proc.GetSelf()->m_flags.value = 0;
    SET_FLAG_IF;
    DianaProcessor_SetFlag(pCallContext,  DI_FLAG_1 );


    SET_REG_EBX(1);
    SET_REG_ECX(0xFFFFFFFF);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_EBX == 1);
    DIANA_TEST_ASSERT(GET_REG_ECX == 0xFFFFFFFF);

    DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.value == 0x213);
}


void test_processor_cmps()
{
    // repe cmps   byte ptr [esi],byte ptr es:[edi] 
    unsigned char code[] = {0xF3, 0xA6, 1, 2};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    proc.GetSelf()->m_flags.value = 0;
    SET_FLAG_IF;
    DianaProcessor_SetFlag(pCallContext,  DI_FLAG_1 );

    SET_REG_EDI(2);
    SET_REG_ESI(3);
    SET_REG_ECX(6);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_EDI == 3);
    DIANA_TEST_ASSERT(GET_REG_ESI == 4);
    DIANA_TEST_ASSERT(GET_REG_ECX == 5);

    DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.value == 0x202);
}

void test_processor_cmps2()
{
    // repe cmps   byte ptr [esi],byte ptr es:[edi] 
    unsigned char code[] = {0xF3, 0x66, 0xA7, 1, 0, 2, 0};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    proc.GetSelf()->m_flags.value = 0;
    SET_FLAG_IF;
    DianaProcessor_SetFlag(pCallContext,  DI_FLAG_1 );

    SET_REG_ESI(3);
    SET_REG_EDI(5);
    SET_REG_ECX(6);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_ESI == 5);
    DIANA_TEST_ASSERT(GET_REG_EDI == 7);
    DIANA_TEST_ASSERT(GET_REG_ECX == 5);

    DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.value == 0x297);
}


void test_processor_cmpxchg()
{
    //  lock cmpxchg dword ptr [ecx],edx
    unsigned char code[] = {0xF0, 0x0F, 0xB1, 0x11,
                            0, 0, 0, 0};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ECX(4);
    SET_REG_EAX(0);
    SET_REG_EDX(10);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_EAX == 0);
    DIANA_TEST_ASSERT(GET_FLAG_ZF);

    OPERAND_SIZE resVal = 0;
    DIANA_TEST_ASSERT(DI_SUCCESS ==
                DianaProcessor_GetMemValue(pCallContext,
                                           0,
                                           4,
                                           4,
                                           &resVal,
                                           0,
                                           reg_DS));

    DIANA_TEST_ASSERT(resVal == 10);
}


void test_processor_cmpxchg2()
{
    //  lock cmpxchg dword ptr [ecx],edx
    unsigned char code[] = {0xF0, 0x0F, 0xB1, 0x11,
                            1, 0, 0, 0};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ECX(4);
    SET_REG_EAX(0);
    SET_REG_EDX(10);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_EAX == 1);
    DIANA_TEST_ASSERT(!GET_FLAG_ZF);

    OPERAND_SIZE resVal = 0;
    DIANA_TEST_ASSERT(DI_SUCCESS ==
                DianaProcessor_GetMemValue(pCallContext,
                                           0,
                                           4,
                                           4,
                                           &resVal,
                                           0,
                                           reg_DS));

    DIANA_TEST_ASSERT(resVal == 1);
}

void test_processor_cwd()
{
    //  cdq (cbw)
    unsigned char code[] = {0x99};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EAX(0x7F000000);
    SET_REG_EDX(10);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_EDX == 0);
}


void test_processor_cwd2()
{
    //  cdq (cbw)
    unsigned char code[] = {0x66, 0x99};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EAX(0x8000);
    SET_REG_EDX(10);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_EDX == 0xFFFF);
}

void test_processor_cmpxchg8b()
{
    //  cmpxchg8b   qword ptr [ecx] 
    unsigned char code[] = {0x0F, 0xC7, 0x09, 0, 0xAA, 0, 0, 0xFF};
    const unsigned char etalon[] = {0x11, 0x11, 0x11, 0x11, 0, 0, 0, 0};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ECX(0);
    SET_REG_EDX(0xFF0000AA);
    SET_REG_EAX(0x0009c70f);
    SET_REG_EBX(0x11111111);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(memcmp(code, etalon, sizeof(code)) == 0);
    DIANA_TEST_ASSERT(GET_FLAG_ZF);
}

void test_processor_cmpxchg8b2()
{
    //  cmpxchg8b   qword ptr [ecx] 
    unsigned char code[] = {0x0F, 0xC7, 0x09, 0, 0xAA, 0, 0, 0xFF};
    const unsigned char etalon[] = {0x0F, 0xC7, 0x09, 0, 0xAA, 0, 0, 0xFF};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ECX(0);
    SET_REG_EDX(0xFF0000A1);
    SET_REG_EAX(0x1009c70f);
    SET_REG_EBX(0x11111111);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(memcmp(code, etalon, sizeof(code)) == 0);
    DIANA_TEST_ASSERT(GET_REG_EDX == 0xFF0000AA);
    DIANA_TEST_ASSERT(GET_REG_EAX == 0x0009c70f);
    DIANA_TEST_ASSERT(!GET_FLAG_ZF);
}

void test_processor_c()
{
    DIANA_TEST(test_processor_c_1());
    DIANA_TEST(test_processor_cmp());
    DIANA_TEST(test_processor_cmps());
    DIANA_TEST(test_processor_cmps2());
    DIANA_TEST(test_processor_cmpxchg());
    DIANA_TEST(test_processor_cmpxchg2());
    DIANA_TEST(test_processor_cwd());
    DIANA_TEST(test_processor_cwd2());
    DIANA_TEST(test_processor_cmpxchg8b());
    DIANA_TEST(test_processor_cmpxchg8b2());
}