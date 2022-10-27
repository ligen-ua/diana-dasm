#ifndef DIANA_ALLOCATORS_H
#define DIANA_ALLOCATORS_H

#include "diana_core.h"

typedef struct _DianaMAllocator
{
    Diana_Allocator m_parent;
}DianaMAllocator;

void Diana_InitMAllocator(DianaMAllocator * pThis);


// Default Allocator functions:
void * Diana_DefaultAllocator_Alloc(DIANA_SIZE_T size);
void  Diana_DefaultAllocator_Free(void * pMemory);


#endif