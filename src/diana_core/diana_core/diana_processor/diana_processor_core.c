#include "diana_processor_core.h"
#include "diana_proc_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_commands.h"
#include "diana_processor_cmd_fpu.h"
#include "diana_disable_warnings.h"
#include "diana_analyze2.h"

void DianaProcessor_GlobalInit()
{
    DianaProcessor_LinkCommands();
    DianaProcessor_ProcImplInit();
    Diana_GlobalInitFPU();
}

static int ProcessorReadStream(void * pThis, 
                               void * pBuffer, 
                               int iBufferSize, 
                               int * readed)
{
    DianaReadStream * pStream = pThis;
    DianaProcessor * pCallContext = 0;
    int res = 0;

    OPERAND_SIZE readedOp = 0;

    pCallContext= (DianaProcessor *)((char*)pStream - (DIANA_SIZE_T)&pCallContext->m_readStream);
    
    res = DianaProcessor_ReadMemory(pCallContext, 
                                    GET_REG_CS,
                                    pCallContext->m_tempRIP, 
                                    pBuffer, 
                                    iBufferSize, 
                                    &readedOp,
                                    0,
                                    reg_CS);
    if (res != DI_SUCCESS)
        return res;

    *readed = (int)readedOp;
    pCallContext->m_tempRIP += readedOp;
    return res;
}

void DianaProcessRandom_Init(DianaProcessRandom * pRandom)
{
    pRandom->state = 0xa35221fd4245cULL;
    pRandom->inc = 0xdfdf343112ULL;
}
DI_UINT32 DianaProcessRandom_Generate32(DianaProcessRandom * pRandom)
{
    DI_UINT64 oldstate = pRandom->state;
    // Advance internal state
    pRandom->state = oldstate * 6364136223846793005ULL + (pRandom->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    {
        DI_UINT32 xorshifted = (DI_UINT32)(((oldstate >> 18u) ^ oldstate) >> 27u);
        DI_UINT32 rot = (DI_UINT32)(oldstate >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((0-rot) & 31));
    }
}

DI_UINT64 DianaProcessRandom_Generate(DianaProcessRandom * pRandom)
{
    DianaRegisterValue_type result;
    result.impl.h = DianaProcessRandom_Generate32(pRandom);
    result.impl.l.value = DianaProcessRandom_Generate32(pRandom);
    return result.value;
}


// {A0E078BD-9C42-40b1-A59E-69900E121D95}
static const DIANA_UUID g_ProcessorId = 
{ 0xa0e078bd, 0x9c42, 0x40b1, { 0xa5, 0x9e, 0x69, 0x90, 0xe, 0x12, 0x1d, 0x95 } };

int DianaProcessor_Init(DianaProcessor * pThis, 
                        DianaRandomReadWriteStream * pMemoryStream,
                        Diana_Allocator * pAllocator,
                        int mode)
{
    DIANA_MEMSET(pThis, 0, sizeof(DianaProcessor)); 
    DIANA_MEMSET(pThis->m_registers, -1, sizeof(pThis->m_registers)); 

    DianaBase_Init(&pThis->m_base, &g_ProcessorId);

    pThis->m_readStream.pReadFnc = ProcessorReadStream;
    Diana_InitContext(&pThis->m_context, mode);
    pThis->m_initialDianaMode  = mode;

    pThis->m_pAllocator = pAllocator;
    pThis->m_pMemoryStream = pMemoryStream;

    DianaProcessRandom_Init(&pThis->m_random);
    {
        int iResult = DianaProcessor_InitProcessorImpl(pThis);
        if (iResult != DI_SUCCESS)
        {
            DianaProcessor_Free(pThis);
            return iResult;
        }
        return 0;
    }
}

void DianaProcessor_Free(DianaProcessor * pThis)
{
    pThis->m_pAllocator->m_free( pThis->m_pAllocator, pThis->m_pRegistersVector );
}

static
int Call(DianaProcessorCommand_type pCommand,
         DianaProcessor * pCallContext)
{
    int res = pCommand(&pCallContext->m_context, pCallContext);

    // clean resources
    // remove flags
    pCallContext->m_stateFlags &= ~pCallContext->m_stateFlagsToRemove;
    pCallContext->m_stateFlagsToRemove = 0;

    return res;
}

static
int CallWithRep(DianaProcessorCommand_type pCommand,
                DianaProcessor * pCallContext,
                int bRepn)

{
    DianaUnifiedRegister usedReg = 0;

    switch( pCallContext->m_result.pInfo->m_pGroupInfo->m_commandId ) {
    case diana_cmd_ins:
    case diana_cmd_lods:
    case diana_cmd_movs:
    case diana_cmd_outs:
    case diana_cmd_stos:
    case diana_cmd_cmps:
    case diana_cmd_scas:
        break;
    default:
        return Call(pCommand, pCallContext);
    }

    switch(pCallContext->m_context.iCurrentCmd_addressSize)
    {
    case DIANA_MODE64:
        usedReg = reg_RCX;
        break;
    case DIANA_MODE32:
        usedReg = reg_ECX;
        break;
    case DIANA_MODE16:
        usedReg = reg_CX;
        break;
    default:
        return DI_ERROR;
    }

    for(;;)
    {
        if (!DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, usedReg)))
            return DI_SUCCESS;

        DI_CHECK(Call(pCommand, pCallContext));
        if (pCallContext->m_stateFlags & DI_PROC_STATE_RIP_CHANGED)
            break;

        SET_REG_RCX(GET_REG_RCX - 1);

        if (bRepn)
        {
            if (GET_FLAG_ZF)
                return DI_SUCCESS;
        }
        else
        {
            // if DI_PROC_STATE_CMD_USES_NORMAL_REP flag is not set 
            if (!(pCallContext->m_stateFlags & DI_PROC_STATE_CMD_USES_NORMAL_REP))
            {
                if (!GET_FLAG_ZF)
                    return DI_SUCCESS;
            }
        }
    }
    return DI_SUCCESS;
}

