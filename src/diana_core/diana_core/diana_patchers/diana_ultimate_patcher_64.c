#include "diana_ultimate_patcher.h"
#include "diana_ultimate_patcher_impl.h"


typedef struct _dianaHook_DianaCommandInfo64
{
    DI_UINT32 newCommandSize;
}DianaHook_DianaCommandInfo64;

static unsigned char g_stubData[] = 
{
    0x55,                                                          // push RBP
    0x51,                                                          // push RCX
    0x52,                                                          // push RDX
    0x41, 0x50,                                                    // push R8
    0x41, 0x51,                                                    // push R9
    0x50,                                                          // push RAX
    0x41, 0x52,                                                    // push R10
    0x41, 0x53,                                                    // push R11
    0x9C,                                                          // PUSHFQ

    0x4C, 0x8B, 0xC4,                                              // mov         R8,  RSP
    0x48, 0x8D, 0x54, 0x24, 0x28,                                  // lea         RDX, [RSP+28h] 
    0x48, 0xB9, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00,    // mov         RCX, 66666666666666h
    0x48, 0xB8, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x00,    // mov         RAX, 55555555555555h

    0x48, 0x83, 0xEC, 0x20,                                        // sub         RSP, 20h 
    0xFF, 0xD0,                                                    // call        RAX
    0x48, 0x83, 0xC4, 0x20,                                        // add         RSP,20h

    0x9D,                                                          // POPFQ
    0x41, 0x5B,                                                    // pop R11
    0x41, 0x5A,                                                    // pop R10
    0x58,                                                          // pop RAX
    0x41, 0x59,                                                    // pop R9
    0x41, 0x58,                                                    // pop R8
    0x5A,                                                          // pop RDX
    0x59,                                                          // pop RCX
    0x5D                                                           // pop RBP
    //0x5C                                                           // pop RSP
};



