#include "test_processor.h"
#include "test_common.h"
#include "test_processor_impl.h"

#include "vector"
static void test_processor1()
{
    // test empty buffer
    CTestProcessor proc(0,0);
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_END);
}


static void test_processor_aaa1()
{
    // test aaa
    unsigned char buff[] = {0x37};

    CTestProcessor proc(buff, 1);

    DianaProcessor_SetFlag(proc.GetSelf(), DI_FLAG_CF);
    DIANA_TEST_ASSERT(DianaProcessor_QueryFlag(proc.GetSelf(), DI_FLAG_CF) == 1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DianaProcessor * pCallContext = proc.GetSelf();
    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 1);
    DIANA_TEST_ASSERT(DianaProcessor_QueryFlag(proc.GetSelf(), DI_FLAG_CF) == 0);
}


static void test_processor_aaa2()
{
    // test aaa
    unsigned char buff[] = {0x37};

    CTestProcessor proc(buff, 1);

    DianaProcessor_SetFlag(proc.GetSelf(), DI_FLAG_AF);
    
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DianaProcessor * pCallContext = proc.GetSelf();
    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 1);

    unsigned char al = (unsigned char)GET_REG_AL;
    DIANA_TEST_ASSERT(al == 6);

    unsigned char ah = (unsigned char)GET_REG_AH;
    DIANA_TEST_ASSERT(ah == 1);

    unsigned short ax = (unsigned short)GET_REG_AX;
    DIANA_TEST_ASSERT(ax == 0x106);

    DIANA_TEST_ASSERT(DianaProcessor_QueryFlag(proc.GetSelf(), DI_FLAG_CF) == 1);
}

static void test_processor_aaa()
{
    test_processor_aaa1();
    test_processor_aaa2();
}


static void test_processor_aad()
{
    unsigned char buff[] = {0xD5, 0x0A};
    CTestProcessor proc(buff, 2);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_AH(26);
    SET_REG_AL(5);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 2);

    unsigned short ax = (unsigned short)GET_REG_AX;
    DIANA_TEST_ASSERT(ax == 0x009);
}

static void test_processor_aam()
{
    unsigned char buff[] = {0xD4, 0x0A};
    CTestProcessor proc(buff, 2);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_AL(0xFF);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 2);

    unsigned short ax = (unsigned short)GET_REG_AX;
    DIANA_TEST_ASSERT(ax == 0x1905);
}

static void test_processor_aas()
{
    unsigned char buff[] = {0x3f};
    CTestProcessor proc(buff, 2);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_AL(0xF);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 1);

    unsigned short ax = (unsigned short)GET_REG_AX;
    DIANA_TEST_ASSERT(ax == 0xFF09);

    DIANA_TEST_ASSERT(DianaProcessor_QueryFlag(proc.GetSelf(), DI_FLAG_CF) == 1);
    DIANA_TEST_ASSERT(DianaProcessor_QueryFlag(proc.GetSelf(), DI_FLAG_AF) == 1);
}

static void test_processor_adc()
{
    //      adc         byte ptr [ebp+eax*4+3],ah 
    const int bufSize = 0x48E4+1;
    unsigned char buff[bufSize] = {0x10, 0x64, 0x85, 0x03};
    buff[bufSize-1] = 5;

    CTestProcessor proc(buff, bufSize);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EBP(0x11);
    SET_REG_EAX(0x1234);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 4);

    char f1 = 0;
    OPERAND_SIZE readed = 0; 
    DIANA_TEST_ASSERT(DianaProcessor_ReadMemory(proc.GetSelf(),
                                          0,
                                          bufSize-1,
                                          &f1, 
                                          1, 
                                          &readed,
                                          0,
                                          reg_DS) == DI_SUCCESS );

    const int result = 0x12 + 5;
    DIANA_TEST_ASSERT(f1 == result);
}

static void test_processor_add()
{
    //      add         byte ptr [ebp+eax*4+3],ah 
    const int bufSize = 0x48E4+1;
    unsigned char buff[bufSize] = {0x10, 0x64, 0x85, 0x03};
    buff[bufSize-1] = 5;

    CTestProcessor proc(buff, bufSize);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EBP(0x11);
    SET_REG_EAX(0x1234);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 4);

    char f1 = 0;
    OPERAND_SIZE readed = 0; 
    DIANA_TEST_ASSERT(DianaProcessor_ReadMemory(proc.GetSelf(),
                                          0,
                                          bufSize-1,
                                          &f1, 
                                          1, 
                                          &readed,
                                          0,
                                          reg_DS) == DI_SUCCESS );

    const int result = 0x12 + 5;
    DIANA_TEST_ASSERT(f1 == result);
}

