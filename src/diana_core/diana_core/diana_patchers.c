#include "diana_patchers.h"
#include "diana_streams.h"
#include "diana_disable_warnings.h"

#ifdef DIANA_CFG_I386 
#ifdef DIANA_CFG_USE_INLINE_ASSEMBLER

#pragma warning(disable:4311)
#pragma warning(disable:4267)

int Diana_GetCmdSizeToMove(const DianaParserResult * pResult, DIANA_SIZE_T cmdSize, DIANA_SIZE_T * pNewSize)
{
    int i = 0;
    // get group info
    DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(pResult->pInfo->m_lGroupId);
    if (!pGroupInfo->m_pLinkedInfo)
    {
        *pNewSize = cmdSize;
        return DI_SUCCESS;
    }
    if (!(pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_CAN_CHANGE_RIP))
    {
        *pNewSize = cmdSize;
        return DI_SUCCESS;
    }

    if (pResult->iLinkedOpCount == 1)
    {
        const DianaLinkedOperand * pOp = pResult->linkedOperands + i;
        if (pOp->type == diana_rel)
        {
            *pNewSize = 5;
            if (pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_IS_JUMP)
            {
                //todo: if CONDITIONAL
                *pNewSize = 6;
            }
        }
        return DI_SUCCESS;
    }
    return DI_ERROR;
}

int Diana_CalculateSizeToMove(void * pPlaceToHook, 
                              DIANA_SIZE_T summSize,
                              DIANA_SIZE_T * pSizeAfterMove)
{
    int status = DI_ERROR;
    char * pBegin = (char * )pPlaceToHook;
    DIANA_SIZE_T cmdSize = 0;
    long tailSize = summSize;
    DIANA_SIZE_T resultSize = 0;

    *pSizeAfterMove = 0;

    for(;tailSize>0; pBegin+=cmdSize, tailSize-= cmdSize)
    {
        DianaParserResult result;
        
        status = Diana_ParseCmdOnBuffer(DIANA_MODE32,
                                        pBegin,
                                         tailSize,
                                         Diana_GetRootLine(),  // IN
                                         &result,  //OUT
                                         &cmdSize);    // OUT
        if (status)
           return status;

        {
            DIANA_SIZE_T newCmnSize;
            status = Diana_GetCmdSizeToMove(&result, cmdSize, &newCmnSize);
            if (status)
                return status;
    
            resultSize += newCmnSize;
        }
    }
    *pSizeAfterMove = resultSize;
    return status;
}

int
Diana_MoveSequence(void * pDestination, 
                   void * pSource,
                   DIANA_SIZE_T summSize,
                   DIANA_SIZE_T * pResSize)
{
    int status = DI_ERROR;
    char * pBegin = (char * )pSource;
    DIANA_SIZE_T cmdSize = 0;
    DIANA_SIZE_T realCmdSize = 0;
    
    DIANA_SIZE_T targetCmdOffset = 0;
    long tailSize = summSize;

    for(;tailSize>0; pBegin+=cmdSize, tailSize-= cmdSize, targetCmdOffset += realCmdSize)
    {
        int iNeedToCopy = 1;
        DianaParserResult result;
        DianaGroupInfo * pGroupInfo = 0;
        
        status = Diana_ParseCmdOnBuffer(DIANA_MODE32,
                                        pBegin,
                                         tailSize,
                                         Diana_GetRootLine(),  // IN
                                         &result,  //OUT
                                         &cmdSize);    // OUT
        if (status)
           return status;


        // analyze 
        pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        if (pGroupInfo->m_pLinkedInfo &&  (pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_CAN_CHANGE_RIP))
        {
            if (result.iLinkedOpCount ==1 && result.linkedOperands[0].type == diana_rel)
            {
                // relative, need correction
                const DianaLinkedOperand * pOp = result.linkedOperands;
                switch(pOp->value.rel.m_size)
                {
                    case 1:
                        {
                            // find another command
                            return DI_ERROR;
                        }
                    case 2:
                        {
                            //if (result.pInfo->m_bHas32BitAnalog)
                            return DI_ERROR;
                        }
                    case 4:
                        realCmdSize = cmdSize;
                        DIANA_MEMCPY((char*)pDestination+targetCmdOffset, pBegin, cmdSize);
                        
                        {
                            unsigned long * pChangedData   = (unsigned long *)((char*)pDestination + targetCmdOffset+ pOp->iOffset);
                            *pChangedData = *pChangedData - ((unsigned long)pChangedData-1)+ (unsigned long)pBegin;
                        }
                        iNeedToCopy = 0;
                        break;
                    default:
                        return DI_ERROR;
                }
            }
        }

        if (iNeedToCopy) // normal copy
        {
            realCmdSize = cmdSize;
            DIANA_MEMCPY((char*)pDestination+targetCmdOffset, pBegin, cmdSize);
        }
    }

    *pResSize = targetCmdOffset;
    return status;
}
                   
