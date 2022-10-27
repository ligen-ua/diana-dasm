#include "diana_core.h"

#include "diana_operands.h"
#include "diana_stream_proxy.h"
#include "diana_core_utils.h"
#include "diana_core_parsers.h"

#ifdef DIANA_USE_CRUNTIME
#include "diana_c_runtime.inc"
#endif


static Diana_LinkedAdditionalGroupInfo g_infoForRets =  {DIANA_GT_CAN_CHANGE_RIP|DIANA_GT_RET, 0};
static Diana_LinkedAdditionalGroupInfo g_infoForCalls = {DIANA_GT_CAN_CHANGE_RIP|DIANA_GT_IS_CALL|DIANA_GT_CAN_GO_TO_THE_NEXT_INSTRUCTION, 0};
static Diana_LinkedAdditionalGroupInfo g_infoForJmps =  {DIANA_GT_CAN_CHANGE_RIP|DIANA_GT_IS_JUMP, 0};
static Diana_LinkedAdditionalGroupInfo g_infoForLoops = {DIANA_GT_CAN_CHANGE_RIP|DIANA_GT_IS_JUMP|DIANA_GT_CAN_GO_TO_THE_NEXT_INSTRUCTION, 0};
static Diana_LinkedAdditionalGroupInfo g_infoForJecxz = {DIANA_GT_CAN_CHANGE_RIP|DIANA_GT_IS_JUMP|DIANA_GT_CAN_GO_TO_THE_NEXT_INSTRUCTION, 1};
static Diana_LinkedAdditionalGroupInfo g_infoForJcc =   {DIANA_GT_CAN_CHANGE_RIP|DIANA_GT_IS_JUMP|DIANA_GT_CAN_GO_TO_THE_NEXT_INSTRUCTION, 0};


typedef struct _dianaProxyReadStream
{
    DianaReadStream m_parent;
    DianaReadStream * m_pOriginalStream;
    int m_iReadSize;
}DianaProxyReadStream;

static int ProxyDianaReadFnc(void * pThisIn, void * pBuffer, int iBufferSize, int * readBytes)
{
    *readBytes = 0;
    {
        DianaProxyReadStream * pThis = pThisIn;
        int iRes = pThis->m_pOriginalStream->pReadFnc(pThis->m_pOriginalStream, pBuffer, iBufferSize, readBytes);
        if (!iRes)
        {
            pThis->m_iReadSize += *readBytes;
        }
        return iRes;
    }
}

int DianaExactRead(DianaReadStream * pThis,
                   void * pBuffer,
                   int iBufferSize)
{
    int read = 0;
    DI_CHECK(pThis->pReadFnc(pThis, pBuffer, iBufferSize, &read));
    if (read != iBufferSize)
        return DI_END_OF_STREAM;
    return DI_SUCCESS;
}

void DianaMovableReadStream_Init(DianaMovableReadStream * pStream,
                                 DianaRead_fnc pReadFnc, 
                                 DianaAnalyzeMoveTo_fnc pMoveTo,
                                 DianaAnalyzeRandomRead_fnc pRandomRead)
{
    pStream->parent.pReadFnc = pReadFnc;
    pStream->pMoveTo = pMoveTo;
    pStream->pRandomRead = pRandomRead;
}

void DianaReadWriteRandomStream_Init(DianaReadWriteRandomStream * pStream,
                                     DianaRead_fnc pReadFnc, 
                                     DianaAnalyzeMoveTo_fnc pMoveTo,
                                     DianaAnalyzeRandomRead_fnc pRandomRead,
                                     DianaAnalyzeRandomWrite_fnc pRandomWrite)
{
    DianaMovableReadStream_Init(&pStream->parent, 
                                pReadFnc, 
                                pMoveTo,
                                pRandomRead);

    pStream->pRandomWrite = pRandomWrite;
}
int Diana_ParseCmd(DianaContext * pContext, //IN
                   DianaBaseGenObject_type * pInitialLine,  // IN
                   DianaReadStream * readStream,    // IN
                   DianaParserResult * pResult)    // OUT
{
    int iRes = 0;
    int iPreviousCacheSize = pContext->cacheSize;
    DianaProxyReadStream proxy;
    DianaParseParams parseParams;

    proxy.m_iReadSize = 0;
    proxy.m_pOriginalStream = readStream;
    proxy.m_parent.pReadFnc = ProxyDianaReadFnc;

    parseParams.pContext = pContext;
    parseParams.pInitialLine = pInitialLine;
    parseParams.pResult = pResult;
    parseParams.readStream = &proxy.m_parent;

    pResult->pInfo = 0;
    pResult->iLinkedOpCount = 0;
    pResult->iPrefix = 0;
    pResult->iRexPrefix = 0;
    pResult->iFullCmdSize = 0;

    iRes = Diana_ParseCmdEx(&parseParams);

    pResult->iPrefix = pContext->iPrefix;
    pResult->iRexPrefix = pContext->iRexPrefix;

    pResult->iFullCmdSize = iPreviousCacheSize + proxy.m_iReadSize - pContext->cacheSize;

    if( pResult->iFullCmdSize > DI_MAX_INSTRUCTION_SIZE )
        return DI_ERROR;

    return iRes;
}

