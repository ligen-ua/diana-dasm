#include "diana_win32.h"
#include "diana_win32_executable_heap.h"

#ifdef DIANA_HAS_WIN32

int DianaWin32_Init()
{
    DI_CHECK(DianaWin32ExecutableHeap_Init());
    return DI_SUCCESS;
}

#endif