void DianaProcessor_ClearCache(DianaProcessor * pThis)
{
    pThis->m_stateFlags = 0;
    Diana_ClearCache(&pThis->m_context);
}

int DianaProcessor_ExecOnce(DianaProcessor * pThis)
{
    // parse next command
    DianaProcessor * pCallContext = pThis;
    OPERAND_SIZE rip = GET_REG_RIP;
    int res = 0;
    int i =0;
    DianaProcessorCommand_type pCommand = 0;
    
    for(i = 0 ; i < pCallContext->m_firePointsCount; ++i)
    {
        DianaProcessorFirePoint  * pPoint = pCallContext->m_firePoints + i;
        if (pPoint->address == rip)
            pPoint->action(pPoint, pThis);
    }

    rip = GET_REG_RIP;
    if (!(pThis->m_stateFlags & DI_PROC_STATE_TEMP_RIP_IS_VALID))
    {
        // first run
        pThis->m_tempRIP = rip;

        pThis->m_stateFlags |= DI_PROC_STATE_TEMP_RIP_IS_VALID;
    }

    pThis->m_stateFlags &= ~DI_PROC_STATE_RIP_CHANGED;

    res = Diana_ParseCmd(&pThis->m_context, 
                         Diana_GetRootLine(),  // IN
                         &pThis->m_readStream,    // IN
                         &pThis->m_result);

    DI_CHECK(res);
    
    // query context 
    pCommand = DIANA_QUERY_PROCESSOR_TAG(pThis->m_result.pInfo->m_pGroupInfo);

    // apply custom rules
    if (pThis->m_customProvider)
    {
        DianaProcessorCommand_type pCustomCommand = 0;
        DI_CHECK(pThis->m_customProvider(pThis, pThis->m_pCustomProviderContext, pThis->m_result.pInfo->m_pGroupInfo, &pCustomCommand));
        if (pCustomCommand)
        {
            pCommand = pCustomCommand;
        }
    }
    if (!pCommand)
    {
        return Diana_OnError(DI_UNSUPPORTED_COMMAND);
    }
    // execute command
    switch(pThis->m_result.iPrefix)
    {
    case DI_PREFIX_REP:
        res = CallWithRep(pCommand, pCallContext, 0);
        break;
    case DI_PREFIX_REPN:
        res = CallWithRep(pCommand, pCallContext, 1);
        break;
    case DI_PREFIX_LOCK:
        if (DI_FLAG_CMD_PREFIX_LOCK == (pCallContext->m_result.pInfo->m_flags & DI_FLAG_CMD_PREFIX_LOCK)) {
            if (pCallContext->m_result.iLinkedOpCount > 0) {
                if (diana_index==pCallContext->m_result.linkedOperands[0].type || diana_memory==pCallContext->m_result.linkedOperands[0].type) {
                    res = Call(pCommand, pCallContext);
                    break;
                }
            }
        }
        res = DI_INVALID_OPCODE;
        break;
    default:
        res = Call(pCommand, pCallContext);
    }

    // clear DI_PROC_STATE_CMD_USES_NORMAL_REP flag
    pCallContext->m_stateFlags &= ~DI_PROC_STATE_CMD_USES_NORMAL_REP;
           
    if (pThis->m_stateFlags & DI_PROC_STATE_RIP_CHANGED || res)
    {
        // RIP is changed by the command
        // temp rip is not valid right now
        pThis->m_stateFlags &= ~DI_PROC_STATE_TEMP_RIP_IS_VALID;
        pThis->m_stateFlags &= ~DI_PROC_STATE_RIP_CHANGED;
        // clear cache
        Diana_ClearCache(&pThis->m_context);

        DI_CHECK(res);
    }
    else
    {
        DI_CHECK(res);

        // shift RIP, usual command
        rip += pThis->m_result.iFullCmdSize;

        // update it
        DianaProcessor_SetValue(pCallContext, 
                                reg_none,  // skip check
                                DianaProcessor_QueryReg(pCallContext, reg_RIP), 
                                rip);
    }
    return DI_SUCCESS;
}