DianaGroupInfo * Diana_GetGroupInfo(long lId)
{
    DianaGroupInfo * pGroups = Diana_GetGroups();
    if (lId > Diana_GetGroupsCount())
        return 0;
    if (lId <1)
        return 0;
    return pGroups + lId - 1;
}

void Diana_FatalBreak()
{
#ifdef _DEBUG 
    #ifdef DIANA_CFG_USE_INLINE_ASSEMBLER
        __asm int 3
    #else
        __debugbreak();
    #endif
#endif
}

void Diana_InitContext(DianaContext * pThis, int Mode)
{
    DIANA_MEMSET(pThis, 0, sizeof(*pThis));

    pThis->cacheIt = DI_CACHE_RESERVED;
    if (Mode == DIANA_MODE64)
    {
        pThis->iAMD64Mode = 1;
        pThis->iMainMode_opsize = DIANA_MODE32;
        pThis->iCurrentCmd_opsize = DIANA_MODE32;

        pThis->iMainMode_addressSize = DIANA_MODE64;
        pThis->iCurrentCmd_addressSize = DIANA_MODE64;
    }
    else
    {
        pThis->iMainMode_opsize = Mode;
        pThis->iCurrentCmd_opsize = Mode;

        pThis->iMainMode_addressSize = Mode;
        pThis->iCurrentCmd_addressSize = Mode;
    }

    pThis->mainMode_sreg = reg_DS;
    pThis->currentCmd_sreg = reg_DS;
}

void Diana_InitContextWithTestMode(DianaContext * pThis, int Mode)
{
    Diana_InitContext(pThis, Mode);
    pThis->testMode = 1;
}

void Diana_ClearCache(DianaContext * pThis)
{
    pThis->cacheIt = DI_CACHE_RESERVED;
    pThis->cacheSize = 0;
}

// debug
static void Diana_Nope(struct _dianaContext * pContext) 
{
    pContext;

    Diana_FatalBreak();
}

// normal
static void Diana_Normal(struct _dianaContext * pContext) 
{  
    pContext->lastPrefixBeforeRex = DI_FULL_CHAR_NULL;
    pContext->prefixesCount = 0;
    pContext->iCurrentCmd_opsize = pContext->iMainMode_opsize;
    pContext->currentCmd_sreg = pContext->mainMode_sreg;
    pContext->iCurrentCmd_addressSize = pContext->iMainMode_addressSize;
    pContext->iPrefix = 0;
    pContext->iSizePrefixes = 0;
    pContext->iRexPrefix = 0;
    pContext->iRexPrefixValue = 0;
}

static int InitJmps(DianaGroupInfo * pGroupInfo)
{
    switch (pGroupInfo->m_commandId)
    {
    // RETS
    case diana_cmd_ret:
    case diana_cmd_retf:
    case diana_cmd_iret:
    case diana_cmd_leave:
        pGroupInfo->m_pLinkedInfo = &g_infoForRets;
        break;
    // CALLS
    case diana_cmd_call:
        pGroupInfo->m_pLinkedInfo = &g_infoForCalls;
        break;
    // LOOPS
    case diana_cmd_loop:
    case diana_cmd_loope:
    case diana_cmd_loopne:
        pGroupInfo->m_pLinkedInfo = &g_infoForLoops;
        break;
    // JMPS
    case diana_cmd_jmp:
        pGroupInfo->m_pLinkedInfo = &g_infoForJmps;
        break;
    case diana_cmd_ja:
    case diana_cmd_jae:
    case diana_cmd_jb:
    case diana_cmd_jbe:
    case diana_cmd_je:
    case diana_cmd_jg:
    case diana_cmd_jge:
    case diana_cmd_jl:
    case diana_cmd_jle:
    case diana_cmd_jne:
    case diana_cmd_jno:
    case diana_cmd_jnp:
    case diana_cmd_jns:
    case diana_cmd_jo:
    case diana_cmd_jp:
    case diana_cmd_js:
        pGroupInfo->m_pLinkedInfo = &g_infoForJcc;
        break;
    case diana_cmd_jecxz:
        pGroupInfo->m_pLinkedInfo = &g_infoForJecxz;
        break;
    default:
        return 0;
    }
    return 1;
}

