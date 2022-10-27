#include "diana_win32_executable_heap_old.h"
#include "windows.h"


static HANDLE g_executableHeap;


int Diana_InitExecutableHeap()
{
    // create heap
    g_executableHeap = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 
                               0,
                               0);
    if (!g_executableHeap)
        return DI_WIN32_ERROR;
    return DI_SUCCESS;
}

static void * HeapAllocator_Alloc(void * pThis, size_t size)
{
    return HeapAlloc(g_executableHeap, 
                     0,
                     size);
}
static void HeapAllocator_Dealloc(void * pThis, void * pBuffer)
{
   HeapFree(g_executableHeap,
                 0,
                 pBuffer);
}
static int HeapAllocator_Patch(void * pThis, void * pDest, const void * pSource, size_t size)
{
    memcpy(pDest, pSource, size);
    return DI_SUCCESS;
}

void * DianaExecutableHeap_Alloc(size_t size)
{
    return HeapAlloc(g_executableHeap, 
                     0,
                     size);
}
void DianaExecutableHeap_Free(void * pBuffer)
{
  HeapFree(g_executableHeap,
                 0,
                 pBuffer);
}

void Diana_InitHeapAllocator(DianaExecutableHeapAllocator * pThis)
{
    Diana_AllocatorInit(&pThis->m_parent, 
                        HeapAllocator_Alloc, 
                        HeapAllocator_Dealloc,
                        HeapAllocator_Patch);
}