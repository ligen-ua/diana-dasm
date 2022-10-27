#include "diana_allocators.h"

static void * MAllocator_Alloc(void * pThis, DIANA_SIZE_T size)
{
    pThis;
    return DIANA_MALLOC(size);
}
static void MAllocator_Dealloc(void * pThis, void * pBuffer)
{
    pThis;
    DIANA_FREE(pBuffer);
}
static int MAllocator_Patch(void * pThis, void * pDest, const void * pSource, DIANA_SIZE_T size)
{
    pThis;
    DIANA_MEMCPY(pDest, pSource, size);
    return DI_SUCCESS;
}
void Diana_InitMAllocator(DianaMAllocator * pThis)
{
    Diana_Allocator  * pAll = &pThis->m_parent;
    Diana_AllocatorInit(pAll, 
                        MAllocator_Alloc, 
                        MAllocator_Dealloc,
                        MAllocator_Patch);
}