void Diana_InitLine(DianaBaseGenObject_type * pRoot);

static void ProcessCmdInfo(DianaBaseGenObject_type * pCurrentLine, 
                           DianaCmdInfo * pInfo, 
                           DI_CHAR opcode)
{
    DianaGroupInfo * pGroupInfo = 0;

    if (pInfo->m_flags & DI_FLAG_CMD_IS_TRUE_PREFIX)
        pInfo->m_linkedPrefixFnc = Diana_Nope;
    else
        pInfo->m_linkedPrefixFnc = Diana_Normal;

    
    pGroupInfo = Diana_GetGroupInfo(pInfo->m_lGroupId);
    
    InitJmps(pGroupInfo);
    
    //--------------------------------------
    // AMD 64 ADDITIONAL INITIALIZATION
    //--------------------------------------
    if (pCurrentLine == Diana_GetRootLine())
    {
        switch (opcode)
        {
        case 0x82: // add/or/adc/sbb/and/sub/xor/cmp
        case 0x9A: // callf

        case 0x06: // push es
        case 0x07: // pop es
        case 0x0E: // push cs
        case 0x16: // push ss
        case 0x17: // pop ss
        case 0x1E: // push ds
        case 0x1F: // pop ds
        case 0x27: // daa
        case 0x2F: // das
        case 0x37: // aaa
        case 0x3F: // aas
        case 0x60: // pushad
        case 0x61: // popad
        case 0x62: // bound
        case 0xC4: // les
        case 0xC5: // lds
        case 0xD4: // aam
        case 0xD5: // aad
        case 0xD6: // setalc
        case 0xEA: // jmpf
        case 0x40: // inc eax
        case 0x41: // inc ecx
        case 0x42: // inc edx
        case 0x43: // inc ebx
        case 0x44: // inc esp
        case 0x45: // inc ebp
        case 0x46: // inc esi
        case 0x47: // inc edi
        case 0x48: // dec eax
        case 0x49: // dec ecx
        case 0x4A: // dec edx
        case 0x4B: // dec ebx
        case 0x4C: // dec esp
        case 0x4D: // dec ebp
        case 0x4E: // dec esi
        case 0x4F: // dec edi
            pInfo->m_flags |= DI_FLAG_CMD_I386;
            break;
        }
    }

    // 2) DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64
    // a) all who can change CS/IP should have def operand size 64
    if (pGroupInfo->m_pLinkedInfo)
    {
        if (pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_CAN_CHANGE_RIP)
        {
            pInfo->m_flags |= DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64;
        }
    }
    // b) pop and push should have def operand size 64
    if (diana_cmd_pop == pGroupInfo->m_commandId)
    {
        pInfo->m_flags |= DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64;
    } else
    if (diana_cmd_popf == pGroupInfo->m_commandId)
    {
        pInfo->m_flags |= DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64;
    } else
    if (diana_cmd_push == pGroupInfo->m_commandId)
    {
        pInfo->m_flags |= DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64;
    } else
    if (diana_cmd_pushf == pGroupInfo->m_commandId)
    {
        pInfo->m_flags |= DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64;
    }
}
void Diana_InitIndex(DianaIndexKeyLine * pRoot)
{
    int i;
    for(i = 0; i < 256; ++i)
    {
        DianaBaseGenObject_type * pLineOrCmd = pRoot->key[i].keyLineOrCmdInfo;
        if (!pLineOrCmd)
            continue;
        if (pLineOrCmd->m_type == DIANA_BASE_GEN_OBJECT_CMD)
        {
            ProcessCmdInfo(&pRoot->parent, (DianaCmdInfo *)pLineOrCmd, (DI_CHAR)i);
            continue;
        }
        Diana_InitLine(pLineOrCmd);
    }
}
void Diana_InitRegularLine(DianaCmdKeyLine * pRoot)
{
    int i;
    for(i = 0; i < pRoot->iKeysCount; ++i)
    {
        DianaBaseGenObject_type * pLineOrCmd = pRoot->key[i].keyLineOrCmdInfo;
        if (pLineOrCmd->m_type == DIANA_BASE_GEN_OBJECT_CMD)
        {
            ProcessCmdInfo(&pRoot->parent, (DianaCmdInfo *)pLineOrCmd, pRoot->key[i].opcode);
            continue;
        }
        Diana_InitLine(pLineOrCmd);
    }
}

