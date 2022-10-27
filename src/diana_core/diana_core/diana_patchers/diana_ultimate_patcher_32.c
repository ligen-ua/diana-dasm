#include "diana_ultimate_patcher.h"
#include "diana_ultimate_patcher_impl.h"

typedef struct _dianaHook_DianaCommandInfo32
{
    DI_UINT32 newCommandSize;
}DianaHook_DianaCommandInfo32;


static unsigned char g_stubData[] = {
0x60,                         // pushad           
0x9C,                         // pushfd           
0x54,                         // push        esp  
0x8D, 0x44, 0x24, 0x28,       // lea         eax,[esp+28h] 
0x50,                         // push        eax  
0x68, 0x66, 0x66, 0x66, 0x06, // push        6666666h 
0xB8, 0x55, 0x55, 0x55, 0x05, // mov         eax,5555555h 
0xFF, 0xD0,                   // call        eax  
0x9D,                         // popfd            
0x61                          // popad            
};


static 
int CopyCommand32(DianaHook_InternalMessage * pMessage,
                  DI_OPERAND_SIZE srcAddress,
                  int srcCmdSize,
                  void ** ppCommandBuffer)
{
    OPERAND_SIZE readBytes = 0;
    void * pCommandBuffer = 0;

    *ppCommandBuffer = 0;
    DI_CHECK(DianaHook_AllocateCmd(pMessage, srcCmdSize, &pCommandBuffer));

    DI_CHECK(pMessage->pReadWriteStream->parent.pRandomRead(&pMessage->pReadWriteStream->parent,
                                                            srcAddress,
                                                            pCommandBuffer, 
                                                            srcCmdSize, 
                                                            &readBytes,
                                                            0));
    if (readBytes != srcCmdSize)
    {
        return DI_ERROR;
    }
    *ppCommandBuffer = pCommandBuffer;
    return DI_SUCCESS;
}

static
int DianaHook_AddTailJump(DianaHook_InternalMessage * pMessage,
                          DI_OPERAND_SIZE jmpCommandAddress,
                          DI_OPERAND_SIZE jmpTargetAddress)
{
    void * pCommandBuffer = 0;
    unsigned char * pCommandBufferOut = 0; 
    DI_CHECK(DianaHook_AllocateCmd(pMessage, 5, &pCommandBuffer));

    pCommandBufferOut = (unsigned char * )pCommandBuffer;
    pCommandBufferOut[0] = 0xE9;
    *(DI_UINT32*)(pCommandBufferOut+1) = (DI_UINT32)jmpTargetAddress - (DI_UINT32)jmpCommandAddress - 5;
    return DI_SUCCESS;
}

static
int DianaHook_AnalyzeCommand(DianaHook_InternalMessage * pMessage,
                            DI_OPERAND_SIZE commandAddress,
                            DI_OPERAND_SIZE newCommandAddress,
                            DianaHook_DianaCommandInfo32 ** ppOutputData,
                            int * pbIsRet)
{
    void * pCommandBuffer = 0;
    const DianaParserResult * pResult = &pMessage->result;
    DianaGroupInfo * pGroupInfo = pResult->pInfo->m_pGroupInfo;
    DianaHook_DianaCommandInfo32 * pOutputData = 0;

    *ppOutputData = 0;
    *pbIsRet = 0;

    if (pGroupInfo->m_pLinkedInfo && pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_RET)
    {
        *pbIsRet = 1;
    }

    DI_CHECK(DianaHook_AllocateMetainfo(pMessage,
                                        sizeof(DianaHook_DianaCommandInfo32), 
                                        (void**)&pOutputData));

    if ((!pGroupInfo->m_pLinkedInfo) || 
        !(pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_CAN_CHANGE_RIP) ||
         (pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_RET) ||
         !(pResult->iLinkedOpCount ==1 && pResult->linkedOperands[0].type == diana_rel))
    {
        // just regular command
        pOutputData->newCommandSize = pResult->iFullCmdSize;
        DI_CHECK(CopyCommand32(pMessage, commandAddress, pOutputData->newCommandSize, &pCommandBuffer));
    }
    else
    {
        // relative instruction, need correction
        const DianaLinkedOperand * pOp = pResult->linkedOperands;
        switch(pOp->value.rel.m_size)
        {
            case 1:
            case 2:
                {
                    // TODO: find similar instruction
                    return DI_ERROR;
                }
            default:
                return DI_ERROR;
            case 4:
                pOutputData->newCommandSize = pResult->iFullCmdSize;
                DI_CHECK(CopyCommand32(pMessage, commandAddress, pOutputData->newCommandSize, &pCommandBuffer));
                
                {
                    DI_UINT32 * pDataToChange = (DI_UINT32 *)((char*)pCommandBuffer + pOp->iOffset);
                    *pDataToChange = *pDataToChange - ((DI_UINT32)newCommandAddress)+ (DI_UINT32)commandAddress;
                }
                break;
        }
    }
    *ppOutputData  = pOutputData;
    return DI_SUCCESS;
}

