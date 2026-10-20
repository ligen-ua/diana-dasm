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
                                    DianaBaseGenObject_type * pLine,
                                    int opcodePosition)
{
    pContext->states[0].pLine = pLine;
    pContext->states[0].position = -1;
    // a rollback to the root starts again at the first byte, never before it: the cache before cacheIt holds
    // bytes of the previous instruction (a VEX selector, a mandatory prefix)
    pContext->states[0].opcodePosition = opcodePosition;

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

    // not in 64-bit mode: 16-bit mode has them too
    if (flagMask & DI_FLAG_CMD_I386 &&
        pParseParams->pContext->iMainMode_addressSize != DIANA_MODE64)
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

        // the virtual VEX entries (c5 + selector) only after a VEX prefix, the others only without: a raw c5 is
        // lds (32-bit c5 09 54 cb matched vandpd); the EVEX entries (62 + selector) only after EVEX
        if (!(((DianaCmdInfo*)pObject)->m_flags & DI_FLAG_CMD_VEX_ENCODED) != !pParseParams->pContext->iVexPrefix)
            return 0;
        if (!(pObject->m_flags & DIANA_BASE_GEN_OBJECT_EVEX) != !pParseParams->pContext->iEvexPrefix)
            return 0;
        // VEX.W0 with W = 1: #UD, or the W1 entry of the opcode
        if ((pObject->m_flags & DIANA_BASE_GEN_OBJECT_VEX_W0) && (pParseParams->pContext->iVexPrefix & DI_VEX_PREFIX_W))
            return 0;
        if ((pObject->m_flags & DIANA_BASE_GEN_OBJECT_VEX_W1) && !(pParseParams->pContext->iVexPrefix & DI_VEX_PREFIX_W))
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
    
    DianaParserContext_Init(&context, pParseParams->pInitialLine, it);
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
        // the state the prefixes change: a failed match is undone for the next try (another mandatory prefix)
        DianaContext * pContext = pParseParams->pContext;
        int savedOpSize = pContext->iCurrentCmd_opsize, savedAddressSize = pContext->iCurrentCmd_addressSize;
        int savedPrefix = pContext->iPrefix, savedSizePrefixes = pContext->iSizePrefixes;
        DianaUnifiedRegister savedSreg = pContext->currentCmd_sreg;
        DI_CHAR savedRex = pContext->iRexPrefix;
        ApplyPrefixes(pParseParams->pContext, 
                      iCurrentPrefix); // exclude iCurrentPrefix

        if (pParseParams->pContext->iSizePrefixes & DIANA_INVALID_STATE)
        {
            iResult = DI_ERROR;
        }
        else
        {

        if (oldPrefixFound)
        {
            pParseParams->pContext->iRexPrefix = 0;
            pParseParams->pContext->iRexPrefixValue = 0;
        }

        pParseParams->pContext->cacheIt = newIt;
        iResult = Diana_LinkOperands(pParseParams->pContext, 
                                     pParseParams->pResult, 
                                     &cmdProxy.parent);
        if (!iResult)
        {
            pParseParams->pContext->cacheSize = cmdProxy.tail_size;
            pParseParams->pContext->cacheIt += oldTailSize - cmdProxy.tail_size;

            if (pParseParams->pContext->cacheIt < DI_CACHE_RESERVED)
            {
                Diana_FatalBreak();
            }
            return iResult;
        }
        }
        // undo for the next try
        pContext->iCurrentCmd_opsize = savedOpSize;
        pContext->iCurrentCmd_addressSize = savedAddressSize;
        pContext->iPrefix = savedPrefix;
        pContext->iSizePrefixes = savedSizePrefixes;
        pContext->currentCmd_sreg = savedSreg;
        pContext->iRexPrefix = savedRex;
        pParseParams->pResult->iLinkedOpCount = 0;
        pParseParams->pResult->pInfo = 0;
        pContext->cacheIt = DI_CACHE_RESERVED;
        pContext->cacheSize = iOriginalCacheSize;
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
        {
            // repeated: the last one counts (64 36 64: fs), it moves to the end
            for (; i + 1 < pContext->prefixesCount; ++i)
                pContext->prefixes[i] = pContext->prefixes[i + 1];
            --pContext->prefixesCount;
            if (pContext->lastPrefixBeforeRex != DI_FULL_CHAR_NULL && pContext->lastPrefixBeforeRex >= pContext->prefixesCount)
                pContext->lastPrefixBeforeRex = pContext->prefixesCount ? pContext->prefixesCount - 1 : DI_FULL_CHAR_NULL;
            break;
        }
    }
    i = pContext->prefixesCount;

    pContext->prefixes[pContext->prefixesCount].prefix = prefix;
    pContext->prefixes[pContext->prefixesCount].linkedPrefixFnc = linkedPrefixFnc;
    ++pContext->prefixesCount;

    if (!isRex)
        pContext->lastPrefixBeforeRex = i;

    return DI_SUCCESS;
}

