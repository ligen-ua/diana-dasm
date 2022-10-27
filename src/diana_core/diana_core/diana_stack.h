#ifndef DIANA_STACK_H
#define DIANA_STACK_H

#include "diana_list.h"


// Chunk structure:
// [data][size:DIANA_SIZE_T]

typedef struct _Diana_StackBlock
{
    Diana_ListNode m_entry;// must be first
    int m_curSizeInBytes;
}Diana_StackBlock;

typedef struct _Diana_Stack
{
    Diana_List m_blockList;
    Diana_StackBlock * m_pCurrentBlock;
    int m_minBlockSize;
    int m_count;
    int m_dataSize;
}Diana_Stack;

int Diana_Stack_Init(Diana_Stack * pStack,
                     int minBlockSize,
                     int dataSize);

void Diana_Stack_Free(Diana_Stack * pStack);

int Diana_Stack_Push(Diana_Stack * pStack,
                     const void * pData);

int Diana_Stack_Pop(Diana_Stack * pStack,
                    void * pData
                    );

void Diana_Stack_Clear(Diana_Stack * pStack);

void * Diana_Stack_GetTopPtr(Diana_Stack * pStack);

#endif