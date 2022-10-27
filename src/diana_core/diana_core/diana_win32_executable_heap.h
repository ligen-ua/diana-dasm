#ifndef DIANA_WIN32_EXECUTABLE_HEAP_H2
#define DIANA_WIN32_EXECUTABLE_HEAP_H2

#include "diana_allocators.h"
typedef struct _DianaWin32ExecutableHeapAllocator
{
    Diana_Allocator m_parent;
}DianaWin32ExecutableHeapAllocator;

void DianaWin32ExecutableHeapAllocator_Init(DianaWin32ExecutableHeapAllocator * pThis);

int DianaWin32ExecutableHeap_Init();
void * DianaWin32ExecutableHeap_Alloc(DIANA_SIZE_T size);
void DianaWin32ExecutableHeap_Free(void * pBuffer);



#endif