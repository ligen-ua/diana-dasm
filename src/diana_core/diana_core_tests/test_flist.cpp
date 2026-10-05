#include "test_flist.h"

extern "C"
{
#include "diana_list.h"
}
#include <vector>

// Diana_FList_List is a circular singly linked list keeping only the tail,
// see the comment in diana_list.h

namespace
{

struct TestNode
{
    Diana_FList_ListNode m_entry; // must be first
    int m_value;
};

struct CollectContext
{
    std::vector<int> values;
    int stopAt;         // set *pbDone after this many visits, 0 - never
    int failAt;         // return an error on this visit, 0 - never
};

int CollectObserver(Diana_FList_ListNode * pNode,
                    void * pContext,
                    int * pbDone)
{
    CollectContext * pCtx = (CollectContext *)pContext;
    pCtx->values.push_back(((TestNode *)pNode)->m_value);
    int visited = (int)pCtx->values.size();
    if (pCtx->failAt && visited == pCtx->failAt)
        return DI_ERROR;
    if (pCtx->stopAt && visited == pCtx->stopAt)
        *pbDone = 1;
    return DI_SUCCESS;
}

int FreeObserver(Diana_FList_ListNode * pNode,
                 void * pContext,
                 int * pbDone)
{
    ++*(int *)pContext;
    // poison before free so a use-after-free in ForEach shows up even without ASan
    pNode->m_pNext = 0;
    delete (TestNode *)pNode;
    *pbDone = 0;
    return DI_SUCCESS;
}

std::vector<int> CollectForEach(Diana_FList_List * pList)
{
    CollectContext ctx;
    ctx.stopAt = 0;
    ctx.failAt = 0;
    DIANA_TEST_ASSERT(Diana_FList_ListForEach(pList, CollectObserver, &ctx) == DI_SUCCESS);
    return ctx.values;
}

std::vector<int> CollectIterate(const Diana_FList_List * pList)
{
    std::vector<int> values;
    for(Diana_FList_ListNode * pNode = Diana_FList_First(pList);
        pNode;
        pNode = Diana_FList_Next(pList, pNode))
    {
        values.push_back(((TestNode *)pNode)->m_value);
        // guard against an endless walk
        DIANA_TEST_ASSERT(values.size() <= 100000);
    }
    return values;
}

std::vector<int> Range(int count)
{
    std::vector<int> values;
    for(int i = 0; i < count; ++i)
        values.push_back(i);
    return values;
}

}

static void test_flist_layout()
{
    // the whole point of the layout: one pointer per head
    DIANA_TEST_ASSERT(sizeof(Diana_FList_List) == sizeof(void*));
    DIANA_TEST_ASSERT(sizeof(Diana_FList_ListNode) == sizeof(void*));
}

static void test_flist_empty()
{
    // memset'ed head must be a valid empty list
    Diana_FList_List list;
    DIANA_MEMSET(&list, 0, sizeof(list));
    DIANA_TEST_ASSERT(Diana_FList_First(&list) == 0);
    DIANA_TEST_ASSERT(CollectForEach(&list).empty());
    DIANA_TEST_ASSERT(CollectIterate(&list).empty());

    // the same via InitList on garbage
    DIANA_MEMSET(&list, 0xCC, sizeof(list));
    Diana_FList_InitList(&list);
    DIANA_TEST_ASSERT(Diana_FList_First(&list) == 0);
    DIANA_TEST_ASSERT(CollectForEach(&list).empty());
}

static void test_flist_push_order(int count)
{
    std::vector<TestNode> nodes(count);
    Diana_FList_List list;
    Diana_FList_InitList(&list);

    for(int i = 0; i < count; ++i)
    {
        nodes[i].m_value = i;
        // garbage in m_pNext must not matter
        nodes[i].m_entry.m_pNext = (Diana_FList_ListNode *)&nodes;
        Diana_FList_PushBack(&list, &nodes[i].m_entry);

        // invariants after every push
        DIANA_TEST_ASSERT(list.m_pLast == &nodes[i].m_entry);
        DIANA_TEST_ASSERT(Diana_FList_First(&list) == &nodes[0].m_entry);
        DIANA_TEST_ASSERT(list.m_pLast->m_pNext == &nodes[0].m_entry);
        DIANA_TEST_ASSERT(Diana_FList_Next(&list, list.m_pLast) == 0);
    }

    DIANA_TEST_ASSERT(CollectForEach(&list) == Range(count));
    DIANA_TEST_ASSERT(CollectIterate(&list) == Range(count));
}

