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

int Di_ProcessVexPrefix(unsigned char firstByte,
                        int * pbVexFound,
                        DianaContext * pContext,
                        DianaReadStream * readStream);

// EVEX (62): the virtual prefix 62 <selector> (W, L'L, map, pp) in the cache, the state in DianaContext
// (iEvexPrefix, iEvexAAA, iEvexLL, iVexPrefix, iVexVVVV, iRexPrefix); outside 64-bit mode 62 is bound when the next
// byte has mod != 11 (*pbEvexFound 0, the bytes read stay in the cache)
int Di_ProcessEvexPrefix(int * pbEvexFound,
                         DianaContext * pContext,
                         DianaReadStream * readStream);
#endif