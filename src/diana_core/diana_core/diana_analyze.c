#include "diana_analyze.h"
#include "diana_stack.h"

typedef struct _DianaAnalyzeSession
{
    Diana_InstructionsOwner * pOwner;
    DianaAnalyzeObserver * pObserver;
    int mode;

    Diana_Stack * pStack;
    DianaContext context;
    DianaParserResult result;
    OPERAND_SIZE maxOffset;
    Diana_RouteInfo curRouteInfo;
    int flags;
}DianaAnalyzeSession;

void DianaAnalyzeObserver_Init(DianaAnalyzeObserver * pThis,
                               DianaMovableReadStream * pStream,
                               DianaAnalyzeAddress_type pAnalyzeJumpAddress)
{
    pThis->m_pStream = pStream;
    pThis->m_pAnalyzeAddress = pAnalyzeJumpAddress;
}

void Diana_Instruction_Init(Diana_Instruction * pInstruction,
                            OPERAND_SIZE offset,
                            int flags)
{
    DIANA_MEMSET(pInstruction, 0, sizeof(*pInstruction));
    
    pInstruction->m_offset = offset;
    pInstruction->m_flags = flags;
    Diana_FList_InitList(&pInstruction->m_referencesFromThisInstruction);
    Diana_FList_InitList(&pInstruction->m_referencesToThisInstruction);
}
int Diana_InstructionsOwner_Init(Diana_InstructionsOwner * pOwner,
                                 OPERAND_SIZE maxOffsetSize,
                                 OPERAND_SIZE minimalExternalAddress)
{
    int res = 0;
    DIANA_MEMSET(pOwner, 0, sizeof(*pOwner));
    pOwner->m_minimalExternalAddress = minimalExternalAddress;

    pOwner->m_pInstructionsVec = DIANA_MALLOC(sizeof(Diana_Instruction) * (DIANA_SIZE_T)maxOffsetSize);
    if (!pOwner->m_pInstructionsVec)
        return DI_OUT_OF_MEMORY;

    pOwner->m_ppPresenceVec = (Diana_Instruction ** )DIANA_MALLOC(sizeof(Diana_Instruction*) * (DIANA_SIZE_T)maxOffsetSize);
    if (!pOwner->m_ppPresenceVec)
    {
        Diana_InstructionsOwner_Free(pOwner);
        return DI_OUT_OF_MEMORY;
    }
    DIANA_MEMSET(pOwner->m_ppPresenceVec, 0, (DIANA_SIZE_T)maxOffsetSize * sizeof(Diana_Instruction*));
    
    res = Diana_Stack_Init(&pOwner->m_xrefs, 1024, sizeof(Diana_XRef));
    if (res != DI_SUCCESS)
    {
        Diana_InstructionsOwner_Free(pOwner);
        return res;
    }
    pOwner->m_stackInited = 1;
    pOwner->m_maxSize = maxOffsetSize;
    pOwner->m_usedSize = 0; 
    return DI_SUCCESS;
}
static
int Diana_InstructionsOwner_ExternalInstructionsDeleter(Diana_FList_ListNode * pNode,
                                                        void * pContext,
                                                        int * pbDone)
{
    Diana_Instruction * pInstruction = (Diana_Instruction * )pNode;
    DIANA_FREE(pInstruction);
    &pContext;
    *pbDone = 0;
    return DI_SUCCESS;
}
void Diana_InstructionsOwner_Free(Diana_InstructionsOwner * pOwner)
{
    Diana_FList_ListForEach(&pOwner->m_externalInstructionsList, Diana_InstructionsOwner_ExternalInstructionsDeleter, 0);
    Diana_FList_InitList(&pOwner->m_externalInstructionsList);
    if (pOwner->m_pInstructionsVec)
    {
        DIANA_FREE(pOwner->m_pInstructionsVec);
    }
    if (pOwner->m_ppPresenceVec)
    {
        DIANA_FREE(pOwner->m_ppPresenceVec);
    }
    if (pOwner->m_stackInited)
    {
        Diana_Stack_Free(&pOwner->m_xrefs);
    }
}
Diana_Instruction * Diana_InstructionsOwner_AllocateInstruction(Diana_InstructionsOwner * pOwner)
{
    Diana_Instruction * pEntry = pOwner->m_pInstructionsVec + pOwner->m_usedSize;
    if (pOwner->m_usedSize >= pOwner->m_maxSize)
        return 0;

    ++pOwner->m_usedSize;
    ++pOwner->m_actualSize;
    return pEntry;
}