static int IsVEXPrefix(DI_CHAR data)
{
    return (data == 0xC4 || data == 0xC5);
}

// the rules of every EVEX entry (the spec doesn't write them per line), after the operands are linked
static int Di_CheckEvex(DianaContext * pContext, DianaParserResult * pResult)
{
    const DianaEvexCmdAttributes * pAttributes = Diana_GetEvexAttributes(pResult->pInfo);
    DI_UINT32 flags = pAttributes ? pAttributes->m_flags : 0;
    int prefix = pContext->iEvexPrefix;
    int i = 0, memory = -1, vsib = 0;
    if (!pAttributes)
        return DI_ERROR;
    for (i = 0; i < pResult->iLinkedOpCount; ++i)
    {
        DianaLinkedOperand * pOp = pResult->linkedOperands + i;
        if (pOp->type == diana_index || pOp->type == diana_memory)
            memory = i;
        vsib |= pOp->pInfo->m_type == diana_orVSIB;
    }
    // the opmask
    if (pContext->iEvexAAA && !(flags & DI_EVEX_MASK))
        return DI_ERROR;
    if (!pContext->iEvexAAA && (flags & DI_EVEX_MASK_REQUIRED))
        return DI_ERROR;
    if (prefix & DI_EVEX_PREFIX_Z)
    {
        // zeroing: the entry allows it, not with a memory destination, not without a mask (the CPU: #UD)
        if (!(flags & DI_EVEX_ZEROING) || memory == 0 || !pContext->iEvexAAA)
            return DI_ERROR;
    }
    // a scalar (LIG): L'L 11 is #UD, except as the rounding of a register form
    if ((flags & DI_EVEX_VL_MASK) == DI_EVEX_VL_LIG && pContext->iEvexLL == 3 &&
        !((prefix & DI_EVEX_PREFIX_B) && memory < 0))
        return DI_ERROR;
    // no vvvv operand: vvvv and V' are 1111, 1 (V' is the VSIB index bit 4)
    if (!DI_VEX_GET_NDS(pResult->pInfo->m_flags) && !vsib && (prefix & DI_EVEX_PREFIX_V1))
        return DI_ERROR;
    if ((flags & DI_EVEX_DISTINCT) && pResult->iLinkedOpCount)
    {
        int dest = Diana_VectorRegisterNumber(pResult->linkedOperands[0].value.recognizedRegister, 0);
        for (i = 1; i < pResult->iLinkedOpCount; ++i)
        {
            DianaLinkedOperand * pOp = pResult->linkedOperands + i;
            if (pOp->type == diana_register && Diana_VectorRegisterNumber(pOp->value.recognizedRegister, 0) == dest)
                return DI_ERROR;
        }
    }
    pResult->evex.isEvex = 1;
    pResult->evex.mask = pContext->iEvexAAA;
    pResult->evex.zeroing = (prefix & DI_EVEX_PREFIX_Z) ? 1 : 0;
    switch (flags & DI_EVEX_VL_MASK)
    {
    case DI_EVEX_VL_128: pResult->evex.vectorSize = 16; break;
    case DI_EVEX_VL_256: pResult->evex.vectorSize = 32; break;
    case DI_EVEX_VL_512: pResult->evex.vectorSize = 64; break;
    default: pResult->evex.vectorSize = 16; break;
    }
    if (prefix & DI_EVEX_PREFIX_B)
    {
        if (memory >= 0)
        {
            // the broadcast: set by the operand linking (Diana_LinkOperands)
            if (!pResult->evex.broadcast)
                return DI_ERROR;
        }
        else
        if (flags & DI_EVEX_ER)
            pResult->evex.rounding = (DI_UINT8)(DIANA_EVEX_ROUND_RN + pContext->iEvexLL);
        else
        if (flags & DI_EVEX_SAE)
            pResult->evex.rounding = DIANA_EVEX_ROUND_SAE;
        else
        if (!(flags & DI_EVEX_B_IGNORED))
            return DI_ERROR;
    }
    return DI_SUCCESS;
}

