#include "test_processor_m_xmm.h"
#include "test_common.h"
#include "test_processor_impl.h"

static void test_processor_fcomp1()
{
    unsigned char code[] = {0xDD, 0x05, 0x0E, 0x00, 0x00, 0x00, // fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
                            0xDC, 0x1D, 0x16, 0x00, 0x00, 0x00, // fcomp       qword ptr [test (492450h)] 
                            0xDF, 0xE0,                         // fnstsw      ax 
                            /// ---
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f, // data1
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f  // data2
    };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    int res =  0;
    // first command:
    //  002DB6FE DD 05 C8 30 46 00 fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
    /// ER:
    //   ST0 = +1.1000000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //   ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //   ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //   ST6 = +0.0000000000000000e+0000   ST7 = +0.0000000000000000e+0000   CTRL = 027F STAT = 3800 

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3800);

    OPERAND_SIZE mmxValue = GET_REG_MM7;
    OPERAND_SIZE fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0x8cccccccccccd000);
    DIANA_TEST_ASSERT(fpuValue == 0x3ff199999999999a);

    //  002DB704 DC 1D 50 24 49 00 fcomp       qword ptr [test (492450h)] 
    // ER:
    //  ST0 = +0.0000000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //  ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //  ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //  ST6 = +0.0000000000000000e+0000   ST7 = +1.1000000000000000e+0000   CTRL = 027F STAT = 0100 

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    mmxValue = GET_REG_MM7;
    fpuValue = GET_REG_FPU_ST7;
    DIANA_TEST_ASSERT(mmxValue == 0x8cccccccccccd000);
    DIANA_TEST_ASSERT(fpuValue == 0x3ff199999999999a);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C0 == 1);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C1 == 0);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C2 == 0);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C3 == 0);

    //  002DB70A DF E0            fnstsw      ax   
    //  EAX = CCCC0100 EBX = 7E32B000 ECX = 00000000 EDX = 01232C98 ESI = 002D40A4 EDI = 00E7FBB4 
    //  EIP = 002DB70C ESP = 00E7FAD8 EBP = 00E7FBB4 EFL = 00000206 

    //  ST0 = +0.0000000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //  ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //  ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //  ST6 = +0.0000000000000000e+0000   ST7 = +1.1000000000000000e+0000   CTRL = 027F STAT = 0100 
    res = proc.ExecOnce();
    OPERAND_SIZE regAX = GET_REG_AX;
    DIANA_TEST_ASSERT(regAX == 0x100);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
}

static void test_processor_fmul()
{
    unsigned char code[] = {0xDD, 0x05, 0x12, 0x00, 0x00, 0x00, // fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
                            0xDC, 0x0D, 0x1A, 0x00, 0x00, 0x00, // fmul        qword ptr [test2 (403940h)] 
                            0xDD, 0x1D, 0x22, 0x00, 0x00, 0x00, // fstp        qword ptr [test3 (41E7E0h)]  
                            /// ---
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f,
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    int res =  0;
    // first command:
    //  002DB6FE DD 05 C8 30 46 00 fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
    /// ER:
    //   ST0 = +1.1000000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //   ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //   ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //   ST6 = +0.0000000000000000e+0000   ST7 = +0.0000000000000000e+0000   CTRL = 027F STAT = 3800 

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3800);

    OPERAND_SIZE mmxValue = GET_REG_MM7;
    OPERAND_SIZE fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0x8cccccccccccd000);
    DIANA_TEST_ASSERT(fpuValue == 0x3ff199999999999a);


    /// ER:
    //   ST0 = +1.3200000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //   ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //   ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //   ST6 = +0.0000000000000000e+0000   ST7 = +0.0000000000000000e+0000   CTRL = 027F STAT = 3A20 

    //   MM0 = 0000000000000000 MM1 = 0000000000000000 MM2 = 0000000000000000 MM3 = 0000000000000000 
    //   MM4 = 0000000000000000 MM5 = 0000000000000000 MM6 = 0000000000000000 MM7 = A8F5C28F5C28F800 

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3A20);

    mmxValue = GET_REG_MM7;
    fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0xA8F5C28F5C28F800);
    DIANA_TEST_ASSERT(fpuValue == 0x3ff51eb851eb851f);

    /// ER:
    //  ST0 = +0.0000000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //  ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //  ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //  ST6 = +0.0000000000000000e+0000   ST7 = +1.3200000000000000e+0000   CTRL = 027F STAT = 0020 
    //  TAGS = FFFF EIP = 0024B721 EDO = 0041E7E0 

    //  MM0 = 0000000000000000 MM1 = 0000000000000000 MM2 = 0000000000000000 MM3 = 0000000000000000 
    //  MM4 = 0000000000000000 MM5 = 0000000000000000 MM6 = 0000000000000000 MM7 = A8F5C28F5C28F800 

  
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x0020);

    mmxValue = GET_REG_MM7;
    fpuValue = GET_REG_FPU_ST7;
    DIANA_TEST_ASSERT(mmxValue == 0xA8F5C28F5C28F800);
    DIANA_TEST_ASSERT(fpuValue == 0x3ff51eb851eb851f);
    DIANA_TEST_ASSERT(GET_REG_FPU_ST0 == 0);
    DIANA_TEST_ASSERT(*(OPERAND_SIZE *)(code+0x22) == 0x3ff51eb851eb851f);
}

