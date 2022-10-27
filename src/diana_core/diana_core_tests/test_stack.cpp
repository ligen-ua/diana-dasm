#include "test_stack.h"

extern "C"
{
#include "diana_stack.h"
}


static 
void test_stack1()
{
    int temp = 0, temp2 = 0;
    Diana_Stack stack;

    // init
    DIANA_TEST_ASSERT(!Diana_Stack_Init(&stack, sizeof(int)*2, sizeof(int)));

    for(int i=0; i<2; ++i)
    {
        DIANA_TEST_ASSERT(stack.m_count == 0);

        // push
        temp = 1;
        DIANA_TEST_ASSERT(!Diana_Stack_Push(&stack,
                                      &temp));
        DIANA_TEST_ASSERT(stack.m_count == 1);

        // push
        temp = 2;
        DIANA_TEST_ASSERT(!Diana_Stack_Push(&stack,
                                      &temp));
        DIANA_TEST_ASSERT(stack.m_count == 2);

        // push
        temp = 3;
        DIANA_TEST_ASSERT(!Diana_Stack_Push(&stack,
                                      &temp));
        DIANA_TEST_ASSERT(stack.m_count == 3);

        // pop
        DIANA_TEST_ASSERT(!Diana_Stack_Pop(&stack,
                                     &temp2));
        DIANA_TEST_ASSERT(temp2 == 3);
        DIANA_TEST_ASSERT(stack.m_count == 2);

        // pop
        DIANA_TEST_ASSERT(!Diana_Stack_Pop(&stack,
                                     &temp2));
        DIANA_TEST_ASSERT(temp2 == 2);
        DIANA_TEST_ASSERT(stack.m_count == 1);

        // pop
        DIANA_TEST_ASSERT(!Diana_Stack_Pop(&stack,
                        &temp2
                        ));
        DIANA_TEST_ASSERT(temp2 == 1);
        DIANA_TEST_ASSERT(stack.m_count == 0);

        // empty pop
        DIANA_TEST_ASSERT(Diana_Stack_Pop(&stack,
                                    &temp2));

    }
    Diana_Stack_Free(&stack);
}

static void test_list1()
{
    Diana_List list1;
    Diana_ListNode node1, node2, node3;
    
    Diana_InitList(&list1);
    DIANA_TEST_ASSERT(list1.m_pFirst == 0);
    DIANA_TEST_ASSERT(list1.m_pLast == 0);
    DIANA_TEST_ASSERT(list1.m_size == 0);

    Diana_PushBack(&list1, &node1);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pPrev == 0);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pNext == 0);
    DIANA_TEST_ASSERT(list1.m_pFirst == &node1);
    DIANA_TEST_ASSERT(list1.m_pLast == &node1);
    DIANA_TEST_ASSERT(list1.m_size == 1);

    Diana_PushBack(&list1, &node2);
    DIANA_TEST_ASSERT(list1.m_pLast != list1.m_pFirst);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pPrev == 0);
    DIANA_TEST_ASSERT(list1.m_pFirst == &node1);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pNext == &node2);
    DIANA_TEST_ASSERT(list1.m_pLast == &node2);
    DIANA_TEST_ASSERT(list1.m_pLast->m_pNext == 0);
    DIANA_TEST_ASSERT(list1.m_pLast->m_pPrev == list1.m_pFirst);
    DIANA_TEST_ASSERT(list1.m_size == 2);

    Diana_PushFront(&list1, &node3);
    DIANA_TEST_ASSERT(node3.m_pPrev == 0);
    DIANA_TEST_ASSERT(node3.m_pNext == &node1);
    DIANA_TEST_ASSERT(node1.m_pPrev == &node3);
    DIANA_TEST_ASSERT(node1.m_pNext == &node2);
    DIANA_TEST_ASSERT(node2.m_pPrev == &node1);
    DIANA_TEST_ASSERT(node2.m_pNext == 0);
    DIANA_TEST_ASSERT(list1.m_pFirst == &node3);
    DIANA_TEST_ASSERT(list1.m_pLast == &node2);
    DIANA_TEST_ASSERT(list1.m_size == 3);

    // -- 
    Diana_EraseNode(&list1, &node3);
    DIANA_TEST_ASSERT(list1.m_pLast != list1.m_pFirst);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pPrev == 0);
    DIANA_TEST_ASSERT(list1.m_pFirst == &node1);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pNext == &node2);
    DIANA_TEST_ASSERT(list1.m_pLast == &node2);
    DIANA_TEST_ASSERT(list1.m_pLast->m_pNext == 0);
    DIANA_TEST_ASSERT(list1.m_pLast->m_pPrev == list1.m_pFirst);
    DIANA_TEST_ASSERT(list1.m_size == 2);
    // 
    Diana_EraseNode(&list1, &node2);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pPrev == 0);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pNext == 0);
    DIANA_TEST_ASSERT(list1.m_pFirst == &node1);
    DIANA_TEST_ASSERT(list1.m_pLast == &node1);
    DIANA_TEST_ASSERT(list1.m_size == 1);
    //
    Diana_EraseNode(&list1, &node1);
    DIANA_TEST_ASSERT(list1.m_pFirst == 0);
    DIANA_TEST_ASSERT(list1.m_pLast == 0);
    DIANA_TEST_ASSERT(list1.m_size == 0);
}

// --- 
static int Diana_FList_ListObserver(Diana_FList_ListNode * pNode,
                                    void * pContext,
                                    int * pbDone)
{
    ++*(int * )pContext;
    return 0;
}
static int CalcSize(Diana_FList_List & list1)
{
    int size = 0;
    int done = 0;
    DIANA_TEST_ASSERT(Diana_FList_ListForEach(&list1, Diana_FList_ListObserver, &size) == 0);
    return size;
}
static void test_flist1()
{
    Diana_FList_List list1;
    Diana_FList_ListNode node1, node2;
    
    Diana_FList_InitList(&list1);
    DIANA_TEST_ASSERT(list1.m_pFirst == 0);
    DIANA_TEST_ASSERT(list1.m_pLast == 0);
    DIANA_TEST_ASSERT(CalcSize(list1) == 0);

    Diana_FList_PushBack(&list1, &node1);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pNext == 0);
    DIANA_TEST_ASSERT(list1.m_pFirst == &node1);
    DIANA_TEST_ASSERT(list1.m_pLast == &node1);
    DIANA_TEST_ASSERT(CalcSize(list1) == 1);

    Diana_FList_PushBack(&list1, &node2);
    DIANA_TEST_ASSERT(list1.m_pLast != list1.m_pFirst);
    DIANA_TEST_ASSERT(list1.m_pFirst == &node1);
    DIANA_TEST_ASSERT(list1.m_pFirst->m_pNext == &node2);
    DIANA_TEST_ASSERT(list1.m_pLast == &node2);
    DIANA_TEST_ASSERT(CalcSize(list1) == 2);
}

void test_stack()
{
    DIANA_TEST(test_list1());
    DIANA_TEST(test_flist1());
    DIANA_TEST(test_stack1());
}