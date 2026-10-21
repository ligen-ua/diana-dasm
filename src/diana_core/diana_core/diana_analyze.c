#include "diana_analyze.h"
#include "diana_stack.h"
#ifndef DIANA_NO_ARM
#include "diana_arm32.h"
#endif

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
    DianaAnalyzeParams params;
    // DI_INSTRUCTION_* of the instruction being analyzed, from its word class (AArch64)
    int instructionFlags;
#ifndef DIANA_NO_ARM
    // AArch64: X0..X30 with a known value (an address), bit n of armKnown; reset at every route start
    DI_UINT64 armRegs[31];
    DI_UINT32 armKnown;
    // the known values that are offsets (DIANA_ANALYZE_ADDRESSES_RELATIVE), not absolute addresses
    DI_UINT32 armRelative;
    // the previous instruction was cmp <armCmpReg>, #armCmpImm (-1: it wasn't)
    int armCmpReg;
    DI_UINT64 armCmpImm;
    // register armBoundReg is below armBoundCount (0: no bound)
    int armBoundReg;
    DI_UINT64 armBoundCount;
    // the bound a route about to be saved gets (b.ls/b.lo to the in-range code)
    int armRouteBoundReg;
    DI_UINT64 armRouteBoundCount;
    // a jump table in armTableReg (-1: none): stage 1 an entry, stage 2 a target = base + (extend(entry) << shift)
    int armTableReg;
    int armTableStage;
    DI_UINT64 armTableAddress;
    int armTableAddressRelative;
    int armTableEntrySize;
    int armTableSigned;
    DI_UINT64 armTableCount;
    DI_UINT64 armTableBase;
    int armTableBaseRelative;
    int armTableExtend;
    int armTableShift;
    /* AArch32: the current route is T32; the mode of a route about to be saved (the current one unless
       the branch switches, BLX) */
    int a32Thumb;
    int a32RouteThumb;
#endif
}DianaAnalyzeSession;

static
int IsStrictRoute(const DianaAnalyzeSession * pSession);

// AArch64 route state: no register is known, the bound comes from the route
static
void ResetArm64State(DianaAnalyzeSession * pSession, const Diana_RouteInfo * pRoute)
{
#ifndef DIANA_NO_ARM
    pSession->armKnown = 0;
    pSession->armCmpReg = -1;
    pSession->armTableReg = -1;
    pSession->armBoundReg = pRoute ? pRoute->armBoundReg : -1;
    pSession->armBoundCount = pRoute ? pRoute->armBoundCount : 0;
#else
    (void)pSession;
    (void)pRoute;
#endif
}

static
int IsArm64(const DianaAnalyzeSession * pSession)
{
    return pSession->params.arch == DIANA_ARCH_AARCH64;
}

/* AArch32: A32 and T32 code, a route is one or the other */
static
int IsArm32(const DianaAnalyzeSession * pSession)
{
    return pSession->params.arch == DIANA_ARCH_ARM32 || pSession->params.arch == DIANA_ARCH_THUMB;
}

/* AArch64 or AArch32: instructions start at aligned addresses, zeros are no code on a strict route */
static
int IsArm(const DianaAnalyzeSession * pSession)
{
    return IsArm64(pSession) || IsArm32(pSession);
}

/* where an instruction can start: 4 (A64, A32), 2 (T32), 1 (x86); bThumb is the mode of an AArch32 route */
static
OPERAND_SIZE GetArmAlignment(const DianaAnalyzeSession * pSession, int bThumb)
{
    if (IsArm32(pSession))
        return bThumb ? 2 : 4;
    return IsArm64(pSession) ? 4 : 1;
}

/* the current route */
static
OPERAND_SIZE GetRouteAlignment(const DianaAnalyzeSession * pSession)
{
#ifndef DIANA_NO_ARM
    return GetArmAlignment(pSession, pSession->a32Thumb);
#else
    return GetArmAlignment(pSession, 0);
#endif
}

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
int AnalyzeJumps(DianaParserResult* pResult,
                 Diana_LinkedAdditionalGroupInfo * pLinkedInfo,
                 OPERAND_SIZE * pNewOffset,
                 OPERAND_SIZE offset,
                 int * pAbsoluteAddress,
                 int * pLinksToData)
{
    const DianaLinkedOperand * pOp = pResult->linkedOperands + pLinkedInfo->relArgrumentNumber;
    *pNewOffset = 0;
    *pAbsoluteAddress = 0;
    *pLinksToData = 0;

    if (pLinkedInfo->relArgrumentNumber >= pResult->iLinkedOpCount)
    {
        return DI_UNSUPPORTED_COMMAND;
    }

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
    default:
        return DI_UNSUPPORTED_COMMAND;
    }
    return DI_SUCCESS;
}

int Diana_AnalyzeJumps(DianaParserResult* pResult,
    OPERAND_SIZE offset,
    OPERAND_SIZE* pNewOffset,
    int* pAbsoluteAddress,
    int* pLinksToData)
{
    Diana_LinkedAdditionalGroupInfo* pLinkedInfo = 0;

    *pNewOffset = 0;
    *pAbsoluteAddress = 0;
    *pLinksToData = 0;
    if (!pResult->pInfo || !pResult->pInfo->m_pGroupInfo)
    {
        return DI_NOT_FOUND;
    }
    pLinkedInfo = pResult->pInfo->m_pGroupInfo->m_pLinkedInfo;
    if (!pLinkedInfo)
    {
        return DI_NOT_FOUND;
    }
    else
    if (!(pLinkedInfo->flags & DIANA_GT_CAN_CHANGE_RIP))
    {
        return DI_NOT_FOUND;
    }

    // handle ret case
    if (pLinkedInfo->flags & (DIANA_GT_RET|DIANA_GT_STOP))
    {
        return DI_NOT_FOUND;
    }

#ifndef DIANA_NO_ARM
    if (DianaA32_GetCmdInfo(pResult))
    {
        // AArch32: only labels; tbb [pc, r0] has no target of its own. T32 blx counts from the
        // address rounded down to 4
        const DianaLinkedOperand * pOp = pResult->linkedOperands + pLinkedInfo->relArgrumentNumber;
        int iRes = 0;
        if (pLinkedInfo->relArgrumentNumber >= pResult->iLinkedOpCount || pOp->type != diana_rel)
            return DI_NOT_FOUND;
        iRes = AnalyzeJumps(pResult, pLinkedInfo, pNewOffset, offset, pAbsoluteAddress, pLinksToData);
        if (iRes == DI_SUCCESS && pOp->arm.qualifier == DIANA_ARM_QUAL_ALIGN_PC)
            *pNewOffset -= (offset - pResult->iFullCmdSize) & 3;
        return iRes;
    }
#endif
    return AnalyzeJumps(pResult,
        pLinkedInfo,
        pNewOffset,
        offset,
        pAbsoluteAddress,
        pLinksToData);
}

static
int SaveNewExternalRoute(DianaAnalyzeSession * pSession,
                         Diana_Instruction * pInstruction,
                         OPERAND_SIZE newOffset,
                         int bLinksToData,
                         int xrefFlags)
{
    int flags = 0;
    Diana_XRef * pXref = 0;
    Diana_Instruction * pTargetInstruction = 0;
    if (pSession->pOwner->m_minimalExternalAddress && newOffset < pSession->pOwner->m_minimalExternalAddress)
    {
        // skip first region
        return DI_SUCCESS;
    }
    if (pSession->mode == DIANA_MODE32)
    {
        if (newOffset >= 0xFFFFFFFFULL)
        {
            return DI_SUCCESS;
        }
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
    pXref->m_flags |= DI_XREF_EXTERNAL | xrefFlags;
    return DI_SUCCESS;
}

// a reference to data inside the module: the target goes to the external list, never to m_ppPresenceVec,
// so it can't block code at that offset
static
int SaveDataXRef(DianaAnalyzeSession * pSession,
                 Diana_Instruction * pInstruction,
                 OPERAND_SIZE offset,
                 int xrefFlags)
{
    Diana_XRef * pXref = 0;
    Diana_Instruction * pTargetInstruction = DIANA_MALLOC(sizeof(Diana_Instruction));
    DI_CHECK_ALLOC(pTargetInstruction);
    Diana_Instruction_Init(pTargetInstruction, offset, DI_INSTRUCTION_DATA);
    Diana_FList_PushBack(&pSession->pOwner->m_externalInstructionsList, &pTargetInstruction->m_routeEntry);
    DI_CHECK(Diana_CreateXRef(pSession->pOwner, pInstruction, pTargetInstruction, &pXref));
    pXref->m_flags |= DI_XREF_DATA | xrefFlags;
    return DI_SUCCESS;
}

// mov eax, [rip+X], mov eax, [X]: the instruction reads or writes X, so X is data and never a route
static
int SaveDataAccess(DianaAnalyzeSession * pSession,
                   Diana_Instruction * pInstruction,
                   OPERAND_SIZE address,
                   int bAbsoluteAddress)
{
    DianaAnalyzeAddressResult_type result = diaJumpInvalid;
    OPERAND_SIZE offset = 0;
    DI_CHECK(pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver,
                                                    address,
                                                    bAbsoluteAddress ? DIANA_ANALYZE_ABSOLUTE_ADDRESS : 0,
                                                    &offset,
                                                    &result));
    switch (result)
    {
    case diaJumpNormal:
        if (IsArm(pSession) && offset < pSession->maxOffset)
        {
            // adrp x2, tbl; add x2, x2, #:lo12:tbl; ldrb w2, [x2, w0]: the address taken by add is data.
            // Its questionable route hasn't run yet: it is rolled back when it starts, the xref stays as data
            Diana_Instruction * pPending = Diana_InstructionsOwner_GetInstruction(pSession->pOwner, offset);
            if (pPending && (pPending->m_flags & DI_INSTRUCTION_IS_LOADING) && !(pPending->m_flags & DI_INSTRUCTION_ROOT))
                pPending->m_flags |= DI_INSTRUCTION_INVALID;
        }
        return SaveDataXRef(pSession, pInstruction, offset, 0);
    case diaJumpExternal:
        return SaveNewExternalRoute(pSession, pInstruction, offset, 1, DI_XREF_DATA);
    default:
        return DI_SUCCESS;
    }
}

static
int SaveNewRoute(DianaAnalyzeSession * pSession,
                 Diana_Instruction * pInstruction,
                 OPERAND_SIZE newOffset,
                 int newRouteFlags,
                 int xrefFlags)
{
    Diana_Instruction * pTargetInstruction = 0;
    Diana_Instruction * pOldTargetInstruction = 0;
    Diana_RouteInfo newRouteInfo;
    Diana_XRef * pXref = 0;

    if (newOffset >= pSession->maxOffset)
        return DI_SUCCESS;

#ifndef DIANA_NO_ARM
    if (IsArm(pSession) &&
        ((pSession->params.imageBase + newOffset) & (GetArmAlignment(pSession, pSession->a32RouteThumb) - 1)))
#else
    if (IsArm(pSession) && ((pSession->params.imageBase + newOffset) & 3))
#endif
    {
        // not an instruction address
        if (xrefFlags & DI_XREF_ADDRESS_TAKEN)
            return SaveDataXRef(pSession, pInstruction, newOffset, xrefFlags);
        return DI_SUCCESS;
    }

    DIANA_MEMSET(&newRouteInfo, 0, sizeof(newRouteInfo));
    newRouteInfo.startOffset = newOffset;
    newRouteInfo.flags = newRouteFlags;
#ifndef DIANA_NO_ARM
    newRouteInfo.armBoundReg = pSession->armRouteBoundReg;
    newRouteInfo.armBoundCount = pSession->armRouteBoundCount;
    newRouteInfo.armThumb = pSession->a32RouteThumb;
#endif

    // create new instruction
    pOldTargetInstruction = Diana_InstructionsOwner_GetInstruction(pSession->pOwner, newOffset);
    if (pOldTargetInstruction &&
        (pOldTargetInstruction->m_flags & DI_INSTRUCTION_INVALID) &&
        (xrefFlags & DI_XREF_ADDRESS_TAKEN))
    {
        // an address of something that is known not to be code
        return SaveDataXRef(pSession, pInstruction, newOffset, xrefFlags);
    }

    DI_CHECK(Diana_InstructionsOwner_QueryOrAllocateInstruction(pSession->pOwner, 
                                                                newOffset, 
                                                                &pTargetInstruction,
                                                                DI_INSTRUCTION_IS_LOADING)); 

    if (!(newRouteFlags & DI_ROUTE_QUESTIONABLE))
    {
        pTargetInstruction->m_flags |= DI_INSTRUCTION_ROOT;
    }

    // register xref-from Instruction to Target
    DI_CHECK(Diana_CreateXRef(pSession->pOwner, pInstruction, pTargetInstruction, &pXref));
    pXref->m_flags |= xrefFlags;

    // do not push new route to stack twice
    if (pOldTargetInstruction)
        return DI_SUCCESS;

    // push to stack
    DI_CHECK(Diana_Stack_Push(pSession->pStack, &newRouteInfo));

    return DI_SUCCESS;
}

