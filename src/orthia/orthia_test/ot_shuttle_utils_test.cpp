#include "orthia_shuttle_interface.h"
#include "test_common.h"
#include "orthia_utils.h"

struct CTestHypervisorInterface:orthia_shuttle::IHypervisorInterface
{
}g_testHypervisorInterface;


static int g_count = 0;
static long ORTHIA_STDCALL PrintStream(const orthia_shuttle::PrintArgument * pText, int count)
{
    g_count += count;
    return 0;
}

static void test_shuttle_utils1()
{
    g_testHypervisorInterface.printStream = PrintStream;
    {
      orthia_shuttle::CPrintStream stream(&g_testHypervisorInterface);
      stream<<"Test"<<123<<L"hi"<<(void*)test_shuttle_utils1;
    }
    DIANA_TEST_ASSERT(g_count == 4);
}
void test_shuttle_utils()
{
    DIANA_TEST(test_shuttle_utils1());
}