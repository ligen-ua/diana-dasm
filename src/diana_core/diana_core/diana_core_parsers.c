#include "diana_core_parsers.h"
#include "diana_core_utils.h"
#include "diana_stream_proxy.h"
#include "diana_operands.h"

typedef struct _dianaParserCmdKey
{
    DI_CHAR chOpcode;
    DI_CHAR extension;
    DI_CHAR has_extension;
}DianaParserCmdKey;


// base lines + optional lines
#define DI_MAX_LINES_COUNT   DI_MAX_OPCODE_COUNT*2

typedef struct _dianaParserContextState
{
    DianaBaseGenObject_type * pLine;
    int position;
    int opcodePosition;
    DI_CHAR flags;
}DianaParserContextState;

#define DI_PARSER_CONTEXT_FLAG_THE_SAME_OPCODE  1 

typedef struct _dianaParserContext
{
    // INTERNAL:
    DianaParserContextState states[DI_MAX_LINES_COUNT];
    int currentState;
    // OUT
    DianaCmdInfo * pResult;

}DianaParserContext;

static void DianaParserContext_Init(DianaParserContext * pContext, 
                                    DianaBaseGenObject_type * pLine)
{
    pContext->states[0].pLine = pLine;
    pContext->states[0].position = -1;
    pContext->states[0].opcodePosition = 0;

    pContext->pResult = 0;

    pContext->currentState = 0;
}

static DianaParserContextState * DianaParserContext_GetCurrentState(DianaParserContext * pContext)
{
    return &pContext->states[pContext->currentState];
}
static DianaParserContextState * DianaParserContext_GetPrevState(DianaParserContext * pContext)
{
    if (!pContext->currentState)
        return 0;
    return &pContext->states[pContext->currentState-1];
}

static void DianaParserContext_PushOpcode(DianaParserContext * pContext,
                                          DianaBaseGenObject_type * pLine,
                                          int iPos,
                                          int opcodePosition,
                                          DI_CHAR flags)
{
    if (pContext->currentState >= DI_MAX_LINES_COUNT)
    {
        Diana_FatalBreak();
        return;
    }
    ++pContext->currentState;
    {
        DianaParserContextState * pState = &pContext->states[pContext->currentState];
        pState->pLine = pLine;
        pState->position = iPos;
        pState->opcodePosition = opcodePosition;
        pState->flags = flags;
    }

}
static void GetKeyByAddress(const unsigned char * pAddress, 
                            int iHaveAdditionalSize, 
                            DianaParserCmdKey * key)
{
    key->has_extension = 0;
    if (iHaveAdditionalSize)
    {
        key->extension = *(pAddress+1);
        key->has_extension = 1;
    }
    key->chOpcode = *pAddress;
}

static int TryMatchKey(DianaBaseGenObject_type * pFoundKey, DianaParseParams * pParseParams)
{
    DI_CHAR flagMask = 0;
    DianaCmdInfo * pInfo = (DianaCmdInfo * )pFoundKey;

    if (pInfo->m_flags & DI_FLAG_CMD_TEST_MODE_ONLY)
    {
        if (!pParseParams->pContext->testMode)
            return 0;
    }

    flagMask = pInfo->m_flags & (DI_FLAG_CMD_AMD64 | DI_FLAG_CMD_I386);
    if (!flagMask)
        return 1;

    if (flagMask & DI_FLAG_CMD_AMD64 &&
        pParseParams->pContext->iMainMode_addressSize == DIANA_MODE64)
    {
        return 1;
    }

    if (flagMask & DI_FLAG_CMD_I386 &&
        pParseParams->pContext->iMainMode_addressSize == DIANA_MODE32)
    {
        return 1;
    }
    return 0;
}