// lea takes the address of its memory operand, every other instruction reads or writes it
static
int IsLea(DianaAnalyzeSession * pSession)
{
    return pSession->result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_lea;
}

// [X] without registers in a memory access: X is data
static
int IsAbsoluteDataAccess(DianaAnalyzeSession * pSession, const DianaLinkedOperand * pOp)
{
    return pOp->value.rmIndex.reg == reg_none &&
           (pOp->value.rmIndex.indexed_reg == reg_none || pOp->value.rmIndex.index == 0) &&
           (pOp->value.rmIndex.dispSize == 4 || pOp->value.rmIndex.dispSize == 8) &&
           !IsLea(pSession);
}

static
int DispatchMode32(DianaAnalyzeSession * pSession,
                   Diana_Instruction * pInstruction,
                   const DianaLinkedOperand * pOp,
                   int * pbFound,
                   OPERAND_SIZE * pSuspectedOp)
{
    *pbFound = 0;
    switch(pOp->type)
    {
    case diana_imm:
        *pbFound = 1;
        *pSuspectedOp = pOp->value.imm;
        break;
    case diana_index:
        if (IsAbsoluteDataAccess(pSession, pOp))
        {
            DI_CHECK(SaveDataAccess(pSession, pInstruction, pOp->value.rmIndex.dispValue, 1));
            break;
        }
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
                OPERAND_SIZE target = nextInstructionOffset + pOp->value.rmIndex.dispValue;
                pInstruction->m_flags |= DI_INSTRUCTION_USES_RIP;
                if (IsLea(pSession))
                {
                    DI_CHECK(SaveNewRoute(pSession,
                                          pInstruction,
                                          target,
                                          DI_ROUTE_QUESTIONABLE,
                                          DI_XREF_ADDRESS_TAKEN));
                    break;
                }
                DI_CHECK(SaveDataAccess(pSession, pInstruction, target, 0));
                break;
            }
            
            // bad case
            pInstruction->m_flags |= DI_INSTRUCTION_USES_UNKNOWN_RIP;
            break;
        }

        if (IsAbsoluteDataAccess(pSession, pOp))
        {
            DI_CHECK(SaveDataAccess(pSession, pInstruction, pOp->value.rmIndex.dispValue, 1));
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

// AArch64 operands: ADR takes an address, LDR (literal) reads data. Immediates are not addresses,
// they are too small; addresses are built in registers (ADRP, MOVZ/MOVK)
static
int DispatchArm64(DianaAnalyzeSession * pSession,
                  Diana_Instruction * pInstruction,
                  const DianaLinkedOperand * pOp,
                  OPERAND_SIZE nextInstructionOffset)
{
    switch (pOp->type)
    {
    case diana_rel:
        // adr: relative to the next instruction like branches
        pInstruction->m_flags |= DI_INSTRUCTION_USES_RIP;
        return SaveNewRoute(pSession,
                            pInstruction,
                            nextInstructionOffset + pOp->value.rel.m_value,
                            DI_ROUTE_QUESTIONABLE,
                            DI_XREF_ADDRESS_TAKEN);
    case diana_index:
        if (pOp->value.rmIndex.reg == reg_A64_PC)
        {
            // ldr (literal): [pc + offset], relative to the next instruction
            pInstruction->m_flags |= DI_INSTRUCTION_USES_RIP;
            return SaveDataAccess(pSession, pInstruction, nextInstructionOffset + pOp->value.rmIndex.dispValue, 0);
        }
        break;
    default:
        break;
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

        if (IsArm64(pSession))
        {
            DI_CHECK(DispatchArm64(pSession, pInstruction, pOp, nextInstructionOffset));
            continue;
        }
        if (pSession->mode == DIANA_MODE32)
        {
            DI_CHECK(DispatchMode32(pSession,
                                    pInstruction,
                                    pOp,
                                    &bFound,
                                    &suspectedOp));
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
                                              0,
                                              DI_XREF_ADDRESS_TAKEN));
            }
            else
            if (result == diaJumpNormal)
            {
                // this is not realy valid command
                DI_CHECK(SaveNewRoute(pSession,
                                    pInstruction,
                                    suspectedOp,
                                    DI_ROUTE_QUESTIONABLE,
                                    DI_XREF_ADDRESS_TAKEN));
            }
        }
    }
    return DI_SUCCESS;
}

#ifndef DIANA_NO_ARM
// ------------------------------------------------------------------ AArch64 register values
// X0..X30 and W0..W30 -> 0..30, -1 for SP, XZR and everything else
static
int GetArm64RegisterIndex(DianaUnifiedRegister reg)
{
    if (reg >= reg_A64_X0 && reg < reg_A64_X0 + 31)
        return (int)(reg - reg_A64_X0);
    if (reg >= reg_A64_W0 && reg < reg_A64_W0 + 31)
        return (int)(reg - reg_A64_W0);
    return -1;
}

// the observer flags of an address: offsets from ADRP/ADR with DIANA_ANALYZE_ADDRESSES_RELATIVE, absolute otherwise
static
int GetArm64AddressFlags(int bRelative)
{
    return bRelative ? 0 : DIANA_ANALYZE_ABSOLUTE_ADDRESS;
}

// the address of offset in the module: an offset with DIANA_ANALYZE_ADDRESSES_RELATIVE, imageBase + offset otherwise
static
DI_UINT64 GetArm64ModuleAddress(const DianaAnalyzeSession * pSession, OPERAND_SIZE offset, int * pRelative)
{
    *pRelative = (pSession->params.flags & DIANA_ANALYZE_ADDRESSES_RELATIVE) != 0;
    return *pRelative ? offset : pSession->params.imageBase + offset;
}

// pRelative may be 0
static
int GetArm64Value(const DianaAnalyzeSession * pSession, const DianaLinkedOperand * pOp, DI_UINT64 * pValue,
                  int * pRelative)
{
    int index = -1;
    if (pOp->type != diana_register || pOp->usedSize != 8)
        return 0;
    index = GetArm64RegisterIndex(pOp->value.recognizedRegister);
    if (index < 0 || !(pSession->armKnown & (1u << index)))
        return 0;
    *pValue = pSession->armRegs[index];
    if (pRelative)
        *pRelative = (pSession->armRelative >> index) & 1;
    return 1;
}

// forgets the registers of the mask: their values, a bound or a jump table in them
static
void ForgetArm64Registers(DianaAnalyzeSession * pSession, DI_UINT32 mask)
{
    pSession->armKnown &= ~mask;
    if (pSession->armCmpReg >= 0 && (mask & (1u << pSession->armCmpReg)))
        pSession->armCmpReg = -1;
    if (pSession->armBoundReg >= 0 && (mask & (1u << pSession->armBoundReg)))
        pSession->armBoundCount = 0;
    if (pSession->armTableReg >= 0 && (mask & (1u << pSession->armTableReg)))
        pSession->armTableReg = -1;
}

static
void SetArm64Value(DianaAnalyzeSession * pSession, const DianaLinkedOperand * pOp, DI_UINT64 value, int bRelative)
{
    int index = GetArm64RegisterIndex(pOp->value.recognizedRegister);
    if (index < 0)
        return;
    ForgetArm64Registers(pSession, 1u << index);
    pSession->armRegs[index] = value;
    pSession->armKnown |= 1u << index;
    if (bRelative)
        pSession->armRelative |= 1u << index;
    else
        pSession->armRelative &= ~(1u << index);
}

static
void DropArm64Register(DianaAnalyzeSession * pSession, DianaUnifiedRegister reg)
{
    int index = GetArm64RegisterIndex(reg);
    if (index >= 0)
        ForgetArm64Registers(pSession, 1u << index);
}

// the target of br/blr Xn (braa Xn, Xm...), an address
static
int GetArm64Target(const DianaAnalyzeSession * pSession, OPERAND_SIZE * pTarget, int * pRelative)
{
    DI_UINT64 value = 0;
    if (pSession->result.iLinkedOpCount < 1 ||
        !GetArm64Value(pSession, pSession->result.linkedOperands, &value, pRelative))
    {
        return 0;
    }
    *pTarget = value;
    return 1;
}

// reads 8 bytes at an address of the module, little-endian
static
int ReadArm64Pointer(DianaAnalyzeSession * pSession, DI_UINT64 address, int bRelative, DI_UINT64 * pValue)
{
    DianaAnalyzeAddressResult_type result = diaJumpInvalid;
    OPERAND_SIZE offset = 0;
    OPERAND_SIZE readBytes = 0;
    unsigned char bytes[8];
    int i = 0;
    if (pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver,
                                               address,
                                               GetArm64AddressFlags(bRelative),
                                               &offset,
                                               &result) != DI_SUCCESS || result != diaJumpNormal)
    {
        return 0;
    }
    if (pSession->pObserver->m_pStream->pRandomRead(pSession->pObserver->m_pStream,
                                                    offset,
                                                    bytes,
                                                    8,
                                                    &readBytes,
                                                    0) != DI_SUCCESS || readBytes != 8)
    {
        return 0;
    }
    *pValue = 0;
    for (i = 7; i >= 0; --i)
        *pValue = (*pValue << 8) | bytes[i];
    return 1;
}

// an address in a register is a reference: code is tried as a questionable route, data stays a data xref
static
int SaveArm64AddressTaken(DianaAnalyzeSession * pSession, Diana_Instruction * pInstruction,
                          DI_UINT64 address, int bRelative)
{
    DianaAnalyzeAddressResult_type result = diaJumpInvalid;
    OPERAND_SIZE offset = 0;
    DI_CHECK(pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver,
                                                    address,
                                                    GetArm64AddressFlags(bRelative),
                                                    &offset,
                                                    &result));
    if (result != diaJumpNormal)
        return DI_SUCCESS;
    return SaveNewRoute(pSession, pInstruction, offset, DI_ROUTE_QUESTIONABLE, DI_XREF_ADDRESS_TAKEN);
}

// a value loaded from memory is an address taken when it points into the module (a GOT entry, ldr x0, =func)
static
int LoadArm64Value(DianaAnalyzeSession * pSession,
                   Diana_Instruction * pInstruction,
                   const DianaLinkedOperand * pTarget,
                   DI_UINT64 address,
                   int bRelative)
{
    DI_UINT64 value = 0;
    // only X0..X30: ldr d0, =1.5 loads a constant, not an address
    int isX = pTarget->type == diana_register &&
              pTarget->value.recognizedRegister >= reg_A64_X0 && pTarget->value.recognizedRegister < reg_A64_X0 + 31;
    // 0 is a null pointer, an unrelocated GOT slot in a file
    if (!isX || !ReadArm64Pointer(pSession, address, bRelative, &value) || !value)
    {
        DropArm64Register(pSession, pTarget->value.recognizedRegister);
        return DI_SUCCESS;
    }
    // a loaded address is absolute
    SetArm64Value(pSession, pTarget, value, 0);
    return SaveArm64AddressTaken(pSession, pInstruction, value, 0);
}

// reads an entry of size bytes at an address of the module, little-endian, sign-extended when bSigned
static
int ReadArm64Entry(DianaAnalyzeSession * pSession, DI_UINT64 address, int bRelative, int size, int bSigned,
                   DI_UINT64 * pValue)
{
    DianaAnalyzeAddressResult_type result = diaJumpInvalid;
    OPERAND_SIZE offset = 0;
    OPERAND_SIZE readBytes = 0;
    unsigned char bytes[8];
    int i = 0;
    if (pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver,
                                               address,
                                               GetArm64AddressFlags(bRelative),
                                               &offset,
                                               &result) != DI_SUCCESS || result != diaJumpNormal)
    {
        return 0;
    }
    if (pSession->pObserver->m_pStream->pRandomRead(pSession->pObserver->m_pStream,
                                                    offset,
                                                    bytes,
                                                    size,
                                                    &readBytes,
                                                    0) != DI_SUCCESS || readBytes != (OPERAND_SIZE)size)
    {
        return 0;
    }
    *pValue = 0;
    for (i = size - 1; i >= 0; --i)
        *pValue = (*pValue << 8) | bytes[i];
    if (bSigned && size < 8 && (*pValue >> (size * 8 - 1)) & 1)
        *pValue |= ~(DI_UINT64)0 << (size * 8);
    return 1;
}

