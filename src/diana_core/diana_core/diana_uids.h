#ifndef DIANA_UIDS_H
#define DIANA_UIDS_H

typedef struct _DIANA_UUID 
{
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} 
DIANA_UUID;

int DIANA_UUID_Compare(const DIANA_UUID * p1, const DIANA_UUID * p2);

typedef struct _DianaBase
{
    DIANA_UUID m_id;
}DianaBase;


void DianaBase_Init(DianaBase * pBase, 
                    const DIANA_UUID * pGuid);

int DianaBase_Match(DianaBase * pBase, 
                    const DIANA_UUID * pGuid);

#endif