extern "C"
{
#include "diana_core.h"
}
void pe_analyze_test();
#include "test_common.h"

int main()
{
    Diana_Init();
    pe_analyze_test();
    return g_diana_resultCode;
}