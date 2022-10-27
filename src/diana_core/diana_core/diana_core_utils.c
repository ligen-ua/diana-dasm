#include "diana_core_utils.h"

// extern, uses at commmon.c
DianaUnifiedRegister g_diana_regsAmd8[4] = {reg_SPL, reg_BPL, reg_SIL, reg_DIL};

static
DianaUnifiedRegister g_regs[4][16] = 
{
    {    reg_AL,    reg_CL,    reg_DL,    reg_BL,    reg_AH,    reg_CH,    reg_DH,    reg_BH,
         reg_R8B,   reg_R9B,   reg_R10B,  reg_R11B,  reg_R12B,  reg_R13B,  reg_R14B,  reg_R15B},
    {    reg_AX,    reg_CX,    reg_DX,    reg_BX,    reg_SP,    reg_BP,    reg_SI,    reg_DI,
         reg_R8W,   reg_R9W,   reg_R10W,  reg_R11W,  reg_R12W,  reg_R13W,  reg_R14W,  reg_R15W},
    {    reg_EAX,   reg_ECX,   reg_EDX,   reg_EBX,   reg_ESP,   reg_EBP,   reg_ESI,   reg_EDI,
         reg_R8D,   reg_R9D,   reg_R10D,  reg_R11D,  reg_R12D,  reg_R13D,  reg_R14D,  reg_R15D},
    {    reg_RAX,   reg_RCX,   reg_RDX,   reg_RBX,   reg_RSP,   reg_RBP,   reg_RSI,   reg_RDI,
         reg_R8,    reg_R9,    reg_R10,   reg_R11,   reg_R12,   reg_R13,   reg_R14,   reg_R15}
};


static DianaUnifiedRegister table3[2][16]= 
{
    {reg_MM0, reg_MM1, reg_MM2, reg_MM3, reg_MM4, reg_MM5, reg_MM6, reg_MM7, 
     reg_MM0, reg_MM1, reg_MM2, reg_MM3, reg_MM4, reg_MM5, reg_MM6, reg_MM7},
    {reg_XMM0, reg_XMM1, reg_XMM2, reg_XMM3, reg_XMM4, reg_XMM5, reg_XMM6, reg_XMM7,
     reg_XMM8, reg_XMM9, reg_XMM10, reg_XMM11, reg_XMM12, reg_XMM13, reg_XMM14, reg_XMM15}
};

static DianaCmdInfo g_rexInfo;
static DianaCmdInfo g_Diana_Rep;
static DianaCmdInfo g_Diana_Repn;
static DianaCmdInfo g_Diana_Lock;
static DianaCmdInfo g_Diana_OpSize;
static DianaCmdInfo g_Diana_AddrSize;
static DianaCmdInfo g_Diana_CS;
static DianaCmdInfo g_Diana_SS;
static DianaCmdInfo g_Diana_DS;
static DianaCmdInfo g_Diana_ES;
static DianaCmdInfo g_Diana_FS;
static DianaCmdInfo g_Diana_GS;
                

int DianaRecognizeMMX(DI_CHAR regId, 
                      DianaUnifiedRegister * pOut)
{
    if (regId < sizeof(table3[0])/sizeof(table3[0][0]))
    {
        *pOut = table3[0][regId];
        return DI_SUCCESS;
    }
    return DI_ERROR;
}

int DianaRecognizeXMM(DI_CHAR regId, 
                      DianaUnifiedRegister * pOut)
{
    if (regId < sizeof(table3[1])/sizeof(table3[1][0]))
    {
        *pOut = table3[1][regId];
        return DI_SUCCESS;
    }
    return DI_ERROR;
}

int DianaRecognizeCommonReg(DI_CHAR iOpSize,
                            DI_CHAR regId, 
                            DianaUnifiedRegister * pOut,
                            int isRexPrefix)
{
    int iIndex = 0;
    switch (iOpSize)
    {
    default:
        return DI_ERROR;

    case 8:
        iIndex = 3;
        break;
    case 4:
        iIndex = 2;
        break;
    case 2:
        iIndex = 1;
        break;
    case 1: 
        iIndex = 0;
    };

    if (regId < sizeof(g_regs[0])/sizeof(g_regs[0][0]))
    {
        *pOut = g_regs[iIndex][regId];
        if (isRexPrefix && iIndex==0 && regId >= 4 && regId < 8)
        {
            // SIL, DIL fix 
            *pOut = g_diana_regsAmd8[regId-4];
        }
        return DI_SUCCESS;
    }
    return DI_ERROR;
}