static void test_processor_fdiv()
{
    unsigned char code[] = {0xDD, 0x05, 0x12, 0x00, 0x00, 0x00, // fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
                            0xDC, 0x35, 0x1A, 0x00, 0x00, 0x00, // fdiv        qword ptr [test2 (403940h)] 
                            0xDD, 0x1D, 0x22, 0x00, 0x00, 0x00, // fstp        qword ptr [test3 (41E7E0h)]  
                            /// ---
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f,
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    int res =  0;
    // first command:
    //  002DB6FE DD 05 C8 30 46 00 fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
    /// ER:
    //   ST0 = +1.1000000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //   ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //   ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //   ST6 = +0.0000000000000000e+0000   ST7 = +0.0000000000000000e+0000   CTRL = 027F STAT = 3800 

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3800);

    OPERAND_SIZE mmxValue = GET_REG_MM7;
    OPERAND_SIZE fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0x8cccccccccccd000);
    DIANA_TEST_ASSERT(fpuValue == 0x3ff199999999999a);


    /// ER:
    //   ST0 = +9.1666666666666674e-0001   ST1 = +0.0000000000000000e+0000   
    //   ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //   ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //   ST6 = +0.0000000000000000e+0000   ST7 = +0.0000000000000000e+0000   CTRL = 027F STAT = 3820 

    //   MM0 = 0000000000000000 MM1 = 0000000000000000 MM2 = 0000000000000000 MM3 = 0000000000000000 
    //   MM4 = 0000000000000000 MM5 = 0000000000000000 MM6 = 0000000000000000 MM7 = EAAAAAAAAAAAB000 

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3820);

    mmxValue = GET_REG_MM7;
    fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0xEAAAAAAAAAAAB000);
    DIANA_TEST_ASSERT(fpuValue == 0x3fed555555555556);

    /// ER:
    //  ST0 = +0.0000000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //  ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //  ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //  ST6 = +0.0000000000000000e+0000   ST7 = +9.1666666666666674e-0001   CTRL = 027F STAT = 0020 
    //  TAGS = FFFF EIP = 0024B721 EDO = 0041E7E0 

    //  MM0 = 0000000000000000 MM1 = 0000000000000000 MM2 = 0000000000000000 MM3 = 0000000000000000 
    //  MM4 = 0000000000000000 MM5 = 0000000000000000 MM6 = 0000000000000000 MM7 = EAAAAAAAAAAAB000 

  
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x0020);

    mmxValue = GET_REG_MM7;
    fpuValue = GET_REG_FPU_ST7;
    DIANA_TEST_ASSERT(mmxValue == 0xEAAAAAAAAAAAB000);
    DIANA_TEST_ASSERT(fpuValue == 0x3fed555555555556);
    DIANA_TEST_ASSERT(GET_REG_FPU_ST0 == 0);
    DIANA_TEST_ASSERT(*(OPERAND_SIZE *)(code+0x22) == 0x3fed555555555556);
}