int Diana_InstructionsOwner_QueryOrAllocateInstruction(Diana_InstructionsOwner * pOwner,
                                                       OPERAND_SIZE address,
                                                       Diana_Instruction ** ppInstruction,
                                                       int flags)
{
    *ppInstruction = pOwner->m_ppPresenceVec[address];
    if (!*ppInstruction)
    {
        *ppInstruction = Diana_InstructionsOwner_AllocateInstruction(pOwner);
        if (!*ppInstruction)
            return DI_OUT_OF_MEMORY;

        Diana_Instruction_Init(*ppInstruction, 
                               address,
                               flags
                               );
        pOwner->m_ppPresenceVec[address] = *ppInstruction;
    }
    return DI_SUCCESS;
}

Diana_Instruction * Diana_InstructionsOwner_GetInstruction(Diana_InstructionsOwner * pOwner, 
                                                           OPERAND_SIZE address)
{
    return pOwner->m_ppPresenceVec[address];
}

static int Diana_CreateXRef(Diana_InstructionsOwner * pOwner,
                            Diana_Instruction * pFromInstruction,
                            Diana_Instruction * pToInstruction,
                            Diana_XRef ** ppXref)
{
    Diana_XRef * pXref = 0;
    Diana_XRef xref;
    DIANA_MEMSET(&xref, 0, sizeof(xref));
    xref.m_subrefs[0].m_pInstruction = pFromInstruction;
    xref.m_subrefs[1].m_pInstruction = pToInstruction;
    DI_CHECK(Diana_Stack_Push(&pOwner->m_xrefs, &xref));

    pXref = (Diana_XRef *)Diana_Stack_GetTopPtr(&pOwner->m_xrefs);
    Diana_FList_PushBack(&pToInstruction->m_referencesToThisInstruction, &(pXref)->m_subrefs[0].m_instructionEntry);
    Diana_FList_PushBack(&pFromInstruction->m_referencesFromThisInstruction, &(pXref)->m_subrefs[1].m_instructionEntry);
    if (ppXref)
    {
        *ppXref = pXref;
    }
    return DI_SUCCESS;
}

static
int AnalyzeJumps(DianaAnalyzeSession * pSession,
                 Diana_LinkedAdditionalGroupInfo * pLinkedInfo,
                 OPERAND_SIZE * pNewOffset,
                 OPERAND_SIZE offset,
                 int * pAbsoluteAddress,
                 int * pLinksToData)
{
    DianaParserResult * pResult = &pSession->result;
    const DianaLinkedOperand * pOp = pResult->linkedOperands + pLinkedInfo->relArgrumentNumber;
    *pNewOffset = 0;
    *pAbsoluteAddress = 0;
    *pLinksToData = 0;

    switch(pOp->type)
    {
    // usual call smth
    case diana_rel:
        *pNewOffset = offset + pOp->value.rel.m_value;
        break;
    // crazy call for really crazy guys
    case diana_call_ptr:
        *pAbsoluteAddress = 1;
        *pNewOffset = pOp->value.ptr.m_address;
        break;
    // call [rbx+0x5435345] 
    case diana_index:
        *pAbsoluteAddress = 1;
        *pNewOffset = pOp->value.rmIndex.dispValue;
        if (pOp->value.rmIndex.reg == reg_RIP)
        {
            *pNewOffset += offset; 
            *pAbsoluteAddress = 0;
        }
        *pLinksToData = 1;
        break;
    case diana_register:
        return DI_UNSUPPORTED_COMMAND;
    }
    return DI_SUCCESS;
}

static
int SaveNewExternalRoute(DianaAnalyzeSession * pSession,
                         Diana_Instruction * pInstruction,
                         OPERAND_SIZE newOffset,
                         int bLinksToData)
{
    int flags = 0;
    Diana_XRef * pXref = 0;
    Diana_Instruction * pTargetInstruction = 0;
    if (pSession->pOwner->m_minimalExternalAddress && newOffset < pSession->pOwner->m_minimalExternalAddress)
    {
        // skip first region
        return DI_SUCCESS;
    }
    pTargetInstruction = DIANA_MALLOC(sizeof(Diana_Instruction));
    DI_CHECK_ALLOC(pTargetInstruction);
    flags = DI_INSTRUCTION_EXTERNAL;
    if (bLinksToData)
    {
        flags |= DI_INSTRUCTION_DATA;
    }
    Diana_Instruction_Init(pTargetInstruction, newOffset, flags);
    Diana_FList_PushBack(&pSession->pOwner->m_externalInstructionsList, &pTargetInstruction->m_routeEntry);
    DI_CHECK(Diana_CreateXRef(pSession->pOwner, pInstruction, pTargetInstruction, &pXref));
    pXref->m_flags |= DI_XREF_EXTERNAL;
    return DI_SUCCESS;
}