void DianaProcessor_InitRawRegister(DianaProcessor * pCallContext,
                                    DianaUnifiedRegister regId,
                                    const DI_CHAR * pValue,
                                    int size)
{
    DianaRegInfo * pReg = DianaProcessor_QueryReg(pCallContext, regId);
    char * pStart = (pCallContext->m_pRegistersVector + pReg->m_offset);
    switch(size)
    {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
        DIANA_MEMCPY(pStart, pValue, size);
        return;
    }
    Diana_FatalBreak();
}

const DI_CHAR * DianaProcessor_QueryRawRegister(DianaProcessor * pCallContext,
                                                DianaUnifiedRegister regId)
{
    DianaRegInfo * pReg = DianaProcessor_QueryReg(pCallContext, regId);
    char * pStart = (pCallContext->m_pRegistersVector + pReg->m_offset);
    return (const DI_CHAR *)pStart;
}

void DianaProcessor_InitCustomCommandProvider(DianaProcessor * pProc,
                                              DianaProcessorCustomCommandProvider_type customProvider,
                                               void * pCustomProviderContext)
{
    pProc->m_customProvider = customProvider;
    pProc->m_pCustomProviderContext = pCustomProviderContext;
}


int DianaProcessor_AnalyzeCallJmpInstruction(DianaContext * pDianaContext,
                                             DianaParserResult * pDianaResult,
                                             DianaProcessor * pCallContext,
                                             OPERAND_SIZE rip,
                                             OPERAND_SIZE * pPlaceToCall)
{
    DianaLinkedOperand * pOp = 0;
    OPERAND_SIZE newRIP = 0;
    if (pDianaResult->iLinkedOpCount != 1)
    {
        return DI_ERROR;
    }
    if (!pDianaResult->pInfo->m_pGroupInfo->m_pLinkedInfo)
    {
        return DI_ERROR;
    }
    if (!(pDianaResult->pInfo->m_pGroupInfo->m_pLinkedInfo->flags & (DIANA_GT_IS_CALL | DIANA_GT_IS_JUMP)))
    {
        return DI_ERROR;
    }
    pOp = pDianaResult->linkedOperands;

    // calculate absolute address
    switch(pDianaResult->linkedOperands[0].type)
    {
    case diana_rel:
        newRIP = rip + 
                 pOp->value.rel.m_value + 
                 pDianaResult->iFullCmdSize;
        break;

    case diana_imm:
        newRIP = pDianaResult->linkedOperands->value.imm;
        break;

    case diana_index:
        if ((pOp->value.rmIndex.indexed_reg == reg_none ||
            pOp->value.rmIndex.index == 0) &&
            pOp->value.rmIndex.dispSize)
        {
            DianaRmIndex  * pIndex = &pOp->value.rmIndex;
            OPERAND_SIZE memSelector = 0, memAddress=0;
            DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                              pCallContext,
                                              pIndex,
                                              &memSelector,
                                              &memAddress));
            DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                                memSelector,
                                                memAddress,
                                                pDianaContext->iCurrentCmd_addressSize,
                                                &newRIP,
                                                0,
                                                pOp->value.rmIndex.seg_reg));

            break;
        }
        if (pOp->value.rmIndex.reg == reg_RIP ||
            pOp->value.rmIndex.indexed_reg == reg_RIP)
        {
                        
            if ((pOp->value.rmIndex.indexed_reg == reg_none ||
                pOp->value.rmIndex.index == 0) ||
                (pOp->value.rmIndex.reg == reg_none &&
                 pOp->value.rmIndex.index == 1))
            {
                newRIP = rip + pOp->value.rmIndex.dispValue + pDianaResult->iFullCmdSize;
                break;
            }
        }

    case diana_register:
    case diana_call_ptr:
    default:
        return DI_ERROR;
    }
    *pPlaceToCall = newRIP;
    return DI_SUCCESS;
}



