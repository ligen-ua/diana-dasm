#include "test_processor_m_xmm.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include "memory"
#include "vector"

extern "C"
{
#include "diana_processor/diana_processor_context.h"
#include "diana_processor/diana_processor_win32_context.h"

}
struct FpuTestState
{
    OPERAND_SIZE mmx[8];
    DI_UINT32 ctrl;
    DI_UINT32 stat;
};

static
void FPU_Exec(CTestProcessor * pProc, 
              int instructionsCount, 
              const FpuTestState * pStates)
{
    const FpuTestState * pCurrentState = pStates;
    DIANA_AUTO_PTR<Diana_Processor_Registers_Context> context(new Diana_Processor_Registers_Context());

    Diana_Processor_Registers_Context * pContext = context.get();
    for(int i = 0; i < instructionsCount; ++i, ++pCurrentState)
    {
        int res = pProc->ExecOnce();
        DIANA_TEST_ASSERT(res == DI_SUCCESS);

        res = DianaProcessor_QueryContext(pProc->GetSelf(), pContext);
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        
        DIANA_TEST_ASSERT(pContext->fpuState.controlWord == pCurrentState->ctrl);
        DIANA_TEST_ASSERT(pContext->fpuState.statusWord == pCurrentState->stat);
        DIANA_TEST_ASSERT(pContext->reg_MM0.value == pCurrentState->mmx[0]);
        DIANA_TEST_ASSERT(pContext->reg_MM1.value == pCurrentState->mmx[1]);
        DIANA_TEST_ASSERT(pContext->reg_MM2.value == pCurrentState->mmx[2]);
        DIANA_TEST_ASSERT(pContext->reg_MM3.value == pCurrentState->mmx[3]);
        DIANA_TEST_ASSERT(pContext->reg_MM4.value == pCurrentState->mmx[4]);
        DIANA_TEST_ASSERT(pContext->reg_MM5.value == pCurrentState->mmx[5]);
        DIANA_TEST_ASSERT(pContext->reg_MM6.value == pCurrentState->mmx[6]);
        DIANA_TEST_ASSERT(pContext->reg_MM7.value == pCurrentState->mmx[7]);
    }
}

static void test_processor_integration1()
{
static FpuTestState states[] = {
    {{0x8000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000}, 
    0x27F, 0x3800}, 

    {{0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 
     0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0xC000000000000000},
    0x27F, 0x0000}, 

    {{0x8000000000000000, 0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 
     0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0x8000000000000000, 0xC000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0xC000000000000000, 0x8000000000000000, 0x8000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x8000000000000000},
    0x27F, 0x0000}, 

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0xC000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0xC000000000000000, 0x8000000000000000, 0x8000000000000000, 0xC000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0xC000000000000000},
    0x27F, 0x0000}, 

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0xC000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0x9000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}, 

    {{0xC000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800} 
};


    //const float a_float = 1;
    //const float b_float = 2;
    //const double a = (double )a_float;
    //double b = (double )b_float;
    //const double d = a+b;
    //const double z = a-b;
    //b = z*z;
    //const double c = (a+b+z*z+d*(a-z*b))/(a+b+z*z);
    unsigned char code[0x100] = 
    {
        0x67, 0xD9, 0x44, 0x24, 0x0C,       //  fld     dword ptr [esp+0Ch]
        0x67, 0xD8, 0x44, 0x24, 0x08,       //  fadd    dword ptr [esp+8]
        0xDD, 0xD9,                         //  fstp    st(1)
        0x67, 0xD9, 0x44, 0x24, 0x08,       //  fld     dword ptr [esp+8]
        0x67, 0xD8, 0x64, 0x24, 0x0C,       //  fsub    dword ptr [esp+0Ch] 
        0xDD, 0xD2,                         //  fst     st(2)
        0xD8, 0xCA,                         //  fmul    st, st(2)
        0xDD, 0xDB,                         //  fstp    st(3)
        0x67, 0xD9, 0x44, 0x24, 0x08,       //  fld     dword ptr [esp+8]
        0xD8, 0xC3,                         //  fadd    st, st(3)
        0xD8, 0xC3,                         //  fadd    st, st(3)
        0xDD, 0xDC,                         //  fstp    st(4)
        0xD9, 0xC1,                         //  fld     st(1) 
        0xD8, 0xCB,                         //  fmul    st, st(3)
        0x67, 0xD8, 0x6C, 0x24, 0x08,       //  fsubr   dword ptr [esp+8]
        0xD8, 0xC9,                         //  fmul    st, st(1)
        0xD8, 0xC4,                         //  fadd    st, st(4) 
        0xD8, 0xF4                          //  fdiv    st, st(4)
    };

    unsigned char args[] = {0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x40};
    const int size = sizeof(code);
    memcpy((code + size - 0x8), &args, 8);

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    SET_REG_RSP(size-4-0xC);

    FPU_Exec(&proc, 18, states);

    DIANA_TEST_ASSERT(0x4008000000000000 == GET_REG_FPU_ST0);
}