static
int SaveNewRoute(DianaAnalyzeSession * pSession,
                 Diana_Instruction * pInstruction,
                 OPERAND_SIZE newOffset,
                 int newRouteFlags)
{
    Diana_Instruction * pTargetInstruction = 0;
    Diana_Instruction * pOldTargetInstruction = 0;
    Diana_RouteInfo newRouteInfo;

    if (newOffset >= pSession->maxOffset)
        return DI_SUCCESS;

    DIANA_MEMSET(&newRouteInfo, 0, sizeof(newRouteInfo));
    newRouteInfo.startOffset = newOffset;
    newRouteInfo.flags = newRouteFlags;

    // create new instruction
    pOldTargetInstruction = Diana_InstructionsOwner_GetInstruction(pSession->pOwner, newOffset);

    DI_CHECK(Diana_InstructionsOwner_QueryOrAllocateInstruction(pSession->pOwner, 
                                                                newOffset, 
                                                                &pTargetInstruction,
                                                                DI_INSTRUCTION_IS_LOADING)); 

    if (!(newRouteFlags & DI_ROUTE_QUESTIONABLE))
    {
        pTargetInstruction->m_flags |= DI_INSTRUCTION_ROOT;
    }

    // register xref-from Instruction to Target
    DI_CHECK(Diana_CreateXRef(pSession->pOwner, pInstruction, pTargetInstruction, 0));

    // do not push new route to stack twice
    if (pOldTargetInstruction)
        return DI_SUCCESS;

    // push to stack
    DI_CHECK(Diana_Stack_Push(pSession->pStack, &newRouteInfo));

    return DI_SUCCESS;
}

static
void DispatchMode32(DianaAnalyzeSession * pSession,
                    Diana_Instruction * pInstruction,
                    const DianaLinkedOperand * pOp,
                    int * pbFound,
                    OPERAND_SIZE * pSuspectedOp)
{
    &pInstruction;
    &pSession;
    *pbFound = 0;
    switch(pOp->type)
    {
    case diana_imm:
        *pbFound = 1;
        *pSuspectedOp = pOp->value.imm;
        break;
    case diana_index:
        if (pOp->value.rmIndex.dispSize == 4 ||
            pOp->value.rmIndex.dispSize ==  8)
        {
            *pbFound = 1;
            *pSuspectedOp = pOp->value.rmIndex.dispValue;
        }
        break;
    default:
        ;
    } 
}

static
int DispatchMode64(DianaAnalyzeSession * pSession,
                   Diana_Instruction * pInstruction,
                   const DianaLinkedOperand * pOp,
                   OPERAND_SIZE nextInstructionOffset,
                   int * pbFound,
                   OPERAND_SIZE * pSuspectedOp)
{
    &pSession;
    *pbFound = 0;
    switch(pOp->type)
    {
    case diana_register:
        if (pOp->value.recognizedRegister == reg_RIP || 
            pOp->value.recognizedRegister == reg_IP)
        {
            pInstruction->m_flags |= DI_INSTRUCTION_USES_RIP;
        }
        break;
    case diana_imm:
        *pbFound = 1;
        *pSuspectedOp = pOp->value.imm;
        break;
    case diana_index:
        if (pOp->value.rmIndex.reg == reg_RIP ||
            pOp->value.rmIndex.indexed_reg == reg_RIP)
        {
            // ooops 
            if ((pOp->value.rmIndex.indexed_reg == reg_none ||
                pOp->value.rmIndex.index == 0) ||
                (pOp->value.rmIndex.reg == reg_none &&
                 pOp->value.rmIndex.index == 1))
            {
                // good case, no index
                DI_CHECK(SaveNewRoute(pSession, 
                                      pInstruction,
                                      nextInstructionOffset + pOp->value.rmIndex.dispValue,
                                      DI_ROUTE_QUESTIONABLE)); 

                pInstruction->m_flags |= DI_INSTRUCTION_USES_RIP;
                break;
            }
            
            // bad case
            pInstruction->m_flags |= DI_INSTRUCTION_USES_UNKNOWN_RIP;
            break;
        }

        // common check
        if (pOp->value.rmIndex.dispSize == 4 ||
            pOp->value.rmIndex.dispSize ==  8)
        {
            *pbFound = 1;
            *pSuspectedOp = pOp->value.rmIndex.dispValue;
        }
        break;
    default:
        ;
    } 
    return DI_SUCCESS;
}        

