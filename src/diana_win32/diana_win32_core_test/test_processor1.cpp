#include "test_win32_common.h"

char g_stack[g_stackSize];

namespace {

void * g_oldESP = 0;

typedef NTSTATUS (WINAPI * NtClosePtr_type)(
    HANDLE Handle
);

HANDLE g_handle =0;
NtClosePtr_type g_ntClose = 0;

void __cdecl test_function_impl()
{
    if (g_ntClose(g_handle) != 0)
    {
        __asm int 3;
    }
    //MessageBoxA(0,0,0,MB_OK);
    /*
    wchar_t buf[] = L"hello, world!";
    DWORD written  = 0;
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), buf, sizeof(buf)/sizeof(buf[0])-1, &written, 0);*/
}
void __cdecl test_function()
{
    test_function_impl();
    return;
    //void * newESP = g_stack + sizeof(g_stack);
    //__asm xor eax, eax
    //__asm xor ebx, ebx
    //__asm xor ecx, ecx
    //__asm xor edx, edx
    //__asm xor edi, edi
    //__asm xor esi, esi

    //__asm push ebp
    //__asm mov g_oldESP, esp

    //__asm mov esp, newESP
    //__asm mov ebp, esp

    //__asm call test_function_impl

    //__asm mov esp, g_oldESP
    //__asm pop ebp
}

}
void test_processor1()
{
    HMODULE hNtDLL = GetModuleHandleW(L"ntdll.dll");
    g_ntClose = (NtClosePtr_type)GetProcAddress(hNtDLL, "NtClose");
    g_handle = CreateEvent(0,0,0,0);

//    test_function();
    DianaWin32Processor proc;

    DIANA_TEST_ASSERT(DianaWin32Processor_Init(&proc, 
                                         (OPERAND_SIZE)g_stack, 
                                         (OPERAND_SIZE)g_stack + sizeof(g_stack))==DI_SUCCESS);
    
    DianaProcessor * pCallContext = &proc.m_processor;
    DianaProcessor_LoadLiveContext32(pCallContext);

    DI_JUMP_TO_RIP((OPERAND_SIZE)(void*)test_function);
    SET_REG_RSP(GET_REG_RSP - 0x2000);
  
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
        //PrintContext(pCallContext);
    }
}
