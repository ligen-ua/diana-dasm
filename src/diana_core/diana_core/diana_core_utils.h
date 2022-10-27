#ifndef DIANA_CORE_UTILS_H
#define DIANA_CORE_UTILS_H

#include "diana_core.h"

typedef struct _dianaParseParams
{
    DianaContext * pContext;
    DianaBaseGenObject_type * pInitialLine;
    DianaReadStream * readStream;
    DianaParserResult * pResult;
}
DianaParseParams;

int Di_ProcessRexPrefix(unsigned char value,
                        int * pbPrefixFound, 
                        DianaContext * pContext,
                        DianaParserResult * pResult);

int Di_ProcessCustomPrefix(unsigned char value,
                           int * pbPrefixFound, 
                           DianaContext * pContext,
                           DianaParserResult * pResult);

void Diana_InitUtils();

DI_CHAR Diana_CacheEatOne(DianaContext * pContext);

int Diana_ReadCache(DianaContext * pContext,
                    DianaReadStream * readStream,
                    int bOptimize);
#endif