static
int AnalyzeAndConstructNormalInstruction(DianaAnalyzeSession * pSession,
                                         Diana_Instruction * pInstruction,
                                         OPERAND_SIZE nextInstructionOffset)
{
    const DianaLinkedOperand * pOp = pSession->result.linkedOperands;
    int i = 0;

    for(i = 0; i < pSession->result.iLinkedOpCount; ++i, ++pOp)
    {
        OPERAND_SIZE suspectedOp = 0;
        int bFound = 0;

        if (pSession->mode == DIANA_MODE32)
        {
            DispatchMode32(pSession,
                           pInstruction,
                           pOp,
                           &bFound,
                           &suspectedOp);
        }
        else
        {
            DI_CHECK(DispatchMode64(pSession,
                                    pInstruction,
                                    pOp,
                                    nextInstructionOffset,
                                    &bFound,
                                    &suspectedOp));
        }

        if (bFound)
        {
            DianaAnalyzeAddressResult_type result = diaJumpInvalid;
            DI_CHECK(pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver, 
                                                        suspectedOp,
                                                        DIANA_ANALYZE_ABSOLUTE_ADDRESS,
                                                        &suspectedOp,
                                                        &result
                                                        ));
            if (result == diaJumpExternal)
            {                        
                DI_CHECK(SaveNewExternalRoute(pSession,
                                              pInstruction,
                                              suspectedOp,
                                              0));
            }
            else
            if (result == diaJumpNormal)
            {
                // this is not realy valid command
                DI_CHECK(SaveNewRoute(pSession, 
                                    pInstruction,
                                    suspectedOp,
                                    DI_ROUTE_QUESTIONABLE));
            }
        }
    }
    return DI_SUCCESS;
}

static
int AnalyzeAndConstructInstruction(DianaAnalyzeSession * pSession,
                                   OPERAND_SIZE instructionOffset,
                                   OPERAND_SIZE nextInstructionOffset,
                                   int * pbNeedReset)
{
    int bLinksToData = 0;
    int bAbsoluteAddress = 0;
    int iRes = 0;
    Diana_Instruction * pInstruction = 0;
    Diana_LinkedAdditionalGroupInfo * pLinkedInfo = 0;
    int bIsNormalInstruction = 0;
    OPERAND_SIZE newOffset = 0;
    DianaAnalyzeAddressResult_type jumpFlags = diaJumpNormal;

    *pbNeedReset = 0;

    DI_CHECK(Diana_InstructionsOwner_QueryOrAllocateInstruction(pSession->pOwner, 
                                                                instructionOffset, 
                                                                &pInstruction,
                                                                0)); // flags for new instruction

    // clear DI_INSTRUCTION_IS_LOADING flag
    Diana_FList_PushBack(&pSession->curRouteInfo.instructions, 
                    &pInstruction->m_routeEntry);

    pInstruction->m_flags = 0;

    if (!pSession->curRouteInfo.flags & DI_ROUTE_QUESTIONABLE)
    {
        pInstruction->m_flags |= DI_INSTRUCTION_ROOT;
    }

    // analyze command
    pLinkedInfo = pSession->result.pInfo->m_pGroupInfo->m_pLinkedInfo;
    if (!pLinkedInfo)
    {
        bIsNormalInstruction = 1;
    }
    else
    if (!(pLinkedInfo->flags & DIANA_GT_CAN_CHANGE_RIP))
    {
        bIsNormalInstruction = 1;
    }

    // analyze normal command
    if (bIsNormalInstruction)
    {
        return AnalyzeAndConstructNormalInstruction(pSession, 
                                                    pInstruction,
                                                    nextInstructionOffset);
    }

    // special instruction found: jmps, jcc's, call, ret, leave
    pInstruction->m_flags |= DI_INSTRUCTION_CAN_CHANGE_CSIP;

    // handle ret case
    if (pLinkedInfo->flags & DIANA_GT_RET)
    {
        pInstruction->m_flags |= DI_INSTRUCTION_IS_RET;
        *pbNeedReset = 1;
        return DI_SUCCESS;
    }

    // handle jumps and others
    if (!(pLinkedInfo->flags & DIANA_GT_CAN_GO_TO_THE_NEXT_INSTRUCTION))
    {
        *pbNeedReset = 1;
    }

    newOffset = 0;
    iRes = AnalyzeJumps(pSession, 
                        pLinkedInfo, 
                        &newOffset,
                        nextInstructionOffset,
                        &bAbsoluteAddress,
                        &bLinksToData);
    
    if (iRes == DI_UNSUPPORTED_COMMAND)
        return DI_SUCCESS;

    DI_CHECK(iRes);
    

    // check new jump address
    DI_CHECK(pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver, 
                                                        newOffset,
                                                        bAbsoluteAddress?DIANA_ANALYZE_ABSOLUTE_ADDRESS:0,
                                                        &newOffset,
                                                        &jumpFlags
                                                        ));
    
    if (bLinksToData && jumpFlags != diaJumpInvalid)
    {
        // need to get the value
        int readFlags =0;
        OPERAND_SIZE address = 0;
        OPERAND_SIZE readBytes = 0;
        int status = 0;
        
        if (jumpFlags == diaJumpExternal)
        {
            readFlags |= DIANA_ANALYZE_RANDOM_READ_ABSOLUTE;
        }
        status = pSession->pObserver->m_pStream->pRandomRead(pSession->pObserver->m_pStream, 
                                                                 newOffset,
                                                                 (char*)&address,
                                                                 pSession->mode,
                                                                 &readBytes,
                                                                 readFlags);
        if (status == DI_SUCCESS && readBytes == pSession->mode)
        {
            bLinksToData = 0;
            newOffset = address;

            DI_CHECK(pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver, 
                                                            newOffset,
                                                            DIANA_ANALYZE_ABSOLUTE_ADDRESS,
                                                            &newOffset,
                                                            &jumpFlags
                                                            ));
        }
    }
    switch(jumpFlags)
    {
    case diaJumpNormal:
        // save new route
        DI_CHECK(SaveNewRoute(pSession,
                              pInstruction,
                              newOffset,
                              pSession->curRouteInfo.flags));
        break;
    case diaJumpExternal:
        DI_CHECK(SaveNewExternalRoute(pSession,
                                      pInstruction,
                                      newOffset,
                                      bLinksToData));
        break;
    default:
        Diana_FatalBreak();
    case diaJumpInvalid: ;
    }

    return DI_SUCCESS;
}