static void test_processor_push()
{
    const int bufSize = 1024;
    unsigned char buff[bufSize]= {0xFF, 0xB4, 0x71, 0xF6, 0x00, 0x00, 0x00}; // push        dword ptr [ecx+esi*2+0F6h]
    
    const int flag = 0x5A5A5A5;
    *(int*)(buff+0xF6) = flag;

    CTestProcessor proc(buff, bufSize);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ESP(0x20);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 7);

    int result = 0;
    OPERAND_SIZE readed = 0; 
    DIANA_TEST_ASSERT(DianaProcessor_ReadMemory(proc.GetSelf(),
                                          0,
                                          0x1C,
                                          &result, 
                                          4, 
                                          &readed,
                                          0,
                                          reg_DS) == DI_SUCCESS );

    DIANA_TEST_ASSERT(flag == result);
    
    // check old value
    DIANA_TEST_ASSERT(DianaProcessor_ReadMemory(proc.GetSelf(),
                                          0,
                                          0x20,
                                          &result, 
                                          4, 
                                          &readed,
                                          0,
                                          reg_DS) == DI_SUCCESS );

    DIANA_TEST_ASSERT(0 == result);

    // check rsp
    DIANA_TEST_ASSERT(GET_REG_ESP == 0x1C);
}


static void test_processor_push_ds()
{
    unsigned char buff[]= {0x1E, 0x00, 0x00, 0x02, 0x01}; // push  ds
    unsigned char etalon[]= {0x1E, 0xFF, 0x7F, 0x02, 0x01}; 
    
    CTestProcessor proc(buff, sizeof(buff));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ESP(0x5);
    SET_REG_DS(0x7FFF);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 1);

    // check rsp
    DIANA_TEST_ASSERT(GET_REG_ESP == 1);

    DIANA_TEST_ASSERT(memcmp(buff, etalon, sizeof(etalon)) == 0)
}


static void test_processor_push_gs()
{
    unsigned char buff[]= {0x0F, 0xA8}; // push  ds
    unsigned char etalon[]= {0xFF, 0x7F}; 
    
    CTestProcessor proc(buff, sizeof(buff), 0, 8);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ESP(8);
    SET_REG_GS(0x7FFF);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 2);

    // check rsp
    DIANA_TEST_ASSERT(GET_REG_ESP == 0);

    DIANA_TEST_ASSERT(memcmp(buff, etalon, sizeof(etalon)) == 0)
}

static void test_processor_push_1()
{
    unsigned char buff[]= {0x66, 0x6A, 0x1}; // push  1 - 2 bytes !!!!
    unsigned char etalon[]= {0x01, 0x00, 0x1}; 
    
    CTestProcessor proc(buff, sizeof(buff));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ESP(0x2);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 3);

    // check rsp
    DIANA_TEST_ASSERT(GET_REG_ESP == 0);

    DIANA_TEST_ASSERT(memcmp(buff, etalon, sizeof(etalon)) == 0)
}


static void test_processor_push_2()
{
    unsigned char buff[]= {0x6A, 0xF5, 0x00, 0x00};   // push        0FFFFFFF5h
    unsigned char etalon[]= {0xf5, 0xff, 0xff, 0xff}; 
    
    CTestProcessor proc(buff, sizeof(buff));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RSP(4);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 2);

    // check rsp
    DIANA_TEST_ASSERT(GET_REG_ESP == 0);

    DIANA_TEST_ASSERT(memcmp(buff, etalon, sizeof(etalon)) == 0)
}




static void test_processor_pushf()
{
    const int bufSize = 1024;
    unsigned char buff[bufSize]= {0x9C}; // pushf
    
    CTestProcessor proc(buff, bufSize);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_ESP(0x20);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 1);

    int result = 0;
    OPERAND_SIZE readed = 0; 
    DIANA_TEST_ASSERT(DianaProcessor_ReadMemory(proc.GetSelf(),
                                          0,
                                          0x1C,
                                          &result, 
                                          4, 
                                          &readed,
                                          0,
                                          reg_DS) == DI_SUCCESS );

    DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.impl.l.value == result);

    // check rsp
    DIANA_TEST_ASSERT(GET_REG_ESP == 0x1C);
}


static void test_processor_sub()
{
    // test normal sub 
    unsigned char buff[4] = {0x66, 0x2D, 0xFF, 0x00}; // sub ax, 0xFF  

    CTestProcessor proc(buff, 4);
    DianaProcessor * pCallContext = proc.GetSelf();

    CLEAR_FLAG_ID;
    SET_REG_EAX(0x1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 4);

    DIANA_TEST_ASSERT(GET_REG_EAX == 0xff02);
    DIANA_TEST_ASSERT(pCallContext->m_flags.value == 0x00000293);
}


