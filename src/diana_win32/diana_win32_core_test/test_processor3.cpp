#include "test_win32_common.h"

static void * g_oldESP = 0;

static void __cdecl test_function()
{
    try
    {
        throw std::runtime_error("hello, world");
    }
    catch(std::exception & e)
    {
        std::cout<<e.what()<<"\n";
    }
}

static void test_processor3_impl()
{
    DianaWin32Processor proc;

    DIANA_TEST_ASSERT(DianaWin32Processor_Init(&proc, 
                                         (OPERAND_SIZE)g_stack, 
                                         (OPERAND_SIZE)g_stack + sizeof(g_stack))==DI_SUCCESS);
    
    DianaProcessor * pCallContext = &proc.m_processor;

    DianaProcessor_LoadLiveContext32(pCallContext);

    DI_JUMP_TO_RIP((OPERAND_SIZE)(void*)test_function);
    SET_REG_RSP((OPERAND_SIZE)g_stack + sizeof(g_stack));
  
    OPERAND_SIZE exitOp = 0x87654321;
    diana_internal_push(pCallContext, &exitOp);

    std::vector<State> states;
    for(int i = 0; ; ++i)
    {
        OPERAND_SIZE rip = GET_REG_RIP;
        if (rip == exitOp)
            break;

        //states.push_back(State(pCallContext));

        int res = DianaProcessor_ExecOnce(pCallContext);
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        if (res != DI_SUCCESS)
        {
            OPERAND_SIZE failedRip  = GET_REG_RIP;
            break;
        }
    }
}

void test_processor3()
{
    DIANA_TEST(test_processor3_impl());
}