#include "diana_list.h"

void Diana_FList_InitList(Diana_FList_List * pList)
{
    DIANA_MEMSET(pList, 0, sizeof(*pList));
}

int Diana_FList_ListForEach(Diana_FList_List * pList,
                            Diana_FList_ListObserver_type pObserver,
                            void * pContext)
{
    Diana_FList_ListNode *  pNode = pList->m_pFirst;
    int bDone = 0;
    while(pNode)
    {
        Diana_FList_ListNode *  pNext = pNode->m_pNext;
        DI_CHECK(pObserver(pNode, pContext, &bDone));
        if (bDone)
            break;
        pNode = pNext;
    }
    return DI_SUCCESS;
}

void Diana_FList_PushBack(Diana_FList_List * pList, 
                          Diana_FList_ListNode * pNode)
{
    pNode->m_pNext = 0;
    if (pList->m_pLast)
    {
        pList->m_pLast->m_pNext = pNode;
        pList->m_pLast = pNode;
    }
    else
    {
        pList->m_pLast = pList->m_pFirst = pNode;
    }
}

//--------------
void Diana_InitList(Diana_List * pList)
{
    DIANA_MEMSET(pList, 0, sizeof(*pList));
}

int Diana_ListForEach(Diana_List * pList,
                      DianaListObserver_type pObserver,
                      void * pContext)
{
    Diana_ListNode *  pNode = pList->m_pFirst;
    int bDone = 0;
    while(pNode)
    {
        Diana_ListNode *  pNext = pNode->m_pNext;
        DI_CHECK(pObserver(pNode, pContext, &bDone));
        if (bDone)
            break;
        pNode = pNext;
    }
    return DI_SUCCESS;
}

void Diana_PushBack(Diana_List * pList, 
                    Diana_ListNode * pNode)
{
    pNode->m_pNext = 0;
    pNode->m_pPrev = 0;
    if (pList->m_pLast)
    {
        pList->m_pLast->m_pNext = pNode;
        pNode->m_pPrev = pList->m_pLast;
        pList->m_pLast = pNode;
    }
    else
    {
        pList->m_pLast = pList->m_pFirst = pNode;
    }
    ++pList->m_size;

}
void Diana_PushFront(Diana_List * pList,
                     Diana_ListNode * pNode)
{
    pNode->m_pNext = 0;
    pNode->m_pPrev = 0;
    if (pList->m_pFirst)
    {
        pList->m_pFirst->m_pPrev = pNode;
        pNode->m_pNext = pList->m_pFirst;
        pList->m_pFirst = pNode;
    }
    else
    {
        pList->m_pLast = pList->m_pFirst = pNode;
    }
    ++pList->m_size;
}

void Diana_InsertAfter(Diana_List * pList,
                       Diana_ListNode * pNode,
                       Diana_ListNode * pNodeToInsert)
{
    Diana_ListNode * pNextNode = pNode->m_pNext;
    if (pList->m_pLast == pNode)
    {
        Diana_PushBack(pList, pNodeToInsert);
        return;
    }
    pNode->m_pNext = pNodeToInsert;
    pNextNode->m_pPrev = pNodeToInsert;

    pNodeToInsert->m_pNext = pNextNode;
    pNodeToInsert->m_pPrev = pNode;
}

void Diana_InsertBefore(Diana_List * pList,
                        Diana_ListNode * pNode,
                        Diana_ListNode * pNodeToInsert)
{
    Diana_ListNode * pPrevNode = pNode->m_pPrev;
    if (pList->m_pFirst == pNode)
    {
        Diana_PushFront(pList, pNodeToInsert);
        return;
    }
    pNode->m_pPrev = pNodeToInsert;
    pPrevNode->m_pNext = pNodeToInsert;

    pNodeToInsert->m_pNext = pNode;
    pNodeToInsert->m_pPrev = pPrevNode;
}

void Diana_EraseNode(Diana_List * pList,
                     Diana_ListNode * pNode)
{
    Diana_ListNode * pNextNode = pNode->m_pNext;
    if (pNextNode) 
        pNextNode->m_pPrev = pNode->m_pPrev;
    else
        pList->m_pLast = pNode->m_pPrev;
    
    if (pNode->m_pPrev)
        pNode->m_pPrev->m_pNext = pNextNode;
    else
        pList->m_pFirst = pNode->m_pNext;
    
    pNode->m_pNext = pNode->m_pPrev = 0;
    --pList->m_size;
}
