#include "diana_win32_executable_heap.h"


#ifdef DIANA_HAS_WIN32

#include "windows.h"


static HANDLE g_executableHeap = 0;


int DianaWin32ExecutableHeap_Init()
{
    // create heap
    if (g_executableHeap)
        return DI_SUCCESS;

    g_executableHeap = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 
                               0,
                               0);
    if (!g_executableHeap)
        return DI_WIN32_ERROR;
    return DI_SUCCESS;
}

static void * HeapAllocator_Alloc(void * pThis, DIANA_SIZE_T size)
{
    &pThis;
    if (!g_executableHeap)
        return 0;
    return HeapAlloc(g_executableHeap, 
                     0,
                     size);
}
static void HeapAllocator_Dealloc(void * pThis, void * pBuffer)
{
    &pThis;
    if (!g_executableHeap)
        return;
    HeapFree(g_executableHeap,
                 0,
                 pBuffer);
}
static int HeapAllocator_Patch(void * pThis, void * pDest, const void * pSource, DIANA_SIZE_T size)
{
    &pThis;
    DIANA_MEMCPY(pDest, pSource, size);
    return DI_SUCCESS;
}

void * DianaWin32ExecutableHeap_Alloc(size_t size)
{
    return HeapAlloc(g_executableHeap, 
                     0,
                     size);
}
void DianaWin32ExecutableHeap_Free(void * pBuffer)
{
  HeapFree(g_executableHeap,
                 0,
                 pBuffer);
}

void DianaWin32ExecutableHeapAllocator_Init(DianaWin32ExecutableHeapAllocator * pThis)
{
    Diana_AllocatorInit(&pThis->m_parent, 
                        HeapAllocator_Alloc, 
                        HeapAllocator_Dealloc,
                        HeapAllocator_Patch);
}


#endif