// the value of an extended or shifted register operand (DIANA_ARM_EXTEND_*, LSL), before the shift
static
DI_UINT64 ExtendArm64(DI_UINT64 value, int extend)
{
    switch (extend)
    {
    case DIANA_ARM_EXTEND_UXTB: return value & 0xFF;
    case DIANA_ARM_EXTEND_UXTH: return value & 0xFFFF;
    case DIANA_ARM_EXTEND_UXTW: return value & 0xFFFFFFFF;
    case DIANA_ARM_EXTEND_SXTB: return (DI_UINT64)(OPERAND_SIZE_SIGNED)(signed char)value;
    case DIANA_ARM_EXTEND_SXTH: return (DI_UINT64)(OPERAND_SIZE_SIGNED)(short)value;
    case DIANA_ARM_EXTEND_SXTW: return (DI_UINT64)(OPERAND_SIZE_SIGNED)(int)value;
    default:                    return value;
    }
}

// the largest jump table that is followed
#define DIANA_ARM64_MAX_JUMP_TABLE  0x4000

// br Xn with a jump table in Xn: every entry is a route of the current kind
static
int IsArm64JumpTable(const DianaAnalyzeSession * pSession)
{
    const DianaLinkedOperand * pOp = pSession->result.linkedOperands;
    if (pSession->armTableReg < 0 || pSession->result.iLinkedOpCount < 1 || pOp->type != diana_register)
        return 0;
    if (GetArm64RegisterIndex(pOp->value.recognizedRegister) != pSession->armTableReg)
        return 0;
    // an entry itself is a target only in a table of 8-byte addresses
    return pSession->armTableStage == 2 || pSession->armTableEntrySize == 8;
}

static
int SaveArm64JumpTable(DianaAnalyzeSession * pSession, Diana_Instruction * pInstruction)
{
    DI_UINT64 i = 0;
    DI_UINT64 previous = 0;
    for (i = 0; i < pSession->armTableCount && i < DIANA_ARM64_MAX_JUMP_TABLE; ++i)
    {
        DI_UINT64 entry = 0;
        DI_UINT64 target = 0;
        DianaAnalyzeAddressResult_type result = diaJumpInvalid;
        OPERAND_SIZE offset = 0;
        if (!ReadArm64Entry(pSession,
                            pSession->armTableAddress + i * pSession->armTableEntrySize,
                            pSession->armTableAddressRelative,
                            pSession->armTableEntrySize,
                            pSession->armTableSigned,
                            &entry))
        {
            break;
        }
        target = pSession->armTableStage == 2
            ? pSession->armTableBase + (ExtendArm64(entry, pSession->armTableExtend) << pSession->armTableShift)
            : entry;
        if (i && target == previous)
            continue;
        previous = target;
        // the entries of an absolute table are absolute addresses
        DI_CHECK(pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver,
                                                        target,
                                                        GetArm64AddressFlags(pSession->armTableStage == 2 &&
                                                                             pSession->armTableBaseRelative),
                                                        &offset,
                                                        &result));
        if (result == diaJumpNormal)
            DI_CHECK(SaveNewRoute(pSession, pInstruction, offset, pSession->curRouteInfo.flags, 0));
    }
    return DI_SUCCESS;
}

// after cmp <armCmpReg>, #armCmpImm: the bound of the register where the condition holds (bHolds), or where
// it doesn't. ls/lo bound it where they hold, hi/hs where they don't (the same numbers in A64 and AArch32)
static
int GetArmCompareBound(const DianaAnalyzeSession * pSession, int cond, int bHolds, DI_UINT64 * pCount)
{
    if (pSession->armCmpReg < 0)
        return 0;
    // signed conditions too: the compiler uses them when the index can't be negative (memset: cmp x2, #32; b.gt)
    switch (cond)
    {
    case 9:     // ls: below or equal where it holds
    case 13:    // le
        *pCount = pSession->armCmpImm + 1;
        return bHolds;
    case 3:     // lo: below where it holds
    case 11:    // lt
        *pCount = pSession->armCmpImm;
        return bHolds;
    case 8:     // hi: below or equal where it doesn't
    case 12:    // gt
        *pCount = pSession->armCmpImm + 1;
        return !bHolds;
    case 2:     // hs: below where it doesn't
    case 10:    // ge
        *pCount = pSession->armCmpImm;
        return !bHolds;
    }
    return 0;
}

// a cmp before b.ls/b.lo bounds the index on the branch target; b.hi/b.hs bound it on the next instruction
static
int GetArm64BranchBound(const DianaAnalyzeSession * pSession, int bTarget, DI_UINT64 * pCount)
{
    if (pSession->result.pInfo->m_pGroupInfo->m_commandId != (DianaCommandId)diana_a64_cmd_b_cond ||
        pSession->result.iLinkedOpCount < 2)
    {
        return 0;
    }
    return GetArmCompareBound(pSession, pSession->result.linkedOperands[1].arm.cond, bTarget, pCount);
}

// the instruction may change NZCV (or is a branch or a call, after which the compare is gone)
static
int ChangesArm64Flags(const DianaAnalyzeSession * pSession,
                      long id,
                      const Diana_LinkedAdditionalGroupInfo * pLinkedInfo,
                      const Diana_Instruction * pInstruction)
{
    (void)pSession;
    if (pLinkedInfo || (pInstruction->m_flags & DI_INSTRUCTION_UNSUPPORTED))
        return 1;
    switch (id)
    {
    case diana_a64_cmd_subs: case diana_a64_cmd_adds: case diana_a64_cmd_ands: case diana_a64_cmd_bics:
    case diana_a64_cmd_adcs: case diana_a64_cmd_sbcs: case diana_a64_cmd_ccmp: case diana_a64_cmd_ccmn:
    case diana_a64_cmd_fcmp: case diana_a64_cmd_fcmpe: case diana_a64_cmd_fccmp: case diana_a64_cmd_fccmpe:
    case diana_a64_cmd_rmif: case diana_a64_cmd_setf8: case diana_a64_cmd_setf16: case diana_a64_cmd_cfinv:
    case diana_a64_cmd_axflag: case diana_a64_cmd_xaflag: case diana_a64_cmd_msr:
        return 1;
    }
    return 0;
}