static void test_processor_sub2()
{
    // test sign extending
    unsigned char buff[4] = {0x66, 0x83, 0xEA, 0xFF}; // sub dx, 0xFF  

    CTestProcessor proc(buff, 4);
    DianaProcessor * pCallContext = proc.GetSelf();

    CLEAR_FLAG_ID;
    SET_REG_EDX(0x1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    OPERAND_SIZE rip = GET_REG_RIP;
    DIANA_TEST_ASSERT(rip == 4);

    DIANA_TEST_ASSERT(GET_REG_EDX == 0x0002);
    DIANA_TEST_ASSERT(pCallContext->m_flags.value == 0x00000213);
}

static void test_processor_enter()
{
    // __asm enter 0x4, 1
    unsigned char buff[] = {0xC8, 0x4, 0x00, 0x01,
                            0,0,0,0,
                            0,0,0,0}; 

    const static unsigned char target[] = 
                            {0xC8, 0x4, 0x00, 0x01,
                            8,0,0,0,
                            4,0,0,0};

    CTestProcessor proc(buff, sizeof(buff));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RSP(0xC);
    SET_REG_RBP(0x4);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(memcmp(target, buff, sizeof(buff))==0);
    OPERAND_SIZE rsp = GET_REG_RSP;
    DIANA_TEST_ASSERT(GET_REG_RSP == 0);
    DIANA_TEST_ASSERT(GET_REG_RBP == 8)
}

static void test_processor_enter2()
{
    std::vector<DI_UINT32> memory(4097);

    unsigned char buff[] = {0xC8, 0x08, 0x00, 0x02}; 

    memory[0] = *(DI_UINT32*)buff;

    DI_UINT32 * pBegin = &memory.front();
    size_t sizeInBytes = memory.size()*4;

    CTestProcessor proc((unsigned char*)pBegin, sizeInBytes);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RBP(sizeInBytes);
    
    DI_UINT32 * pStackPtr = pBegin + memory.size();
    for(int i = 1; i <= 32; i++)
    {
        *--pStackPtr = i;
    }
    SET_REG_RSP( (char*)pStackPtr  - (char*)pBegin);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_RSP == 16240);
    DIANA_TEST_ASSERT(GET_REG_RBP == 16256);

    DIANA_TEST_ASSERT(memory[4061] == 0);
    DIANA_TEST_ASSERT(memory[4062] == 16256);
    DIANA_TEST_ASSERT(memory[4063] == 1);
    DIANA_TEST_ASSERT(memory[4064] == 16388);
    DIANA_TEST_ASSERT(memory[4065] == 32);
}

static void test_processor_enter3()
{
    std::vector<DI_UINT16> memory(1024);

    unsigned char buff[] = {0x66, 0xC8, 0x08, 0x00, 0x02}; 

    memcpy(&memory.front(), buff, sizeof(buff));

    DI_UINT16 * pBegin = &memory.front();
    size_t sizeInBytes = memory.size()*sizeof(DI_UINT16);

    CTestProcessor proc((unsigned char*)pBegin, sizeInBytes);
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_RBP(sizeInBytes);    
    
    DI_UINT16 * pStackPtr = pBegin + memory.size();
    for(int i = 1; i <= 32; i++)
    {
        *--pStackPtr = i;
    }
    SET_REG_RSP( (char*)pStackPtr  - (char*)pBegin);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
}

static void test_processor_xlat()
{
    // rep stos 
    unsigned char code[] = {0xD7, 1, 2, 3};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_EBX(1);
    SET_REG_AL(1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(GET_REG_AL == 2);
}

void test_processor_test()
{
// test        al,al 
    unsigned char code[] = {0x84, 0xC0};

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();

    SET_REG_AL(1);

    pCallContext->m_flags.impl.l.value = 0x216;
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    
    DIANA_TEST_ASSERT(pCallContext->m_flags.impl.l.value == 0x202);
}

void test_processor()
{
    test_processor1();
    test_processor_aaa();
    test_processor_aad();
    test_processor_aam();
    test_processor_aas();
    test_processor_adc();
    test_processor_add();
    test_processor_push();
    test_processor_push_ds();
    test_processor_push_1();
    test_processor_push_2();
    test_processor_pushf();
    test_processor_sub();
    test_processor_sub2();
    test_processor_enter();
    test_processor_enter2();
    test_processor_enter3();
    test_processor_xlat();
    test_processor_test();
    test_processor_push_gs();
}