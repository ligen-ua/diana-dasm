#include "diana_stack.h"

int Diana_Stack_Init(Diana_Stack * pStack,
                     int minBlockSize,
                     int dataSize)
{
    DIANA_MEMSET(pStack, 0, sizeof(*pStack));
    Diana_InitList(&pStack->m_blockList);
    pStack->m_minBlockSize = minBlockSize;
    pStack->m_dataSize = dataSize;
    return DI_SUCCESS;
}

static int deleter(Diana_ListNode * pNode,
                   void * pContext,
                   int * pbDone)
{
    Diana_StackBlock * pBlock = (Diana_StackBlock*)pNode;
    DIANA_FREE(pBlock);

    pContext;
    pbDone;

    return DI_SUCCESS;
}

void Diana_Stack_Free(Diana_Stack * pStack)
{
    Diana_ListForEach(&pStack->m_blockList, deleter, 0);
}

int Diana_Stack_Push(Diana_Stack * pStack,
                     const void * pData)
{
    char * pDataPlace = 0;
    if (pStack->m_pCurrentBlock)
    {
        for(;;)
        {
            if (pStack->m_minBlockSize - 
                pStack->m_pCurrentBlock->m_curSizeInBytes >= pStack->m_dataSize)
            {
                pDataPlace = (char*)pStack->m_pCurrentBlock + sizeof(Diana_StackBlock) + pStack->m_pCurrentBlock->m_curSizeInBytes;
                DIANA_MEMCPY(pDataPlace, pData, pStack->m_dataSize);
                
                pStack->m_pCurrentBlock->m_curSizeInBytes += pStack->m_dataSize;
                ++pStack->m_count;
                return DI_SUCCESS;
            }
            if (pStack->m_pCurrentBlock->m_entry.m_pNext)
            {
                pStack->m_pCurrentBlock = (Diana_StackBlock*)pStack->m_pCurrentBlock->m_entry.m_pNext;
                continue;
            }     
            break;
        }        
    }
    // need insert
    {
        Diana_StackBlock * pBlock = DIANA_MALLOC(sizeof(Diana_StackBlock)+pStack->m_minBlockSize);
        if (!pBlock)
            return DI_OUT_OF_MEMORY;

        DIANA_MEMSET(pBlock, 0, sizeof(*pBlock));

        pDataPlace = (char*)pBlock + sizeof(Diana_StackBlock);
        DIANA_MEMCPY(pDataPlace, pData, pStack->m_dataSize);
        pBlock->m_curSizeInBytes = pStack->m_dataSize;

        Diana_PushBack(&pStack->m_blockList, &pBlock->m_entry);
        pStack->m_pCurrentBlock = pBlock;
        ++pStack->m_count;
    }
    return DI_SUCCESS;
}
int Diana_Stack_Pop(Diana_Stack * pStack,
                    void * pData)
{
    char * pDataPlace = Diana_Stack_GetTopPtr(pStack);
    if (!pDataPlace)
        return DI_ERROR;

    DIANA_MEMCPY(pData, pDataPlace, pStack->m_dataSize);
    pStack->m_pCurrentBlock->m_curSizeInBytes -= pStack->m_dataSize;
    --pStack->m_count;
    return DI_SUCCESS;

}
void Diana_Stack_Clear(Diana_Stack * pStack)
{
    for(;;)
    {
        char * pDataPlace = Diana_Stack_GetTopPtr(pStack);
        if (!pDataPlace)
            return;

        pStack->m_pCurrentBlock->m_curSizeInBytes -= pStack->m_dataSize;
        --pStack->m_count;
    }
}
void * Diana_Stack_GetTopPtr(Diana_Stack * pStack)
{
    char * pDataPlace = 0;
    if (!pStack->m_pCurrentBlock)
        return 0;
    if (!pStack->m_count)
        return 0;

    if (pStack->m_pCurrentBlock->m_curSizeInBytes < pStack->m_dataSize)
    {
        pStack->m_pCurrentBlock = (Diana_StackBlock*)pStack->m_pCurrentBlock->m_entry.m_pPrev;
    }
    if (!pStack->m_pCurrentBlock)
    {
        return 0;
    }

    pDataPlace = (char*)pStack->m_pCurrentBlock + sizeof(Diana_StackBlock) + pStack->m_pCurrentBlock->m_curSizeInBytes - pStack->m_dataSize;
    return pDataPlace;
}