static int PatchSequence32(void * pPlaceToHook, 
                           DIANA_SIZE_T summSize,
                           Diana_PatchHandlerFunction_type pPatchFnc,
                           void * pPatchContext,
                           Diana_Allocator * pAllocator,
                           void ** ppOriginalFunction)
{
    // alloc stub: summSize + 5
    void * pStub = 0;
    int status = DI_ERROR;
    DIANA_SIZE_T codeSizeAfterMove = 0;
    DIANA_SIZE_T stubSize = 0;
    DIANA_SIZE_T jumpOffset = 0;
    unsigned long * pFunctionArg = 0;
    unsigned char * pOriginalFunction = 0;
    
    status = Diana_CalculateSizeToMove(pPlaceToHook, summSize, &codeSizeAfterMove);
    if (status)
        goto err;


    // CREATE STUB
    /*
    push ESP
    push context
    call stdcall_function
    */

    stubSize = codeSizeAfterMove 
                            + 6 // push size
                            + 5 // call stub
                            + 5 // jmp res
                            ;

    pStub = (*pAllocator->m_alloc)(pAllocator, stubSize);
    if (!pStub)
        goto err;

    // set begin and end
    *(unsigned char*)pStub = 0x54;
    *((unsigned char*)pStub+1) = 0x68;
    DIANA_MEMCPY((unsigned char*)pStub+2, &pPatchContext, 4);
    
    *((unsigned char*)pStub+6) = 0xE8;
    pFunctionArg = (unsigned long*)((unsigned char*)pStub+7);
    *pFunctionArg = (unsigned long)pPatchFnc - (unsigned long)pFunctionArg-4;

    pOriginalFunction = (unsigned char*)pStub+stubSize-codeSizeAfterMove-5;
    status = Diana_MoveSequence(pOriginalFunction, pPlaceToHook, summSize, &jumpOffset);
    if (status)
        goto err;

    // put jump
    jumpOffset += stubSize-codeSizeAfterMove-5;

    *((unsigned char*)pStub+jumpOffset) = 0xE9;
    pFunctionArg = (unsigned long*)((unsigned char*)pStub+jumpOffset+1);
    *pFunctionArg = ((unsigned long)pPlaceToHook + summSize) - (unsigned long)pFunctionArg-4;
    
    // stub is done - set hook!
    {
        char buf[5] = {0xE9, 0,0,0,0};
        pFunctionArg = (unsigned long *)(buf+1);
        *pFunctionArg = (unsigned long)pStub - (unsigned long)pPlaceToHook-5;

        status = pAllocator->m_patch(pAllocator, pPlaceToHook, buf, 5);
        if (status)
            goto err;
    }

    if (ppOriginalFunction)
    {
        *ppOriginalFunction = pOriginalFunction;
    }
    return DI_SUCCESS;


err:
    if (pStub)
        (*pAllocator->m_free)(pAllocator, pStub);
    return status;
}

int Diana_PatchSomething32(void * pPlaceToHook,
                           DIANA_SIZE_T size,
                           Diana_PatchHandlerFunction_type pPatchFnc,
                           void * pPatchContext,
                           Diana_Allocator * pAllocator,
                           void ** ppOriginalFunction)
{
    DianaParserResult result;
    DIANA_SIZE_T cmdSize = 0;
    DIANA_SIZE_T cmdOffset = 0;
    DIANA_SIZE_T summSize = 0;
    int iRes = DI_ERROR;

    while(cmdOffset<size)
    {
        iRes = Diana_ParseCmdOnBuffer(DIANA_MODE32,
                                          (char*)pPlaceToHook+cmdOffset,
                                          size-summSize,
                                          Diana_GetRootLine(),  // IN
                                          &result,  //OUT
                                          &cmdSize);    // OUT
        if (iRes)
            return iRes;

        summSize += cmdSize;

        // logic:
        if (summSize >= 5)
            return PatchSequence32(pPlaceToHook, 
                                   summSize, 
                                   pPatchFnc, 
                                   pPatchContext, 
                                   pAllocator,
                                   ppOriginalFunction);

        // next:
        cmdOffset = summSize;
    }
    return iRes;
}


static unsigned char g_retStub[]= {0x68, 0, 0, 0, 0,              // push ret          :0
                                   0x60,                          // pushad            :5
                                   0x9C,                          // pushf             :6
                                   0x54,                          // push esp          :7
                                   0x68, 0, 0, 0, 0,              // push pContext     :8
                                   0x68, 0, 0, 0, 0,              // push ret2         :13
                                   0x68, 0, 0, 0, 0,              // push function     :18
                                   0xC3,                          // ret               :23
                                   
                                   //   deallocate hook
                                   0x68, 0, 0, 0, 0,  // push hook         :24
                                   0x68, 0, 0, 0, 0,  // push function2    :29
                                   0xC3               // ret               :34
}; 

#define DI_RET_OFFSET_ORIGINAL_RETURN      1
#define DI_RET_OFFSET_CONTEXT              9
#define DI_RET_OFFSET_RET2                 14
#define DI_RET_OFFSET_FUNCTION             19
#define DI_RET_OFFSET_HOOK                 25
#define DI_RET_OFFSET_FUNCTION2            30


