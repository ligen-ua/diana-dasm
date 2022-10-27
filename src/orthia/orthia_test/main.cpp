#include "test_common.h"
#include "diana_core_cpp.h"
extern "C"
{
#include "diana_processor/diana_processor_core.h"
#include "diana_win32.h"
}
#include "windows.h"
#include "vector"
#include "map"
#pragma warning(disable:4996)

std::vector<char> g_buffer;
std::map<void*, void*> g_testMap;

static void test_vm_fake()
{
    g_buffer.resize(1024);
    memcpy(&g_buffer.front(), "hello, world", 12);
    memcpy(&g_buffer.front()+25, ":)", 2);
}

static void test_crash()
{
    g_testMap[(void*)1] = &test_vm_fake;
    g_testMap[(void*)2] = &test_crash;
    g_testMap[(void*)3] = &g_buffer;
    g_testMap[(void*)4] = &g_testMap;
    // integration test for exec command testing:
    volatile static int global = 0;
    volatile int x = 0;
    volatile int * pData = (int * )0x12345678;
    // do the following commands:
    // CASE 1. Exec
    // .load orthia.dll;!orthia.profile /f %temp%\test.db; 
    // .dvalloc /b 0x12340000 0x10000
    // !orthia.exec 100 
    // CASE 2. vm call
    // .load orthia.dll;!orthia.profile /f %temp%\test.db; !orthia.vm_vm_def
    // !orthia.vm_vm_call 0 orthia_test!test_vm_fake --print
    // CASE 3. shuttle
    // .load orthia.dll;!orthia.profile /f %temp%\test.db; !orthia.vm_vm_def
    // !orthia.vm_mod_load 0 1 0 orthia_shuttle.dll
    // !orthia.vm_mod_shcall 0 1 0 ping 1 2 3
    // old studios:
    // - 32 bit
    // !orthia.vm_mod_shcall 0 1 1 dinkumware_map_for_each orthia_test!g_testMap 8
    // - 64 bit
    // !orthia.vm_mod_shcall 0 1 1 dinkumware_map_for_each orthia_test!g_testMap 0x10
    // new ones:
    // !orthia.vm_mod_shcall 0 1 1 dinkumware_map_for_each orthia_test!g_testMap

    if (((char*)pData)[0] == (char)1 && 
        pData[1] == 2 && 
        ((long long *)pData)[3] == 3)
    {
        strcpy((char*)(pData+4), "hello, world");
        if (((double*)pData)[4] == 1.1)
        {
            *(char*)(pData+256) = 1;
            *((double*)(char*)(pData+256)) = 1.5;
        }
    }
    ULONG id = GetCurrentThreadId(); 
    // id should be in registers, usually in EAX
    global = 10;
    x = id/x;
    test_vm_fake();
}

void test_utils();
void test_memory_manager();
void test_memory_cache();
void test_vm();
void test_pe();
void test_vm_shuttle();
void test_shuttle_utils();
void test_tokenizer();

int main(int argc, char * argv[])
{
    if (argc == 2 && strcmp(argv[1], "/crash") == 0)
    {
        test_crash();
    }
    Diana_Init();
    DianaProcessor_GlobalInit();
    DianaWin32_Init();

    if (argc == 2 && strcmp(argv[1], "/shuttle") == 0)
    {
        test_vm_shuttle();
    }
    test_tokenizer();
    test_shuttle_utils();
    test_pe();
    test_memory_cache();
    test_vm();
    test_memory_manager();
    test_utils();
    return 0;
}