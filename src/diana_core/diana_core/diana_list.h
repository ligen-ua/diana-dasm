#ifndef DIANA_LIST_H
#define DIANA_LIST_H

#include "diana_core.h"

/*
   Diana_FList_List - intrusive, forward-only, append-only list.

   LAYOUT
   ------
   Circular singly linked list that keeps ONLY the tail pointer:

        m_pLast ---------------------------+
                                           v
        [first] -> [second] -> ... -> [last] --+
           ^                                   |
           +-----------------------------------+

     - empty list:      m_pLast == 0  (so an all-zero / memset'ed head IS a
                        valid empty list; Diana_Instruction_Init and
                        Diana_InstructionsOwner_Init rely on that)
     - first node:      m_pLast->m_pNext
     - single node:     m_pLast->m_pNext == m_pLast

   WHY
   ---
   Three FList members (one node + two heads) are embedded in every
   Diana_Instruction, and the analyzer allocates one Diana_Instruction per
   byte of the analyzed range (Diana_InstructionsOwner::m_pInstructionsVec).
   The classic {m_pFirst, m_pLast} head costs two pointers; this layout costs
   one, which saves 2 * sizeof(void*) per instruction
   (16 bytes on x64, 8 bytes on x86) with O(1) PushBack preserved.

   WHY NOT AN XOR LINKED LIST
   --------------------------
   An XOR list (node stores prev ^ next) was considered and rejected:
     - it saves nothing here: it still needs one link per node plus a
       head AND a tail pointer, i.e. more than this layout; its only gain is
       backward traversal, which no caller needs;
     - pointer -> uintptr_t -> pointer arithmetic is not portable to
       capability architectures (CHERI / Arm Morello) where it invalidates
       the pointer; it works on x86/x64/ARM32/ARM64 (also with MTE/HWASan
       tags, since XOR restores them exactly), but that is a fragile
       guarantee to build on;
     - debuggers / natvis cannot follow the links, and leak checkers
       (LeakSanitizer, Valgrind) cannot see nodes reachable only through
       XOR'ed values, so heap-allocated nodes look leaked.
   Hence no XOR variant and no per-platform #define switch.

   INVARIANTS / RULES
   ------------------
     - in a non-empty list m_pNext is NEVER 0 (the tail points back to the
       head). Do NOT walk the list by hand with "while (p) p = p->m_pNext" -
       it never terminates. Use Diana_FList_First / Diana_FList_Next or
       Diana_FList_ListForEach.
     - a node may be in at most one FList at a time (PushBack overwrites
       m_pNext).
     - there is no removal; re-initialize the whole list instead.
     - Diana_FList_ListForEach allows the observer to free / reuse the
       current node: the next pointer and the "is last" flag are captured
       before the observer is called.
*/
typedef struct _Diana_FList_ListNode
{
    struct _Diana_FList_ListNode * m_pNext;
}Diana_FList_ListNode;

typedef struct _Diana_FList_List
{
    Diana_FList_ListNode * m_pLast;  // 0 if empty, m_pLast->m_pNext is the first node
}Diana_FList_List;


typedef int (*Diana_FList_ListObserver_type)(Diana_FList_ListNode * pNode,
                                             void * pContext,
                                             int * pbDone);

int Diana_FList_ListForEach(Diana_FList_List * pList,
                            Diana_FList_ListObserver_type pObserver,
                            void * pContext);


void Diana_FList_InitList(Diana_FList_List * pList);

void Diana_FList_PushBack(Diana_FList_List * pList,
                          Diana_FList_ListNode * pNode);

// returns 0 if the list is empty
Diana_FList_ListNode * Diana_FList_First(const Diana_FList_List * pList);

// returns 0 if pNode is the last node
Diana_FList_ListNode * Diana_FList_Next(const Diana_FList_List * pList,
                                        const Diana_FList_ListNode * pNode);

//---------------
typedef struct _Diana_ListNode
{
    struct _Diana_ListNode * m_pNext;
    struct _Diana_ListNode * m_pPrev;
}Diana_ListNode;

typedef struct _Diana_List
{
    Diana_ListNode * m_pFirst;
    Diana_ListNode * m_pLast;
    OPERAND_SIZE m_size;
}Diana_List;


typedef int (*DianaListObserver_type)(Diana_ListNode * pNode,
                                      void * pContext,
                                      int * pbDone);

int Diana_ListForEach(Diana_List * pList,
                      DianaListObserver_type pObserver,
                      void * pContext);


void Diana_InitList(Diana_List * pList);

void Diana_PushBack(Diana_List * pList, 
                    Diana_ListNode * pNode);

void Diana_PushFront(Diana_List * pList,
                     Diana_ListNode * pNode);

void Diana_InsertAfter(Diana_List * pList,
                       Diana_ListNode * pNode,
                       Diana_ListNode * pNodeToInsert);

void Diana_InsertBefore(Diana_List * pList,
                        Diana_ListNode * pNode,
                        Diana_ListNode * pNodeToInsert);

void Diana_EraseNode(Diana_List * pList,
                     Diana_ListNode * pNode);

#endif