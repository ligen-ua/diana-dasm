#include "diana_uids.h"
#include "diana_core.h"


int DIANA_UUID_Compare(const DIANA_UUID * p1, const DIANA_UUID * p2)
{
    if (p1->Data1 < p2->Data1)
        return -1;
    if (p1->Data1 > p2->Data1)
        return 1;

    if (p1->Data2 < p2->Data2)
        return -1;
    if (p1->Data2 > p2->Data2)
        return 1;

    if (p1->Data3 < p2->Data3)
        return -1;
    if (p1->Data3 > p2->Data3)
        return 1;

    return DIANA_MEMCMP(p1->Data4, p2->Data4, sizeof(p1->Data4));
}

void DianaBase_Init(DianaBase * pBase, 
                    const DIANA_UUID * pGuid)
{
    pBase->m_id = *pGuid;
}

int DianaBase_Match(DianaBase * pBase, 
                    const DIANA_UUID * pGuid)
{
    return 0 == DIANA_UUID_Compare(&pBase->m_id, pGuid);
}
