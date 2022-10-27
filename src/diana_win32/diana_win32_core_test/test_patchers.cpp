#include "test_win32_common.h"

extern "C"
{
#include "diana_win32_executable_heap_old.h"
#include "diana_win32_exceptions.h"
}

static int g_context = 0x55AA;
static DianaExecutableHeapAllocator g_allocator;

// test function Diana_PatchSomething32
_declspec(naked) static int __stdcall function0(int x, int y)
{
    __asm
    {
    nop
    push ebp
    mov ebp, esp

    mov eax, [ebp + 0x8]
    add eax, [ebp + 0xc]

    mov esp, ebp
    pop ebp
    ret 8
    }
}
void __stdcall function0_patch(void * pContext, void * pOriginalESP)
{
    if((DIANA_ARG(pOriginalESP, 1, int) + DIANA_ARG(pOriginalESP, 2, int) == 13))
    {
        DIANA_RETURN(8, 14);
    }
}

void __stdcall function0_patch2(void * pContext, void * pOriginalESP)
{
    if((DIANA_ARG(pOriginalESP, 1, int) + DIANA_ARG(pOriginalESP, 2, int) > 100))
    {
        DIANA_RETURN(8, 0);
    }
}


static void test_patchers0()
{
    void * pOriginalFunction = 0;
    Diana_EnsureWriteAccess(function0);
    int iRes = Diana_PatchSomething32(function0,
                                      10,
                                        function0_patch,
                                      &g_context,
                                      &g_allocator.m_parent,
                                      &pOriginalFunction
                                      );
    DIANA_TEST_ASSERT(!iRes);
        
    // test reentrancy
    iRes = Diana_PatchSomething32(function0,
                                      10,
                                        function0_patch2,
                                      &g_context,
                                      &g_allocator.m_parent,
                                      0
                                      );
    DIANA_TEST_ASSERT(!iRes);


    // run tests
    int res = function0(10,3);
    DIANA_TEST_ASSERT(res == 14);

    res = ((int (__stdcall *)(int x, int y))pOriginalFunction)(10, 3);
    DIANA_TEST_ASSERT(res == 13);

    // second handler
    res = function0(10,300);
    DIANA_TEST_ASSERT(res == 0);
}

// test ex function
_declspec(naked) static int __stdcall function1(int x, int y)
{
    __asm
    {
    push ebp
    mov ebp, esp

    mov eax, [ebp + 0x8]
    add eax, [ebp + 0xc]

    mov esp, ebp
    pop ebp
    ret 8
    }
}

void __stdcall function1_patch_ret(DianaPatcher_RetHook32 * pRetHook,
                                   void * pRegistersSet)
{
    DIANA_TEST_ASSERT(pRetHook->pContext == &g_context);
    int a = DIANA_ARG(pRetHook->pCopiedArgs, 1, int);
    int b = DIANA_ARG(pRetHook->pCopiedArgs, 2, int);
    DIANA_TEST_ASSERT(a + b == 13);

    unsigned int & eaxVal = DIANA_PATCHER_GET_EAX(pRegistersSet);
    DIANA_TEST_ASSERT(eaxVal == 13);

    // set new result
    eaxVal = g_context;

    // test 42
    DIANA_TEST_ASSERT(pRetHook->pContext == &g_context);
    DIANA_TEST_ASSERT(*(int*)pRetHook->pCallContext == 42);
}

static int g_testReturn = 0;
void __stdcall function1_patch(void * pContext, 
                               void * pOriginalESP,
                               void * pInputRegisters)
{
    DIANA_TEST_ASSERT(pContext == &g_context);

    if (g_testReturn)
    {
        if (DIANA_ARG(pOriginalESP, 1, int) + DIANA_ARG(pOriginalESP, 2, int) == 13)
        {
            DIANA_RETURN_EX(8, 13);
        }
    }

    if (DIANA_ARG(pOriginalESP, 1, int) + DIANA_ARG(pOriginalESP, 2, int) == 13)
    {
        int  * pValue = 0;
        int iRes = Diana_SetupRet(&g_allocator.m_parent,
                                  pOriginalESP,
                                  function1_patch_ret, 
                                  pContext,
                                  2,
                                  sizeof(int),
                                  (void**)&pValue
                                  );
        DIANA_TEST_ASSERT(iRes == 0);
        *pValue = 42;
    }

    if (DIANA_ARG(pOriginalESP, 1, int) + DIANA_ARG(pOriginalESP, 2, int) == 0)
    {
        unsigned int & esiVar = DIANA_PATCHER_GET_ESI(pInputRegisters);
        DIANA_TEST_ASSERT(esiVar == 100500);
    }
}

static void test_patchers1()
{
    Diana_EnsureWriteAccess(function1);
    int iRes = Diana_PatchSomethingEx(function1,
                                      10,
                                      function1_patch,
                                      &g_context,
                                      &g_allocator.m_parent,
                                      0);

    DIANA_TEST_ASSERT(!iRes);

    // test 1 - simple
    int c = function1(1,3);
    DIANA_TEST_ASSERT(c == 4);
    
    // test 2 - Diana_SetupRet32 testing
    c = function1(10,3);
    DIANA_TEST_ASSERT(c == g_context);


    // test 3 - registers
    __asm 
    {
        push esi
        mov esi, 100500
        push 0
        push 0
        call function1
        pop esi
    }

    // test 4 - return
    g_testReturn = 1;

    c = function1(10,3);
    DIANA_TEST_ASSERT(c == 13);

    g_testReturn = 0;
}

void test_patchers()
{
    Diana_InitHeapAllocator(&g_allocator);
    DIANA_TEST(test_patchers0());
    DIANA_TEST(test_patchers1());
}