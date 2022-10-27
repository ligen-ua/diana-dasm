#include "test_win32_common.h"

void __stdcall test_function(long long * pResult)
{
    double x1 = 55.55;
    double x2 = 1234567.0;
    double x3 = x1*x2;
    *pResult = (long long)x3;
}

static void test_processor_fpu1()
{
    DianaWin32Processor proc;

    DIANA_TEST_ASSERT(DianaWin32Processor_Init(&proc,
                                         (OPERAND_SIZE)g_stack, 
                                         (OPERAND_SIZE)g_stack + sizeof(g_stack))==DI_SUCCESS);
    
    DianaProcessor * pCallContext = &proc.m_processor;

    DianaProcessor_LoadLiveContext32(pCallContext);

    DI_JUMP_TO_RIP((OPERAND_SIZE)(void*)test_function);
    SET_REG_RSP((OPERAND_SIZE)g_stack + sizeof(g_stack));

    long long original_result = 0;
    test_function(&original_result);

    long long result = 0;
  
    {
        OPERAND_SIZE addressOfResult = (OPERAND_SIZE)&result;
        diana_internal_push(pCallContext, &addressOfResult);
    }

    OPERAND_SIZE exitOp = 0x87654321;
    diana_internal_push(pCallContext, &exitOp);

    std::vector<State> states;
    for(int i = 0; ; ++i)
    {
        OPERAND_SIZE rip = GET_REG_RIP;
        if (rip == exitOp)
            break;

        states.push_back(State(pCallContext));

        int res = DianaProcessor_ExecOnce(pCallContext);
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        if (res != DI_SUCCESS)
        {
            OPERAND_SIZE failedRip  = GET_REG_RIP;
            break;
        }
    }
    DIANA_TEST_ASSERT(result == original_result);
}

void test_processor_fpu()
{
    DIANA_TEST(test_processor_fpu1());
}