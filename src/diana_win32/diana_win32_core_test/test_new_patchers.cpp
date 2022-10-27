#include "test_common.h"
extern "C"
{
#include "diana_patchers/diana_ultimate_patcher.h"
}

static void test_function_to_hook()
{
}

static void test_patchers_impl()
{
}

void test_patchers()
{
    DIANA_TEST(test_patchers_impl());
}