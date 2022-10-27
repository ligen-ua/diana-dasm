extern "C"
{
#include "diana_win32_core.h"
}

void test_streams();
void test_processor1();
void test_processor2();
void test_processor3();
void test_processor4();
void test_patchers();
void test_processor_fpu();

#include "iostream"

// WOW64 stuff
typedef BOOL (WINAPI * IsWow64Process_type) (HANDLE, PBOOL);
static IsWow64Process_type g_pIsWow64Process;

static BOOL IsWow64()
{
    BOOL bIsWow64 = FALSE;
    g_pIsWow64Process = (IsWow64Process_type)GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
    if (g_pIsWow64Process)
    {
        g_pIsWow64Process(GetCurrentProcess(), &bIsWow64);
    }
    return bIsWow64;
}

int main()
{
    Diana_Init();
    DianaProcessor_GlobalInit();
#ifdef DIANA_CFG_I386 
    DianaWin32Processor_GlobalInit();

    test_patchers();
    test_streams();

    test_processor_fpu();
    test_processor4();
    if(!IsWow64())
    {
        // TEMPORARY FIX: emulator conflicts with MS emulator
        test_processor1();
        test_processor2();
        test_processor3();
    }
#endif

    return 0;
}