static int AreKeysGood(DianaBaseGenObject_type * pFoundKey, 
                       const DianaParserCmdKey  * pKey)
{
    DianaCmdInfo * pInfo = (DianaCmdInfo * )pFoundKey;
    
    if (pInfo->m_flags & DI_FLAG_CMD_POSTBYTE_USED)
    {
        // ensure that we have enough space
        if (!pKey->has_extension)
            return 0;
    }

    if (pInfo->m_extension_mask)
    {
        if (!pKey->has_extension)
            return 0;

        if ((pInfo->m_extension & pInfo->m_extension_mask) != (pInfo->m_extension_mask & pKey->extension))
            return 0;
    }

    if (pInfo->m_extension_deny_mask)
    {
        if (!pKey->has_extension)
            return 0;

        if ((pInfo->m_extension & pInfo->m_extension_deny_mask) == (pInfo->m_extension_deny_mask & pKey->extension))
            return 0;
    }
    return 1;
}

static
int DispatchKey(DianaParserContext * pContext,
                DianaParseParams * pParseParams,
                const DianaParserCmdKey  * pKey,
                DianaCmdKey * pCmdKey,
                int * pbRetry,
                int bIgnoreSubLines,
                int opcodePosition)
{
    DianaBaseGenObject_type * pObject = 0;
    *pbRetry = 0;

    if (pCmdKey->opcode != pKey->chOpcode)
    {
        Diana_FatalBreak();
        return 0;
    }

    pObject = (DianaBaseGenObject_type * )pCmdKey->keyLineOrCmdInfo;
    if (!pObject)
        return 0;

    switch(pObject->m_type)
    {
    case DIANA_BASE_GEN_OBJECT_CMD:
        // result found
        // check proc mode and compare
        if (!TryMatchKey(pObject, pParseParams))
            return 0;

        if (!AreKeysGood(pObject, pKey))
            return 0;
        
        pContext->pResult = (DianaCmdInfo*)pObject;
        break;

    case DIANA_BASE_GEN_OBJECT_LINE:
    case DIANA_BASE_GEN_OBJECT_INDEX:
        if (pObject->m_flags & DIANA_BASE_GEN_OBJECT_THE_SAME_OPCODE)
        {
            *pbRetry = 1;
        }
        else
        {
            ++opcodePosition;
            if (bIgnoreSubLines)
                return 0;
        }

        DianaParserContext_PushOpcode(pContext, 
                                      pObject,
                                      -1,
                                      opcodePosition,
                                      (pObject->m_flags & DIANA_BASE_GEN_OBJECT_THE_SAME_OPCODE) ?  DI_PARSER_CONTEXT_FLAG_THE_SAME_OPCODE : 0
                                      );
    }
    return 1;

}