typedef struct _UtilsProcessorContext
{
    DianaReadStream parent;
    DianaProcessor * pCallContext;
    OPERAND_SIZE streamPointer;
}UtilsProcessorContext;

static int UtilsProcessorReadStream(void * pThis, 
                                    void * pBuffer, 
                                    int iBufferSize, 
                                    int * readed)
{
    UtilsProcessorContext * pUtilsProcessorContext = (UtilsProcessorContext * )pThis;
    DianaProcessor * pCallContext = pUtilsProcessorContext->pCallContext;
    int res = 0;
    OPERAND_SIZE readedOp = 0;
   
    res = DianaProcessor_ReadMemory(pCallContext, 
                                    GET_REG_CS,
                                    pUtilsProcessorContext->streamPointer, 
                                    pBuffer, 
                                    iBufferSize, 
                                    &readedOp,
                                    0,
                                    reg_CS);
    if (res != DI_SUCCESS)
        return res;

    pUtilsProcessorContext->streamPointer += readedOp;
    *readed = (int)readedOp;
    return res;
}

const int g_scanSize = 0x1000;
static int DianaProcessor_QueryFunctionDiff(DianaProcessor * pProc,
                                            OPERAND_SIZE rip,
                                            OPERAND_SIZE_SIGNED * pDiff)
{
    int i = 0;
    DianaContext context;
    DianaParserResult result;

    UtilsProcessorContext readStream;
    Diana_InitContext(&context, pProc->m_context.iMainMode_addressSize);

    readStream.parent.pReadFnc = UtilsProcessorReadStream;
    readStream.pCallContext = pProc;
    readStream.streamPointer = rip;
    *pDiff = 0;
    for(;i < g_scanSize; ++i)
    {
        int iRes = Diana_ParseCmd(&context,
                                  Diana_GetRootLine(),
                                  &readStream.parent,
                                  &result);
        DI_CHECK(iRes);
        if (result.pInfo->m_pGroupInfo->m_pLinkedInfo &&
            (result.pInfo->m_pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_IS_JUMP))
        {
            OPERAND_SIZE newRIP = 0;
            DI_CHECK(DianaProcessor_AnalyzeCallJmpInstruction(&context, 
                                                              &result,
                                                              pProc,
                                                              rip,
                                                              &newRIP));
             if (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_jmp ||
                 newRIP > rip)
             {
                 // go there
                rip = newRIP;
                Diana_ClearCache(&context);
                readStream.streamPointer = rip;
                continue;
             }
        }

        if (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_jmp &&
            result.iLinkedOpCount == 1 &&
            result.linkedOperands->type == diana_call_ptr &&
            result.linkedOperands->value.ptr.m_segment == 0x33)
        {
            //  WoW64 Heaven's Gate
            *pDiff = 0;
            return DI_SUCCESS;
        }
        if (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_int)
        {
            break;
        }
        if (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_ret)
        {
            if (result.iLinkedOpCount == 0)
            {
                return DI_SUCCESS;
            }
            if (result.iLinkedOpCount != 1)
            {
                return DI_ERROR;
            }
            if (result.linkedOperands[0].type != diana_imm)
            {
                return DI_ERROR;
            }
            *pDiff = result.linkedOperands[0].value.imm;
            return DI_SUCCESS;
        }
        rip += result.iFullCmdSize;
    }
    *pDiff = 0;
    return DI_SUCCESS;
}

