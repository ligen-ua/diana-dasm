#ifndef DIANA_WIN32_EXECUTABLE_HEAP_H
#define DIANA_WIN32_EXECUTABLE_HEAP_H

#include "diana_allocators.h"
typedef struct _DianaExecutableHeapAllocator
{
    Diana_Allocator m_parent;
}DianaExecutableHeapAllocator;

void Diana_InitHeapAllocator(DianaExecutableHeapAllocator * pThis);

int Diana_InitExecutableHeap();
void * DianaExecutableHeap_Alloc(size_t size);
void DianaExecutableHeap_Free(void * pBuffer);



#endif