// updates the known registers after an instruction and saves the references it makes:
// adrp Xd; add Xd, Xn, #imm; ldr Xt, [Xn, #imm]; ldr Xt, literal; mov Xd, Xm. Everything else forgets the registers
// it names: dropping too much only loses resolution, never makes a wrong reference
static
int TrackArm64(DianaAnalyzeSession * pSession,
               Diana_Instruction * pInstruction,
               OPERAND_SIZE instructionOffset,
               OPERAND_SIZE nextInstructionOffset)
{
    const DianaParserResult * pResult = &pSession->result;
    const DianaLinkedOperand * pOps = pResult->linkedOperands;
    long id = (long)pResult->pInfo->m_pGroupInfo->m_commandId;
    const Diana_LinkedAdditionalGroupInfo * pLinkedInfo = pResult->pInfo->m_pGroupInfo->m_pLinkedInfo;
    DI_UINT64 base = 0;
    DI_UINT64 count = 0;
    int bRelative = 0;
    int i = 0;
    int cmpReg = pSession->armCmpReg;

    if (GetArm64BranchBound(pSession, 0, &count))
    {
        pSession->armBoundReg = cmpReg;
        pSession->armBoundCount = count;
    }
    // the compare holds until the flags change: compilers schedule other instructions between cmp and b.hi
    if (ChangesArm64Flags(pSession, id, pLinkedInfo, pInstruction))
        pSession->armCmpReg = -1;

    if (id == diana_a64_cmd_subs && pResult->iLinkedOpCount == 3 &&
        pOps[0].type == diana_register &&
        (pOps[0].value.recognizedRegister == reg_A64_WZR || pOps[0].value.recognizedRegister == reg_A64_XZR) &&
        pOps[1].type == diana_register && GetArm64RegisterIndex(pOps[1].value.recognizedRegister) >= 0 &&
        pOps[2].type == diana_imm && pOps[2].arm.shiftAmount == 0)
    {
        // cmp Rn, #imm: only the flags change
        pSession->armCmpReg = GetArm64RegisterIndex(pOps[1].value.recognizedRegister);
        pSession->armCmpImm = pOps[2].value.imm;
        return DI_SUCCESS;
    }
    if (id == diana_a64_cmd_adr && pResult->iLinkedOpCount == 2 && pOps[1].type == diana_rel)
    {
        // the reference is made by DispatchArm64
        base = GetArm64ModuleAddress(pSession, nextInstructionOffset + pOps[1].value.rel.m_value, &bRelative);
        SetArm64Value(pSession, pOps, base, bRelative);
        return DI_SUCCESS;
    }
    if ((id == diana_a64_cmd_ldrb || id == diana_a64_cmd_ldrh || id == diana_a64_cmd_ldrsb ||
         id == diana_a64_cmd_ldrsh || id == diana_a64_cmd_ldrsw || id == diana_a64_cmd_ldr) &&
        pResult->iLinkedOpCount == 2 && pOps[1].type == diana_index && pOps[1].arm.opKind == DIANA_A64_OPK_MEM_REGOFF &&
        pSession->armBoundCount && GetArm64RegisterIndex(pOps[1].value.rmIndex.indexed_reg) == pSession->armBoundReg)
    {
        // ldrb w2, [x2, w0, uxtw]: an entry of a table at a known address, the index is bounded
        int size = id == diana_a64_cmd_ldrb || id == diana_a64_cmd_ldrsb ? 1 :
                   id == diana_a64_cmd_ldrh || id == diana_a64_cmd_ldrsh ? 2 :
                   id == diana_a64_cmd_ldrsw ? 4 : (int)pOps[0].usedSize;
        int scale = size == 1 ? 0 : size == 2 ? 1 : size == 4 ? 2 : 3;
		int scaled = 0;
        DianaLinkedOperand baseOp = pOps[1];
        baseOp.type = diana_register;
        baseOp.usedSize = 8;
        baseOp.value.recognizedRegister = pOps[1].value.rmIndex.reg;
        // the index is scaled by the entry size: no shift for bytes, #log2(size) otherwise
        scaled = scale == 0 ? pOps[1].arm.shiftAmount == 0
                                : (pOps[1].arm.hasAmount && pOps[1].arm.shiftAmount == scale);
        if (scaled && pOps[0].type == diana_register &&
            GetArm64RegisterIndex(pOps[0].value.recognizedRegister) >= 0 &&
            GetArm64Value(pSession, &baseOp, &base, &bRelative))
        {
            int index = GetArm64RegisterIndex(pOps[0].value.recognizedRegister);
            // ldr w0, [x1, w0, uxtw #2] overwrites the index: the bound goes to the table first
            DI_UINT64 tableCount = pSession->armBoundCount;
            DI_CHECK(SaveDataAccess(pSession, pInstruction, base, !bRelative));
            ForgetArm64Registers(pSession, 1u << index);
            pSession->armTableReg = index;
            pSession->armTableStage = 1;
            pSession->armTableAddress = base;
            pSession->armTableAddressRelative = bRelative;
            pSession->armTableEntrySize = size;
            pSession->armTableSigned = id == diana_a64_cmd_ldrsb || id == diana_a64_cmd_ldrsh || id == diana_a64_cmd_ldrsw;
            pSession->armTableCount = tableCount;
            return DI_SUCCESS;
        }
    }
    if (id == diana_a64_cmd_add && pResult->iLinkedOpCount == 3 && pSession->armTableReg >= 0 &&
        pSession->armTableStage == 1 && pOps[0].usedSize == 8 &&
        pOps[2].type == diana_arm_shifted_reg &&
        GetArm64RegisterIndex(pOps[2].value.recognizedRegister) == pSession->armTableReg &&
        GetArm64Value(pSession, pOps + 1, &base, &bRelative) &&
        (pOps[2].arm.shiftType == DIANA_ARM_SHIFT_LSL || pOps[2].arm.shiftType >= DIANA_ARM_EXTEND_UXTB))
    {
        // add x2, x0, w2, sxtb #2: the target of the entry
        int index = GetArm64RegisterIndex(pOps[0].value.recognizedRegister);
        if (index >= 0)
        {
            int extend = pOps[2].arm.shiftType;
            int shift = pOps[2].arm.shiftAmount;
            ForgetArm64Registers(pSession, 1u << index);
            pSession->armTableReg = index;
            pSession->armTableStage = 2;
            pSession->armTableBase = base;
            pSession->armTableBaseRelative = bRelative;
            pSession->armTableExtend = extend;
            pSession->armTableShift = shift;
            return DI_SUCCESS;
        }
    }

    if (pInstruction->m_flags & DI_INSTRUCTION_UNSUPPORTED)
    {
        // the registers it writes are unknown
        ForgetArm64Registers(pSession, 0x7FFFFFFFu);
        return DI_SUCCESS;
    }
    if (pLinkedInfo && (pLinkedInfo->flags & DIANA_GT_IS_CALL))
    {
        // the callee may change x0..x18 and x30
        ForgetArm64Registers(pSession, 0x4007FFFFu);
        return DI_SUCCESS;
    }
    if (id == diana_a64_cmd_adrp && pResult->iLinkedOpCount == 2 && pOps[1].type == diana_arm_special)
    {
        if (pSession->params.flags & DIANA_ANALYZE_IMAGE_BASE_UNKNOWN)
        {
            DropArm64Register(pSession, pOps[0].value.recognizedRegister);
            return DI_SUCCESS;
        }
        base = GetArm64ModuleAddress(pSession, instructionOffset, &bRelative);
        SetArm64Value(pSession, pOps, (base & ~(DI_UINT64)0xFFF) + pOps[1].value.imm, bRelative);
        return DI_SUCCESS;
    }
    if (id == diana_a64_cmd_add && pResult->iLinkedOpCount == 3 && pOps[2].type == diana_imm &&
        pOps[0].usedSize == 8 && GetArm64Value(pSession, pOps + 1, &base, &bRelative))
    {
        // add Xd, Xn, #imm{, lsl #12}
        DI_UINT64 value = base + (pOps[2].value.imm << pOps[2].arm.shiftAmount);
        SetArm64Value(pSession, pOps, value, bRelative);
        return SaveArm64AddressTaken(pSession, pInstruction, value, bRelative);
    }
    if (id == diana_a64_cmd_orr && pResult->iLinkedOpCount == 3 &&
        pOps[1].type == diana_register && pOps[1].value.recognizedRegister == reg_A64_XZR &&
        pOps[2].type == diana_arm_shifted_reg && pOps[2].arm.shiftAmount == 0)
    {
        // mov Xd, Xm
        DianaLinkedOperand source = pOps[2];
        source.type = diana_register;
        if (GetArm64Value(pSession, &source, &base, &bRelative))
            SetArm64Value(pSession, pOps, base, bRelative);
        else
            DropArm64Register(pSession, pOps[0].value.recognizedRegister);
        return DI_SUCCESS;
    }
    if (id == diana_a64_cmd_ldr && pResult->iLinkedOpCount == 2 && pOps[1].type == diana_index)
    {
        if (pOps[1].value.rmIndex.reg == reg_A64_PC)
        {
            // ldr Xt, literal: the data xref is made by DispatchArm64
            base = GetArm64ModuleAddress(pSession, nextInstructionOffset + pOps[1].value.rmIndex.dispValue, &bRelative);
            return LoadArm64Value(pSession, pInstruction, pOps, base, bRelative);
        }
        if (pOps[1].arm.opKind == DIANA_A64_OPK_MEM_UOFF)
        {
            DianaLinkedOperand baseOp = pOps[1];
            baseOp.type = diana_register;
            baseOp.usedSize = 8;
            baseOp.value.recognizedRegister = pOps[1].value.rmIndex.reg;
            if (GetArm64Value(pSession, &baseOp, &base, &bRelative))
            {
                // ldr Xt, [Xn, #imm]: Xn + imm is data, the value may be an address
                DI_UINT64 address = base + pOps[1].value.rmIndex.dispValue;
                DI_CHECK(SaveDataAccess(pSession, pInstruction, address, !bRelative));
                return LoadArm64Value(pSession, pInstruction, pOps, address, bRelative);
            }
        }
    }
    // other loads and stores through a known base: the address is data
    for (i = 0; i < pResult->iLinkedOpCount; ++i)
    {
        const DianaLinkedOperand * pOp = pOps + i;
        if (pOp->type == diana_index && pOp->arm.opKind == DIANA_A64_OPK_MEM_UOFF && pOp->arm.writeback == 0)
        {
            DianaLinkedOperand baseOp = *pOp;
            baseOp.type = diana_register;
            baseOp.usedSize = 8;
            baseOp.value.recognizedRegister = pOp->value.rmIndex.reg;
            if (GetArm64Value(pSession, &baseOp, &base, &bRelative))
                DI_CHECK(SaveDataAccess(pSession, pInstruction, base + pOp->value.rmIndex.dispValue, !bRelative));
        }
    }
    // forget the registers the instruction names; a base with writeback changes too
    for (i = 0; i < pResult->iLinkedOpCount; ++i)
    {
        const DianaLinkedOperand * pOp = pOps + i;
        if (pOp->type == diana_register || pOp->type == diana_arm_shifted_reg)
            DropArm64Register(pSession, pOp->value.recognizedRegister);
        else
        if (pOp->type == diana_index && pOp->arm.writeback)
            DropArm64Register(pSession, pOp->value.rmIndex.reg);
    }
    return DI_SUCCESS;
}

/* ------------------------------------------------------------------ AArch32 (A32 and T32)
   The routes are A32 or T32 code: BLX <label> switches, BX/BLX <register> and LDR PC take the mode from bit 0
   of the address. Every instruction can be conditional (a condition field, an IT block): a branch, a return
   or a write to PC that is conditional goes on to the next instruction too. The register values use the
   AArch64 slots: R0..R14 are 0..14 */

/* how an instruction that is no branch writes PC */
#define DIANA_A32_PC_NONE           0
#define DIANA_A32_PC_RETURN         1   /* pop {pc}, ldm sp!, {..., pc}, mov pc, lr, subs pc, lr, #4 */
#define DIANA_A32_PC_JUMP           2   /* to a target that isn't known */
#define DIANA_A32_PC_LITERAL        3   /* ldr pc, [pc, #x]: the target is in a literal */
#define DIANA_A32_PC_TABLE_LOAD     4   /* ldr pc, [pc, rX, lsl #2]: a table of addresses after the instruction */
#define DIANA_A32_PC_TABLE_ADD      5   /* add pc, pc, rX, lsl #2: a table of branches after the instruction */

/* R0..R14 -> 0..14, -1 for PC and everything else */
static
int GetArm32RegisterIndex(DianaUnifiedRegister reg)
{
    if (reg >= reg_A32_R0 && reg < reg_A32_PC)
        return (int)(reg - reg_A32_R0);
    return -1;
}

/* the route that starts is A32 or T32 code: the decoder follows, outside of any IT block */
static
void SetArm32RouteMode(DianaAnalyzeSession * pSession, int bThumb)
{
    pSession->a32Thumb = bThumb;
    pSession->a32RouteThumb = bThumb;
    pSession->context.iArch = bThumb ? DIANA_ARCH_THUMB : DIANA_ARCH_ARM32;
    DianaA32_ResetItState(&pSession->context);
}

/* a register operand: the register, or a register shifted by nothing (mov r0, r1 has a shifted register) */
static
int IsArm32RegisterOperand(const DianaLinkedOperand * pOp, DianaUnifiedRegister reg)
{
    if (pOp->type == diana_arm_shifted_reg)
    {
        if (pOp->arm.qualifier == DIANA_ARM_QUAL_SHIFT_REG || pOp->arm.shiftAmount ||
            (pOp->arm.shiftType != DIANA_ARM_SHIFT_NONE && pOp->arm.shiftType != DIANA_ARM_SHIFT_LSL))
        {
            return 0;
        }
    }
    else
    if (pOp->type != diana_register)
    {
        return 0;
    }
    return reg == reg_none || pOp->value.recognizedRegister == reg;
}

/* the known value of a register operand; pRelative may be 0 */
static
int GetArm32Value(const DianaAnalyzeSession * pSession, const DianaLinkedOperand * pOp, DI_UINT64 * pValue,
                  int * pRelative)
{
    int index = -1;
    if (!IsArm32RegisterOperand(pOp, reg_none))
        return 0;
    index = GetArm32RegisterIndex(pOp->value.recognizedRegister);
    if (index < 0 || !(pSession->armKnown & (1u << index)))
        return 0;
    *pValue = pSession->armRegs[index];
    if (pRelative)
        *pRelative = (pSession->armRelative >> index) & 1;
    return 1;
}

static
void SetArm32Value(DianaAnalyzeSession * pSession, DianaUnifiedRegister reg, DI_UINT64 value, int bRelative)
{
    int index = GetArm32RegisterIndex(reg);
    if (index < 0)
        return;
    ForgetArm64Registers(pSession, 1u << index);
    pSession->armRegs[index] = value & 0xFFFFFFFFu;
    pSession->armKnown |= 1u << index;
    if (bRelative)
        pSession->armRelative |= 1u << index;
    else
        pSession->armRelative &= ~(1u << index);
}

static
void DropArm32Register(DianaAnalyzeSession * pSession, DianaUnifiedRegister reg)
{
    int index = GetArm32RegisterIndex(reg);
    if (index >= 0)
        ForgetArm64Registers(pSession, 1u << index);
}

/* the offset PC reads as: the address + 8 (A32), the address + 4 (T32) */
static
OPERAND_SIZE GetArm32PcOffset(const DianaAnalyzeSession * pSession, OPERAND_SIZE instructionOffset)
{
    return instructionOffset + (pSession->a32Thumb ? 4 : 8);
}

/* the base of a literal, [pc, #x]: PC, rounded down to 4 in T32 */
static
OPERAND_SIZE GetArm32LiteralBase(const DianaAnalyzeSession * pSession, OPERAND_SIZE instructionOffset)
{
    OPERAND_SIZE pc = GetArm32PcOffset(pSession, instructionOffset);
    if (pSession->a32Thumb)
        pc -= (pSession->params.imageBase + pc) & 3;
    return pc;
}

/* the target of a label (diana_rel), relative to the next instruction or to the address rounded down to 4 */
static
OPERAND_SIZE GetArm32LabelTarget(const DianaAnalyzeSession * pSession,
                                 const DianaLinkedOperand * pOp,
                                 OPERAND_SIZE instructionOffset,
                                 OPERAND_SIZE nextInstructionOffset)
{
    OPERAND_SIZE target = nextInstructionOffset + pOp->value.rel.m_value;
    if (pOp->arm.qualifier == DIANA_ARM_QUAL_ALIGN_PC)
        target -= (pSession->params.imageBase + instructionOffset) & 3;
    return target;
}

/* a route of the given mode */
static
int SaveArm32Route(DianaAnalyzeSession * pSession,
                   Diana_Instruction * pInstruction,
                   OPERAND_SIZE offset,
                   int bThumb,
                   int routeFlags,
                   int xrefFlags)
{
    int status = 0;
    pSession->a32RouteThumb = bThumb;
    status = SaveNewRoute(pSession, pInstruction, offset, routeFlags, xrefFlags);
    pSession->a32RouteThumb = pSession->a32Thumb;
    return status;
}

/* the words at [offset, offset + size) are data (a literal, a table): no route starts there, so the scan
   doesn't take them for code. They are rolled back instructions, as data that a strict route tried */
static
int MarkArm32Data(DianaAnalyzeSession * pSession, OPERAND_SIZE offset, OPERAND_SIZE size)
{
    OPERAND_SIZE end = 0;
    Diana_Instruction * pInstruction = 0;
    if (offset >= pSession->maxOffset)
        return DI_SUCCESS;
    end = offset + size;
    if (end > pSession->maxOffset || end < offset)
        end = pSession->maxOffset;
    offset -= (pSession->params.imageBase + offset) & 1;
    for (; offset < end; offset += 2)
    {
        if (Diana_InstructionsOwner_GetInstruction(pSession->pOwner, offset))
            continue;
        DI_CHECK(Diana_InstructionsOwner_QueryOrAllocateInstruction(pSession->pOwner,
                                                                    offset,
                                                                    &pInstruction,
                                                                    DI_INSTRUCTION_INVALID | DI_INSTRUCTION_DATA));
        /* m_actualSize counts the valid instructions */
        --pSession->pOwner->m_actualSize;
    }
    return DI_SUCCESS;
}