#define DI_RET_RET2                        24

static void _stdcall Diana_FreeRetHookImpl(DianaPatcher_RetHook32 * pHook)
{
    pHook->pAllocator->m_free(pHook->pAllocator, pHook);
}

__declspec(naked) static void _stdcall Diana_FreeRetHook(DianaPatcher_RetHook32 * pHook)
{
    pHook;

    __asm
    {
    push ebp
    mov ebp, esp

    mov eax, [ebp + 0x4]
    push eax
    call Diana_FreeRetHookImpl

    mov esp, ebp
    pop ebp
    
    add esp, 4

    popfd
    popad
    ret 
    }
}

static void InitRetStub(DianaPatcher_RetHook32 * pRet, 
                        void * pOriginalRet)
{
    DIANA_MEMCPY(pRet->retStub, g_retStub, sizeof(g_retStub));

    *(void ** )(pRet->retStub + DI_RET_OFFSET_ORIGINAL_RETURN) = pOriginalRet;
    *(void ** )(pRet->retStub + DI_RET_OFFSET_CONTEXT) = pRet;
    *(void ** )(pRet->retStub + DI_RET_OFFSET_RET2) = pRet->retStub + DI_RET_RET2;
    *(void ** )(pRet->retStub + DI_RET_OFFSET_FUNCTION) = pRet->pFunction;
    *(void ** )(pRet->retStub + DI_RET_OFFSET_HOOK) = pRet;
    *(void ** )(pRet->retStub + DI_RET_OFFSET_FUNCTION2) = Diana_FreeRetHook;
}

int Diana_SetupRet(Diana_Allocator * pAllocator,
                   void * pOriginalESP,
                   Diana_RetHandlerFunction_type pFunction, 
                   void * pContext,
                   int paramsCount,
                   int callContextSizeInBytes,
                   void ** ppCallContext)
{
    DianaPatcher_RetHook32 * pRet = 0;
    
    if (callContextSizeInBytes)
    {
        if (!ppCallContext)
        {
            return DI_INVALID_INPUT;
        }
    }    

    pRet = pAllocator->m_alloc(pAllocator, 
                               sizeof(DianaPatcher_RetHook32) + callContextSizeInBytes +
                               (paramsCount+1)*sizeof(unsigned int));

    DI_CHECK_ALLOC(pRet);

    pRet->pAllocator = pAllocator;
    pRet->pContext = pContext;
    pRet->pOriginalESP = pOriginalESP;
    pRet->pFunction = pFunction;
    pRet->pCopiedArgs = (unsigned int*)(pRet+1);
    pRet->pCallContext = pRet->pCopiedArgs + paramsCount + 1;
    
    InitRetStub(pRet, DIANA_RET_PTR(pOriginalESP));

    DIANA_MEMCPY(pRet->pCopiedArgs, 
           (unsigned int*)pOriginalESP,
           (paramsCount+1)*sizeof(unsigned int));

    DIANA_RET_PTR(pOriginalESP) = pRet->retStub;

    if (ppCallContext)
    {
        *ppCallContext = pRet->pCallContext;
    }
    return DI_SUCCESS;
}

typedef struct _dianaPatcher_HookInfoEx
{
    void * pPatchContext;
    Diana_PatchHandlerFunction2_type pPatchFnc;
}DianaPatcher_HookInfoEx;


__declspec(naked) static void _stdcall Diana_Hook(DianaPatcher_HookInfoEx * pContext, 
                                                  void * pOriginalESP)
{
    pContext;
    pOriginalESP;

    __asm
    {
    push ebp
    mov ebp, esp

    pushad
    pushfd
    
    push esp
    mov eax, [ebp + 0x8] // pContext
    push [ebp + 0xc] 
    push [eax]
    mov eax, [eax + 4]
    call eax

    popfd
    popad

    mov esp, ebp
    pop ebp
    ret 8
    }
}

int Diana_PatchSomethingEx(void * pPlaceToHook,
                           DIANA_SIZE_T size,
                           Diana_PatchHandlerFunction2_type pPatchFnc,
                           void * pPatchContext,
                           Diana_Allocator * pAllocator,
                           unsigned char ** ppOriginalFunction)
{
    int res = 0;
    DianaPatcher_HookInfoEx * pInfo = pAllocator->m_alloc(pAllocator, 
                                                          sizeof(DianaPatcher_HookInfoEx));
    DI_CHECK_ALLOC(pInfo);

    pInfo->pPatchContext = pPatchContext;
    pInfo->pPatchFnc = pPatchFnc;

    res = Diana_PatchSomething32(pPlaceToHook,
                                 size,
                                 Diana_Hook,
                                 pInfo,
                                 pAllocator,
                                 ppOriginalFunction);
    if (!res)
        return DI_SUCCESS;

    pAllocator->m_free(pAllocator, pInfo);
    return res;
}

#endif
#endif