static
int SwitchToLastState(DianaAnalyzeSession * pSession,
                      Diana_RouteInfo * pCurRouteInfo,
                      OPERAND_SIZE * pOffset)
{
    DI_CHECK(Diana_Stack_Pop(pSession->pStack, pCurRouteInfo));
    *pOffset = pCurRouteInfo->startOffset;
    DI_CHECK(pSession->pObserver->m_pStream->pMoveTo(pSession->pObserver->m_pStream, pCurRouteInfo->startOffset));
    Diana_ClearCache(&pSession->context);
    return DI_SUCCESS;
}

Diana_XRef * Diana_CastXREF(Diana_FList_ListNode * pNode,
                            int index)
{
    Diana_SubXRef * pSubXREF = (Diana_SubXRef * )pNode;
    return DIANA_CONTAINING_RECORD(pSubXREF, Diana_XRef, m_subrefs[index]);
}

static
int XrefRouteMarker(Diana_FList_ListNode * pNode,
                    void * pContext,
                    int * pbDone)
{
    Diana_XRef * pXRef = Diana_CastXREF(pNode, (int)(DIANA_SIZE_T)pContext);
    pXRef->m_flags |= DI_XREF_INVALID;
    &pbDone;
    return DI_SUCCESS;
}

static
int RouteMarker(Diana_FList_ListNode * pNode,
                void * pContext,
                int * pbDone)
{
    Diana_Instruction * pInstruction = (Diana_Instruction * )pNode;
    DianaAnalyzeSession * pSession  = pContext;
    &pbDone;
    pInstruction->m_flags |= DI_INSTRUCTION_INVALID;

    Diana_FList_ListForEach(&pInstruction->m_referencesToThisInstruction, 
                             XrefRouteMarker, 
                             (void*)0);

    Diana_FList_ListForEach(&pInstruction->m_referencesFromThisInstruction, 
                              XrefRouteMarker, 
                              (void*)1);

    --pSession->pOwner->m_actualSize;
    return DI_SUCCESS;
}

static
void RollbackCurrentRoute(DianaAnalyzeSession * pSession)
{
    Diana_FList_ListForEach(&pSession->curRouteInfo.instructions,
                      RouteMarker,
                      pSession);
}

