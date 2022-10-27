#include "test_win32_common.h"


static int __stdcall test_function(int testParam1, int testParam2)
{
    return testParam1 + testParam2;
}

static void test_processor4_impl()
{
    DIANA_TEST_ASSERT(DianaWin32Processor_EhookSTD((void*)test_function, 8, 2) == DI_SUCCESS);
    int res = test_function(2, 3);
    DIANA_TEST_ASSERT(res == 5);
}

void test_processor4()
{
    DIANA_TEST(test_processor4_impl());
}