static void test_flist_sizes()
{
    test_flist_push_order(1);
    test_flist_push_order(2);
    test_flist_push_order(3);
    test_flist_push_order(1000);
}

static void test_flist_early_stop()
{
    const int count = 5;
    std::vector<TestNode> nodes(count);
    Diana_FList_List list;
    Diana_FList_InitList(&list);
    for(int i = 0; i < count; ++i)
    {
        nodes[i].m_value = i;
        Diana_FList_PushBack(&list, &nodes[i].m_entry);
    }

    for(int k = 1; k <= count; ++k)
    {
        CollectContext ctx;
        ctx.stopAt = k;
        ctx.failAt = 0;
        DIANA_TEST_ASSERT(Diana_FList_ListForEach(&list, CollectObserver, &ctx) == DI_SUCCESS);
        DIANA_TEST_ASSERT(ctx.values == Range(k));
    }
}

static void test_flist_error()
{
    const int count = 4;
    std::vector<TestNode> nodes(count);
    Diana_FList_List list;
    Diana_FList_InitList(&list);
    for(int i = 0; i < count; ++i)
    {
        nodes[i].m_value = i;
        Diana_FList_PushBack(&list, &nodes[i].m_entry);
    }

    CollectContext ctx;
    ctx.stopAt = 0;
    ctx.failAt = 2;
    DIANA_TEST_ASSERT(Diana_FList_ListForEach(&list, CollectObserver, &ctx) == DI_ERROR);
    DIANA_TEST_ASSERT(ctx.values == Range(2));
}

static void test_flist_free_during_walk()
{
    // the same pattern as Diana_InstructionsOwner_ExternalInstructionsDeleter
    for(int count = 1; count <= 4; ++count)
    {
        Diana_FList_List list;
        Diana_FList_InitList(&list);
        for(int i = 0; i < count; ++i)
        {
            TestNode * pNode = new TestNode;
            pNode->m_value = i;
            Diana_FList_PushBack(&list, &pNode->m_entry);
        }

        int freed = 0;
        DIANA_TEST_ASSERT(Diana_FList_ListForEach(&list, FreeObserver, &freed) == DI_SUCCESS);
        DIANA_TEST_ASSERT(freed == count);

        Diana_FList_InitList(&list);
        DIANA_TEST_ASSERT(Diana_FList_First(&list) == 0);
    }
}

static void test_flist_reinit()
{
    TestNode nodes[3];
    Diana_FList_List list;
    Diana_FList_InitList(&list);
    for(int i = 0; i < 3; ++i)
    {
        nodes[i].m_value = i;
        Diana_FList_PushBack(&list, &nodes[i].m_entry);
    }
    Diana_FList_InitList(&list);
    DIANA_TEST_ASSERT(Diana_FList_First(&list) == 0);
    DIANA_TEST_ASSERT(CollectForEach(&list).empty());

    // nodes can be reused in a fresh list
    for(int i = 2; i >= 0; --i)
        Diana_FList_PushBack(&list, &nodes[i].m_entry);
    std::vector<int> expected;
    expected.push_back(2);
    expected.push_back(1);
    expected.push_back(0);
    DIANA_TEST_ASSERT(CollectIterate(&list) == expected);
}

void test_flist()
{
    DIANA_TEST(test_flist_layout());
    DIANA_TEST(test_flist_empty());
    DIANA_TEST(test_flist_sizes());
    DIANA_TEST(test_flist_early_stop());
    DIANA_TEST(test_flist_error());
    DIANA_TEST(test_flist_free_during_walk());
    DIANA_TEST(test_flist_reinit());
}
