#pragma once

#include "windows.h"

namespace dd
{

struct ListNode
{
    ListNode * pNext;
    void * pData;
    DWORD size;
    int id;
};

struct List
{
    ListNode   m_processingNode;
    ListNode   m_fakeNode;
    ListNode * m_pFirst;
    ListNode * m_pLast;
    long       m_fakeNodeAdded;
};


}