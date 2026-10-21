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


static DianaUnifiedRegister table3[3][16]=
{
    {reg_MM0, reg_MM1, reg_MM2, reg_MM3, reg_MM4, reg_MM5, reg_MM6, reg_MM7,
     reg_MM0, reg_MM1, reg_MM2, reg_MM3, reg_MM4, reg_MM5, reg_MM6, reg_MM7},
    {reg_XMM0, reg_XMM1, reg_XMM2, reg_XMM3, reg_XMM4, reg_XMM5, reg_XMM6, reg_XMM7,
     reg_XMM8, reg_XMM9, reg_XMM10, reg_XMM11, reg_XMM12, reg_XMM13, reg_XMM14, reg_XMM15},
    {reg_YMM0, reg_YMM1, reg_YMM2, reg_YMM3, reg_YMM4, reg_YMM5, reg_YMM6, reg_YMM7,
     reg_YMM8, reg_YMM9, reg_YMM10, reg_YMM11, reg_YMM12, reg_YMM13, reg_YMM14, reg_YMM15}
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

DianaUnifiedRegister Diana_VectorRegister(int size, int number)
{
    if (number < 0 || number > 31)
        return reg_none;
    switch (size)
    {
    case 16:
        return (DianaUnifiedRegister)(number < 16 ? reg_XMM0 + number : reg_XMM16 + number - 16);
    case 32:
        return (DianaUnifiedRegister)(number < 16 ? reg_YMM0 + number : reg_YMM16 + number - 16);
    case 64:
        return (DianaUnifiedRegister)(reg_ZMM0 + number);
    }
    return reg_none;
}

int Diana_VectorRegisterNumber(DianaUnifiedRegister reg, int * pSize)
{
    int size = 0, number = -1;
    if (reg >= reg_XMM0 && reg <= reg_XMM15)
        size = 16, number = reg - reg_XMM0;
    else if (reg >= reg_XMM16 && reg <= reg_XMM31)
        size = 16, number = reg - reg_XMM16 + 16;
    else if (reg >= reg_YMM0 && reg <= reg_YMM15)
        size = 32, number = reg - reg_YMM0;
    else if (reg >= reg_YMM16 && reg <= reg_YMM31)
        size = 32, number = reg - reg_YMM16 + 16;
    else if (reg >= reg_ZMM0 && reg <= reg_ZMM31)
        size = 64, number = reg - reg_ZMM0;
    if (pSize)
        *pSize = size;
    return number;
}

int DianaRecognizeXMM(DI_CHAR regId,
                      DianaUnifiedRegister * pOut)
{
    // 16..31: EVEX only
    if ((unsigned char)regId < 32)
    {
        *pOut = Diana_VectorRegister(16, (unsigned char)regId);
        return DI_SUCCESS;
    }
    return DI_ERROR;
}

int DianaRecognizeYMM(DI_CHAR regId,
                      DianaUnifiedRegister * pOut)
{
    if ((unsigned char)regId < 32)
    {
        *pOut = Diana_VectorRegister(32, (unsigned char)regId);
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
    // 64-bit mode: no effect on the address, an fs or gs before stays (the override is still reported)
    if (!pContext->iAMD64Mode || (pContext->currentCmd_sreg != reg_FS && pContext->currentCmd_sreg != reg_GS))
        pContext->currentCmd_sreg = reg_CS;
    pContext->iRexPrefix = 0;
}
static void Diana_SS(struct _dianaContext * pContext) 
{  
    // 64-bit mode: no effect on the address, an fs or gs before stays (the override is still reported)
    if (!pContext->iAMD64Mode || (pContext->currentCmd_sreg != reg_FS && pContext->currentCmd_sreg != reg_GS))
        pContext->currentCmd_sreg = reg_SS;
    pContext->iRexPrefix = 0;
}
static void Diana_DS(struct _dianaContext * pContext) 
{  
    // 64-bit mode: no effect on the address, an fs or gs before stays (the override is still reported)
    if (!pContext->iAMD64Mode || (pContext->currentCmd_sreg != reg_FS && pContext->currentCmd_sreg != reg_GS))
        pContext->currentCmd_sreg = reg_DS;
    pContext->iRexPrefix = 0;
}
static void Diana_ES(struct _dianaContext * pContext) 
{  
    // 64-bit mode: no effect on the address, an fs or gs before stays (the override is still reported)
    if (!pContext->iAMD64Mode || (pContext->currentCmd_sreg != reg_FS && pContext->currentCmd_sreg != reg_GS))
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

int Di_ProcessVexPrefix(unsigned char firstByte,
                        int * pbVexFound,
                        DianaContext * pContext,
                        DianaReadStream * readStream)
{
    int iResult = 0;
    unsigned char b2, b3;
    int R, X, B, W, L, pp, map;
    unsigned char selectorByte;
    int cacheIt = pContext->cacheIt;
    int cacheSize = pContext->cacheSize;

    *pbVexFound = 0;

    if (firstByte == 0xC5)
    {
        // 2-byte VEX: need 1 more byte
        if (cacheSize < 1)
        {
            int bytesRead = 0;
            iResult = readStream->pReadFnc(readStream,
                                           pContext->cache + cacheIt + cacheSize,
                                           1, &bytesRead);
            if (iResult) return iResult;
            cacheSize += bytesRead;
        }
        if (cacheSize < 1) return DI_END_OF_STREAM;

        b2 = pContext->cache[cacheIt];
        if (!pContext->iAMD64Mode && (b2 & 0xC0) != 0xC0)
        {
            // lds: keep what was read
            pContext->cacheSize = cacheSize;
            return DI_SUCCESS;
        }
        R      = (~b2 >> 7) & 1;
        pContext->iVexVVVV = (~b2 >> 3) & 0xF;
        L      = (b2 >> 2) & 1;
        pp     = b2 & 0x3;
        map    = 0;  // 0F map
        W      = 0;
        X      = 0;
        B      = 0;
        ++cacheIt; --cacheSize;
    }
    else  /* 0xC4 */
    {
        // 3-byte VEX: need 2 more bytes
        while (cacheSize < 2)
        {
            int bytesRead = 0;
            iResult = readStream->pReadFnc(readStream,
                                           pContext->cache + cacheIt + cacheSize,
                                           2 - cacheSize, &bytesRead);
            if (iResult) return iResult;
            if (!bytesRead)
            {
                // the end of the stream: les/lds or a cut VEX prefix, keep what was read
                pContext->cacheSize = cacheSize;
                return pContext->iAMD64Mode ? DI_END_OF_STREAM : DI_SUCCESS;
            }
            cacheSize += bytesRead;
        }
        if (cacheSize < 2) return DI_END_OF_STREAM;

        b2 = pContext->cache[cacheIt];
        b3 = pContext->cache[cacheIt + 1];
        if (!pContext->iAMD64Mode && (b2 & 0xC0) != 0xC0)
        {
            // les: keep what was read
            pContext->cacheSize = cacheSize;
            return DI_SUCCESS;
        }

        R = (~b2 >> 7) & 1;
        X = (~b2 >> 6) & 1;
        B = (~b2 >> 5) & 1;
        {
            int mmmmm = b2 & 0x1F;
            switch (mmmmm)
            {
            case 1:  map = 0; break;  // 0F
            case 2:  map = 1; break;  // 0F38
            case 3:  map = 2; break;  // 0F3A
            default:
                // a reserved map: #UD (objdump: (bad))
                pContext->cacheSize = cacheSize;
                return DI_ERROR;
            }
        }
        W      = (b3 >> 7) & 1;
        pContext->iVexVVVV = (~b3 >> 3) & 0xF;
        L      = (b3 >> 2) & 1;
        pp     = b3 & 0x3;

        cacheIt  += 2;
        cacheSize -= 2;
    }

    // the raw W: a W0 instruction with W1 is #UD in every mode (DIANA_BASE_GEN_OBJECT_VEX_W0)
    pContext->iVexPrefix = (DI_CHAR)(DI_VEX_PREFIX_PRESENT | (W ? DI_VEX_PREFIX_W : 0));
    if (!pContext->iAMD64Mode)
    {
        // 16/32-bit: no register above 7, W of the general registers is ignored; vvvv bit 3 is ignored where vvvv
        // is a register (DIANA_VEX_VVVV_REG), an instruction without a vvvv operand needs 1111 (the CPU: #UD)
        R = X = B = W = 0;
    }
    pContext->iRexPrefix = (DI_CHAR)(0x40 | (W << 3) | (R << 2) | (X << 1) | B);
    if (W)
        pContext->iCurrentCmd_opsize = 8;

    // virtual selector byte encodes pp, map, L
    selectorByte = (unsigned char)((pp << 3) | (map << 1) | L);

    // pack remaining real bytes at cache[2..], then put virtual prefix at [0,1]
    memmove(pContext->cache + DI_CACHE_RESERVED, pContext->cache + cacheIt, cacheSize);
    pContext->cache[0] = 0xC5;
    pContext->cache[1] = selectorByte;
    pContext->cacheIt  = 0;
    pContext->cacheSize = cacheSize + DI_CACHE_RESERVED;

    // top up cache from stream
    if (pContext->cacheIt + pContext->cacheSize < DI_CACHE_SIZE)
    {
        int bytesRead = 0;
        readStream->pReadFnc(readStream,
                             pContext->cache + pContext->cacheIt + pContext->cacheSize,
                             DI_CACHE_SIZE - pContext->cacheSize - pContext->cacheIt,
                             &bytesRead);
        pContext->cacheSize += bytesRead;
    }

    *pbVexFound = 1;
    return DI_SUCCESS;
}

int Di_ProcessEvexPrefix(int * pbEvexFound,
                         DianaContext * pContext,
                         DianaReadStream * readStream)
{
    unsigned char p0, p1, p2;
    int R, X, B, R1, V1, W, LL, pathLL, pp, map, z, b;
    unsigned char selectorByte;
    int cacheIt = pContext->cacheIt;
    int cacheSize = pContext->cacheSize;

    *pbEvexFound = 0;

    // P0 P1 P2, the opcode and the ModRM byte (EVEX.b on a register form changes the path)
    while (cacheSize < 5 && cacheIt + cacheSize < DI_CACHE_SIZE)
    {
        int bytesRead = 0;
        int iResult = readStream->pReadFnc(readStream, pContext->cache + cacheIt + cacheSize,
                                           5 - cacheSize, &bytesRead);
        if (iResult) return iResult;
        if (!bytesRead)
            break;
        cacheSize += bytesRead;
    }
    if (cacheSize < 1 || (!pContext->iAMD64Mode && (pContext->cache[cacheIt] & 0xC0) != 0xC0))
    {
        // bound (or the end of the stream): keep what was read
        pContext->cacheSize = cacheSize;
        return pContext->iAMD64Mode && cacheSize < 1 ? DI_END_OF_STREAM : DI_SUCCESS;
    }
    if (cacheSize < 4)
    {
        pContext->cacheSize = cacheSize;
        return DI_END_OF_STREAM;
    }
    p0 = pContext->cache[cacheIt];
    p1 = pContext->cache[cacheIt + 1];
    p2 = pContext->cache[cacheIt + 2];

    // P0: R X B R' 0 m m m (bit 3 is APX: #UD here); maps 1, 2, 3, 5, 6
    // P1: W v v v v 1 p p (bit 2 0 is #UD: AVX10.2/APX)
    // P2: z L' L b V' a a a
    map = p0 & 7;
    if ((p0 & 0x08) || !(p1 & 0x04) || map == 0 || map == 4 || map == 7)
    {
        pContext->cacheSize = cacheSize;
        return DI_ERROR;
    }
    R  = (~p0 >> 7) & 1;
    X  = (~p0 >> 6) & 1;
    B  = (~p0 >> 5) & 1;
    R1 = (~p0 >> 4) & 1;
    W  = (p1 >> 7) & 1;
    pp = p1 & 3;
    z  = (p2 >> 7) & 1;
    LL = (p2 >> 5) & 3;
    b  = (p2 >> 4) & 1;
    V1 = (~p2 >> 3) & 1;

    pContext->iVexVVVV = (~p1 >> 3) & 0xF;
    pContext->iVexPrefix = (DI_CHAR)(DI_VEX_PREFIX_PRESENT | (W ? DI_VEX_PREFIX_W : 0));
    pContext->iEvexAAA = (DI_CHAR)(p2 & 7);
    pContext->iEvexLL = (DI_CHAR)LL;
    if (!pContext->iAMD64Mode)
    {
        // 16/32-bit: no register above 7: V' is 1 (not inverted: 0), else #UD; R' and B are ignored (the CPU,
        // evex_probe.py --mode32), W of the general registers too (W stays in the path: it selects
        // vmovdqu32/vmovdqu64)
        if (V1)
        {
            pContext->cacheSize = cacheSize;
            return DI_ERROR;
        }
        R = X = B = R1 = 0;
    }
    pContext->iEvexPrefix = (DI_CHAR)(DI_EVEX_PREFIX_PRESENT | (R1 ? DI_EVEX_PREFIX_R1 : 0) |
                                      (V1 ? DI_EVEX_PREFIX_V1 : 0) | (X ? DI_EVEX_PREFIX_X : 0) |
                                      (b ? DI_EVEX_PREFIX_B : 0) | (z ? DI_EVEX_PREFIX_Z : 0));
    pContext->iRexPrefix = (DI_CHAR)(0x40 | ((pContext->iAMD64Mode && W) << 3) | (R << 2) | (X << 1) | B);
    if (pContext->iAMD64Mode && W)
        pContext->iCurrentCmd_opsize = 8;

    // EVEX.b on a register form: L'L is the rounding, the vector length is 512 (or a scalar)
    pathLL = LL;
    if (b && cacheSize >= 5 && (pContext->cache[cacheIt + 4] & 0xC0) == 0xC0)
        pathLL = 2;
    selectorByte = (unsigned char)((W << 7) | (pathLL << 5) | (map << 2) | pp);

    cacheIt += 3;
    cacheSize -= 3;
    memmove(pContext->cache + DI_CACHE_RESERVED, pContext->cache + cacheIt, cacheSize);
    pContext->cache[0] = 0x62;
    pContext->cache[1] = selectorByte;
    pContext->cacheIt  = 0;
    pContext->cacheSize = cacheSize + DI_CACHE_RESERVED;

    if (pContext->cacheIt + pContext->cacheSize < DI_CACHE_SIZE)
    {
        int bytesRead = 0;
        readStream->pReadFnc(readStream,
                             pContext->cache + pContext->cacheIt + pContext->cacheSize,
                             DI_CACHE_SIZE - pContext->cacheSize - pContext->cacheIt,
                             &bytesRead);
        pContext->cacheSize += bytesRead;
    }
    *pbEvexFound = 1;
    return DI_SUCCESS;
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
