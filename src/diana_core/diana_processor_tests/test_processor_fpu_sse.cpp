#include "test_processor_m_xmm.h"
#include "test_common.h"
#include "test_processor_impl.h"

//#include "windows.h"
//ULONG oldProtect = 0;
//VirtualProtect(code, 1024, PAGE_EXECUTE_READWRITE, &oldProtect);

static void test_processor_fpu_mulsd1()
{
    unsigned char code[] = {0xF2, 0x0F, 0x10, 0x05, 0x15, 0x00, 0x00, 0x00, // movsd       xmm0,mmword ptr [test_a (7FF7D2ED2020h)] 
                            0xF2, 0x0F, 0x59, 0x05, 0x15, 0x00, 0x00, 0x00, // mulsd       xmm0,mmword ptr [test_b (7FF7D2ED2010h)] 
                            0xF2, 0x0F, 0x11, 0x44, 0x24, 0x20,             // movsd       mmword ptr [test],xmm0   // [rsp+20h]
                            0xF2, 0x48, 0x0F, 0x2C, 0x44, 0x24, 0x20,       // cvttsd2si   rax,mmword ptr [test]    // [rsp+20h]

                            /// ---
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f, // data1
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f, // data2
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // result : 2d offset
    };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    SET_REG_RSP(0xD);
    SET_REG_RAX(0xAAAAAAAAAAAAAAAA);

    int res = proc.Exec(4);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(*(OPERAND_SIZE*)(code + 0x2d) == 0x3ff51eb851eb851f);

    DianaRegisterXMM_type xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.u64[0] ==  0x3FF51EB851EB851F);
    DIANA_TEST_ASSERT(xmm0.u64[1] ==  0x0000000000000000);

    DIANA_TEST_ASSERT(GET_REG_RAX == 1);
}

static void test_processor_fpu_cvttsd2si_32()
{
    unsigned char code[] = {0xF2, 0x0F, 0x10, 0x05, 0x14, 0x00, 0x00, 0x00, // movsd       xmm0,mmword ptr [test_a (7FF7D2ED2020h)] 
                            0xF2, 0x0F, 0x59, 0x05, 0x14, 0x00, 0x00, 0x00, // mulsd       xmm0,mmword ptr [test_b (7FF7D2ED2010h)] 
                            0xF2, 0x0F, 0x11, 0x44, 0x24, 0x20,             // movsd       mmword ptr [test],xmm0   // [rsp+20h]
                            0xF2, 0x0F, 0x2C, 0x44, 0x24, 0x20,             // cvttsd2si   eax,mmword ptr [test]    // [rsp+20h]

                            /// ---
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f, // data1
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f, // data2
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // result : 2C offset
    };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    SET_REG_RSP(0xC);
    SET_REG_RAX(0xAAAAAAAABBBBBBBB);

    int res = proc.Exec(4);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(*(OPERAND_SIZE*)(code + 0x2C) == 0x3ff51eb851eb851f);

    DianaRegisterXMM_type xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.u64[0] ==  0x3FF51EB851EB851F);
    DIANA_TEST_ASSERT(xmm0.u64[1] ==  0x0000000000000000);

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xAAAAAAAA00000001);
}

static void test_processor_fpu_divsd1()
{
    unsigned char code[] = {0xF2, 0x0F, 0x10, 0x05, 0x08, 0x00, 0x00, 0x00, // movsd       xmm0,mmword ptr [test_b (7FF781632020h)] 
                            0xF2, 0x0F, 0x5E, 0x05, 0x08, 0x00, 0x00, 0x00, // divsd       xmm0,mmword ptr [test_a (7FF781632010h)] 
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f, // data1
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f  // data2

    };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DianaRegisterXMM_type xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.u64[0] ==  0x3FED555555555556);
    DIANA_TEST_ASSERT(xmm0.u64[1] ==  0x0000000000000000);
}

static void test_processor_fpu_addsd()
{
    unsigned char code[] = {0xF2, 0x0F, 0x10, 0x05, 0x08, 0x00, 0x00, 0x00, // movsd       xmm0,mmword ptr [test_b (7FF781632020h)] 
                            0xF2, 0x0F, 0x58, 0x05, 0x08, 0x00, 0x00, 0x00, // addsd       xmm0,mmword ptr [test_b (7FF76D252010h)] 
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f, // data1
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f  // data2

    };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DianaRegisterXMM_type xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.u64[0] ==  0x4002666666666666);
    DIANA_TEST_ASSERT(xmm0.u64[1] ==  0x0000000000000000);
}

static void test_processor_fpu_subsd()
{
    unsigned char code[] = {0xF2, 0x0F, 0x10, 0x05, 0x08, 0x00, 0x00, 0x00, // movsd       xmm0,mmword ptr [test_b (7FF781632020h)] 
                            0xF2, 0x0F, 0x5C, 0x05, 0x08, 0x00, 0x00, 0x00, // subsd       xmm0,mmword ptr [test_b (7FF6F4512010h)] 
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f, // data1
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f  // data2

    };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    DianaRegisterXMM_type xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.u64[0] ==  0xBFB9999999999990);
    DIANA_TEST_ASSERT(xmm0.u64[1] ==  0x0000000000000000);
}