static
int FindNextCmdKey(DianaParserContext * pContext, 
                   DianaParseParams * pParseParams,
                   DianaParserCmdKey  * pKey,
                   int bIgnoreSubLines,
                   int opcodePosition)
{
    int bRetry = 0;
    DianaParserContextState * pState = DianaParserContext_GetCurrentState(pContext);

    ++pState->position;
    pContext->pResult = 0;


    if (pState->pLine->m_type == DIANA_BASE_GEN_OBJECT_INDEX)
    {
        DianaIndexKeyLine * pLine = (DianaIndexKeyLine * )pState->pLine;
        DianaIndexKey * pIndexKey = &pLine->key[(DIANA_SIZE_T)pKey->chOpcode];
        DianaCmdKey cmdKey;

        if ((DIANA_SIZE_T)pState->position > (DIANA_SIZE_T)pKey->chOpcode)
        {
            return 0;
        }
        pState->position = pKey->chOpcode;
        if (!pIndexKey)
        {
            return 0;
        }
        
        cmdKey.keyLineOrCmdInfo =  pIndexKey->keyLineOrCmdInfo;
        cmdKey.opcode = pKey->chOpcode;

        if (!DispatchKey(pContext,
                        pParseParams,
                        pKey,
                        &cmdKey,
                        &bRetry,
                        bIgnoreSubLines,
                        opcodePosition
                        ))
        {
            return 0;
        }
        if (bRetry)
        {
            pState = DianaParserContext_GetCurrentState(pContext);
        }
        else
        {
            return 1;
        }
    }

    if (pState->pLine->m_type == DIANA_BASE_GEN_OBJECT_LINE)
    {
        DianaCmdKeyLine * pLine = (DianaCmdKeyLine * )pState->pLine;

        for(; pState->position < pLine->iKeysCount; ++pState->position)
        {
            DianaCmdKey * pCmdKey = &pLine->key[pState->position];

            if (pCmdKey->opcode != pKey->chOpcode)
                continue;

            if (DispatchKey(pContext,
                            pParseParams,
                            pKey,
                            pCmdKey,
                            &bRetry,
                            bIgnoreSubLines,
                            opcodePosition))
            {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}


static
int FindNextCmdKey_Rollback(DianaParserContext * pContext, 
                             int * pIt)
{
    DianaParserContextState * pState = DianaParserContext_GetCurrentState(pContext);
    DianaParserContextState * pPrevState = DianaParserContext_GetPrevState(pContext);
    if (!pPrevState) 
        return 0;

    &pState;
    --pContext->currentState;
    *pIt = pPrevState->opcodePosition;
    return 1;
}

static
int FindNextCmdKey_Finalize(DianaParserContext * pContext, 
                            DianaParseParams * pParseParams)
{
    DianaParserCmdKey prevKey;
    DianaParserContextState * pState = DianaParserContext_GetCurrentState(pContext);
    if (pState->position != -1 || !pContext->currentState)
    {
        return 0;
    }
    // rollback one state and try found smth
    --pContext->currentState;
    pState = DianaParserContext_GetCurrentState(pContext);

    GetKeyByAddress(pParseParams->pContext->cache + pState->opcodePosition,
                    pParseParams->pContext->cacheIt + pParseParams->pContext->cacheSize - pState->opcodePosition - 1, 
                    &prevKey);


    return FindNextCmdKey(pContext, 
                          pParseParams,
                          &prevKey,
                          1,
                          pState->opcodePosition);
}
static
int Diana_ParseCmdImpl(DianaParseParams * pParseParams, // IN
                       DianaStreamProxy * pOutProxy, // OUT
                       int * pOutIt)    // OUT
{
    DianaParserContext context;
    DianaParserCmdKey key;

    int it = pParseParams->pContext->cacheIt;
    int it_end = pParseParams->pContext->cacheIt + pParseParams->pContext->cacheSize;

    if (!pParseParams->pContext->cacheSize)
    {
        return DI_ERROR;
    }
    
    DianaParserContext_Init(&context, pParseParams->pInitialLine);
    for(;;)
    {
        GetKeyByAddress(pParseParams->pContext->cache + it, 
                        pParseParams->pContext->cacheIt + pParseParams->pContext->cacheSize - it - 1, 
                        &key);

        
        if (FindNextCmdKey(&context, pParseParams, &key, 0, it))
        {
            // smth found, check is it line or result
            if (context.pResult)
            {
                Diana_InitStreamProxy(pOutProxy, 
                                      pParseParams->readStream,
                                      pParseParams->pContext->cache+it, 
                                      pParseParams->pContext->cacheIt + pParseParams->pContext->cacheSize - it);
                *pOutIt = it;

                pParseParams->pResult->pInfo = context.pResult;
                return DI_SUCCESS;
            }
            if (it + 1 < it_end)
            {
                ++it;
                continue;
            }
            // no data in cache, rollback
        }
        
        // rollback
        if (!FindNextCmdKey_Rollback(&context, &it))
        {
            // rollback failed, exit
            break;
        }
    }
    return DI_ERROR;
}

static void ApplyPrefixes(DianaContext * pContext,
                          unsigned int iToExclude)
{
    DI_FULL_CHAR i = 0;
    for( ; i < pContext->prefixesCount; ++i)
    {
        if (i != iToExclude)
        {
            if (!pContext->prefixes[i].linkedPrefixFnc)
            {
                // ERROR: Diana_Init() is not called!
                Diana_FatalBreak();
            }
            pContext->prefixes[i].linkedPrefixFnc(pContext);
        }
    }
}

static
int TryMatch(DianaParseParams * pParseParams,
             int dataValid,
             DI_CHAR data,
             int iOriginalCacheSize,
             DI_FULL_CHAR iCurrentPrefix)
{
    DianaStreamProxy cmdProxy;
    int iResult = 0;
    int newIt = 0;
    int oldPrefixFound = 0;

    if (iCurrentPrefix == DI_FULL_CHAR_NULL)
    {
        if (dataValid)
        {
            pParseParams->pContext->cache[1] = data;
            pParseParams->pContext->cacheIt = 1;
            pParseParams->pContext->cacheSize = iOriginalCacheSize + 1;        
        }
        else
        {
            pParseParams->pContext->cacheIt = DI_CACHE_RESERVED;
            pParseParams->pContext->cacheSize = iOriginalCacheSize;
        }
    }
    else
    {
        if (dataValid)
        {
            pParseParams->pContext->cache[0] = pParseParams->pContext->prefixes[iCurrentPrefix].prefix;
            pParseParams->pContext->cache[1] = data;
            pParseParams->pContext->cacheIt = 0;
            pParseParams->pContext->cacheSize = iOriginalCacheSize + 2;
        }
        else
        {
            pParseParams->pContext->cache[1] = pParseParams->pContext->prefixes[iCurrentPrefix].prefix;
            pParseParams->pContext->cacheIt = 1;
            pParseParams->pContext->cacheSize = iOriginalCacheSize + 1;
        }
    }

    // analyze first byte 
    if (iCurrentPrefix == DI_FULL_CHAR_NULL)
    {
        // [REX][OLDPREFIX][CMD] case
        Di_ProcessCustomPrefix(pParseParams->pContext->cache[pParseParams->pContext->cacheIt],
                               &oldPrefixFound,
                               pParseParams->pContext,
                               0);
    }
                      

    
    newIt = 0;
    iResult = Diana_ParseCmdImpl(pParseParams,
                                 &cmdProxy,
                                 &newIt);
    if (!iResult)
    {
        // something found
        int oldTailSize = cmdProxy.tail_size;
        ApplyPrefixes(pParseParams->pContext, 
                      iCurrentPrefix); // exclude iCurrentPrefix

        if (pParseParams->pContext->iSizePrefixes & DIANA_INVALID_STATE)
        {
            return DI_ERROR;
        }

        if (oldPrefixFound)
        {
            pParseParams->pContext->iRexPrefix = 0;
            pParseParams->pContext->iRexPrefixValue = 0;
        }

        pParseParams->pContext->cacheIt = newIt;
        iResult = Diana_LinkOperands(pParseParams->pContext, 
                                     pParseParams->pResult, 
                                     &cmdProxy.parent);
        
        pParseParams->pContext->cacheSize = cmdProxy.tail_size;
        pParseParams->pContext->cacheIt += oldTailSize - cmdProxy.tail_size;

        if (pParseParams->pContext->cacheIt < DI_CACHE_RESERVED)
        {
            Diana_FatalBreak();
        }
        return iResult;
    }

    pParseParams->pContext->cacheIt = DI_CACHE_RESERVED;
    pParseParams->pContext->cacheSize = iOriginalCacheSize;
    return iResult;
}

int Diana_InsertPrefix(DianaContext * pContext, 
                       DI_CHAR prefix,
                       Diana_PrefixFnc linkedPrefixFnc,
                       int isRex
                       )
{
    DI_FULL_CHAR i = 0;

    if (pContext->prefixesCount >= DI_MAX_PREFIXES_COUNT) 
        return DI_ERROR;

    for(; i < pContext->prefixesCount; ++i)
    {
        if (pContext->prefixes[i].prefix == prefix)
            return DI_SUCCESS;
    }

    pContext->prefixes[pContext->prefixesCount].prefix = prefix;
    pContext->prefixes[pContext->prefixesCount].linkedPrefixFnc = linkedPrefixFnc;
    ++pContext->prefixesCount;

    if (!isRex)
        pContext->lastPrefixBeforeRex = i;

    return DI_SUCCESS;
}

int Diana_ParseCmdEx(DianaParseParams * pParseParams)    // OUT
{
    int iResult = 0;
    int prefixFound = 0;
    DI_CHAR data = 0;
    int dataValid = 1;
    int iBytesCounter = 0;
    int iOriginalCacheSize = 0;
    int nopFlags = 0;
    pParseParams->pContext->prefixesCount = 0;
    
    Diana_ResetPrefixes(pParseParams->pContext);
    DIANA_MEMSET(pParseParams->pResult, 0, sizeof(DianaParserResult));

    // check prefixes
    for(;;++iBytesCounter)
    {
        if( iBytesCounter > DI_MAX_PREFIXES_COUNT )
            return DI_ERROR;
        iResult = Diana_ReadCache(pParseParams->pContext, 
                                  pParseParams->readStream, 
                                  0);
        if (iResult)
            return iResult;

        data = Diana_CacheEatOne(pParseParams->pContext);

        if (!iBytesCounter || !nopFlags)
        {
            // check nop
            switch(data)
            {
            case 0x90:
                pParseParams->pResult->iFullCmdSize = 1;
                pParseParams->pResult->iLinkedOpCount = 0;
                pParseParams->pResult->pInfo = Diana_GetNopInfo();
                return DI_SUCCESS;
            };
        }

        // parse prefixes
        nopFlags |= Di_ProcessCustomPrefix(data, 
                                           &prefixFound,
                                           pParseParams->pContext, 
                                           pParseParams->pResult);
    
        if (!prefixFound)
            break;

        iResult = Diana_InsertPrefix(pParseParams->pContext, 
                                     data,
                                     pParseParams->pResult->pInfo->m_linkedPrefixFnc,
                                     0);
        if (iResult)
        {
            return iResult;
        }
    }

     // check rex prefix
    if (pParseParams->pContext->iAMD64Mode)
    {
        iResult = Di_ProcessRexPrefix(data, 
                                      &prefixFound, 
                                      pParseParams->pContext, 
                                      pParseParams->pResult);
        if (iResult)
            return iResult;

        if (prefixFound)
        {
            iResult = Diana_InsertPrefix(pParseParams->pContext,
                                         data,
                                         pParseParams->pResult->pInfo->m_linkedPrefixFnc,
                                         1);
            if (iResult)
            {
                return iResult;
            }
            dataValid = 0;
        }
    }

    iResult = Diana_ReadCache(pParseParams->pContext, 
                              pParseParams->readStream, 
                              1);
    if (iResult && !dataValid)
        return iResult;


    pParseParams->pResult->pInfo = 0;

    // parse cmd 
    iOriginalCacheSize = pParseParams->pContext->cacheSize;

    if (pParseParams->pContext->lastPrefixBeforeRex != DI_FULL_CHAR_NULL)
    {
        iResult = TryMatch(pParseParams,
                           dataValid,
                           data,
                           iOriginalCacheSize,
                           pParseParams->pContext->lastPrefixBeforeRex);

        if (!iResult)
            return iResult;
    }

    // prefixes not matched as commands, try to match command
    iResult = TryMatch(pParseParams,
                       dataValid,
                       data,
                       iOriginalCacheSize,
                       DI_FULL_CHAR_NULL);

    if (iResult && !dataValid)
    {
        if (pParseParams->pContext->cacheSize)
            Diana_CacheEatOne(pParseParams->pContext);
    }

    return iResult;
}