/* data at an offset of the module that an instruction reads: a literal or a table */
static
int SaveArm32Data(DianaAnalyzeSession * pSession,
                  Diana_Instruction * pInstruction,
                  OPERAND_SIZE offset,
                  OPERAND_SIZE size)
{
    int bRelative = 0;
    DI_UINT64 address = GetArm64ModuleAddress(pSession, offset, &bRelative);
    DI_CHECK(SaveDataAccess(pSession, pInstruction, address, !bRelative));
    return MarkArm32Data(pSession, offset, size);
}

/* an address in a register or in memory: bit 0 set is T32 code, clear is A32 code in A32 code; in T32 code
   (where function pointers have bit 0 set) an even address is data */
static
int SaveArm32AddressTaken(DianaAnalyzeSession * pSession, Diana_Instruction * pInstruction,
                          DI_UINT64 address, int bRelative)
{
    DianaAnalyzeAddressResult_type result = diaJumpInvalid;
    OPERAND_SIZE offset = 0;
    int bThumb = (int)(address & 1);
    DI_CHECK(pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver,
                                                    address & ~(DI_UINT64)1,
                                                    GetArm64AddressFlags(bRelative),
                                                    &offset,
                                                    &result));
    if (result != diaJumpNormal || offset >= pSession->maxOffset)
        return DI_SUCCESS;
    if (!bThumb && pSession->a32Thumb)
        return SaveDataXRef(pSession, pInstruction, offset, DI_XREF_ADDRESS_TAKEN);
    return SaveArm32Route(pSession, pInstruction, offset, bThumb, DI_ROUTE_QUESTIONABLE, DI_XREF_ADDRESS_TAKEN);
}

/* a branch to an address (bx r3, ldr pc, =func): bit 0 selects T32 */
static
int SaveArm32BranchTo(DianaAnalyzeSession * pSession, Diana_Instruction * pInstruction,
                      DI_UINT64 address, int bRelative)
{
    DianaAnalyzeAddressResult_type result = diaJumpInvalid;
    OPERAND_SIZE offset = 0;
    DI_CHECK(pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver,
                                                    address & ~(DI_UINT64)1,
                                                    GetArm64AddressFlags(bRelative),
                                                    &offset,
                                                    &result));
    switch (result)
    {
    case diaJumpNormal:
        return SaveArm32Route(pSession, pInstruction, offset, (int)(address & 1), pSession->curRouteInfo.flags, 0);
    case diaJumpExternal:
        return SaveNewExternalRoute(pSession, pInstruction, offset, 0, 0);
    default:
        return DI_SUCCESS;
    }
}

/* a register loaded from memory: its value, and an address taken when the value points into the module */
static
int LoadArm32Value(DianaAnalyzeSession * pSession,
                   Diana_Instruction * pInstruction,
                   DianaUnifiedRegister reg,
                   DI_UINT64 address,
                   int bRelative)
{
    DI_UINT64 value = 0;
    if (GetArm32RegisterIndex(reg) < 0)
        return DI_SUCCESS;
    /* 0 is a null pointer */
    if (!ReadArm64Entry(pSession, address, bRelative, 4, 0, &value) || !value)
    {
        DropArm32Register(pSession, reg);
        return DI_SUCCESS;
    }
    /* a value from memory is absolute */
    SetArm32Value(pSession, reg, value, 0);
    return SaveArm32AddressTaken(pSession, pInstruction, value, 0);
}

/* all zeros: andeq r0, r0, r0 (A32), movs r0, r0 (T32), what padding and empty data decode to */
static
int IsArm32ZeroWord(const DianaParserResult * pResult)
{
    int i = 0;
    for (i = 0; i < pResult->bytesCount; ++i)
    {
        if (pResult->bytes[i])
            return 0;
    }
    return pResult->bytesCount > 0;
}

/* how a word that isn't a branch writes PC, DIANA_A32_PC_* */
static
int GetArm32PcWrite(const DianaAnalyzeSession * pSession)
{
    const DianaParserResult * pResult = &pSession->result;
    const DianaLinkedOperand * pOps = pResult->linkedOperands;
    long id = (long)pResult->pInfo->m_pGroupInfo->m_commandId;
    int count = pResult->iLinkedOpCount;
    int i = 0;
    for (i = 0; i < count; ++i)
    {
        if (pOps[i].type != diana_arm_regmask || !(pOps[i].value.imm & 0x8000))
            continue;
        switch (id)
        {
        case diana_a32_cmd_pop:
            return DIANA_A32_PC_RETURN;
        case diana_a32_cmd_ldm:
        case diana_a32_cmd_ldmib:
        case diana_a32_cmd_ldmda:
        case diana_a32_cmd_ldmdb:
            /* ldm sp!, {r4, pc} returns, ldm r0, {..., pc} jumps */
            return count >= 1 && IsArm32RegisterOperand(pOps, reg_A32_SP) ? DIANA_A32_PC_RETURN : DIANA_A32_PC_JUMP;
        default:
            /* push, stm: PC is stored */
            return DIANA_A32_PC_NONE;
        }
    }
    if (count < 1 || pOps[0].type != diana_register || pOps[0].value.recognizedRegister != reg_A32_PC)
        return DIANA_A32_PC_NONE;
    switch (id)
    {
    case diana_a32_cmd_pop:
        return DIANA_A32_PC_RETURN;
    case diana_a32_cmd_ldr:
        if (count >= 2 && pOps[1].type == diana_index)
        {
            const DianaRmIndex * pIndex = &pOps[1].value.rmIndex;
            /* ldr pc, [sp], #4 */
            if (pIndex->reg == reg_A32_SP && pOps[1].arm.writeback == DIANA_ARM_WRITEBACK_POST)
                return DIANA_A32_PC_RETURN;
            if (pIndex->reg == reg_A32_PC && pOps[1].arm.writeback == DIANA_ARM_WRITEBACK_NONE)
                return pIndex->indexed_reg == reg_none ? DIANA_A32_PC_LITERAL : DIANA_A32_PC_TABLE_LOAD;
        }
        return DIANA_A32_PC_JUMP;
    case diana_a32_cmd_mov:
    case diana_a32_cmd_movs:
    case diana_a32_cmd_sub:
    case diana_a32_cmd_subs:
        /* mov pc, lr; subs pc, lr, #4 (an exception return) */
        if (count >= 2 && IsArm32RegisterOperand(pOps + 1, reg_A32_LR))
            return DIANA_A32_PC_RETURN;
        return DIANA_A32_PC_JUMP;
    case diana_a32_cmd_add:
    case diana_a32_cmd_adds:
        if (count == 3 && IsArm32RegisterOperand(pOps + 1, reg_A32_PC) && pOps[2].type == diana_arm_shifted_reg)
            return DIANA_A32_PC_TABLE_ADD;
        return DIANA_A32_PC_JUMP;
    case diana_a32_cmd_ldrb: case diana_a32_cmd_ldrh: case diana_a32_cmd_ldrsb: case diana_a32_cmd_ldrsh:
    case diana_a32_cmd_mvn: case diana_a32_cmd_mvns: case diana_a32_cmd_orr: case diana_a32_cmd_orrs:
    case diana_a32_cmd_and: case diana_a32_cmd_ands: case diana_a32_cmd_eor: case diana_a32_cmd_eors:
    case diana_a32_cmd_bic: case diana_a32_cmd_bics: case diana_a32_cmd_rsb: case diana_a32_cmd_rsbs:
    case diana_a32_cmd_adc: case diana_a32_cmd_adcs: case diana_a32_cmd_sbc: case diana_a32_cmd_sbcs:
    case diana_a32_cmd_rsc:
        return DIANA_A32_PC_JUMP;
    }
    return DIANA_A32_PC_NONE;
}

/* the bound of the index register of a table: the instruction's own condition after cmp (ldrls pc, ...),
   or a bound the route has (cmp r0, #3; bhi default; tbb [pc, r0]) */
static
int GetArm32TableCount(const DianaAnalyzeSession * pSession, DianaUnifiedRegister indexReg, DI_UINT64 * pCount)
{
    int index = GetArm32RegisterIndex(indexReg);
    int cond = DianaA32_GetCondition(&pSession->result);
    if (index < 0)
        return 0;
    if (cond != DIANA_A32_COND_AL && pSession->armCmpReg == index &&
        GetArmCompareBound(pSession, cond, 1, pCount))
    {
        return *pCount != 0;
    }
    if (pSession->armBoundCount && pSession->armBoundReg == index)
    {
        *pCount = pSession->armBoundCount;
        return 1;
    }
    return 0;
}

/* tbb [pc, rX], tbh [pc, rX, lsl #1]: a table of byte or halfword offsets after the instruction,
   the target is PC + 2 * entry */
static
int SaveArm32BranchTable(DianaAnalyzeSession * pSession,
                         Diana_Instruction * pInstruction,
                         OPERAND_SIZE instructionOffset,
                         int entrySize)
{
    const DianaLinkedOperand * pOp = pSession->result.linkedOperands;
    OPERAND_SIZE base = 0;
    DI_UINT64 count = 0;
    DI_UINT64 i = 0;
    if (pSession->result.iLinkedOpCount < 1 || pOp->type != diana_index || pOp->value.rmIndex.reg != reg_A32_PC ||
        !GetArm32TableCount(pSession, pOp->value.rmIndex.indexed_reg, &count))
    {
        return DI_SUCCESS;
    }
    if (count > DIANA_ARM64_MAX_JUMP_TABLE)
        count = DIANA_ARM64_MAX_JUMP_TABLE;
    base = GetArm32PcOffset(pSession, instructionOffset);
    DI_CHECK(SaveArm32Data(pSession, pInstruction, base, count * entrySize));
    for (i = 0; i < count; ++i)
    {
        DI_UINT64 entry = 0;
        int bRelative = 0;
        DI_UINT64 address = GetArm64ModuleAddress(pSession, base + i * entrySize, &bRelative);
        if (!ReadArm64Entry(pSession, address, bRelative, entrySize, 0, &entry))
            break;
        DI_CHECK(SaveArm32Route(pSession, pInstruction, base + 2 * entry, 1, pSession->curRouteInfo.flags, 0));
    }
    return DI_SUCCESS;
}

/* A32 ldr pc, [pc, rX, lsl #2] (addresses) and add pc, pc, rX, lsl #2 (branches): the table is right after,
   at PC */
static
int SaveArm32PcTable(DianaAnalyzeSession * pSession,
                     Diana_Instruction * pInstruction,
                     OPERAND_SIZE instructionOffset,
                     int bAddresses)
{
    const DianaLinkedOperand * pOp = pSession->result.linkedOperands + (bAddresses ? 1 : 2);
    DianaUnifiedRegister indexReg = reg_none;
    OPERAND_SIZE base = GetArm32PcOffset(pSession, instructionOffset);
    DI_UINT64 count = 0;
    DI_UINT64 i = 0;
    if (bAddresses)
    {
        if (pOp->arm.qualifier == DIANA_ARM_QUAL_NEGATIVE || pOp->arm.shiftType != DIANA_ARM_SHIFT_LSL ||
            pOp->arm.shiftAmount != 2)
        {
            return DI_SUCCESS;
        }
        indexReg = pOp->value.rmIndex.indexed_reg;
    }
    else
    {
        if (pOp->arm.qualifier == DIANA_ARM_QUAL_SHIFT_REG || pOp->arm.shiftType != DIANA_ARM_SHIFT_LSL ||
            pOp->arm.shiftAmount != 2)
        {
            return DI_SUCCESS;
        }
        indexReg = pOp->value.recognizedRegister;
    }
    if (!GetArm32TableCount(pSession, indexReg, &count))
        return DI_SUCCESS;
    if (count > DIANA_ARM64_MAX_JUMP_TABLE)
        count = DIANA_ARM64_MAX_JUMP_TABLE;
    if (bAddresses)
        DI_CHECK(SaveArm32Data(pSession, pInstruction, base, count * 4));
    for (i = 0; i < count; ++i)
    {
        if (bAddresses)
        {
            DI_UINT64 entry = 0;
            int bRelative = 0;
            DI_UINT64 address = GetArm64ModuleAddress(pSession, base + i * 4, &bRelative);
            if (!ReadArm64Entry(pSession, address, bRelative, 4, 0, &entry))
                break;
            DI_CHECK(SaveArm32BranchTo(pSession, pInstruction, entry, 0));
        }
        else
        {
            DI_CHECK(SaveArm32Route(pSession, pInstruction, base + i * 4, 0, pSession->curRouteInfo.flags, 0));
        }
    }
    return DI_SUCCESS;
}