static
int AnalyzeStack(DianaProcessor * pCallContext, 
                 OPERAND_SIZE rip,
                 OPERAND_SIZE currentFunctionRSP, 
                 OPERAND_SIZE_SIGNED currentFunctionStackDiff, 
                 OPERAND_SIZE * pNewRIP,
                 OPERAND_SIZE * pNewRSP)
{
    int i = 0;
    OPERAND_SIZE ptr = 0;
    if (currentFunctionStackDiff < 0)
    {
        currentFunctionStackDiff = 0;
    }
    &rip;
    ptr = currentFunctionRSP + currentFunctionStackDiff;
    for(;i<0x10;++i, ptr += pCallContext->m_context.iCurrentCmd_addressSize)
    {
        OPERAND_SIZE value = 0;
        DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                            GET_REG_SS,
                                            ptr,
                                            pCallContext->m_context.iCurrentCmd_addressSize,
                                            &value,
                                            0,
                                            reg_SS));
        if (value)
        {
            DI_CHAR buffer[5];
            OPERAND_SIZE bytesRead = 0;
            int status = DianaProcessor_ReadMemory(pCallContext,
                                                GET_REG_DS,
                                                value-5,
                                                buffer,
                                                sizeof(buffer),
                                                &bytesRead,
                                                0,
                                                reg_DS);
            if (status == DI_SUCCESS && bytesRead == sizeof(buffer))
            {
                if (buffer[0] == 0xE8)
                {
                    *pNewRIP = value - 5;
                    *pNewRSP = ptr + pCallContext->m_context.iCurrentCmd_addressSize;
                    return 1;
                }
            }
        }
    }
    
    return 0;
}
static
int DianaProcessor_QueryCurrentStack_Impl(DianaProcessor * pProc,
                                          DianaProcessorStackHandler_type pObserver,
                                          void * pCustomContext,
                                          int * pReportCount,
                                          OPERAND_SIZE rip,
                                          OPERAND_SIZE currentFunctionRSP)
{
    DianaProcessor * pCallContext = pProc;
    int i = 0;
    OPERAND_SIZE_SIGNED currentFunctionStackDiff = 0;
    DianaContext context;
    DianaParserResult result;
    UtilsProcessorContext readStream;

    Diana_InitContext(&context, pProc->m_context.iMainMode_addressSize);

    *pReportCount = 0;
    readStream.parent.pReadFnc = UtilsProcessorReadStream;
    readStream.pCallContext = pProc;
    readStream.streamPointer = rip;
   
    for(;i < g_scanSize; ++i)
    {
        OPERAND_SIZE_SIGNED diff = 0;
        int isCall = 0;
        int woW64 = 0;
        
        int iRes = Diana_ParseCmd(&context,
                                  Diana_GetRootLine(),
                                  &readStream.parent,
                                  &result);
        DI_CHECK(iRes);
        if (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_int)
        {
            // try
            OPERAND_SIZE newRIP = 0;
            if (AnalyzeStack(pProc, 
                             rip, 
                             currentFunctionRSP, 
                             currentFunctionStackDiff, 
                             &newRIP,
                             &currentFunctionRSP))
            {
                rip = newRIP;
                Diana_ClearCache(&context);
                readStream.streamPointer = rip;
                currentFunctionStackDiff = 0;
                continue;
            }
            break;
        }

        if (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_jmp &&
            result.iLinkedOpCount == 1 &&
            result.linkedOperands->type == diana_call_ptr &&
            result.linkedOperands->value.ptr.m_segment == 0x33)
        {
            //  WoW64 Heaven's Gate
            woW64 = 1;
            currentFunctionStackDiff = 0;
        }

        if (woW64 || result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_ret)
        {
            // query ret
            int continueExecution = 0;
            int retSize = 0;
            OPERAND_SIZE offset = currentFunctionRSP + 
                                  currentFunctionStackDiff;
            OPERAND_SIZE newRIP = 0;
            DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                                GET_REG_SS,
                                                offset,
                                                context.iCurrentCmd_addressSize,
                                                &newRIP,
                                                0,
                                                reg_SS));

            ++*pReportCount;
            DI_CHECK(pObserver(pProc, pCustomContext, newRIP, &continueExecution));
            
            if (!continueExecution)
            {
                return DI_SUCCESS;
            }
            // go further
            if (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_ret &&
                result.iLinkedOpCount == 1)
            {
                retSize = (int)result.linkedOperands[0].value.imm;
            }
            currentFunctionStackDiff = 0;
            currentFunctionRSP = offset + context.iCurrentCmd_addressSize + retSize;
            rip = newRIP;
            Diana_ClearCache(&context);
            readStream.streamPointer = rip;
            continue;
        }

        if (result.pInfo->m_pGroupInfo->m_pLinkedInfo &&
            (result.pInfo->m_pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_IS_JUMP))
        {
            OPERAND_SIZE newRIP = 0;
            DI_CHECK(DianaProcessor_AnalyzeCallJmpInstruction(&context, 
                                                              &result,
                                                              pProc,
                                                              rip,
                                                              &newRIP));
             if (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_jmp ||
                 newRIP > rip)
             {
                 // go there
                rip = newRIP;
                Diana_ClearCache(&context);
                readStream.streamPointer = rip;
                continue;
             }
        }
        {
            int status = Diana_AnalyzeInstructionStackDiff(&context,
                                           &result,
                                           &diff,
                                           &isCall);
            if (status == DI_END)
            {
                status = DI_SUCCESS;
            }
        }
        if (isCall)
        {
            OPERAND_SIZE newRIP = 0;
            diff = 0;
            DI_CHECK(DianaProcessor_AnalyzeCallJmpInstruction(&context, 
                                                              &result,
                                                              pProc,
                                                              rip,
                                                              &newRIP));

            DI_CHECK(DianaProcessor_QueryFunctionDiff(pProc,
                                                      newRIP,
                                                      &diff));
        }
        currentFunctionStackDiff += diff;
        rip += result.iFullCmdSize;
    }
    return DI_END;
}