// an EVEX instruction: the virtual prefix 62 <selector> is in the cache (Di_ProcessEvexPrefix)
static int Di_ParseEvexCmd(DianaParseParams * pParseParams)
{
    DianaContext * pContext = pParseParams->pContext;
    DianaStreamProxy cmdProxy;
    int newIt = 0, oldTailSize = 0, iResult = 0;
    DI_CHAR rex = pContext->iRexPrefix;
    int opSize = DI_REX_HAS_FLAG_W(rex) ? 8 : 4;
    DI_FULL_CHAR i = 0;
    // 66, F2, F3, LOCK before EVEX: #UD
    for (; i < pContext->prefixesCount; ++i)
    {
        DI_CHAR prefix = pContext->prefixes[i].prefix;
        if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3 || prefix == 0xF0)
            return DI_ERROR;
    }
    iResult = Diana_ParseCmdImpl(pParseParams, &cmdProxy, &newIt);
    if (iResult) return iResult;
    if (!(pParseParams->pResult->pInfo->m_parent.m_flags & DIANA_BASE_GEN_OBJECT_EVEX))
        return DI_ERROR;
    oldTailSize = cmdProxy.tail_size;
    ApplyPrefixes(pContext, DI_FULL_CHAR_NULL);
    if (pContext->iSizePrefixes & DIANA_INVALID_STATE) return DI_ERROR;
    // the segment and 67 prefixes clear the REX state: R X B W of EVEX stay
    pContext->iRexPrefix = rex;
    pContext->iCurrentCmd_opsize = opSize;
    pContext->cacheIt = newIt;
    iResult = Diana_LinkOperands(pContext, pParseParams->pResult, &cmdProxy.parent);
    // no vvvv operand: vvvv must be 1111 (0 here, it is stored inverted)
    if (!iResult && !DI_VEX_GET_NDS(pParseParams->pResult->pInfo->m_flags) && pContext->iVexVVVV)
        return DI_ERROR;
    if (!iResult)
        iResult = Di_CheckEvex(pContext, pParseParams->pResult);
    pContext->cacheSize = cmdProxy.tail_size;
    pContext->cacheIt += oldTailSize - cmdProxy.tail_size;
    if (!iResult && pContext->cacheIt < DI_CACHE_RESERVED) Diana_FatalBreak();
    return iResult;
}