/* the operands of any instruction: ADR takes an address, [pc, #x] reads a literal */
static
int DispatchArm32(DianaAnalyzeSession * pSession,
                  Diana_Instruction * pInstruction,
                  OPERAND_SIZE instructionOffset,
                  OPERAND_SIZE nextInstructionOffset)
{
    const DianaParserResult * pResult = &pSession->result;
    long id = (long)pResult->pInfo->m_pGroupInfo->m_commandId;
    int i = 0;
    for (i = 0; i < pResult->iLinkedOpCount; ++i)
    {
        const DianaLinkedOperand * pOp = pResult->linkedOperands + i;
        if (pOp->type == diana_rel && id == diana_a32_cmd_adr)
        {
            OPERAND_SIZE target = GetArm32LabelTarget(pSession, pOp, instructionOffset, nextInstructionOffset);
            pInstruction->m_flags |= DI_INSTRUCTION_USES_RIP;
            DI_CHECK(SaveArm32Route(pSession, pInstruction, target, pSession->a32Thumb,
                                    DI_ROUTE_QUESTIONABLE, DI_XREF_ADDRESS_TAKEN));
        }
        else
        if (pOp->type == diana_index && pOp->value.rmIndex.reg == reg_A32_PC &&
            pOp->value.rmIndex.indexed_reg == reg_none && pOp->arm.writeback == DIANA_ARM_WRITEBACK_NONE &&
            id != diana_a32_cmd_pld)
        {
            /* ldr r0, [pc, #8]: the literal is data; ldrd and vldr d0 read 8 bytes */
            OPERAND_SIZE offset = GetArm32LiteralBase(pSession, instructionOffset) + pOp->value.rmIndex.dispValue;
            pInstruction->m_flags |= DI_INSTRUCTION_USES_RIP;
            DI_CHECK(SaveArm32Data(pSession, pInstruction, offset, id == diana_a32_cmd_ldrd ? 8 : 4));
        }
    }
    return DI_SUCCESS;
}

/* an AArch32 instruction: its operands, then where it goes */
static
int AnalyzeArm32Instruction(DianaAnalyzeSession * pSession,
                            Diana_Instruction * pInstruction,
                            OPERAND_SIZE instructionOffset,
                            OPERAND_SIZE nextInstructionOffset,
                            int * pbNeedReset)
{
    const DianaParserResult * pResult = &pSession->result;
    const DianaLinkedOperand * pOps = pResult->linkedOperands;
    const Diana_LinkedAdditionalGroupInfo * pLinkedInfo = pResult->pInfo->m_pGroupInfo->m_pLinkedInfo;
    long id = (long)pResult->pInfo->m_pGroupInfo->m_commandId;
    int cond = DianaA32_GetCondition(pResult);
    int bConditional = cond != DIANA_A32_COND_AL;
    int pcWrite = pLinkedInfo ? DIANA_A32_PC_NONE : GetArm32PcWrite(pSession);
    int i = 0;

    DI_CHECK(DispatchArm32(pSession, pInstruction, instructionOffset, nextInstructionOffset));
    if (!pLinkedInfo && pcWrite == DIANA_A32_PC_NONE)
        return DI_SUCCESS;

    pInstruction->m_flags |= DI_INSTRUCTION_CAN_CHANGE_CSIP;
    /* a condition that fails goes on to the next instruction */
    if (!bConditional && !(pLinkedInfo && (pLinkedInfo->flags & DIANA_GT_CAN_GO_TO_THE_NEXT_INSTRUCTION)))
        *pbNeedReset = 1;

    if (pLinkedInfo)
    {
        if (pLinkedInfo->flags & DIANA_GT_STOP)
            return DI_SUCCESS;
        if (pLinkedInfo->flags & DIANA_GT_RET)
        {
            /* eret */
            if (!bConditional)
                pInstruction->m_flags |= DI_INSTRUCTION_IS_RET;
            return DI_SUCCESS;
        }
        if (id == diana_a32_cmd_tbb || id == diana_a32_cmd_tbh)
            return SaveArm32BranchTable(pSession, pInstruction, instructionOffset, id == diana_a32_cmd_tbh ? 2 : 1);
        if (pResult->iLinkedOpCount >= 1 && pOps[0].type == diana_register)
        {
            /* bx lr returns; bx r3, blx r3 go where r3 points */
            DI_UINT64 value = 0;
            int bRelative = 0;
            if (id == diana_a32_cmd_bx && pOps[0].value.recognizedRegister == reg_A32_LR)
            {
                if (!bConditional)
                    pInstruction->m_flags |= DI_INSTRUCTION_IS_RET;
                return DI_SUCCESS;
            }
            if (GetArm32Value(pSession, pOps, &value, &bRelative))
                return SaveArm32BranchTo(pSession, pInstruction, value, bRelative);
            return DI_SUCCESS;
        }
        /* b, bl, blx, cbz, cbnz: the label */
        for (i = 0; i < pResult->iLinkedOpCount; ++i)
        {
            if (pOps[i].type == diana_rel)
            {
                OPERAND_SIZE target = GetArm32LabelTarget(pSession, pOps + i, instructionOffset, nextInstructionOffset);
                OPERAND_SIZE offset = 0;
                DianaAnalyzeAddressResult_type result = diaJumpInvalid;
                /* blx <label> switches between A32 and T32 */
                int bThumb = id == diana_a32_cmd_blx ? !pSession->a32Thumb : pSession->a32Thumb;
                int status = 0;
                DI_CHECK(pSession->pObserver->m_pAnalyzeAddress(pSession->pObserver, target, 0, &offset, &result));
                if (result == diaJumpExternal)
                    return SaveNewExternalRoute(pSession, pInstruction, offset, 0, 0);
                if (result != diaJumpNormal)
                    return DI_SUCCESS;
                /* cmp r0, #10; bls <here> bounds r0 there */
                if (id == diana_a32_cmd_b && bConditional &&
                    GetArmCompareBound(pSession, cond, 1, &pSession->armRouteBoundCount))
                {
                    pSession->armRouteBoundReg = pSession->armCmpReg;
                }
                status = SaveArm32Route(pSession, pInstruction, offset, bThumb, pSession->curRouteInfo.flags, 0);
                pSession->armRouteBoundReg = -1;
                pSession->armRouteBoundCount = 0;
                return status;
            }
        }
        return DI_SUCCESS;
    }

    switch (pcWrite)
    {
    case DIANA_A32_PC_RETURN:
        if (!bConditional)
            pInstruction->m_flags |= DI_INSTRUCTION_IS_RET;
        break;
    case DIANA_A32_PC_LITERAL:
        {
            /* ldr pc, =func (a veneer): the literal holds the target */
            DI_UINT64 target = 0;
            int bRelative = 0;
            DI_UINT64 address = GetArm64ModuleAddress(pSession,
                                                      GetArm32LiteralBase(pSession, instructionOffset) +
                                                      pOps[1].value.rmIndex.dispValue,
                                                      &bRelative);
            if (ReadArm64Entry(pSession, address, bRelative, 4, 0, &target))
                DI_CHECK(SaveArm32BranchTo(pSession, pInstruction, target, 0));
        }
        break;
    case DIANA_A32_PC_TABLE_LOAD:
        return SaveArm32PcTable(pSession, pInstruction, instructionOffset, 1);
    case DIANA_A32_PC_TABLE_ADD:
        return SaveArm32PcTable(pSession, pInstruction, instructionOffset, 0);
    default:
        break;
    }
    return DI_SUCCESS;
}

/* AArch32: is the decoded word an instruction here? Sets pSession->instructionFlags */
static
int CheckArm32Word(DianaAnalyzeSession * pSession)
{
    const DianaA32CmdInfo * pCmd = DianaA32_GetCmdInfo(&pSession->result);
    pSession->instructionFlags = pSession->a32Thumb ? DI_INSTRUCTION_THUMB : 0;
    switch (DianaA32_GetWordClass(&pSession->result))
    {
    case DIANA_A64_WORD_SUPPORTED:
        break;
    case DIANA_A64_WORD_UNSUPPORTED:
        /* valid, not decoded yet: never a reason to stop */
        pSession->instructionFlags |= DI_INSTRUCTION_UNSUPPORTED;
        return 1;
    default:
        return 0;
    }
    /* zeros are data, unless a root route says they are code */
    if (IsArm32ZeroWord(&pSession->result) && IsStrictRoute(pSession))
        return 0;
    if (pSession->params.pArmProfile && !DianaA32_IsCmdEnabled(pCmd, pSession->params.pArmProfile->a32Features))
        pSession->instructionFlags |= DI_INSTRUCTION_OUTSIDE_PROFILE;
    return 1;
}

/* the instruction may change the flags (a branch or a call: the compare is gone after it too) */
static
int ChangesArm32Flags(const DianaAnalyzeSession * pSession,
                      const Diana_LinkedAdditionalGroupInfo * pLinkedInfo,
                      const Diana_Instruction * pInstruction)
{
    const DianaA32CmdInfo * pCmd = DianaA32_GetCmdInfo(&pSession->result);
    const char * pName = pSession->result.pInfo->m_pGroupInfo->m_pName;
    int length = 0;
    if (pLinkedInfo || !pCmd || (pInstruction->m_flags & DI_INSTRUCTION_UNSUPPORTED))
        return 1;
    switch ((long)pSession->result.pInfo->m_pGroupInfo->m_commandId)
    {
    case diana_a32_cmd_cmp: case diana_a32_cmd_cmn: case diana_a32_cmd_tst: case diana_a32_cmd_teq:
    case diana_a32_cmd_msr:
        return 1;
    }
    /* a 16-bit T32 adds is "add" in the tables and sets the flags outside an IT block */
    if ((pCmd->flags & DIANA_A32_CMDF_S_OUTSIDE_IT) && !(pSession->result.iPrefix & DIANA_A32_PREFIX_IN_IT))
        return 1;
    /* adds, subs, movs, lsls, muls... */
    while (pName[length])
        ++length;
    return length > 1 && pName[length - 1] == 's';
}

/* updates the known registers after an instruction and saves the references it makes:
   movw/movt, mov rd, #imm; ldr rd, [pc, #x]; ldr rd, [rn, #imm]; adr; add rd, pc (PIC); add rd, rn, #imm;
   mov rd, rm. Everything else forgets the registers it names, like TrackArm64 */