void Diana_InitLine(DianaBaseGenObject_type * pRoot)
{
    if (pRoot->m_type == DIANA_BASE_GEN_OBJECT_INDEX)
    {
        Diana_InitIndex((DianaIndexKeyLine * )pRoot);
        return;
    }
    if (pRoot->m_type == DIANA_BASE_GEN_OBJECT_LINE)
    {
        Diana_InitRegularLine((DianaCmdKeyLine *)pRoot);
        return;
    }
}

void Diana_ResetPrefixes(DianaContext * pContext)
{
    Diana_Normal(pContext);
}

void Diana_Init()
{
    // init prefixes
    DianaBaseGenObject_type * pRoot = Diana_GetRootLine();
    Diana_InitLine(pRoot);

    Diana_InitUtils();
}

void Diana_CacheEatOneSafe(DianaContext * pContext)
{
    if (!pContext->cacheSize || pContext->cacheIt>=DI_CACHE_SIZE)
    {
        return;
    }
    ++pContext->cacheIt;
    --pContext->cacheSize;
    return;
}


void Diana_AllocatorInit(Diana_Allocator * pAllocator,
                         Diana_Alloc_type alloc,    
                         Diana_Free_type free,
                         Diana_Patcher_type patch)
{
    pAllocator->m_alloc = alloc;
    pAllocator->m_free = free;
    pAllocator->m_patch = patch;
}

int Diana_OnError(int code)
{
    return code;
}

int Diana_ConvertOpSizeToSizeT(const OPERAND_SIZE * pOpSize, DIANA_SIZE_T * pSizeT)
{
     *pSizeT = 0;
#ifdef DIANA_CFG_I386 
    if (*pOpSize > 0xFFFFFFFFULL)
    {
        return DI_ERROR;
    }
#endif
    *pSizeT = (DIANA_SIZE_T)*pOpSize;
    return DI_SUCCESS;
}
int Diana_SafeAdd(OPERAND_SIZE * pResult, OPERAND_SIZE arg)
{
    if ((0xFFFFFFFFFFFFFFFFULL - *pResult) < arg)
    {
        return DI_OVERFLOW;
    }
    *pResult += arg;
    return DI_SUCCESS;
}


const char * Diana_QueryErrorText_Silent(int value)
{
#define DI_ERR_DEF_CASE(X)  case X: return #X;

    switch(value)
    {
    DI_ERR_DEF_CASE(DI_SUCCESS)
    DI_ERR_DEF_CASE(DI_END)
    DI_ERR_DEF_CASE(DI_ERROR)
    DI_ERR_DEF_CASE(DI_OUT_OF_MEMORY)
    DI_ERR_DEF_CASE(DI_DIVISION_BY_ZERO)
    DI_ERR_DEF_CASE(DI_UNSUPPORTED_COMMAND)
    DI_ERR_DEF_CASE(DI_DIVISION_OVERFLOW)
    DI_ERR_DEF_CASE(DI_INVALID_CONFIGURATION)
    DI_ERR_DEF_CASE(DI_WIN32_ERROR)
    DI_ERR_DEF_CASE(DI_ERROR_NOT_USED_BY_CORE)
    DI_ERR_DEF_CASE(DI_INVALID_INPUT)
    DI_ERR_DEF_CASE(DI_INVALID_OPCODE)
    DI_ERR_DEF_CASE(DI_END_OF_STREAM)
    DI_ERR_DEF_CASE(DI_GP)
    DI_ERR_DEF_CASE(DI_INTERRUPT)
    DI_ERR_DEF_CASE(DI_PARTIAL_READ_WRITE)
    DI_ERR_DEF_CASE(DI_ERROR_NOT_IMPLEMENTED)
    DI_ERR_DEF_CASE(DI_OVERFLOW)
    DI_ERR_DEF_CASE(DI_NOT_FOUND)
    }

#undef DI_ERR_DEF_CASE
    return 0;
}