int Di_ProcessRexPrefix(unsigned char value,
                        int * pbPrefixFound, 
                        DianaContext * pContext,
                        DianaParserResult * pResult)
{
    *pbPrefixFound = 0;

    if (pContext->iAMD64Mode && value >= DI_REX_PREFIX_START && value <= DI_REX_PREFIX_END)
    {
        pContext->iRexPrefixValue = value;
        pResult->pInfo = &g_rexInfo;
        *pbPrefixFound = 1;
    }
    return 0;
}

int Di_ProcessCustomPrefix(unsigned char value,
                           int * pbPrefixFound, 
                           DianaContext * pContext,
                           DianaParserResult * pResult)
{
    // result: IsPrefixCanNotBeUsedWithRepInstruction
    int result = 1;
    pContext;

    *pbPrefixFound = 0;

    switch(value)
    {
    case 0xF3: //     L"REP",                   L"REPE/REPZ prefix",           true},
        if (pResult)
            pResult->pInfo = &g_Diana_Rep;
        break;
    case 0xF2: //     L"REPN",                  L"REPNE/REPNZ prefix",         true},
        if (pResult)
            pResult->pInfo = &g_Diana_Repn;
        break;
    case 0xF0: //     L"LOCK",                  L"LOCK prefix",                true},
        if (pResult)
            pResult->pInfo = &g_Diana_Lock;
        break;
    case 0x2E: //     L"prefix_CS",             L"CS segment override prefix", true},
        if (pResult)
            pResult->pInfo = &g_Diana_CS;
        break;
    case 0x36: //     L"prefix_SS",             L"SS segment override prefix", true},
        if (pResult)
            pResult->pInfo = &g_Diana_SS;
        break;
    case 0x3E: //     L"prefix_DS",             L"DS segment override prefix", true},
        if (pResult)
            pResult->pInfo = &g_Diana_DS;
        break;
    case 0x26: //     L"prefix_ES",             L"ES segment override prefix", true},
        if (pResult)
            pResult->pInfo = &g_Diana_ES;
        break;
    case 0x64: //     L"prefix_FS",             L"FS segment override prefix", true},
        if (pResult)
            pResult->pInfo = &g_Diana_FS;
        break;
    case 0x65: //     L"prefix_GS",             L"GS segment override prefix", true},
        if (pResult)
            pResult->pInfo = &g_Diana_GS;
        break;
    case 0x66: //     L"prefix_OperandSize",    L"Operand-size override",      true},
        if (pResult)
            pResult->pInfo = &g_Diana_OpSize;
        result = 0;
        break;
    case 0x67: //     L"prefix_AddressSize",    L"Address-size override",      true}
        if (pResult)
            pResult->pInfo = &g_Diana_AddrSize;
        result = 0;
        break;
    default:
        return result;
    }
    *pbPrefixFound = 1;
    return result;
}


void Diana_DispatchSIB(unsigned char sib, int * pSS, int *pIndex, int *pBase)
{
    *pSS = (sib&0xC0)>>6;
    *pIndex = (sib & 0x38) >> 3;
    *pBase = sib & 7;
}
DI_CHAR Diana_GetReg(unsigned char postbyte)
{
    return (postbyte & 0x38) >> 3;
}
DI_CHAR Diana_GetRm(unsigned char postbyte)
{
    return postbyte & 7;
}
DI_CHAR Diana_GetMod(unsigned char postbyte)
{
    return (postbyte & 0xC0) >> 6;
}