static void test_processor_fsub()
{
    unsigned char code[] = {0xDD, 0x05, 0x0C, 0x00, 0x00, 0x00, // fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
                            0xDC, 0x2D, 0x14, 0x00, 0x00, 0x00, // fsub        qword ptr [test2 (403940h)] 
                            /// ---
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f,
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f
    };
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    int res =  0;
    // first command:
    //  002DB6FE DD 05 C8 30 46 00 fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
    /// ER:
    //   ST0 = +1.1000000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //   ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //   ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //   ST6 = +0.0000000000000000e+0000   ST7 = +0.0000000000000000e+0000   CTRL = 027F STAT = 3800 

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3800);

    OPERAND_SIZE mmxValue = GET_REG_MM7;
    OPERAND_SIZE fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0x8cccccccccccd000);
    DIANA_TEST_ASSERT(fpuValue == 0x3ff199999999999a);


    /// ER:
    //   ST0 = +9.9999999999999866e-0002   ST1 = +0.0000000000000000e+0000   
    //   ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //   ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //   ST6 = +0.0000000000000000e+0000   ST7 = +0.0000000000000000e+0000   CTRL = 027F STAT = 3800 

    //   MM0 = 0000000000000000 MM1 = 0000000000000000 MM2 = 0000000000000000 MM3 = 0000000000000000 
    //   MM4 = 0000000000000000 MM5 = 0000000000000000 MM6 = 0000000000000000 MM7 = CCCCCCCCCCCC8000

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3800);

    mmxValue = GET_REG_MM7;
    fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0xCCCCCCCCCCCC8000);
    DIANA_TEST_ASSERT(fpuValue == 0x3fb9999999999990);
}

static void test_processor_fsubr()
{
    unsigned char code[] = {0xDD, 0x05, 0x09, 0x00, 0x00, 0x00, // fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
                            0xDC, 0x6D, 0xF4, // fsubr       qword ptr [EBP + 0xFFFFFFF4]
                            /// ---
                            0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f,
                            0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f
    };
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    int res =  0;
    // first command:
    //  002DB6FE DD 05 C8 30 46 00 fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
    /// ER:
    //   ST0 = +1.1000000000000000e+0000   ST1 = +0.0000000000000000e+0000   
    //   ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //   ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //   ST6 = +0.0000000000000000e+0000   ST7 = +0.0000000000000000e+0000   CTRL = 027F STAT = 3800 

    SET_REG_RBP(0x11+0xC);
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3800);

    OPERAND_SIZE mmxValue = GET_REG_MM7;
    OPERAND_SIZE fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0x8cccccccccccd000);
    DIANA_TEST_ASSERT(fpuValue == 0x3ff199999999999a);


    /// ER:
    //   ST0 = +9.9999999999999866e-0002   ST1 = +0.0000000000000000e+0000   
    //   ST2 = +0.0000000000000000e+0000   ST3 = +0.0000000000000000e+0000   
    //   ST4 = +0.0000000000000000e+0000   ST5 = +0.0000000000000000e+0000   
    //   ST6 = +0.0000000000000000e+0000   ST7 = +0.0000000000000000e+0000   CTRL = 027F STAT = 3800 

    //   MM0 = 0000000000000000 MM1 = 0000000000000000 MM2 = 0000000000000000 MM3 = 0000000000000000 
    //   MM4 = 0000000000000000 MM5 = 0000000000000000 MM6 = 0000000000000000 MM7 = CCCCCCCCCCCC8000

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3800);

    mmxValue = GET_REG_MM7;
    fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0xCCCCCCCCCCCC8000);
    DIANA_TEST_ASSERT(fpuValue == 0x3fb9999999999990);
}

static void test_processor_fsqrt()
{
    unsigned char code[] = {0xDD, 0x05, 0x08, 0x00, 0x00, 0x00, // fld         qword ptr [__real@3ff199999999999a (4630C8h)] 
                            0xD9, 0xFA,                         // fsqrt
                            /// ---
                            0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x66, 0x40
    };
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    double dddd = 182.25;
    double dddd2 = 13.5;
    int res =  0;
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3800);

    OPERAND_SIZE mmxValue = GET_REG_MM7;
    OPERAND_SIZE fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0xb640000000000000);
    DIANA_TEST_ASSERT(fpuValue == 0x4066c80000000000);

    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x27F);
    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3800);

    mmxValue = GET_REG_MM7;
    fpuValue = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT(mmxValue == 0xd800000000000000);
    DIANA_TEST_ASSERT(fpuValue == 0x402b000000000000);
}

void test_processor_fpu()
{
    DIANA_TEST(test_processor_fsqrt());
    DIANA_TEST(test_processor_fdiv());
    DIANA_TEST(test_processor_fsub());
    DIANA_TEST(test_processor_fsubr());
    DIANA_TEST(test_processor_fmul());
    DIANA_TEST(test_processor_fcomp1());
}