static
int Diana_AnalyzeCodeImpl(DianaAnalyzeSession * pSession,
                          OPERAND_SIZE initialOffset)
{
    int iRes = 0;
    OPERAND_SIZE offset = initialOffset;
    OPERAND_SIZE prevOffset = 0;
    
    int bNeedReset = 0;
    Diana_InitContext(&pSession->context, pSession->mode);
    Diana_Stack_Clear(pSession->pStack);

    // init cur route
    pSession->curRouteInfo.startOffset = offset;
    pSession->curRouteInfo.flags = 0;
    for(;;)
    {
        if (offset >= pSession->maxOffset || bNeedReset)
        {
            // just go out
            if (!pSession->pStack->m_count)
                break;

            DI_CHECK(SwitchToLastState(pSession, 
                                       &pSession->curRouteInfo, 
                                       &offset));
        }
        bNeedReset = 0;

        prevOffset = offset;

        iRes = Diana_ParseCmd(&pSession->context,
                              Diana_GetRootLine(),
                              &pSession->pObserver->m_pStream->parent,
                              &pSession->result);

        offset += pSession->result.iFullCmdSize;
        if (iRes)
        {
            // instruction not recognized properly
            if (pSession->flags & DI_ANALYSE_BREAK_ON_INVALID_CMD ||
                pSession->curRouteInfo.flags & DI_ROUTE_QUESTIONABLE)
            {
                RollbackCurrentRoute(pSession);
                bNeedReset = 1;
                continue;
            }

            offset = prevOffset + 1;
            DI_CHECK(pSession->pObserver->m_pStream->pMoveTo(pSession->pObserver->m_pStream, 
                                                             offset));
            Diana_ClearCache(&pSession->context);

            continue;
        }
        // check if instruction is always registered
        {
            Diana_Instruction * pInstruction = Diana_InstructionsOwner_GetInstruction(pSession->pOwner, prevOffset);
            if (pInstruction)
            {
                if (pInstruction->m_flags & DI_INSTRUCTION_INVALID)
                {
                    if (pSession->curRouteInfo.flags & DI_ROUTE_QUESTIONABLE)
                    {
                        RollbackCurrentRoute(pSession);
                    }
                    bNeedReset = 1;
                    continue;
                }
                if (!(pInstruction->m_flags & DI_INSTRUCTION_IS_LOADING))
                {
                    bNeedReset = 1;                 
                    continue;
                }
            }
        }
        // analyze instruction
        DI_CHECK(AnalyzeAndConstructInstruction(pSession,
                                                prevOffset,
                                                offset,
                                                &bNeedReset));
        if (offset >= pSession->maxOffset)
        {
            bNeedReset = 1;
            continue;
        }
    }
    return DI_SUCCESS;
}

int Diana_AnalyzeCodeEx(Diana_InstructionsOwner * pOwner,
                        DianaAnalyzeObserver * pObserver,
                        int mode,
                        OPERAND_SIZE initialOffset,
                        OPERAND_SIZE maxOffset,
                        Diana_Stack * pStack,
                        int flags)
{
    DianaAnalyzeSession session;
    DIANA_MEMSET(&session, 0, sizeof(session));

    session.maxOffset = maxOffset;
    session.pOwner = pOwner;
    session.pObserver = pObserver;
    session.mode = mode;
    session.pStack = pStack;
    session.flags = flags;
    
    {
        Diana_Instruction * pInstruction = Diana_InstructionsOwner_GetInstruction(pOwner, initialOffset);
        if (pInstruction)
        {
            return DI_SUCCESS;
        }
    }

    DI_CHECK(pObserver->m_pStream->pMoveTo(pObserver->m_pStream, initialOffset));
    DI_CHECK(Diana_AnalyzeCodeImpl(&session,
                                    initialOffset));

    return DI_SUCCESS;
}

int Diana_AnalyzeCode(Diana_InstructionsOwner * pOwner,
                      DianaAnalyzeObserver * pObserver,
                      int mode,
                      OPERAND_SIZE initialOffset,
                      OPERAND_SIZE maxOffset)
{
    int status = 0;
    Diana_Stack stack;
    DI_CHECK(Diana_Stack_Init(&stack, 4096, sizeof(Diana_RouteInfo)));
    status = Diana_AnalyzeCodeEx(pOwner,
                               pObserver,
                               mode,
                               initialOffset,
                               maxOffset,
                               &stack,
                               0);

    Diana_Stack_Free(&stack);
    return status;

}