static 
int DianaHook_PatchSequence32(DianaHook_InternalMessage * pMessage,
                              const DI_OPERAND_SIZE * pAllocatedAddress)
{

    DI_OPERAND_SIZE addressOfOriginal = *pAllocatedAddress + (int)sizeof(g_stubData);
    unsigned char hook[5];
    unsigned char stubData[sizeof(g_stubData)];
    OPERAND_SIZE writeData = 0;

    DIANA_MEMCPY(stubData, g_stubData, sizeof(g_stubData));

    *(DI_UINT32*)(stubData+9) = (DI_UINT32)pMessage->patchContext;
    *(DI_UINT32*)(stubData+14) = (DI_UINT32)pMessage->hookFunction;

    DI_CHECK(pMessage->pReadWriteStream->pRandomWrite(pMessage->pReadWriteStream,
                                                      *pAllocatedAddress,
                                                      stubData,
                                                      (int)sizeof(stubData),
                                                      &writeData,
                                                      0));

    DI_CHECK(pMessage->pReadWriteStream->pRandomWrite(pMessage->pReadWriteStream,
                                                      addressOfOriginal,
                                                      pMessage->workBufferRaw, 
                                                      (int)pMessage->workBufferRawSize,
                                                      &writeData,
                                                      0));

    hook[0] = 0xE9;
    *(DI_UINT32*)(hook+1) = (DI_UINT32)*pAllocatedAddress - (DI_UINT32)pMessage->addressToHook - 5;

    if (pMessage->originalFunctionPointer != (DI_OPERAND_SIZE)-1)
    {
        DI_CHECK(pMessage->pReadWriteStream->pRandomWrite(pMessage->pReadWriteStream,
                                                        pMessage->originalFunctionPointer,
                                                        &addressOfOriginal,
                                                        pMessage->processorMode,
                                                        &writeData,
                                                        0));
    }

    DI_CHECK(pMessage->pReadWriteStream->pRandomWrite(pMessage->pReadWriteStream,
                                                      pMessage->addressToHook,
                                                      hook, 
                                                      (int)sizeof(hook),
                                                      &writeData,
                                                      0));
    return DI_SUCCESS;
}


#define DIANA_HOOK_TRAMPOLINE_SIZE_IN_BYTES      5
#define DIANA_HOOK_MAX_JUMPS_COUNT              20

int DianaHook_PatchStream32(DianaHook_InternalMessage * pMessage)
{
    DianaHook_DianaCommandInfo32 * pCommandInfo = 0;
    int status = 0;
    int srcSequenceSummSize = 0;
    int destSequenceSummSize = 0;
    OPERAND_SIZE allocatedAddress = 0;
    int allocationSucceeded = 0;
    int prevWasRet = 0;

    Diana_InitContext(&pMessage->context, pMessage->processorMode);

    DI_CHECK_GOTO(pMessage->pReadWriteStream->parent.pMoveTo(pMessage->pReadWriteStream, 
                                                            pMessage->addressToHook));


    DI_CHECK_GOTO(pMessage->pAllocator->alloc(pMessage->pAllocator, 
                                1024, 
                                &allocatedAddress, 
                                &pMessage->addressToHook,
                                DIANA_HOOK_FLASG_ALLOCATE_EXECUTABLE));
    allocationSucceeded = 1;
    for(;srcSequenceSummSize < DIANA_HOOK_TRAMPOLINE_SIZE_IN_BYTES;)
    {
        if (prevWasRet)
        {
            DI_CHECK_GOTO(DI_END);
        }
        DI_CHECK_GOTO(Diana_ParseCmd(&pMessage->context,
                              Diana_GetRootLine(),
                              &pMessage->pReadWriteStream->parent.parent,
                              &pMessage->result));

        if (!srcSequenceSummSize)
        {
            // first command
            if (pMessage->result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_jmp)
            {
                if (pMessage->result.iLinkedOpCount == 1 &&
                    pMessage->result.linkedOperands->usedSize == 4 &&
                    !(pMessage->pCustomOptions && (pMessage->pCustomOptions->flags & DIANA_HOOK_CUSTOM_OPTION_DONT_FOLLOW_JUMPS)) && 
                    pMessage->jumpsCounter < DIANA_HOOK_MAX_JUMPS_COUNT)
                {
                    OPERAND_SIZE addressToHook = pMessage->addressToHook + srcSequenceSummSize - pMessage->result.linkedOperands->value.rel.m_value + pMessage->result.iFullCmdSize;
                    srcSequenceSummSize = 0;
                    pMessage->addressToHook = addressToHook;
                    ++pMessage->jumpsCounter;
                    DianaHook_ClearBuffers(pMessage);
                    continue;
                }
                DI_CHECK_GOTO(DI_ERROR_NOT_IMPLEMENTED);
            }
        }
        DI_CHECK_GOTO(DianaHook_AnalyzeCommand(pMessage,
                                               pMessage->addressToHook + srcSequenceSummSize,  
                                               allocatedAddress + destSequenceSummSize + sizeof(g_stubData),
                                               &pCommandInfo,
                                               &prevWasRet));
        
        srcSequenceSummSize += pMessage->result.iFullCmdSize;
        destSequenceSummSize += pCommandInfo->newCommandSize;
    }

    DI_CHECK_GOTO(DianaHook_AddTailJump(pMessage,
                                        allocatedAddress + destSequenceSummSize + sizeof(g_stubData),
                                        pMessage->addressToHook + srcSequenceSummSize));

    DI_CHECK_GOTO(DianaHook_PatchSequence32(pMessage,
                                            &allocatedAddress));

cleanup:
    if (status && allocationSucceeded)
    {
        pMessage->pAllocator->free(pMessage->pAllocator, &allocatedAddress);
    }
    return status;

}