#include "diana_win32.h"
#include "diana_win32_executable_heap.h"

int DianaWin32_Init()
{
#ifdef DIANA_HAS_WIN32
    DI_CHECK(DianaWin32ExecutableHeap_Init());
#endif
    return DI_SUCCESS;
}