static void test_processor_fpu_comisd()
{
    {
        unsigned char code[] = {0xF2, 0x0F, 0x10, 0x05, 0x08, 0x00, 0x00, 0x00, // movsd       xmm0,mmword ptr [test_b (7FF781632020h)] 
                                0x66, 0x0F, 0x2F, 0x05, 0x08, 0x00, 0x00, 0x00, // comisd      xmm0,mmword ptr [test_b (7FF69F592010h)] 
                                0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f, // data2
                                0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f  // data1
                            
    
        };

        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
        DianaProcessor * pCallContext = proc.GetSelf();
        DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
        pFpu->controlWord = 0x027f;
    
        int res = proc.Exec(2);
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.impl.l.impl.l.value == 0x202);
    }
    {
        unsigned char code[] = {0xF2, 0x0F, 0x10, 0x05, 0x08, 0x00, 0x00, 0x00, // movsd       xmm0,mmword ptr [test_b (7FF781632020h)] 
                                0x66, 0x0F, 0x2F, 0x05, 0x08, 0x00, 0x00, 0x00, // comisd      xmm0,mmword ptr [test_b (7FF69F592010h)] 
                                0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f,  // data1
                                0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f  // data2
                            
    
        };

        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
        DianaProcessor * pCallContext = proc.GetSelf();
        DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
        pFpu->controlWord = 0x027f;
    
        int res = proc.Exec(2);
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.impl.l.impl.l.value == 0x203);
    }
    {
        unsigned char code[] = {0xF2, 0x0F, 0x10, 0x05, 0x08, 0x00, 0x00, 0x00, // movsd       xmm0,mmword ptr [test_b (7FF781632020h)] 
                                0x66, 0x0F, 0x2F, 0x05, 0x08, 0x00, 0x00, 0x00, // comisd      xmm0,mmword ptr [test_b (7FF69F592010h)] 
                                0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f, // data1
                                0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f  // data1
                            
    
        };

        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
        DianaProcessor * pCallContext = proc.GetSelf();
        DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
        pFpu->controlWord = 0x027f;
    
        int res = proc.Exec(2);
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.impl.l.impl.l.value == 0x242);
    }
}

static void test_processor_fpu_pshufd()
{
    // 75187c04 660f70011b      pshufd  xmm0,xmmword ptr [ecx],1b
    {
        unsigned char code[] = { 0x66, 0x0f, 0x70, 0x01, 0x1b, 0x00, 0x00, 0x00, 
                                 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 
                                 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0xf1, 0xf2};

        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
        DianaProcessor * pCallContext = proc.GetSelf();
        DianaFPU * pFpu = &proc.GetSelf()->m_fpu;

        DianaRegisterXMM_type xmm0 = { 0 };
        xmm0.u64[0] = 0x090a0b0c0d0e0f10ULL;
        xmm0.u64[1] = 0x0102030405060708ULL;
        SET_REG_XMM0(xmm0);
        SET_REG_ECX(8);

        int res = proc.Exec(1);
        DIANA_TEST_ASSERT(res == DI_SUCCESS);

        xmm0 = GET_REG_XMM0;

        DIANA_TEST_ASSERT(xmm0.u32[0] == 0xf2f1f0e0);
        DIANA_TEST_ASSERT(xmm0.u32[1] == 0xd0c0b0a0);
        DIANA_TEST_ASSERT(xmm0.u32[2] == 0x80706050);
        DIANA_TEST_ASSERT(xmm0.u32[3] == 0x40302010);
    }

    // 75187c04 660f70011b      pshufd  xmm0,xmm0 ,0
    {
        unsigned char code[] = { 0x66, 0x0F, 0x70, 0xC0, 0x00 };

        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
        DianaProcessor * pCallContext = proc.GetSelf();
        DianaFPU * pFpu = &proc.GetSelf()->m_fpu;

        DianaRegisterXMM_type xmm0 = { 0 };
        xmm0.u64[0] = 0x0203040506070800;
        xmm0.u64[1] = 0x1;
        SET_REG_XMM0(xmm0);

        int res = proc.Exec(1);
        DIANA_TEST_ASSERT(res == DI_SUCCESS);

        xmm0 = GET_REG_XMM0;

        DIANA_TEST_ASSERT(xmm0.u32[0] == 0x06070800);
        DIANA_TEST_ASSERT(xmm0.u32[1] == 0x06070800);
        DIANA_TEST_ASSERT(xmm0.u32[2] == 0x06070800);
        DIANA_TEST_ASSERT(xmm0.u32[3] == 0x06070800);
    }

}


void test_processor_fpu_sse()
{
    DIANA_TEST(test_processor_fpu_mulsd1());
    DIANA_TEST(test_processor_fpu_cvttsd2si_32());
    DIANA_TEST(test_processor_fpu_divsd1());
    DIANA_TEST(test_processor_fpu_addsd());
    DIANA_TEST(test_processor_fpu_subsd());
    DIANA_TEST(test_processor_fpu_comisd());
    DIANA_TEST(test_processor_fpu_pshufd());
}