static void test_processor_integration2()
{
static FpuTestState states[] = {
    {{0x8CCCCCCCCCCCD000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0xD89D89D89D89D800, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3820},

    {{0xA666666666666800, 0xD89D89D89D89D800, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3020},

    {{0x9745D1745D174000, 0xD89D89D89D89D800, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3020},

    {{0x81CA4B3055EE1800, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x9745D1745D174000},
    0x27F, 0x3A20},

    {{0x8666666666666800, 0x81CA4B3055EE1800, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3020},

    {{0xCEC4EC4EC4EC5000, 0x81CA4B3055EE1800, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3220},

    {{0xE92CC157B8644000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0xCEC4EC4EC4EC5000},
    0x27F, 0x3820},

    {{0xA666666666666800, 0xE92CC157B8644000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3020},

    {{0x9E79E79E79E7A000, 0xE92CC157B8644000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3220},

    {{0x88659D9FAB6F1000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x9E79E79E79E7A000},
    0x27F, 0x3820},

    {{0x8666666666666800, 0x88659D9FAB6F1000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3020},

    {{0x8666666666666800, 0x88659D9FAB6F1000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3020},

    {{0x81E6DF42BBA6B800, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x8666666666666800},
    0x27F, 0x3820},

    {{0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x8666666666666800, 0x81E6DF42BBA6B800},
    0x27F, 0x0020}
};



    unsigned char code[0x100] = 
    {
        0xDD, 0x05, 0x78, 0x00, 0x00, 0x00,  //  fld         qword ptr [x1 (200E98h)] 
        0xDC, 0x35, 0x80, 0x00, 0x00, 0x00,  //  fdiv        qword ptr [x2 (200EA0h)] 
        0xDD, 0x05, 0x80, 0x00, 0x00, 0x00,  //  fld         qword ptr [x2 (200EA0h)] 
        0xDC, 0x35, 0x78, 0x00, 0x00, 0x00,  //  fdiv        qword ptr [x1 (200E98h)] 
        0xDE, 0xC1,                          //  faddp       st(1),st 
        0xDD, 0x05, 0x88, 0x00, 0x00, 0x00,  //  fld         qword ptr [x3 (200EA8h)] 
        0xDC, 0x35, 0x80, 0x00, 0x00, 0x00,  //  fdiv        qword ptr [x2 (200EA0h)] 
        0xDE, 0xC1,                          //  faddp       st(1),st 
        0xDD, 0x05, 0x80, 0x00, 0x00, 0x00,  //  fld         qword ptr [x2 (200EA0h)] 
        0xDC, 0x35, 0x88, 0x00, 0x00, 0x00,  //  fdiv        qword ptr [x3 (200EA8h)] 
        0xDE, 0xC1,                          //  faddp       st(1),st 
        0xDD, 0x05, 0x88, 0x00, 0x00, 0x00,  //  fld         qword ptr [x3 (200EA8h)] 
        0xDC, 0x0D, 0x70, 0x00, 0x00, 0x00,  //  fmul        qword ptr [__real@c000000000000000 (1E38D0h)] 
        0xDE, 0xF9,                          //  fdivp       st(1),st 
        0xDD, 0x1D, 0x60, 0x00, 0x00, 0x00,  //  fstp        qword ptr [x4 (202A50h)] 

    };


    OPERAND_SIZE args[] = {0xc000000000000000,
                           0x3ff199999999999a,
                           0x3ff4cccccccccccd,
                           0x4000cccccccccccd};

    memcpy(code+0x70, args, sizeof(args)); 

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    FPU_Exec(&proc, 15, states);

    DIANA_TEST_ASSERT(*(OPERAND_SIZE *)(code + 0x60)== 0xbff03cdbe85774d7);
    DIANA_TEST_ASSERT(GET_REG_FPU_ST7 == 0xbff03cdbe85774d7);
}



static void test_processor_integration3()
{
static FpuTestState states[] = {
    {{0x8000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0xC000000000000000},
    0x27F, 0x0000},

    {{0x8000000000000000, 0xC000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0x8000000000000000, 0xC000000000000000, 0x0000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0xC000000000000000, 0x8000000000000000, 0x8000000000000000, 0x0000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x8000000000000000},
    0x27F, 0x0000},

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0xC000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0xC000000000000000, 0x8000000000000000, 0x8000000000000000, 0xC000000000000000,
    0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0xC000000000000000},
    0x27F, 0x0000},

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0x8000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0xC000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0x9000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800},

    {{0xC000000000000000, 0xC000000000000000, 0x8000000000000000, 0x8000000000000000,
    0xC000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000},
    0x27F, 0x3800}
};


    //const float a_float = 1;
    //const float b_float = 2;
    //const double a = (double )a_float;
    //double b = (double )b_float;
    //const double d = a+b;
    //const double z = a-b;
    //b = z*z;
    //const double c = (a+b+z*z+d*(a-z*b))/(a+b+z*z);

    unsigned char code[0x100] = 
    {
        0xD9, 0x44, 0x24, 0x0C,       //  fld     dword ptr [esp+0Ch]
        0xD8, 0x44, 0x24, 0x08,       //  fadd    dword ptr [esp+8]
        0xDD, 0xD9,                   //  fstp    st(1)
        0xD9, 0x44, 0x24, 0x08,       //  fld     dword ptr [esp+8]
        0xD8, 0x64, 0x24, 0x0C,       //  fsub    dword ptr [esp+0Ch] 
        0xDD, 0xD2,                   //  fst     st(2)
        0xD8, 0xCA,                   //  fmul    st, st(2)
        0xDD, 0xDB,                   //  fstp    st(3)
        0xD9, 0x44, 0x24, 0x08,       //  fld     dword ptr [esp+8]
        0xD8, 0xC3,                   //  fadd    st, st(3)
        0xD8, 0xC3,                   //  fadd    st, st(3)
        0xDD, 0xDC,                   //  fstp    st(4)
        0xD9, 0xC1,                   //  fld     st(1) 
        0xD8, 0xCB,                   //  fmul    st, st(3)
        0xD8, 0x6C, 0x24, 0x08,       //  fsubr   dword ptr [esp+8]
        0xD8, 0xC9,                   //  fmul    st, st(1)
        0xD8, 0xC4,                   //  fadd    st, st(4) 
        0xD8, 0xF4                    //  fdiv    st, st(4)
    };

    unsigned char args[] = {0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x40};
    const int size = sizeof(code);
    memcpy((code + size - 0x8), &args, 8);

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;

    SET_REG_RSP(size-4-0xC);

    FPU_Exec(&proc, 18, states);

    DIANA_TEST_ASSERT(0x4008000000000000 == GET_REG_FPU_ST0);
}

static void test_processor_integration4()
{
    unsigned char code[]  = 
    {
        0xDD, 0x45, 0xF4,                       //  fld         qword ptr [test] ; fld     qword ptr [rbp-0Ch]
        0xE8, 0x01, 0x00, 0x00, 0x00,           //  call function
        0xC3,                                   //  ret

        0x55,                                   //  push        ebp  
        0x8B, 0xEC,                             //  mov         ebp,esp 
        0x83, 0xEC, 0x20,                       //  sub         esp,20h 
        0x83, 0xE4, 0xF0,                       //  and         esp,0FFFFFFF0h 
        0xD9, 0xC0,                             //  fld         st(0) 
        0xD9, 0x54, 0x24, 0x18,                 //  fst         dword ptr [esp+18h] 
        0xDF, 0x7C, 0x24, 0x10,                 //  fistp       qword ptr [esp+10h] 
        0xDF, 0x6C, 0x24, 0x10,                 //  fild        qword ptr [esp+10h] 
        0x8B, 0x54, 0x24, 0x18,                 //  mov         edx,dword ptr [esp+18h] 
        0x8B, 0x44, 0x24, 0x10,                 //  mov         eax,dword ptr [esp+10h] 
        0x85, 0xC0,                             //  test        eax,eax 
        0x74, 0x3C,                             //  je          integer_QnaN_or_zero (44EC35h) 
        0xDE, 0xE9,                             //  fsubp       st(1),st 
        0x85, 0xD2,                             //  test        edx,edx 
        0x79, 0x1E,                             //  jns         positive (44EC1Dh) 
        0xD9, 0x1C, 0x24,                       //  fstp        dword ptr [esp] 
        0x8B, 0x0C, 0x24,                       //  mov         ecx,dword ptr [esp] 
        0x81, 0xF1, 0x00, 0x00, 0x00, 0x80,     //  xor         ecx,80000000h 
        0x81, 0xC1, 0xFF, 0xFF, 0xFF, 0x7F,     //  add         ecx,7FFFFFFFh 
        0x83, 0xD0, 0x00,                       //  adc         eax,0 
        0x8B, 0x54, 0x24, 0x14,                 //  mov         edx,dword ptr [esp+14h] 
        0x83, 0xD2, 0x00,                       //  adc         edx,0 
        0xEB, 0x2C,                             //  jmp         localexit (44EC49h) 
        0xD9, 0x1C, 0x24,                       //  fstp        dword ptr [esp] 
        0x8B, 0x0C, 0x24,                       //  mov         ecx,dword ptr [esp] 
        0x81, 0xC1, 0xFF, 0xFF, 0xFF, 0x7F,     //  add         ecx,7FFFFFFFh 
        0x83, 0xD8, 0x00,                       //  sbb         eax,0 
        0x8B, 0x54, 0x24, 0x14,                 //  mov         edx,dword ptr [esp+14h] 
        0x83, 0xDA, 0x00,                       //  sbb         edx,0 
        0xEB, 0x14,                             //  jmp         localexit (44EC49h) 
        0x8B, 0x54, 0x24, 0x14,                 //  mov         edx,dword ptr [esp+14h] 
        0xF7, 0xC2, 0xFF, 0xFF, 0xFF, 0x7F,     //  test        edx,7FFFFFFFh 
        0x75, 0xB8,                             //  jne         arg_is_not_integer_QnaN (44EBF9h) 
        0xD9, 0x5C, 0x24, 0x18,                 //  fstp        dword ptr [esp+18h] 
        0xD9, 0x5C, 0x24, 0x18,                 //  fstp        dword ptr [esp+18h] 
        0xC9,                                   //  leave
        0xC3                                    //  ret
    };

    // DATA: 
    unsigned char data[] = {0x47, 0xe1, 0x7a, 0x14, 0xae, 0x47, 0x5c, 0xc0}; // test 

    std::vector<char> allspace;
    // pack code
    allspace.insert(allspace.end(), code, code + sizeof(code));
    while(allspace.size() % 0x100)
    {
        allspace.push_back(0);
    }

    // pack data
    OPERAND_SIZE dataOffset = allspace.size();
    allspace.insert(allspace.end(), (char*)data, (char*)data + sizeof(data));
    while(allspace.size() % 0x100)
    {
        allspace.push_back(0);
    }
    // pack stack
    allspace.resize(allspace.size() + 0x8000);

    OPERAND_SIZE stackStart = allspace.size() - 0x20;

    *(unsigned int*)&allspace[(size_t)stackStart] = 0x55555555;
    *(unsigned int*)&allspace[(size_t)stackStart+4] = (unsigned int)dataOffset;


    CTestProcessor proc((unsigned char*)&allspace.front(), allspace.size(), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &proc.GetSelf()->m_fpu;
    pFpu->controlWord = 0x027f;
    pFpu->statusWord = 0x0020;

    SET_REG_ESP(stackStart);
    SET_REG_EBP(dataOffset + 0xC);

    for(int i =0; ;++i)
    {
        int iRes = proc.ExecOnce();
        DIANA_TEST_ASSERT(iRes == 0);

        switch(i)
        {
            case 0:
                {
                    DIANA_TEST_ASSERT(pFpu->statusWord == 0x3820);
                    DIANA_TEST_ASSERT(GET_REG_MM7 == 0xE23D70A3D70A3800);
                    DIANA_TEST_ASSERT(GET_REG_FPU_ST0 == 0xc05c47ae147ae147);
                }
                break;
        }
        if (GET_REG_RIP == 0x55555555)
            break;
    }

    DIANA_TEST_ASSERT(GET_REG_RAX == 0xffffff8f);

}

static OPERAND_SIZE ContextMM(const Diana_Processor_Registers_Context * pContext, int i)
{
    const DianaRegisterValue_type * pMM = &pContext->reg_MM0;
    return pMM[i].value;
}

// reg_MM0..7 of the context are ST(0)..ST(7) in both modes: the physical register k (MMk) is ST((k - TOP) & 7)
static void test_processor_context_mm_order(int mode)
{
    unsigned char code[0x10] = {0};
    CTestProcessor proc(code, sizeof(code), 0, mode);
    DianaProcessor * pCallContext = proc.GetSelf();

    DIANA_AUTO_PTR<Diana_Processor_Registers_Context> context(new Diana_Processor_Registers_Context());
    Diana_Processor_Registers_Context * pContext = context.get();
    DIANA_TEST_ASSERT(DianaProcessor_QueryContext(pCallContext, pContext) == DI_SUCCESS);

    const int top = 5;
    pContext->fpuState.statusWord = (DI_UINT16)(top << 11);
    pContext->fpuStateFlags = DIANA_PROCESSOR_CONTEXT_FPU_STATE_VALID;
    DianaRegisterValue_type * pMM = &pContext->reg_MM0;
    for (int i = 0; i < 8; ++i)
        pMM[i].value = 0x1111111111111111ULL * (i + 1);
    DIANA_TEST_ASSERT(DianaProcessor_InitContext(pCallContext, pContext) == DI_SUCCESS);

    DIANA_TEST_ASSERT(GET_REG_MM0 == ContextMM(pContext, (0 - top) & 7));
    DIANA_TEST_ASSERT(GET_REG_MM1 == ContextMM(pContext, (1 - top) & 7));
    DIANA_TEST_ASSERT(GET_REG_MM2 == ContextMM(pContext, (2 - top) & 7));
    DIANA_TEST_ASSERT(GET_REG_MM3 == ContextMM(pContext, (3 - top) & 7));
    DIANA_TEST_ASSERT(GET_REG_MM4 == ContextMM(pContext, (4 - top) & 7));
    DIANA_TEST_ASSERT(GET_REG_MM5 == ContextMM(pContext, (5 - top) & 7));
    DIANA_TEST_ASSERT(GET_REG_MM6 == ContextMM(pContext, (6 - top) & 7));
    DIANA_TEST_ASSERT(GET_REG_MM7 == ContextMM(pContext, (7 - top) & 7));

    DIANA_AUTO_PTR<Diana_Processor_Registers_Context> context2(new Diana_Processor_Registers_Context());
    DIANA_TEST_ASSERT(DianaProcessor_QueryContext(pCallContext, context2.get()) == DI_SUCCESS);
    for (int i = 0; i < 8; ++i)
        DIANA_TEST_ASSERT(ContextMM(context2.get(), i) == ContextMM(pContext, i));
}

// the Windows CONTEXT: FSAVE RegisterArea of 10-byte ST(i) and the full tag word (32 bits), FXSAVE FloatRegisters
// and the abridged tag word (64 bits)
static void test_processor_context_win32()
{
    DIANA_AUTO_PTR<Diana_Processor_Registers_Context> context(new Diana_Processor_Registers_Context());
    Diana_Processor_Registers_Context * pContext = context.get();

    static DIANA_CONTEXT_NTLIKE_32 context32Storage;
    DIANA_CONTEXT_NTLIKE_32 * context32 = &context32Storage;
    memset(context32, 0, sizeof(DIANA_CONTEXT_NTLIKE_32));
    for (int i = 0; i < 8; ++i)
    {
        OPERAND_SIZE significand = 0x8000000000000001ULL + 0x0101010101010100ULL * i;
        DI_UINT16 exponent = (DI_UINT16)(0x3FFF + i);
        memcpy(&context32->FloatSave.RegisterArea[10 * i], &significand, 8);
        memcpy(&context32->FloatSave.RegisterArea[10 * i + 8], &exponent, 2);
    }
    context32->FloatSave.TagWord = 0xFFFF & ~((3u << 2 * 7) | (3u << 2 * 6)) & ~(2u << 2 * 2); // 7, 6: valid; 2: zero
    context32->FloatSave.StatusWord = 6 << 11;
    DIANA_TEST_ASSERT(DianaProcessor_ConvertContextToIndependent_Win32(context32, pContext) == DI_SUCCESS);
    for (int i = 0; i < 8; ++i)
        DIANA_TEST_ASSERT(ContextMM(pContext, i) == 0x8000000000000001ULL + 0x0101010101010100ULL * i);
    DIANA_TEST_ASSERT(pContext->fpuState.statusWord == 6 << 11);
    for (int k = 0; k < 8; ++k)
        DIANA_TEST_ASSERT((pContext->fpuState.registerFlags[k] == DI_PROCESSOR_FPU_REGISTER_BUSY) ==
                          (k == 7 || k == 6 || k == 2));

    static DIANA_CONTEXT_NTLIKE_64 context64Storage; // aligned 16
    DIANA_CONTEXT_NTLIKE_64 * context64 = &context64Storage;
    memset(context64, 0, sizeof(DIANA_CONTEXT_NTLIKE_64));
    for (int i = 0; i < 8; ++i)
    {
        context64->FltSave.FloatRegisters[i].Low = 0x8000000000000001ULL + 0x0101010101010100ULL * i;
        context64->FltSave.FloatRegisters[i].High = 0x3FFF + i;
    }
    context64->FltSave.TagWord = (DI_CHAR)0xC4; // 7, 6, 2
    DIANA_TEST_ASSERT(DianaProcessor_ConvertContextToIndependent_X64(context64, pContext) == DI_SUCCESS);
    for (int i = 0; i < 8; ++i)
        DIANA_TEST_ASSERT(ContextMM(pContext, i) == 0x8000000000000001ULL + 0x0101010101010100ULL * i);
    for (int k = 0; k < 8; ++k)
        DIANA_TEST_ASSERT((pContext->fpuState.registerFlags[k] == DI_PROCESSOR_FPU_REGISTER_BUSY) ==
                          (k == 7 || k == 6 || k == 2));
}

void test_processor_fpu_integration()
{
    test_processor_integration1();
    test_processor_integration2();
    test_processor_integration3();
    test_processor_integration4();
    test_processor_context_mm_order(DIANA_MODE32);
    test_processor_context_mm_order(DIANA_MODE64);
    test_processor_context_win32();
}