int DianaProcessor_QueryCurrentStack_EBP_32(DianaProcessor * pCallContext,
                                          DianaProcessorStackHandler_type pObserver,
                                          void * pCustomContext,
                                          int * pReportCount)
{
    DI_UINT32 frame[2];
    OPERAND_SIZE currentEbp = GET_REG_RBP, test = 0, readBytes = 0;
    int continueExecution = 0;
    for(;;)
    {
        DI_CHECK(DianaProcessor_ReadMemory(pCallContext,
                                                GET_REG_SS,
                                                currentEbp,
                                                frame,
                                                8,
                                                &readBytes,
                                                0,
                                                reg_SS));
    
        if (readBytes != 8)
        {
            return DI_ERROR;
        }
        DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                                GET_REG_CS,
                                                frame[1],
                                                pCallContext->m_context.iCurrentCmd_addressSize,
                                                &test,
                                                0,
                                                reg_SS));
        ++*pReportCount;            
        DI_CHECK(pObserver(pCallContext, pCustomContext, frame[1], &continueExecution));
        if (!continueExecution)
        {
            return DI_SUCCESS;
        }
        currentEbp = frame[0];
    }
}

int DianaProcessor_QueryCurrentStack(DianaProcessor * pProc,
                                      DianaProcessorStackHandler_type pObserver,
                                      void * pCustomContext)
{
    DianaProcessor * pCallContext = pProc;
    int reportCount = 0;
    int result = 0;

    if (pProc->m_context.iMainMode_addressSize == DIANA_MODE32)
    {
        result = DianaProcessor_QueryCurrentStack_EBP_32(pProc,
                                                         pObserver,
                                                         pCustomContext,
                                                         &reportCount);
    }
    else
    {
        result = DianaProcessor_QueryCurrentStack_Impl(pProc,
                                                       pObserver,
                                                       pCustomContext,
                                                       &reportCount,
                                                       GET_REG_RIP,
                                                       GET_REG_RSP);
    }
    if (!reportCount && pProc->m_lastCallRSP)
    {
        // try saved information
        result = DianaProcessor_QueryCurrentStack_Impl(pProc,
                                                       pObserver,
                                                       pCustomContext,
                                                       &reportCount,
                                                       pProc->m_lastCallRIP,
                                                       pProc->m_lastCallRSP);
    }
    return result;
}
