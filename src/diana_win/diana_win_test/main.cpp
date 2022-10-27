extern "C"
{
#include "diana_core.h"
}
void pe_analyze_test();

int main()
{
    Diana_Init();
    pe_analyze_test();
    return 0;
}