static void Diana_Lock(struct _dianaContext * pContext) 
{  
    pContext->iPrefix = DI_PREFIX_LOCK;
    pContext->iRexPrefix = 0;
}
static void Diana_Rep(struct _dianaContext * pContext) 
{  
    pContext->iPrefix = DI_PREFIX_REP;
    pContext->iRexPrefix = 0;
}
static void Diana_Repn(struct _dianaContext * pContext) 
{  
    pContext->iPrefix = DI_PREFIX_REPN;
    pContext->iRexPrefix = 0;
}
static void Diana_CS(struct _dianaContext * pContext) 
{  
    pContext->currentCmd_sreg = reg_CS;
    pContext->iRexPrefix = 0;
}
static void Diana_SS(struct _dianaContext * pContext) 
{  
    pContext->currentCmd_sreg = reg_SS;
    pContext->iRexPrefix = 0;
}
static void Diana_DS(struct _dianaContext * pContext) 
{  
    pContext->currentCmd_sreg = reg_DS;
    pContext->iRexPrefix = 0;
}
static void Diana_ES(struct _dianaContext * pContext) 
{  
    pContext->currentCmd_sreg = reg_ES;
    pContext->iRexPrefix = 0;
}
static void Diana_FS(struct _dianaContext * pContext) 
{  
    pContext->currentCmd_sreg = reg_FS;
    pContext->iRexPrefix = 0;
}
static void Diana_GS(struct _dianaContext * pContext) 
{  
    pContext->currentCmd_sreg = reg_GS;
    pContext->iRexPrefix = 0;
}
// operand size override
static void Diana_OpSize(struct _dianaContext * pContext) 
{  
    pContext->iRexPrefix = 0;

    if (pContext->iSizePrefixes & DIANA_HAS_OPERAND_SIZE_PREFIX)
        return;

    pContext->iSizePrefixes |= DIANA_HAS_OPERAND_SIZE_PREFIX;

    if (pContext->iCurrentCmd_opsize == 8)
    {
        pContext->iSizePrefixes |= DIANA_INVALID_STATE;
        return;
    }
    pContext->iCurrentCmd_opsize = 6 - pContext->iMainMode_opsize;
}
static void Diana_AddrSize(struct _dianaContext * pContext) 
{  
    pContext->iRexPrefix = 0;

    if (pContext->iSizePrefixes & DIANA_HAS_ADDRESS_SIZE_PREFIX)
        return;

    pContext->iSizePrefixes |= DIANA_HAS_ADDRESS_SIZE_PREFIX;
    if (pContext->iCurrentCmd_addressSize == 8)
    {
        pContext->iCurrentCmd_addressSize = 4;
        return;
    }
    pContext->iCurrentCmd_addressSize = 6 - pContext->iMainMode_addressSize;
}

static void Diana_Rex(struct _dianaContext * pContext) 
{
    pContext->iRexPrefix = pContext->iRexPrefixValue;
    if (DI_REX_HAS_FLAG_W(pContext->iRexPrefix))
    {
        pContext->iCurrentCmd_opsize = 8;
    }
}

void Diana_InitUtils()
{
    // prefixes
    g_rexInfo.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;
    g_rexInfo.m_linkedPrefixFnc = Diana_Rex;


    g_Diana_Lock.m_linkedPrefixFnc = Diana_Lock;
    g_Diana_Lock.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_Rep.m_linkedPrefixFnc = Diana_Rep;
    g_Diana_Rep.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_Repn.m_linkedPrefixFnc = Diana_Repn;
    g_Diana_Repn.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_OpSize.m_linkedPrefixFnc = Diana_OpSize;
    g_Diana_OpSize.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_AddrSize.m_linkedPrefixFnc = Diana_AddrSize;
    g_Diana_AddrSize.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_CS.m_linkedPrefixFnc = Diana_CS;
    g_Diana_CS.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_SS.m_linkedPrefixFnc = Diana_SS;
    g_Diana_SS.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_DS.m_linkedPrefixFnc = Diana_DS;
    g_Diana_DS.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_ES.m_linkedPrefixFnc = Diana_ES;
    g_Diana_ES.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_FS.m_linkedPrefixFnc = Diana_FS;
    g_Diana_FS.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;

    g_Diana_GS.m_linkedPrefixFnc = Diana_GS;
    g_Diana_GS.m_flags = DI_FLAG_CMD_IS_TRUE_PREFIX;
}

DI_CHAR Diana_CacheEatOne(DianaContext * pContext)
{
    DI_CHAR data = pContext->cache[pContext->cacheIt];
    if (!pContext->cacheSize || pContext->cacheIt>=DI_CACHE_SIZE)
    {
        Diana_FatalBreak();
    }
    ++pContext->cacheIt;
    --pContext->cacheSize;
    return data;
}


int Diana_ReadCache(DianaContext * pContext,
                    DianaReadStream * readStream,
                    int bOptimize)
{
    if (bOptimize)
    {
        memmove(pContext->cache + DI_CACHE_RESERVED, 
                pContext->cache + pContext->cacheIt, 
                pContext->cacheSize);
        pContext->cacheIt = DI_CACHE_RESERVED;
    }

    if (pContext->cacheIt >= DI_CACHE_SIZE)
    {
        pContext->cacheIt = DI_CACHE_RESERVED;
    }

    if (pContext->cacheIt + pContext->cacheSize < DI_CACHE_SIZE)
    {
        // first read 
        int bytesRead = 0;
        int iResult = readStream->pReadFnc(readStream, 
                                           pContext->cache + pContext->cacheIt + pContext->cacheSize, 
                                           DI_CACHE_SIZE - pContext->cacheSize - pContext->cacheIt, 
                                           &bytesRead);
        if (iResult != DI_END)
        {
            if (iResult != DI_SUCCESS)
                return iResult;
            pContext->cacheSize += bytesRead;
        }
        
    }

    if (pContext->cacheSize == 0)
        return DI_END;

    return DI_SUCCESS;
}