static
int TrackArm32(DianaAnalyzeSession * pSession,
               Diana_Instruction * pInstruction,
               OPERAND_SIZE instructionOffset,
               OPERAND_SIZE nextInstructionOffset)
{
    const DianaParserResult * pResult = &pSession->result;
    const DianaLinkedOperand * pOps = pResult->linkedOperands;
    long id = (long)pResult->pInfo->m_pGroupInfo->m_commandId;
    const Diana_LinkedAdditionalGroupInfo * pLinkedInfo = pResult->pInfo->m_pGroupInfo->m_pLinkedInfo;
    int count = pResult->iLinkedOpCount;
    int cond = DianaA32_GetCondition(pResult);
    int cmpReg = pSession->armCmpReg;
    DI_UINT64 value = 0;
    DI_UINT64 base = 0;
    DI_UINT64 bound = 0;
    int bRelative = 0;
    int i = 0;

    if (!pInstruction)
        return DI_SUCCESS;
    /* cmp r0, #3; bhi default: the bound on the next instruction */
    if (id == diana_a32_cmd_b && cond != DIANA_A32_COND_AL && GetArmCompareBound(pSession, cond, 0, &bound))
    {
        pSession->armBoundReg = cmpReg;
        pSession->armBoundCount = bound;
    }
    /* the compare holds until the flags change */
    if (ChangesArm32Flags(pSession, pLinkedInfo, pInstruction))
        pSession->armCmpReg = -1;

    if (id == diana_a32_cmd_cmp && count == 2 && cond == DIANA_A32_COND_AL &&
        IsArm32RegisterOperand(pOps, reg_none) && GetArm32RegisterIndex(pOps[0].value.recognizedRegister) >= 0 &&
        pOps[1].type == diana_imm)
    {
        /* cmp rn, #imm: only the flags change */
        pSession->armCmpReg = GetArm32RegisterIndex(pOps[0].value.recognizedRegister);
        pSession->armCmpImm = pOps[1].value.imm;
        return DI_SUCCESS;
    }
    if (pInstruction->m_flags & DI_INSTRUCTION_UNSUPPORTED)
    {
        ForgetArm64Registers(pSession, 0x7FFFu);
        return DI_SUCCESS;
    }
    if (pLinkedInfo && (pLinkedInfo->flags & DIANA_GT_IS_CALL))
    {
        /* the callee may change r0..r3, r12 and lr */
        ForgetArm64Registers(pSession, 0x500Fu);
        return DI_SUCCESS;
    }
    if (cond == DIANA_A32_COND_AL && count >= 2 && pOps[0].type == diana_register &&
        GetArm32RegisterIndex(pOps[0].value.recognizedRegister) >= 0)
    {
        DianaUnifiedRegister rd = pOps[0].value.recognizedRegister;
        if (id == diana_a32_cmd_mov && count == 2 && pOps[1].type == diana_imm)
        {
            /* mov rd, #imm, movw rd, #imm: a constant, the low half of an address */
            SetArm32Value(pSession, rd, pOps[1].value.imm, 0);
            return DI_SUCCESS;
        }
        if (id == diana_a32_cmd_movt && count == 2 && pOps[1].type == diana_imm)
        {
            /* movt rd, #imm: the high half */
            if (!GetArm32Value(pSession, pOps, &base, &bRelative))
            {
                DropArm32Register(pSession, rd);
                return DI_SUCCESS;
            }
            value = (base & 0xFFFF) | ((pOps[1].value.imm & 0xFFFF) << 16);
            SetArm32Value(pSession, rd, value, 0);
            return SaveArm32AddressTaken(pSession, pInstruction, value, 0);
        }
        if (id == diana_a32_cmd_adr && count == 2 && pOps[1].type == diana_rel)
        {
            /* the reference is made by DispatchArm32 */
            base = GetArm64ModuleAddress(pSession,
                                         GetArm32LabelTarget(pSession, pOps + 1, instructionOffset, nextInstructionOffset),
                                         &bRelative);
            SetArm32Value(pSession, rd, base, bRelative);
            return DI_SUCCESS;
        }
        if (id == diana_a32_cmd_ldr && count == 2 && pOps[1].type == diana_index &&
            pOps[1].value.rmIndex.indexed_reg == reg_none && pOps[1].arm.writeback == DIANA_ARM_WRITEBACK_NONE)
        {
            if (pOps[1].value.rmIndex.reg == reg_A32_PC)
            {
                /* ldr rd, [pc, #x]: the data xref is made by DispatchArm32 */
                base = GetArm64ModuleAddress(pSession,
                                             GetArm32LiteralBase(pSession, instructionOffset) + pOps[1].value.rmIndex.dispValue,
                                             &bRelative);
                return LoadArm32Value(pSession, pInstruction, rd, base, bRelative);
            }
            else
            {
                DianaLinkedOperand baseOp = pOps[1];
                baseOp.type = diana_register;
                baseOp.value.recognizedRegister = pOps[1].value.rmIndex.reg;
                if (GetArm32Value(pSession, &baseOp, &base, &bRelative))
                {
                    /* ldr rd, [rn, #imm]: rn + imm is data, the value may be an address */
                    DI_UINT64 address = (base + pOps[1].value.rmIndex.dispValue) & 0xFFFFFFFFu;
                    DI_CHECK(SaveDataAccess(pSession, pInstruction, address, !bRelative));
                    return LoadArm32Value(pSession, pInstruction, rd, address, bRelative);
                }
            }
        }
        if (id == diana_a32_cmd_add && (count == 2 || count == 3))
        {
            /* add r3, pc (T32 PIC), add r3, pc, r3 (A32), add rd, rn, #imm */
            const DianaLinkedOperand * pLeft = count == 2 ? pOps : pOps + 1;
            const DianaLinkedOperand * pRight = count == 2 ? pOps + 1 : pOps + 2;
            const DianaLinkedOperand * pOther = 0;
            if (IsArm32RegisterOperand(pRight, reg_A32_PC))
                pOther = pLeft;
            else
            if (IsArm32RegisterOperand(pLeft, reg_A32_PC))
                pOther = pRight;
            if (pOther && GetArm32Value(pSession, pOther, &value, 0))
            {
                base = GetArm64ModuleAddress(pSession, GetArm32PcOffset(pSession, instructionOffset), &bRelative);
                value = (value + base) & 0xFFFFFFFFu;
                SetArm32Value(pSession, rd, value, bRelative);
                return SaveArm32AddressTaken(pSession, pInstruction, value, bRelative);
            }
            if (!pOther && pRight->type == diana_imm && GetArm32Value(pSession, pLeft, &base, &bRelative))
            {
                value = (base + pRight->value.imm) & 0xFFFFFFFFu;
                SetArm32Value(pSession, rd, value, bRelative);
                return SaveArm32AddressTaken(pSession, pInstruction, value, bRelative);
            }
        }
        if (id == diana_a32_cmd_mov && count == 2 && IsArm32RegisterOperand(pOps + 1, reg_none))
        {
            /* mov rd, rm */
            if (GetArm32Value(pSession, pOps + 1, &base, &bRelative))
                SetArm32Value(pSession, rd, base, bRelative);
            else
                DropArm32Register(pSession, rd);
            return DI_SUCCESS;
        }
    }
    /* loads and stores through a known base: the address is data */
    for (i = 0; i < count; ++i)
    {
        const DianaLinkedOperand * pOp = pOps + i;
        if (pOp->type == diana_index && pOp->value.rmIndex.reg != reg_A32_PC &&
            pOp->value.rmIndex.indexed_reg == reg_none && pOp->arm.writeback == DIANA_ARM_WRITEBACK_NONE)
        {
            DianaLinkedOperand baseOp = *pOp;
            baseOp.type = diana_register;
            baseOp.value.recognizedRegister = pOp->value.rmIndex.reg;
            if (GetArm32Value(pSession, &baseOp, &base, &bRelative))
                DI_CHECK(SaveDataAccess(pSession, pInstruction, (base + pOp->value.rmIndex.dispValue) & 0xFFFFFFFFu,
                                        !bRelative));
        }
    }
    /* forget the registers the instruction names, a list (ldm, pop), a base with writeback */
    for (i = 0; i < count; ++i)
    {
        const DianaLinkedOperand * pOp = pOps + i;
        if (pOp->type == diana_register || pOp->type == diana_arm_shifted_reg)
            DropArm32Register(pSession, pOp->value.recognizedRegister);
        else
        if (pOp->type == diana_arm_regmask)
            ForgetArm64Registers(pSession, (DI_UINT32)(pOp->value.imm & 0x7FFF));
        else
        if (pOp->type == diana_index && pOp->arm.writeback)
            DropArm32Register(pSession, pOp->value.rmIndex.reg);
    }
    return DI_SUCCESS;
}
#endif

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

    pInstruction->m_flags = pSession->instructionFlags;

    if (!(pSession->curRouteInfo.flags & DI_ROUTE_QUESTIONABLE))
    {
        pInstruction->m_flags |= DI_INSTRUCTION_ROOT;
    }

#ifndef DIANA_NO_ARM
    if (IsArm32(pSession))
    {
        return AnalyzeArm32Instruction(pSession, pInstruction, instructionOffset, nextInstructionOffset, pbNeedReset);
    }
#endif
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

    // brk, hlt, udf: no next instruction and no target
    if (pLinkedInfo->flags & DIANA_GT_STOP)
    {
        *pbNeedReset = 1;
        return DI_SUCCESS;
    }

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
    iRes = AnalyzeJumps(&pSession->result,
                        pLinkedInfo, 
                        &newOffset,
                        nextInstructionOffset,
                        &bAbsoluteAddress,
                        &bLinksToData);
    
    if (iRes == DI_UNSUPPORTED_COMMAND)
    {
#ifndef DIANA_NO_ARM
		int bRelative = 0;
        // a switch: br through a jump table
        if (IsArm64(pSession) && IsArm64JumpTable(pSession))
            return SaveArm64JumpTable(pSession, pInstruction);
        // br x16 after adrp x16; ldr x16, [x16, #off]: an import thunk
        if (!IsArm64(pSession) || !GetArm64Target(pSession, &newOffset, &bRelative))
            return DI_SUCCESS;
        bAbsoluteAddress = !bRelative;
        iRes = DI_SUCCESS;
#else
        return DI_SUCCESS;
#endif
    }

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
        // save new route; AArch64: cmp w0, #10; b.ls <here> bounds w0 there
#ifndef DIANA_NO_ARM
        if (IsArm64(pSession) && GetArm64BranchBound(pSession, 1, &pSession->armRouteBoundCount))
            pSession->armRouteBoundReg = pSession->armCmpReg;
#endif
        DI_CHECK(SaveNewRoute(pSession,
                              pInstruction,
                              newOffset,
                              pSession->curRouteInfo.flags,
                              0));
#ifndef DIANA_NO_ARM
        pSession->armRouteBoundReg = -1;
        pSession->armRouteBoundCount = 0;
#endif
        break;
    case diaJumpExternal:
        DI_CHECK(SaveNewExternalRoute(pSession,
                                      pInstruction,
                                      newOffset,
                                      bLinksToData,
                                      0));
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
    ResetArm64State(pSession, pCurRouteInfo);
#ifndef DIANA_NO_ARM
    if (IsArm32(pSession))
        SetArm32RouteMode(pSession, pCurRouteInfo->armThumb);
#endif
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

typedef struct _DianaRollbackContext
{
    DianaAnalyzeSession * pSession;
    int status;
}DianaRollbackContext;

// an address taken from outside the rolled back route stays as a data xref: the reference is real,
// only the target is not code
static
int AddressTakenKeeper(Diana_FList_ListNode * pNode,
                       void * pContext,
                       int * pbDone)
{
    DianaRollbackContext * pRollback = pContext;
    Diana_XRef * pXRef = Diana_CastXREF(pNode, 0);
    Diana_Instruction * pSource = pXRef->m_subrefs[0].m_pInstruction;
    *pbDone = 0;
    if (!(pXRef->m_flags & DI_XREF_ADDRESS_TAKEN) ||
        (pXRef->m_flags & DI_XREF_DATA) ||
        (pSource->m_flags & DI_INSTRUCTION_INVALID))
    {
        return DI_SUCCESS;
    }
    pRollback->status = SaveDataXRef(pRollback->pSession,
                                     pSource,
                                     pXRef->m_subrefs[1].m_pInstruction->m_offset,
                                     DI_XREF_ADDRESS_TAKEN);
    *pbDone = pRollback->status != DI_SUCCESS;
    return pRollback->status;
}

static
int AddressTakenRouteKeeper(Diana_FList_ListNode * pNode,
                            void * pContext,
                            int * pbDone)
{
    Diana_Instruction * pInstruction = (Diana_Instruction * )pNode;
    DianaRollbackContext * pRollback = pContext;
    Diana_FList_ListForEach(&pInstruction->m_referencesToThisInstruction,
                            AddressTakenKeeper,
                            pRollback);
    *pbDone = pRollback->status != DI_SUCCESS;
    return pRollback->status;
}

static
int RollbackCurrentRoute(DianaAnalyzeSession * pSession)
{
    DianaRollbackContext rollback;
    int bDone = 0;
    // SaveNewRoute made the first instruction; it is still loading if the route failed on it
    Diana_Instruction * pStart = Diana_InstructionsOwner_GetInstruction(pSession->pOwner,
                                                                         pSession->curRouteInfo.startOffset);
    if (pStart && !(pStart->m_flags & DI_INSTRUCTION_IS_LOADING))
        pStart = 0;

    rollback.pSession = pSession;
    rollback.status = DI_SUCCESS;
    Diana_FList_ListForEach(&pSession->curRouteInfo.instructions,
                      RouteMarker,
                      pSession);
    if (pStart)
        RouteMarker(&pStart->m_routeEntry, pSession, &bDone);
    // after all of the route is invalid, so a source inside the route is skipped
    Diana_FList_ListForEach(&pSession->curRouteInfo.instructions,
                            AddressTakenRouteKeeper,
                            &rollback);
    if (pStart && rollback.status == DI_SUCCESS)
        AddressTakenRouteKeeper(&pStart->m_routeEntry, &rollback, &bDone);
    return rollback.status;
}