static 
int CopyCommand64(DianaHook_InternalMessage * pMessage,
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
int DianaHook_AnalyzeCommand(DianaHook_InternalMessage * pMessage,
                            DI_OPERAND_SIZE commandAddress,
                            DI_OPERAND_SIZE newCommandAddress,
                            DianaHook_DianaCommandInfo64 ** ppOutputData,
                            int * pbIsRet)
{
    void * pCommandBuffer = 0;
    const DianaParserResult * pResult = &pMessage->result;
    DianaGroupInfo * pGroupInfo = pResult->pInfo->m_pGroupInfo;
    DianaHook_DianaCommandInfo64 * pOutputData = 0;

    *ppOutputData = 0;
    *pbIsRet = 0;

    if (pGroupInfo->m_pLinkedInfo && pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_RET)
    {
        *pbIsRet = 1;
    }

    DI_CHECK(DianaHook_AllocateMetainfo(pMessage,
                                        sizeof(DianaHook_DianaCommandInfo64), 
                                        (void**)&pOutputData));

    if ((!pGroupInfo->m_pLinkedInfo) || 
        !(pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_CAN_CHANGE_RIP) ||
         (pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_RET) ||
         !(pResult->iLinkedOpCount ==1 && pResult->linkedOperands[0].type == diana_rel))
    {
        // just regular command
        pOutputData->newCommandSize = pResult->iFullCmdSize;
        DI_CHECK(CopyCommand64(pMessage, commandAddress, pOutputData->newCommandSize, &pCommandBuffer));
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
                DI_CHECK(CopyCommand64(pMessage, commandAddress, pOutputData->newCommandSize, &pCommandBuffer));
                
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


static unsigned char g_farJump[] = 
{
    0xff, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp qword ptr [RIP + 0]
    0x66, 0x66, 0x66, 0x66, 0x77, 0x77, 0x77, 0x77
};

static
int DianaHook_AddTailJump(DianaHook_InternalMessage * pMessage,
                          DI_OPERAND_SIZE jmpCommandAddress,
                          DI_OPERAND_SIZE jmpTargetAddress)
{
    void * pCommandBuffer = 0;
    unsigned char * pCommandBufferOut = 0; 

    if (DianaHook_Diff(jmpCommandAddress, jmpTargetAddress) <= 0x7FFFFFFFULL)
    {
        DI_CHECK(DianaHook_AllocateCmd(pMessage, 5, &pCommandBuffer));
        pCommandBufferOut = (unsigned char * )pCommandBuffer;
        pCommandBufferOut[0] = 0xE9;
        *(DI_UINT32*)(pCommandBufferOut+1) = (DI_UINT32)jmpTargetAddress - (DI_UINT32)jmpCommandAddress - 5;
        return DI_SUCCESS;
    }
    // put far jmp
    DI_CHECK(DianaHook_AllocateCmd(pMessage, sizeof(g_farJump), &pCommandBuffer));
    pCommandBufferOut = (unsigned char * )pCommandBuffer;
    DIANA_MEMCPY(pCommandBufferOut, g_farJump, sizeof(g_farJump));
    *(DI_UINT32*)(pCommandBufferOut+6) = (DI_UINT32)jmpTargetAddress;
    *(DI_UINT32*)(pCommandBufferOut+10) = (DI_UINT32)(jmpTargetAddress>>32);
    return DI_SUCCESS;
}
static int DianaHook_CommonMove64(DianaHook_InternalMessage * pMessage, 
                                OPERAND_SIZE allocatedAddress,
                                int trampolineSizeInBytes,
                                int addTailJump)
{
    int status = 0;
    int srcSequenceSummSize = 0;
    int destSequenceSummSize = 0;
    int prevWasRet = 0;
    DianaHook_DianaCommandInfo64 * pCommandInfo = 0;
    
    Diana_InitContext(&pMessage->context, pMessage->processorMode);

    DI_CHECK_GOTO(pMessage->pReadWriteStream->parent.pMoveTo(pMessage->pReadWriteStream, 
                                                            pMessage->addressToHook));


    for(;srcSequenceSummSize < trampolineSizeInBytes;)
    {
        if (prevWasRet)
        {
            DI_CHECK_GOTO(DI_END);
        }

        DI_CHECK_GOTO(Diana_ParseCmd(&pMessage->context,
                              Diana_GetRootLine(),
                              &pMessage->pReadWriteStream->parent.parent,
                              &pMessage->result));


        DI_CHECK_GOTO(DianaHook_AnalyzeCommand(pMessage,
                                               pMessage->addressToHook + srcSequenceSummSize,  
                                               allocatedAddress + destSequenceSummSize + sizeof(g_stubData),
                                               &pCommandInfo,
                                               &prevWasRet));
        
        srcSequenceSummSize += pMessage->result.iFullCmdSize;
        destSequenceSummSize += pCommandInfo->newCommandSize;
    }

    if (addTailJump)
    {
        DI_CHECK_GOTO(DianaHook_AddTailJump(pMessage,
                                            allocatedAddress + destSequenceSummSize + sizeof(g_stubData),
                                            pMessage->addressToHook + srcSequenceSummSize));
    }
cleanup:

    return status;
}

// near version
#define DIANA_HOOK_TRAMPOLINE_SIZE_IN_BYTES_64_NEAR      5

static 
int InitStubData(DianaHook_InternalMessage * pMessage,
                 const DI_OPERAND_SIZE * pAllocatedAddress,
                 unsigned char * stubData,
                 DI_OPERAND_SIZE * pResAddressOfOriginal)
{
    DI_OPERAND_SIZE addressOfOriginal = *pAllocatedAddress + (int)sizeof(g_stubData);
    OPERAND_SIZE writeData = 0;
    DIANA_MEMCPY(stubData, g_stubData, sizeof(g_stubData));

    *(DI_UINT64*)(stubData + 0x17) = (DI_UINT64)pMessage->patchContext;
    *(DI_UINT64*)(stubData + 0x21) = (DI_UINT64)pMessage->hookFunction;

    DI_CHECK(pMessage->pReadWriteStream->pRandomWrite(pMessage->pReadWriteStream,
                                                      *pAllocatedAddress,
                                                      stubData,
                                                      (int)sizeof(g_stubData),
                                                      &writeData,
                                                      0));

    DI_CHECK(pMessage->pReadWriteStream->pRandomWrite(pMessage->pReadWriteStream,
                                                      addressOfOriginal,
                                                      pMessage->workBufferRaw, 
                                                      (int)pMessage->workBufferRawSize,
                                                      &writeData,
                                                      0));
    *pResAddressOfOriginal = addressOfOriginal;
    return 0;
}
static 
int DianaHook_PatchSequence64_Near(DianaHook_InternalMessage * pMessage,
                                   const DI_OPERAND_SIZE * pAllocatedAddress)
{

    DI_OPERAND_SIZE addressOfOriginal = 0;
    unsigned char hook[5];
    unsigned char stubData[sizeof(g_stubData)];
    OPERAND_SIZE writeData = 0;

    DI_CHECK(InitStubData(pMessage, pAllocatedAddress, stubData, &addressOfOriginal));

    hook[0] = 0xE9;
    *(DI_UINT32*)(hook+1) = (DI_UINT32)((DI_UINT64)*pAllocatedAddress - (DI_UINT64)pMessage->addressToHook - 5);

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
static int DianaHook_PatchWithNearTrampoline(DianaHook_InternalMessage * pMessage, 
                                             OPERAND_SIZE allocatedAddress)
{
    int status = 0;
    DI_CHECK_GOTO(DianaHook_CommonMove64(pMessage, 
                                         allocatedAddress,
                                         DIANA_HOOK_TRAMPOLINE_SIZE_IN_BYTES_64_NEAR,
                                         1));

    DI_CHECK_GOTO(DianaHook_PatchSequence64_Near(pMessage, 
                                                 &allocatedAddress));
                                    
cleanup:

    return status;
}

// far version
int DianaHook_PatchSequence64_Far(DianaHook_InternalMessage * pMessage,
                                   const DI_OPERAND_SIZE * pAllocatedAddress)
{

    DI_OPERAND_SIZE addressOfOriginal = 0;
    unsigned char hook[sizeof(g_farJump)];
    unsigned char stubData[sizeof(g_stubData)];
    OPERAND_SIZE writeData = 0;
    DIANA_MEMCPY(hook, g_farJump, sizeof(g_farJump));

    DI_CHECK(InitStubData(pMessage, pAllocatedAddress, stubData, &addressOfOriginal));

    *(DI_UINT32*)(hook+6) = (DI_UINT32)((DI_UINT64)*pAllocatedAddress);
    *(DI_UINT32*)(hook+10) = (DI_UINT32)(((DI_UINT64)*pAllocatedAddress)>>32);

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
static int DianaHook_PatchWithFarCall(DianaHook_InternalMessage * pMessage, 
                                      OPERAND_SIZE allocatedAddress)
{
    int status = 0;
    DI_CHECK_GOTO(DianaHook_CommonMove64(pMessage, 
                                         allocatedAddress,
                                         sizeof(g_farJump),
                                         1));

    DI_CHECK_GOTO(DianaHook_PatchSequence64_Far(pMessage, 
                                                 &allocatedAddress));
                                    
cleanup:
    return status;
}


static unsigned char g_pushRet[] =
{
    0x68, 0x66, 0x66, 0x66, 0x66,                    // push    66666666h
    0xC3                                             // ret
};

int DianaHook_PatchSequence64_PushRet(DianaHook_InternalMessage * pMessage,
                                      const DI_OPERAND_SIZE * pAllocatedAddress)
{

    DI_OPERAND_SIZE addressOfOriginal = 0;
    unsigned char hook[sizeof(g_pushRet)];
    unsigned char stubData[sizeof(g_stubData)];
    OPERAND_SIZE writeData = 0;
    DIANA_MEMCPY(hook, g_pushRet, sizeof(g_pushRet));

    DI_CHECK(InitStubData(pMessage, pAllocatedAddress, stubData, &addressOfOriginal));

    *(DI_UINT32*)(hook+1) = (DI_UINT32)((DI_UINT64)*pAllocatedAddress);

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

static int DianaHook_PatchWithSinglePushRet(DianaHook_InternalMessage * pMessage, 
                                            OPERAND_SIZE allocatedAddress)
{
    int status = 0;
    DI_CHECK_GOTO(DianaHook_CommonMove64(pMessage, 
                                         allocatedAddress,
                                         sizeof(g_pushRet),
                                         1));

    DI_CHECK_GOTO(DianaHook_PatchSequence64_PushRet(pMessage, 
                                                &allocatedAddress));
                                    
cleanup:
    return status;
}

int DianaHook_PatchStream64(DianaHook_InternalMessage * pMessage)
{
    OPERAND_SIZE allocatedAddress = 0;
    int allocationSucceeded = 0;
    int status = 0;

    DI_CHECK_GOTO(pMessage->pAllocator->alloc(pMessage->pAllocator, 
                                1024, 
                                &allocatedAddress, 
                                &pMessage->addressToHook,
                                DIANA_HOOK_FLASG_ALLOCATE_EXECUTABLE));
    allocationSucceeded = 1;

    
    if (pMessage->pCustomOptions)
    {
        if (pMessage->pCustomOptions->flags & DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP)
        {
            status = DianaHook_PatchWithFarCall(pMessage, allocatedAddress);
            goto cleanup;
        }
        if (pMessage->pCustomOptions->flags & DIANA_HOOK_CUSTOM_OPTION_PUT_NEAR_JMP)
        {
            if (DianaHook_Diff(allocatedAddress, pMessage->addressToHook) < 0x7FFFFFFFULL)
            {
                status = DianaHook_PatchWithNearTrampoline(pMessage, allocatedAddress);
                goto cleanup;
            }
        }
    }

    // autodetect
    if (DianaHook_Diff(allocatedAddress, pMessage->addressToHook) > 0x7FFFFFFFULL ||
        (pMessage->pCustomOptions && (pMessage->pCustomOptions->flags & DIANA_HOOK_CUSTOM_OPTION_TEST_SINGLE_PUSH)))
    {
        if (allocatedAddress <= 0x7FFFFFFFULL)
        {
            status = DianaHook_PatchWithSinglePushRet(pMessage, allocatedAddress);
        }
        else
        {
            status = DianaHook_PatchWithFarCall(pMessage, allocatedAddress);
        }
    }
    else
    {
        status = DianaHook_PatchWithNearTrampoline(pMessage, allocatedAddress);
    }

cleanup:
    if (status && allocationSucceeded)
    {
        pMessage->pAllocator->free(pMessage->pAllocator, &allocatedAddress);
    }
    return status;
}   