int Diana_ParseCmdEx(DianaParseParams* pParseParams)    // OUT
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
    pParseParams->pContext->iVexPrefix = 0;
    pParseParams->pContext->iVexVVVV   = 0;
    pParseParams->pContext->iEvexPrefix = 0;
    pParseParams->pContext->iEvexAAA = 0;
    pParseParams->pContext->iEvexLL = 0;
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
        {
            // a REX before a legacy prefix has no effect (the CPU ignores it): drop it, read on (40 2e c5 f8 77 is
            // vzeroupper)
            DianaContext * pContext = pParseParams->pContext;
            if (pContext->iAMD64Mode && data >= DI_REX_PREFIX_START && data <= DI_REX_PREFIX_END)
            {
                int nextIsPrefix = 0;
                if (!Diana_ReadCache(pContext, pParseParams->readStream, 0) && pContext->cacheSize)
                    Di_ProcessCustomPrefix(pContext->cache[pContext->cacheIt], &nextIsPrefix, pContext, 0);
                if (nextIsPrefix)
                    continue;
            }
            break;
        }

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

    // REX right before VEX (c4, c5 are VEX in 64-bit mode) or EVEX (62): #UD
    if (!dataValid && pParseParams->pContext->cacheSize &&
        (IsVEXPrefix(pParseParams->pContext->cache[pParseParams->pContext->cacheIt]) ||
         pParseParams->pContext->cache[pParseParams->pContext->cacheIt] == 0x62))
        return DI_ERROR;

    // 90 is nop after any prefix: F3 90 is pause (a table entry), REX.B 90 is xchg r8, rax
    {
        DianaContext * pContext = pParseParams->pContext;
        int op = dataValid ? data : (pContext->cacheSize ? pContext->cache[pContext->cacheIt] : -1);
        DI_FULL_CHAR i = 0;
        int hasF3 = 0;
        for (; i < pContext->prefixesCount; ++i)
            hasF3 |= pContext->prefixes[i].prefix == 0xF3;
        if (op == 0x90 && !hasF3 && !(pContext->iRexPrefixValue & 1))
        {
            if (!dataValid)
                Diana_CacheEatOne(pContext);
            pParseParams->pResult->iLinkedOpCount = 0;
            pParseParams->pResult->pInfo = Diana_GetNopInfo();
            return DI_SUCCESS;
        }
    }


    pParseParams->pResult->pInfo = 0;

    // parse cmd
    iOriginalCacheSize = pParseParams->pContext->cacheSize;

    // EVEX (62; outside 64-bit mode only when the next byte has bits 7..6 set, else bound)
    if (dataValid && data == 0x62)
    {
        int evexFound = 0;
        iResult = Di_ProcessEvexPrefix(&evexFound, pParseParams->pContext, pParseParams->readStream);
        if (iResult) return iResult;
        iOriginalCacheSize = pParseParams->pContext->cacheSize;
        if (evexFound)
            return Di_ParseEvexCmd(pParseParams);
    }

    // VEX prefix interception (C4/C5 not handled by legacy prefix loop; outside 64-bit mode only when the next
    // byte has bits 7..6 set, else les/lds)
    if (dataValid && IsVEXPrefix(data))
    {
        int vexFound = 0;
        iResult = Di_ProcessVexPrefix(data, &vexFound, pParseParams->pContext, pParseParams->readStream);
        if (iResult) return iResult;
        // les/lds: the bytes Di_ProcessVexPrefix read are in the cache now
        iOriginalCacheSize = pParseParams->pContext->cacheSize;
        if (vexFound)
        {
            DianaStreamProxy cmdProxy;
            int newIt = 0;
            int oldTailSize = 0;
            DI_CHAR vexRex = pParseParams->pContext->iRexPrefix;
            // the general registers of VEX instructions are 32-bit, 64-bit with W: no 16-bit form (66 is #UD)
            int vexOpSize = DI_REX_HAS_FLAG_W(vexRex) ? 8 : 4;
            DI_FULL_CHAR i = 0;
            // 66, F2, F3, LOCK before VEX: #UD
            for (; i < pParseParams->pContext->prefixesCount; ++i)
            {
                DI_CHAR prefix = pParseParams->pContext->prefixes[i].prefix;
                if (prefix == 0x66 || prefix == 0xF2 || prefix == 0xF3 || prefix == 0xF0)
                    return DI_ERROR;
            }
            iResult = Diana_ParseCmdImpl(pParseParams, &cmdProxy, &newIt);
            if (iResult) return iResult;
            if (!(pParseParams->pResult->pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED))
            {
                // Non-VEX instruction (e.g., legacy LDS) matched the virtual VEX cache - reject it
                return DI_ERROR;
            }
            oldTailSize = cmdProxy.tail_size;
            ApplyPrefixes(pParseParams->pContext, DI_FULL_CHAR_NULL);
            if (pParseParams->pContext->iSizePrefixes & DIANA_INVALID_STATE) return DI_ERROR;
            // the segment and 67 prefixes clear the REX state: R X B W of VEX stay
            pParseParams->pContext->iRexPrefix = vexRex;
            pParseParams->pContext->iCurrentCmd_opsize = vexOpSize;
            pParseParams->pContext->cacheIt = newIt;
            iResult = Diana_LinkOperands(pParseParams->pContext, pParseParams->pResult, &cmdProxy.parent);
            // no vvvv operand: vvvv must be 1111 (0 here, it is stored inverted), else #UD
            if (!iResult && !DI_VEX_GET_NDS(pParseParams->pResult->pInfo->m_flags) && pParseParams->pContext->iVexVVVV)
                return DI_ERROR;
            pParseParams->pContext->cacheSize = cmdProxy.tail_size;
            pParseParams->pContext->cacheIt += oldTailSize - cmdProxy.tail_size;
            if (!iResult && pParseParams->pContext->cacheIt < DI_CACHE_RESERVED) Diana_FatalBreak();
            return iResult;
        }
    }

    // the mandatory prefix: F2 or F3 (the last of them) before 66, whatever the order and the segment prefixes
    // between (66 f3 0f 7f is movdqu, 66 64 0f 11 is movupd); then the last prefix; then none
    {
        DI_FULL_CHAR i = 0, repPrefix = DI_FULL_CHAR_NULL, sizePrefix = DI_FULL_CHAR_NULL;
        DI_FULL_CHAR lastPrefix = pParseParams->pContext->lastPrefixBeforeRex;
        for (; i < pParseParams->pContext->prefixesCount; ++i)
        {
            DI_CHAR prefix = pParseParams->pContext->prefixes[i].prefix;
            if (prefix == 0xF2 || prefix == 0xF3)
                repPrefix = i;
            if (prefix == 0x66)
                sizePrefix = i;
        }
        if (repPrefix != DI_FULL_CHAR_NULL)
        {
            iResult = TryMatch(pParseParams, dataValid, data, iOriginalCacheSize, repPrefix);
            if (!iResult)
                return iResult;
        }
        if (sizePrefix != DI_FULL_CHAR_NULL)
        {
            iResult = TryMatch(pParseParams, dataValid, data, iOriginalCacheSize, sizePrefix);
            if (!iResult)
                return iResult;
        }
        if (lastPrefix != DI_FULL_CHAR_NULL && lastPrefix != repPrefix && lastPrefix != sizePrefix)
        {
            iResult = TryMatch(pParseParams, dataValid, data, iOriginalCacheSize, lastPrefix);
            if (!iResult)
                return iResult;
        }
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
        {
            Diana_CacheEatOne(pParseParams->pContext);
        }
    }
    return iResult;
}