// a route that has to prove it is code: questionable (an address taken), or DI_ANALYSE_BREAK_ON_INVALID_CMD
// (a pointer found in data); it is rolled back at the first word that isn't code
static
int IsStrictRoute(const DianaAnalyzeSession * pSession)
{
    return (pSession->curRouteInfo.flags & DI_ROUTE_QUESTIONABLE) || (pSession->flags & DI_ANALYSE_BREAK_ON_INVALID_CMD);
}

#ifndef DIANA_NO_ARM
// AArch64: is the decoded word an instruction here? Sets pSession->instructionFlags
static
int CheckArm64Word(DianaAnalyzeSession * pSession)
{
    const DianaA64CmdInfo * pCmd = DianaA64_GetCmdInfo(&pSession->result);
    pSession->instructionFlags = 0;
    switch (DianaA64_GetWordClass(&pSession->result))
    {
    case DIANA_A64_WORD_SUPPORTED:
        break;
    case DIANA_A64_WORD_UNSUPPORTED:
        // valid, not decoded yet: never a reason to stop
        pSession->instructionFlags |= DI_INSTRUCTION_UNSUPPORTED;
        return 1;
    default:
        return 0;
    }
    // udf #0 is what zeros decode to: data, unless a root route says it's code
    if (pCmd->header.m_pGroupInfo->m_commandId == (DianaCommandId)diana_a64_cmd_udf && IsStrictRoute(pSession))
    {
        return 0;
    }
    if (pSession->params.pArmProfile && !DianaA64_IsCmdEnabled(pCmd, pSession->params.pArmProfile->features))
    {
        pSession->instructionFlags |= DI_INSTRUCTION_OUTSIDE_PROFILE;
    }
    return 1;
}
#endif

static
int Diana_AnalyzeCodeImpl(DianaAnalyzeSession * pSession,
                          OPERAND_SIZE initialOffset)
{
    int iRes = 0;
    OPERAND_SIZE offset = initialOffset;
    OPERAND_SIZE prevOffset = 0;
    
    int bNeedReset = 0;
    Diana_InitContextEx(&pSession->context, pSession->mode, pSession->params.arch);
    Diana_Stack_Clear(pSession->pStack);

    // init cur route
    ResetArm64State(pSession, 0);
#ifndef DIANA_NO_ARM
    if (IsArm32(pSession))
        SetArm32RouteMode(pSession, pSession->params.arch == DIANA_ARCH_THUMB);
#endif
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
        pSession->instructionFlags = 0;

        if (IsArm(pSession) && ((pSession->params.imageBase + offset) & (GetRouteAlignment(pSession) - 1)))
        {
            // not an instruction address
            if (pSession->flags & DI_ANALYSE_BREAK_ON_INVALID_CMD ||
                pSession->curRouteInfo.flags & DI_ROUTE_QUESTIONABLE)
            {
                DI_CHECK(RollbackCurrentRoute(pSession));
            }
            bNeedReset = 1;
            continue;
        }

        iRes = Diana_ParseCmd(&pSession->context,
                              Diana_GetRootLine(),
                              &pSession->pObserver->m_pStream->parent,
                              &pSession->result);

        offset += pSession->result.iFullCmdSize;
#ifndef DIANA_NO_ARM
        if (!iRes && IsArm64(pSession) && !CheckArm64Word(pSession))
        {
            iRes = DI_INVALID_OPCODE;
        }
        if (!iRes && IsArm32(pSession) && !CheckArm32Word(pSession))
        {
            iRes = DI_INVALID_OPCODE;
        }
#endif
        if (iRes)
        {
            // instruction not recognized properly
            if (pSession->flags & DI_ANALYSE_BREAK_ON_INVALID_CMD ||
                pSession->curRouteInfo.flags & DI_ROUTE_QUESTIONABLE)
            {
                DI_CHECK(RollbackCurrentRoute(pSession));
                bNeedReset = 1;
                continue;
            }

            // AArch64 instructions are 4 bytes
            offset = prevOffset + GetRouteAlignment(pSession);
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
                        DI_CHECK(RollbackCurrentRoute(pSession));
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
#ifndef DIANA_NO_ARM
        if (IsArm64(pSession))
        {
            DI_CHECK(TrackArm64(pSession,
                                Diana_InstructionsOwner_GetInstruction(pSession->pOwner, prevOffset),
                                prevOffset,
                                offset));
        }
        if (IsArm32(pSession))
        {
            DI_CHECK(TrackArm32(pSession,
                                Diana_InstructionsOwner_GetInstruction(pSession->pOwner, prevOffset),
                                prevOffset,
                                offset));
        }
#endif
        if (offset >= pSession->maxOffset)
        {
            // ARM: a questionable or strict route that runs to the end without a terminator is no code
            if (IsArm(pSession) && !bNeedReset && IsStrictRoute(pSession))
            {
                DI_CHECK(RollbackCurrentRoute(pSession));
            }
            bNeedReset = 1;
            continue;
        }
    }
    return DI_SUCCESS;
}

void DianaAnalyzeParams_Init(DianaAnalyzeParams * pParams, int arch)
{
    DIANA_MEMSET(pParams, 0, sizeof(*pParams));
    pParams->arch = arch;
}

int Diana_AnalyzeCodeEx2(Diana_InstructionsOwner * pOwner,
                         DianaAnalyzeObserver * pObserver,
                         int mode,
                         OPERAND_SIZE initialOffset,
                         OPERAND_SIZE maxOffset,
                         Diana_Stack * pStack,
                         int flags,
                         const DianaAnalyzeParams * pParams)
{
    DianaAnalyzeSession session;
    DIANA_MEMSET(&session, 0, sizeof(session));

    session.maxOffset = maxOffset;
    session.pOwner = pOwner;
    session.pObserver = pObserver;
    session.mode = mode;
    session.pStack = pStack;
    session.flags = flags;
    if (pParams)
        session.params = *pParams;
    else
        DianaAnalyzeParams_Init(&session.params, DIANA_ARCH_X86);
#ifndef DIANA_NO_ARM
    if (session.params.arch != DIANA_ARCH_X86 && session.params.arch != DIANA_ARCH_AARCH64 &&
        session.params.arch != DIANA_ARCH_ARM32 && session.params.arch != DIANA_ARCH_THUMB)
#else
    if (session.params.arch != DIANA_ARCH_X86)
#endif
        return DI_UNSUPPORTED;
        
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

// HINT #34, #36, #38
#define DIANA_A64_BTI_C     0xd503245fu
#define DIANA_A64_BTI_J     0xd503249fu
#define DIANA_A64_BTI_JC    0xd50324dfu

int Diana_AnalyzeLandingPads(Diana_InstructionsOwner * pOwner,
                             DianaAnalyzeObserver * pObserver,
                             int mode,
                             OPERAND_SIZE startOffset,
                             OPERAND_SIZE endOffset,
                             OPERAND_SIZE maxOffset,
                             Diana_Stack * pStack,
                             const DianaAnalyzeParams * pParams)
{
#ifndef DIANA_NO_ARM
    unsigned char buffer[4096];
    OPERAND_SIZE offset = 0;
    if (!pParams || pParams->arch != DIANA_ARCH_AARCH64)
        return DI_SUCCESS;
    // the first 4-aligned address
    offset = startOffset + ((4 - ((pParams->imageBase + startOffset) & 3)) & 3);
    while (offset + 4 <= endOffset)
    {
        OPERAND_SIZE readBytes = 0;
        OPERAND_SIZE i = 0;
        int size = (int)sizeof(buffer);
        if (endOffset - offset < (OPERAND_SIZE)size)
            size = (int)(endOffset - offset);
        if (pObserver->m_pStream->pRandomRead(pObserver->m_pStream, offset, buffer, size, &readBytes, 0) || !readBytes)
        {
            // not readable: a gap between segments
            offset += sizeof(buffer);
            continue;
        }
        for (; i + 4 <= readBytes; i += 4)
        {
            // instructions are little-endian, also in big-endian files
            DI_UINT32 word = (DI_UINT32)buffer[i] | ((DI_UINT32)buffer[i + 1] << 8) |
                             ((DI_UINT32)buffer[i + 2] << 16) | ((DI_UINT32)buffer[i + 3] << 24);
            if (word != DIANA_A64_BTI_C && word != DIANA_A64_BTI_J && word != DIANA_A64_BTI_JC)
                continue;
            // an offset with an instruction, valid or rolled back, is skipped by Diana_AnalyzeCodeEx2
            DI_CHECK(Diana_AnalyzeCodeEx2(pOwner, pObserver, mode, offset + i, maxOffset, pStack,
                                          DI_ANALYSE_BREAK_ON_INVALID_CMD, pParams));
        }
        offset += readBytes & ~(OPERAND_SIZE)3;
        if (readBytes < 4)
            offset += 4;
    }
#else
    (void)pOwner; (void)pObserver; (void)mode; (void)startOffset; (void)endOffset;
    (void)maxOffset; (void)pStack; (void)pParams;
#endif
    return DI_SUCCESS;
}

/* AArch32: the second halfword of an instruction that starts 2 bytes before offset, an A32 one or a 32-bit T32 one */
static
int IsInsideArm32Instruction(Diana_InstructionsOwner * pOwner,
                             DianaAnalyzeObserver * pObserver,
                             OPERAND_SIZE offset)
{
#ifndef DIANA_NO_ARM
    Diana_Instruction * pPrev = 0;
    unsigned char bytes[2];
    OPERAND_SIZE readBytes = 0;
    if (offset < 2)
        return 0;
    pPrev = Diana_InstructionsOwner_GetInstruction(pOwner, offset - 2);
    if (!pPrev || (pPrev->m_flags & DI_INSTRUCTION_INVALID))
        return 0;
    if (!(pPrev->m_flags & DI_INSTRUCTION_THUMB))
        return 1;
    if (pObserver->m_pStream->pRandomRead(pObserver->m_pStream, offset - 2, bytes, 2, &readBytes, 0) || readBytes != 2)
        return 0;
    return DianaA32_GetT32Size((DI_UINT32)bytes[0] | ((DI_UINT32)bytes[1] << 8)) == 4;
#else
    (void)pOwner; (void)pObserver; (void)offset;
    return 0;
#endif
}

int Diana_AnalyzeScanThrough(Diana_InstructionsOwner * pOwner,
                             DianaAnalyzeObserver * pObserver,
                             int mode,
                             OPERAND_SIZE startOffset,
                             OPERAND_SIZE endOffset,
                             OPERAND_SIZE maxOffset,
                             Diana_Stack * pStack,
                             const DianaAnalyzeParams * pParams)
{
    int arch = pParams ? pParams->arch : DIANA_ARCH_X86;
    OPERAND_SIZE imageBase = pParams ? pParams->imageBase : 0;
    OPERAND_SIZE step = 1;
    OPERAND_SIZE offset = startOffset;
    switch (arch)
    {
    case DIANA_ARCH_AARCH64:
    case DIANA_ARCH_ARM32:
        step = 4;
        break;
    case DIANA_ARCH_THUMB:
        step = 2;
        break;
    default:
        break;
    }
    /* the first aligned address */
    offset += (step - ((imageBase + startOffset) & (step - 1))) & (step - 1);
    for (; offset < endOffset && offset < maxOffset; offset += step)
    {
        if ((arch == DIANA_ARCH_THUMB || arch == DIANA_ARCH_ARM32) && IsInsideArm32Instruction(pOwner, pObserver, offset))
            continue;
        /* an offset with an instruction, valid or rolled back, is skipped by Diana_AnalyzeCodeEx2 */
        DI_CHECK(Diana_AnalyzeCodeEx2(pOwner, pObserver, mode, offset, maxOffset, pStack,
                                      DI_ANALYSE_BREAK_ON_INVALID_CMD, pParams));
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
    return Diana_AnalyzeCodeEx2(pOwner, pObserver, mode, initialOffset, maxOffset, pStack, flags, 0);
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