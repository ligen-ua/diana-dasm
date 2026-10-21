#include "diana_operands.h"
#include "diana_tables_16.h"
#include "diana_tables_32.h"

int Diana_GetDI(DI_CHAR reg, DianaUnifiedRegister * pReg)
{
    switch(reg)
    {
        case 8:
            *pReg = reg_RDI;
            break;
        case 4:
            *pReg = reg_EDI;
            break;
        case 2:
            *pReg = reg_DI;
            break;
        default:
            return DI_ERROR;
    };
    return 0;
}
int Diana_GetSI(DI_CHAR reg, DianaUnifiedRegister * pReg)
{
    switch(reg)
    {
        case 8:
            *pReg = reg_RSI;
            break;
        case 4:
            *pReg = reg_ESI;
            break;
        case 2:
            *pReg = reg_SI;
            break;
        default:
            return DI_ERROR;
    };
    return 0;
}

// sregs
static DianaUnifiedRegister g_sregs[]  = {reg_ES,    reg_CS,    reg_SS,    reg_DS,    reg_FS,    reg_GS};
int Diana_RecognizeSreg(DI_CHAR reg, DianaUnifiedRegister * pReg)
{
    if (reg<0)
        return DI_ERROR;
    if (reg>=sizeof(g_sregs)/sizeof(g_sregs[0]))
        return DI_ERROR;
    *pReg = g_sregs[reg];
    return 0;
}

// cregs
static DianaUnifiedRegister g_cregs[] = {reg_CR0,   reg_CR1,   reg_CR2,   reg_CR3,   reg_CR4,   reg_CR5,   reg_CR6,   reg_CR7,
                                         reg_CR8,   reg_CR9,   reg_CR10,   reg_CR11,   reg_CR12,   reg_CR13,   reg_CR14,   reg_CR15};
int Diana_RecognizeCreg(DI_CHAR reg, DianaUnifiedRegister * pReg)
{
    if (reg<0)
        return DI_ERROR;
    if (reg>=sizeof(g_cregs)/sizeof(g_cregs[0]))
        return DI_ERROR;
    *pReg = g_cregs[reg];
    return 0;
}

// dregs
static DianaUnifiedRegister g_dregs[] = {reg_DR0,   reg_DR1,   reg_DR2,   reg_DR3,   reg_DR4,   reg_DR5,   reg_DR6,   reg_DR7,
                                         reg_DR8,   reg_DR9,   reg_DR10,   reg_DR11,   reg_DR12,   reg_DR13,   reg_DR14,   reg_DR15};
int Diana_RecognizeDreg(DI_CHAR reg, DianaUnifiedRegister * pReg)
{
    if (reg<0)
        return DI_ERROR;
    if (reg>=sizeof(g_dregs)/sizeof(g_dregs[0]))
        return DI_ERROR;
    *pReg = g_dregs[reg];
    return 0;
}
// tregs
static DianaUnifiedRegister g_tregs[] = {reg_TR0,   reg_TR1,   reg_TR2,   reg_TR3,   reg_TR4,   reg_TR5,   reg_TR6,   reg_TR7};
int Diana_RecognizeTreg(DI_CHAR reg, DianaUnifiedRegister * pReg)
{
    if (reg<0)
        return DI_ERROR;
    if (reg>=sizeof(g_tregs)/sizeof(g_tregs[0]))
        return DI_ERROR;
    *pReg = g_tregs[reg];
    return 0;
}


static void InitLinkedOp(DianaLinkedOperand * pLinkedOp)
{
    DIANA_MEMSET(pLinkedOp, 0, sizeof(*pLinkedOp));
    pLinkedOp->iOffset = 0;
    pLinkedOp->type = diana_none;
}


Diana_ReadIndexStructure_type DianaGetHandler(int i32BitUsed) 
{
    if (i32BitUsed)
        return &Diana_ReadIndexStructure32;
    else
        return &Diana_ReadIndexStructure16;
}

int DianaReadValueAsLong(DianaReadStream * readStream, 
                         int iValueSize, 
                         DI_UINT64 * pRes, 
                         int iSigned)
{
    char buffer[sizeof(DI_UINT64)];
    int readBytes = 0;
    int iRes = 0;
    
    switch(iValueSize)
    {
    case 1:
    case 2:
    case 4:
    case 8:
        break;
    default:
        return DI_ERROR;
    };

    iRes = readStream->pReadFnc(readStream, buffer, iValueSize, &readBytes);
    if (iRes)
        return iRes;
    if (readBytes != iValueSize)
        return DI_ERROR;

    switch(iValueSize)
    {
    case 1:
        *pRes = *(unsigned char*)buffer;
        if (iSigned)
            if (*pRes & 0x80)
                *pRes = (DI_UINT64)*pRes - 1 - 0xFFUL;
        break;
    case 2:
        *pRes = *(unsigned short*)buffer;
        if (iSigned)
            if (*pRes & 0x8000)
                *pRes = (DI_UINT64)*pRes - 1 - 0xFFFFUL;
        break;
    case 4:
        *pRes = *(unsigned int*)buffer;
        if (iSigned)
            if (*pRes & 0x80000000)
                *pRes = (DI_UINT64)*pRes - 1 - 0xFFFFFFFFUL;
        break;

    case 8:
        *pRes = *(DI_UINT64*)buffer;
        if (iSigned)
            if (*pRes & 0x8000000000000000ULL)
                *pRes = (DI_UINT64)*pRes - 1 - 0xFFFFFFFFFFFFFFFFULL;
        break;
    };
    
    return iRes;
}

#define  MAX_IMM  (2)

DI_CHAR Diana_GetReg2(DianaContext * pContext, unsigned char postByte)
{
    if (pContext->iAMD64Mode && pContext->iRexPrefix)
    {
        if (DI_REX_HAS_FLAG_R(pContext->iRexPrefix))
        {
            DI_CHAR res = Diana_GetReg( postByte );
            res |= 0x08;
            return res;
        }
    }
    return Diana_GetReg( postByte );
}

DI_CHAR Diana_GetRm2(DianaContext * pContext, unsigned char postByte)
{
    if (pContext->iAMD64Mode && pContext->iRexPrefix)
    {
        if (DI_REX_HAS_FLAG_B(pContext->iRexPrefix))
        {
            DI_CHAR res = Diana_GetRm( postByte );
            res |= 0x08;
            return res;
        }
    }
    return Diana_GetRm( postByte );
}

// the register of VEX.vvvv: bit 3 is ignored outside 64-bit mode
#define DIANA_VEX_VVVV_REG(pContext) ((DI_CHAR)((pContext)->iVexVVVV & ((pContext)->iAMD64Mode ? 0xF : 7)))
// EVEX: bit 4 of a vector register, R' (ModRM.reg), V' (vvvv), X (ModRM.rm with mod 11)
#define DIANA_EVEX_BIT4(pContext, flag) (((pContext)->iEvexPrefix & (flag)) ? 0x10 : 0)

// the size of a vector register operand type, 0 for another type
static int Diana_VectorTypeSize(int type)
{
    switch (type)
    {
    case diana_orRegXMM: case diana_orMemoryXMM: return 16;
    case diana_orRegYMM: case diana_orMemoryYMM: return 32;
    case diana_orRegZMM: case diana_orMemoryZMM: return 64;
    }
    return 0;
}

// the segment of an r/m memory operand: a segment prefix, else SS when the base is BP or SP (16-bit: BP, the
// 16-bit tables keep it in indexed_reg), else DS (Intel SDM 3.7.4)
static DianaUnifiedRegister Diana_GetMemorySegment(DianaContext * pContext, const DianaRmIndex * pIndex)
{
    DI_FULL_CHAR i = 0;
    for (; i < pContext->prefixesCount; ++i)
    {
        switch (pContext->prefixes[i].prefix)
        {
        case 0x26: case 0x2E: case 0x36: case 0x3E: case 0x64: case 0x65:
            return pContext->currentCmd_sreg;
        }
    }
    switch (pIndex->reg)
    {
    case reg_BP: case reg_SP: case reg_EBP: case reg_ESP: case reg_RBP: case reg_RSP:
        return reg_SS;
    default:
        break;
    }
    if (pIndex->indexed_reg == reg_BP)
        return reg_SS;
    return pContext->currentCmd_sreg;
}

int Diana_LinkOperands(DianaContext * pContext, //IN
                       DianaParserResult * pResult, 
                       DianaReadStream * readStream)
{
    int i = 0;
    int iDispIsSigned = 1;
    int iCurOffset = 0;

    int iRes = 0;
    DI_CHAR fpuRegCode = DI_CHAR_NULL;
    DI_CHAR RegCode = DI_CHAR_NULL;
    DI_FULL_CHAR PostByte = DI_FULL_CHAR_NULL;

    int iRegisterCodeAsOpcodePartUsed = pResult->pInfo->m_flags & DI_FLAG_CMD_REGISTER_AS_OPCODE;
    // a ModRM filter (/digit, a mod or rm mask); the reg field is an opcode extension only when the filter covers it
    // (/r with a mod filter, vmovss /*3: the reg field is an operand)
    int iPostbyteFilter = (pResult->pInfo->m_extension_deny_mask | pResult->pInfo->m_extension_mask);
    int iExtensionUsed = iPostbyteFilter & 0x38;
    int iFullPostbyteUsed = pResult->pInfo->m_flags & DI_FLAG_CMD_POSTBYTE_USED;
    int iCSIPSize = pResult->pInfo->m_iCSIPExtensionSizeInBytes;
    int bHas32BitAnalog = (pResult->pInfo->m_flags & DI_FLAG_CMD_HAS32BIT_ANALOG);

    DianaLinkedOperand * pLinkedImmOp[MAX_IMM] = {0,0};
    DianaLinkedOperand * pLinkedIs4Op = 0;   // VEX is4: a register in imm8[7:4]
    int iCurImm = 0;

    DianaRmIndex * pLinkedDispOp = 0;
    DianaLinkedOperand * pLinkedDispOpFull = 0;
    int iEvexDisp8Scale = 0;    // EVEX: N of disp8*N
        
    if (pContext->iRexPrefix && (pResult->pInfo->m_flags & DI_FLAG_CMD_IGNORE_REX_PREFIX))
    {
        // x87: no register is extended, but X and B still extend the memory operand's index and base
        pContext->iRexPrefix &= 0x43;
        if (pContext->iRexPrefix == 0x40)
            pContext->iRexPrefix = 0;
    }

    if(iPostbyteFilter || iFullPostbyteUsed) 
    {
        // check postbyte and sib
        unsigned char buffer[2];
        int read = 0;
        int err  = 0;
        err = readStream->pReadFnc(readStream, buffer, 2, &read);
        if (err)
            return err;
        if (read != 2)
            return DI_END_OF_STREAM;

        PostByte = buffer[1];
                
        if(!iExtensionUsed)
            RegCode = Diana_GetReg2(pContext, ( DI_CHAR )PostByte);

        // mov to/from CRn, DRn, TRn: the general register is in r/m whatever mod says, no displacement follows
        for (i = 0; i < pResult->pInfo->m_operandCount; ++i)
        {
            int type = pResult->pInfo->m_operands[i].m_type;
            if (type == diana_orAnyCR || type == diana_orDR6or7 || type == diana_orTR6or7)
                PostByte |= 0xC0;
        }

        iCurOffset+=2;
    }
    else
    {
        char buffer[1];

        // no postbyte
        int read = 0;
        int err = 0;
        err = readStream->pReadFnc(readStream, buffer, 1, &read);
        if (err)
            return err;
        if (read != 1)
            return DI_END_OF_STREAM;

        if (iRegisterCodeAsOpcodePartUsed)
        {
            // read register number
            if (pResult->pInfo->m_flags & DI_FLAG_CMD_FPU_I)
            {
                fpuRegCode = Diana_GetRm(buffer[0]);
            }
            else
            {
                RegCode = Diana_GetRm2(pContext, buffer[0]);
            }
        }


        ++iCurOffset;
    }

    if (pContext->iAMD64Mode && pResult->pInfo->m_flags & DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64)
    {
        if (pContext->iCurrentCmd_opsize == 4)
            pContext->iCurrentCmd_opsize = 8;
    }

    // in, out, ins, outs: the port is 8, 16 or 32 bits, REX.W has no effect (objdump: rex.W in eax,dx)
    if (pContext->iCurrentCmd_opsize == 8)
    {
        switch (pResult->pInfo->m_pGroupInfo->m_commandId)
        {
        case diana_cmd_in:
        case diana_cmd_out:
        case diana_cmd_ins:
        case diana_cmd_outs:
            pContext->iCurrentCmd_opsize = 4;
            break;
        }
    }

    // read structure
    {
    int iVexRegConsumed = 0; /* tracks whether ModRM reg field was consumed for a GPR operand */
    int iRegFieldUsed = 0;   /* an operand took the ModRM reg field: the next register operand is r/m */
    for (i = 0; i < pResult->pInfo->m_operandCount; ++i, ++pResult->iLinkedOpCount)
    {
        DianaLinkedOperand * pLinkedOp = pResult->linkedOperands + pResult->iLinkedOpCount;
        DianaOperandInfo * pOperInfo = pResult->pInfo->m_operands + i;
        DI_CHAR opSizeUsed = 0;
        DI_CHAR addrSizeUsed = 0;

        InitLinkedOp(pLinkedOp);

        // prefixes id 
        opSizeUsed = pOperInfo->m_size;

        if ((!opSizeUsed) || (bHas32BitAnalog && opSizeUsed != 1))
        {
            opSizeUsed = ( unsigned char )pContext->iCurrentCmd_opsize;
        }

        // handle DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64
        if (pContext->iAMD64Mode && pResult->pInfo->m_flags & DI_FLAG_CMD_AMD_DEFAULT_OPSIZE_64)
        {
            if (opSizeUsed == 4)
                opSizeUsed = 8;
        }

        if (pOperInfo->m_type == diana_orRegistry32)
        {
            // regOnly32, regOnly64 (EVEX vpbroadcastq zmm, r64)
            opSizeUsed = pOperInfo->m_size == 8 ? 8 : 4;
        }

        // reg32-64, reg/mem32-64 (SSE, crc32): 32-bit, 64-bit with REX.W, never 16-bit
        if ((pOperInfo->m_type == diana_orRegistry || pOperInfo->m_type == diana_orRegMem) &&
            pOperInfo->m_size == 4 && pOperInfo->m_size2 == 8)
        {
            opSizeUsed = (pContext->iAMD64Mode && DI_REX_HAS_FLAG_W(pContext->iRexPrefix)) ? 8 : 4;
        }

        addrSizeUsed = ( unsigned char )pContext->iCurrentCmd_addressSize;

        pLinkedOp->pInfo = pOperInfo;
        pLinkedOp->usedSize = opSizeUsed;
        pLinkedOp->usedAddressSize = addrSizeUsed;

        switch(pOperInfo->m_type)
        {
        case diana_orRegistry:
            pLinkedOp->type = diana_register;
            {
                DI_CHAR regToUse = RegCode;
                if (pContext->iVexPrefix && DI_VEX_GET_NDS(pResult->pInfo->m_flags))
                {
                    if (RegCode == DI_CHAR_NULL)
                    {
                        /* opcode extension consumes reg field (e.g. BLSR /1);
                           destination comes from vvvv (NDD) */
                        regToUse = DIANA_VEX_VVVV_REG(pContext);
                    }
                    else if (iVexRegConsumed)
                    {
                        /* reg field already consumed by earlier GPR operand;
                           this operand comes from vvvv (NDS second source) */
                        regToUse = DIANA_VEX_VVVV_REG(pContext);
                    }
                    else
                    {
                        iVexRegConsumed = 1;
                    }
                }
                if (regToUse == RegCode && iFullPostbyteUsed && !iExtensionUsed)
                {
                    iRegFieldUsed = 1;
                    // EVEX: a general register in ModRM.reg needs R' = 1 (not inverted: 0)
                    if (pContext->iEvexPrefix & DI_EVEX_PREFIX_R1)
                        return DI_ERROR;
                }
                if (DianaRecognizeCommonReg(opSizeUsed,
                                            regToUse,
                                            &pLinkedOp->value.recognizedRegister,
                                            pContext->iRexPrefix))
                    return DI_ERROR;
            }
            break;
             
        case diana_orRegMem_exact:
            pLinkedOp->usedSize = pOperInfo->m_size;
            opSizeUsed = pOperInfo->m_size;
            // no break!
        case diana_orMemOnly:
            if (pOperInfo->m_type == diana_orMemOnly && pOperInfo->m_size)
            {
                pLinkedOp->usedSize = pOperInfo->m_size;
                opSizeUsed = pOperInfo->m_size;
            }
            // no break!
        case diana_orReg32mem16:
        case diana_orReg16_32_64_mem16:
        case diana_orMemoryMMX:
        case diana_orMemoryXMM:
        case diana_orMemoryYMM:
        case diana_orMemoryZMM:
        case diana_orMemoryK:
        case diana_orVSIB:
        case diana_orRegMem:
            // VSIB needs a SIB byte: no 16-bit addressing
            if (pOperInfo->m_type == diana_orVSIB && addrSizeUsed == 2)
                return DI_ERROR;
            if (DianaGetHandler(addrSizeUsed!=2)(pContext,
                                                 pLinkedOp,
                                                 opSizeUsed,
                                                 ( DI_CHAR )PostByte,
                                                 readStream,
                                                 &pLinkedOp->value,
                                                 &pLinkedOp->type))
                return DI_ERROR;
            // orMemOnly: mod 11 is no form of this instruction
            if ((pOperInfo->m_type == diana_orMemOnly || pOperInfo->m_type == diana_orVSIB) &&
                pLinkedOp->type != diana_index)
                return DI_ERROR;
            
            if (pLinkedOp->type == diana_index)
            {
                if (diana_orReg16_32_64_mem16 == pOperInfo->m_type ||
                    diana_orReg32mem16 == pOperInfo->m_type)
                {
                    // special for glorious reg16/32/64/mem16 8C opcode
                    // m_size2: the memory size of reg32/mem8 (pinsrb, pextrb), 2 by default
                    pLinkedOp->usedSize = pOperInfo->m_size2 ? pOperInfo->m_size2 : 2;
                }
                else if (diana_orMemoryXMM == pOperInfo->m_type)
                {
                    // m_size2: the memory size of xmm/m32, xmm/m64 (spec tokens xmm/mem32...), 16 by default
                    pLinkedOp->usedSize = pOperInfo->m_size2 ? pOperInfo->m_size2 : 16;
                }
                else if (diana_orMemoryYMM == pOperInfo->m_type)
                {
                    pLinkedOp->usedSize = pOperInfo->m_size2 ? pOperInfo->m_size2 : 32;
                }
                else if (diana_orMemoryZMM == pOperInfo->m_type)
                {
                    pLinkedOp->usedSize = pOperInfo->m_size2 ? pOperInfo->m_size2 : 64;
                }
                else if (diana_orMemoryK == pOperInfo->m_type)
                {
                    pLinkedOp->usedSize = pOperInfo->m_size2 ? pOperInfo->m_size2 : 8;
                }
                else if (diana_orVSIB == pOperInfo->m_type)
                {
                    // the size of one element
                    pLinkedOp->usedSize = pOperInfo->m_size;
                }
                else if (diana_orMemoryMMX == pOperInfo->m_type)
                {
                    pLinkedOp->usedSize = pOperInfo->m_size2 ? pOperInfo->m_size2 : 8;
                }
                pLinkedOp->value.rmIndex.seg_reg = Diana_GetMemorySegment(pContext, &pLinkedOp->value.rmIndex);
                if (pLinkedOp->value.rmIndex.dispSize)
                {
                    pLinkedDispOp = &pLinkedOp->value.rmIndex;
                    pLinkedDispOpFull = pLinkedOp;
                }
                if (pContext->iEvexPrefix)
                {
                    // EVEX: the memory operand is one element broadcast with EVEX.b; the disp8 scale N
                    const DianaEvexCmdAttributes * pEvex = Diana_GetEvexAttributes(pResult->pInfo);
                    int bcst = pEvex ? (int)((pEvex->m_flags & DI_EVEX_BCST_MASK) >> DI_EVEX_BCST_SHIFT) : 0;
                    int n = pEvex ? (int)((pEvex->m_flags & DI_EVEX_N_MASK) >> DI_EVEX_N_SHIFT) : 0;
                    if ((pContext->iEvexPrefix & DI_EVEX_PREFIX_B) && bcst)
                    {
                        int vl = (pEvex->m_flags & DI_EVEX_VL_MASK) == DI_EVEX_VL_512 ? 64 :
                                 (pEvex->m_flags & DI_EVEX_VL_MASK) == DI_EVEX_VL_256 ? 32 : 16;
                        int element = 1 << bcst;
                        // the elements of the vector length, not of the memory size: vcvtpd2ps ymm, [m64]{1to8}
                        pResult->evex.broadcast = (DI_UINT8)(vl / element);
                        pLinkedOp->usedSize = element;
                    }
                    iEvexDisp8Scale = n ? 1 << (n - 1) : pLinkedOp->usedSize;
                }
            }
            break;

        case diana_orImmediate:
            if (pOperInfo->m_value == DI_CHAR_NULL)
            {
                if (iCurImm>=MAX_IMM)
                {
                    return DI_ERROR;
                }
                pLinkedOp->type = diana_imm;
                pLinkedImmOp[iCurImm] = pLinkedOp; 
                ++iCurImm;
            }
            else
            {
                pLinkedOp->type = diana_imm;
                pLinkedOp->value.imm = pOperInfo->m_value;
            }
            break;

        case diana_orEAX:
            pLinkedOp->type = diana_register;
            switch(pLinkedOp->usedSize)
            {
            case 8:
                pLinkedOp->value.recognizedRegister = reg_RAX;
                break;
            case 4:
                pLinkedOp->value.recognizedRegister = reg_EAX;
                break;
            case 2:
                pLinkedOp->value.recognizedRegister = reg_AX;
                break;
            case 1:
                pLinkedOp->value.recognizedRegister = reg_AL;
                break;
            default:
                return DI_ERROR;
            };
            break;

        case diana_orDX:
            pLinkedOp->type = diana_register;
            pLinkedOp->value.recognizedRegister = reg_DX;
            break;

        case diana_orECX:
            pLinkedOp->type = diana_register;
            // jcxz, jecxz, jrcxz (e3, the only relative operand with a counter): the counter has the address size
            if (pResult->pInfo->m_operandCount == 2 && pResult->pInfo->m_operands[1].m_type == diana_orRel)
                pLinkedOp->usedSize = addrSizeUsed;
            switch(pLinkedOp->usedSize)
            {
            case 8:
                pLinkedOp->value.recognizedRegister = reg_RCX;
                break;
            case 4:
                pLinkedOp->value.recognizedRegister = reg_ECX;
                break;
            case 2:
                pLinkedOp->value.recognizedRegister = reg_CX;
                break;
            case 1:
                pLinkedOp->value.recognizedRegister = reg_CL;
                break;
            default:
                return DI_ERROR;
            };
            break;

        case diana_orSreg:
            pLinkedOp->type = diana_register;
            if (RegCode == DI_CHAR_NULL)
            {
                if (pOperInfo->m_sreg_type != diana_reg_none)
                {
                    RegCode = ( DI_CHAR )(pOperInfo->m_sreg_type-1);
                }
                else
                    return DI_ERROR;
            }
            pLinkedOp->usedSize = 2;
            iRes = Diana_RecognizeSreg(RegCode, &pLinkedOp->value.recognizedRegister);
            if (iRes)
                return iRes;
            break;

        case diana_orOffset:
            pLinkedOp->type = diana_index;
            // 67 changes the size of the offset too
            pLinkedOp->value.rmIndex.dispSize = ( unsigned char )pContext->iCurrentCmd_addressSize;
            pLinkedOp->value.rmIndex.index = 0;
            pLinkedOp->value.rmIndex.indexed_reg = reg_none;
            pLinkedOp->value.rmIndex.reg = reg_none;
            pLinkedOp->value.rmIndex.seg_reg = pContext->currentCmd_sreg;
            pLinkedDispOp = &pLinkedOp->value.rmIndex;
            pLinkedDispOpFull = pLinkedOp;
            iDispIsSigned = 0;
            pLinkedOp->usedAddressSize = pContext->iCurrentCmd_addressSize;
            break;

        case diana_orAnyCR:
            pLinkedOp->type = diana_register;
            iRes = Diana_RecognizeCreg(RegCode, &pLinkedOp->value.recognizedRegister);
            if (iRes)
                return iRes;
            break;

        case diana_orDR6or7:
            pLinkedOp->type = diana_register;
            iRes = Diana_RecognizeDreg(RegCode, &pLinkedOp->value.recognizedRegister);
            if (iRes)
                return iRes;
            break;

        case diana_orTR6or7:
            pLinkedOp->type = diana_register;
            iRes = Diana_RecognizeTreg(RegCode, &pLinkedOp->value.recognizedRegister);
            if (iRes)
                return iRes;
            break;

        case diana_orPtr:
            if (pResult->pInfo->m_operandCount!=1)
            {
                return DI_ERROR;
            }
            pLinkedOp->type = diana_call_ptr;

            pLinkedOp->value.ptr.m_segment_size = 2;
            pLinkedOp->value.ptr.m_address_size = pLinkedOp->usedSize;
            pLinkedOp->iOffset = iCurOffset;

            iRes = DianaReadValueAsLong(readStream, 
                                        pLinkedOp->value.ptr.m_address_size, 
                                        &pLinkedOp->value.ptr.m_address,
                                        0);
            if (iRes)
                return iRes;

            iRes = DianaReadValueAsLong(readStream, 
                                        pLinkedOp->value.ptr.m_segment_size, 
                                        &pLinkedOp->value.ptr.m_segment,
                                        0);

            if (iRes)
                return iRes;
            break;
                
        case diana_orRel:
        case diana_orRel16x32:
            // commented out because of
            // E3  cb         JCXZ CX,rel8      9+m,5    Jump short if ECX(CX) register is 0
            //if (pResult->pInfo->m_operandCount!=1)
            //{
            //    return DI_ERROR;
            //}
            if (iCSIPSize!= DI_CHAR_NULL)
            {
                if (bHas32BitAnalog && iCSIPSize!=1)
                    iCSIPSize = pContext->iCurrentCmd_opsize;

                if (iCSIPSize == 8)
                    iCSIPSize = 4;
            } else
            {
                return DI_ERROR;
            }

            pLinkedOp->iOffset = iCurOffset;
            pLinkedOp->value.rel.m_size = iCSIPSize;
            pLinkedOp->type = diana_rel;
            iRes = DianaReadValueAsLong(readStream,
                                        pLinkedOp->value.rel.m_size,
                                        &pLinkedOp->value.rel.m_value,
                                        1);

            if (iRes)
                return iRes;
            break;
        
         case diana_orMemory_exact:
            pLinkedOp->usedSize = pOperInfo->m_size;
            opSizeUsed = pOperInfo->m_size;
            // no break!
               
        case diana_orMemory16x32:
        case diana_orMemory:
            if (PostByte == DI_FULL_CHAR_NULL)
            {
                pLinkedOp->type = diana_reserved_reg;
                break;
            }
            if (DianaGetHandler(addrSizeUsed!=2)(pContext,
                                                 pLinkedOp,
                                                 opSizeUsed,
                                                 ( DI_CHAR )PostByte, 
                                                 readStream,
                                                 &pLinkedOp->value,
                                                 &pLinkedOp->type))
                return DI_ERROR;
            
            if (pLinkedOp->type == diana_index)
            {
                DianaRmIndex index = pLinkedOp->value.rmIndex;
                pLinkedOp->value.memory.m_index = index;

                pLinkedOp->value.rmIndex.seg_reg = Diana_GetMemorySegment(pContext, &index);
                if (index.dispSize)
                {
                    pLinkedDispOp = &pLinkedOp->value.memory.m_index;
                    pLinkedDispOpFull = pLinkedOp;
                }

            }
            else
            {
                return DI_ERROR;
            }
            pLinkedOp->type = diana_memory;
            break;

        case diana_orCL:
            pLinkedOp->type = diana_register;
            pLinkedOp->value.recognizedRegister = reg_CL;
            break;

        case diana_orESI_ptr:
            iRes = Diana_GetSI(addrSizeUsed, &pLinkedOp->value.rmIndex.reg);
            if (iRes)
                return iRes;

            pLinkedOp->type = diana_index;
            pLinkedOp->value.rmIndex.dispSize = 0;
            pLinkedOp->value.rmIndex.dispValue = 0;
            pLinkedOp->value.rmIndex.index = 0;
            pLinkedOp->value.rmIndex.indexed_reg = reg_none;
            pLinkedOp->value.rmIndex.seg_reg = pContext->currentCmd_sreg;
            break;

        case diana_orEsDi:
            iRes = Diana_GetDI(addrSizeUsed, &pLinkedOp->value.rmIndex.reg);
            if (iRes)
                return iRes;
            
            pLinkedOp->type = diana_index;
            pLinkedOp->value.rmIndex.dispSize = 0;
            pLinkedOp->value.rmIndex.dispValue = 0;
            pLinkedOp->value.rmIndex.index = 0;
            pLinkedOp->value.rmIndex.indexed_reg = reg_none;
            pLinkedOp->value.rmIndex.seg_reg = reg_ES;
            break;

        case diana_orFPU_ST0:
        case diana_orFPU_ST:
            pLinkedOp->type = diana_register;
            pLinkedOp->value.recognizedRegister = reg_fpu_ST0;
            break;
            
        case diana_orFPU_STi:
            pLinkedOp->type = diana_register;
            if (fpuRegCode == DI_CHAR_NULL)
            {
                Diana_FatalBreak();
                return DI_ERROR;
            }
            pLinkedOp->value.recognizedRegister = reg_fpu_ST0 + fpuRegCode;
            break;

        case diana_orXMM0:
            // the implicit xmm0 (blendvps, pblendvb)
            pLinkedOp->type = diana_register;
            pLinkedOp->value.recognizedRegister = reg_XMM0;
            pLinkedOp->usedSize = 16;
            break;

        case diana_orRegIs4:
            // VEX is4 (vblendvps): read as the imm8, the register is imm8[7:4] (below)
            if (iCurImm >= MAX_IMM)
                return DI_ERROR;
            pLinkedOp->type = diana_imm;
            pLinkedOp->usedSize = 1;
            pLinkedImmOp[iCurImm] = pLinkedOp;
            ++iCurImm;
            pLinkedIs4Op = pLinkedOp;
            break;

        case diana_orRegXMM:
        case diana_orRegYMM:
        case diana_orRegZMM:
        case diana_orRegK:
        case diana_orRegMMX:
        case diana_orRegistry32:
            // a register operand: VEX.vvvv (NDS: the second operand, DDS: the third, the gathers' mask), else the
            // ModRM reg field while it is free, else r/m (the reg field is an opcode extension /digit or the operand
            // before took it), mod must be 11
            pLinkedOp->type = diana_register;
            {
                DI_CHAR code = 0;
                int isMMX = pOperInfo->m_type == diana_orRegMMX;
                int isK = pOperInfo->m_type == diana_orRegK;
                int vectorSize = Diana_VectorTypeSize(pOperInfo->m_type);
                if (PostByte == DI_FULL_CHAR_NULL)
                    return DI_ERROR;
                if (pOperInfo->m_type != diana_orRegistry32 && pOperInfo->m_type != diana_orRegMMX &&
                    pContext->iVexPrefix && ((DI_VEX_GET_NDS(pResult->pInfo->m_flags) == 1 && i == 1) ||
                                             (DI_VEX_GET_NDS(pResult->pInfo->m_flags) == 3 && i == 2) ||
                                             // NDD: the destination is VEX.vvvv (vpsllw xmm1, xmm2, imm8: /6 and
                                             // xmm2 in r/m)
                                             (DI_VEX_GET_NDS(pResult->pInfo->m_flags) == 2 && i == 0)))
                {
                    code = DIANA_VEX_VVVV_REG(pContext);
                    if (isK)
                    {
                        // an opmask in vvvv: bit 3 is 0 (not inverted) in 64-bit mode, V' too
                        if ((pContext->iAMD64Mode && (code & 8)) || (pContext->iEvexPrefix & DI_EVEX_PREFIX_V1))
                            return DI_ERROR;
                    }
                    else
                        code |= DIANA_EVEX_BIT4(pContext, DI_EVEX_PREFIX_V1);
                }
                else
                if (!iExtensionUsed && !iRegFieldUsed)
                {
                    iRegFieldUsed = 1;
                    code = isMMX ? Diana_GetReg(( DI_CHAR )PostByte) : Diana_GetReg2(pContext, ( DI_CHAR )PostByte);
                    if (isK)
                    {
                        // an opmask in ModRM.reg: R and R' are 1 (not inverted: 0)
                        if ((code & 8) || (pContext->iEvexPrefix & DI_EVEX_PREFIX_R1))
                            return DI_ERROR;
                    }
                    else
                    if (vectorSize)
                        code |= DIANA_EVEX_BIT4(pContext, DI_EVEX_PREFIX_R1);
                    else
                    if (pContext->iEvexPrefix & DI_EVEX_PREFIX_R1)
                        return DI_ERROR;
                }
                else
                {
                    if (Diana_GetMod(( DI_CHAR )PostByte) != 3)
                        return DI_ERROR;
                    code = isMMX ? Diana_GetRm(( DI_CHAR )PostByte) : Diana_GetRm2(pContext, ( DI_CHAR )PostByte);
                    // an opmask in r/m: B and X are ignored; a vector register: X is bit 4
                    if (isK)
                        code &= 7;
                    else
                    if (vectorSize)
                        code |= DIANA_EVEX_BIT4(pContext, DI_EVEX_PREFIX_X);
                }
                switch (pOperInfo->m_type)
                {
                case diana_orRegXMM:
                case diana_orRegYMM:
                case diana_orRegZMM:
                    pLinkedOp->value.recognizedRegister = Diana_VectorRegister(vectorSize, code);
                    pLinkedOp->usedSize = vectorSize;
                    break;
                case diana_orRegK:
                    pLinkedOp->value.recognizedRegister = (DianaUnifiedRegister)(reg_K0 + (code & 7));
                    pLinkedOp->usedSize = 8;
                    break;
                case diana_orRegMMX:
                    pLinkedOp->value.recognizedRegister = reg_MM0 + code;
                    pLinkedOp->usedSize = 8;
                    break;
                default:
                    if (DianaRecognizeCommonReg(opSizeUsed, code, &pLinkedOp->value.recognizedRegister,
                                                pContext->iRexPrefix))
                        return DI_ERROR;
                }
            }
            break;

        default:
            Diana_FatalBreak();
            return DI_ERROR;
        }
    }
    } /* iVexRegConsumed scope */
    // read value
    if (pLinkedDispOp)
    {
        pLinkedDispOpFull->iOffset = iCurOffset;
        iRes = DianaReadValueAsLong(readStream,
                                    pLinkedDispOp->dispSize,
                                    &pLinkedDispOp->dispValue,
                                    iDispIsSigned);
        iCurOffset += pLinkedDispOp->dispSize;
        // EVEX: disp8*N; dispSize stays 1 (the encoding), dispValue is the displacement
        if (!iRes && pContext->iEvexPrefix && pLinkedDispOp->dispSize == 1 && iEvexDisp8Scale > 1)
            pLinkedDispOp->dispValue *= (DI_UINT64)iEvexDisp8Scale;
    }
    if (pContext->iEvexPrefix)
        pResult->evex.disp8Scale = (DI_UINT8)iEvexDisp8Scale;
    if (iRes)
        return iRes;

    if (iCurImm)
    {
        DI_CHAR imms[MAX_IMM] = {0, 0};
        i =0;
        imms[0] = pResult->pInfo->m_iImmediateOperandSizeInBytes;
        imms[1] = pResult->pInfo->m_iImmediateOperandSizeInBytes2;

        for(;i<iCurImm;++i)
        {
            int iImmUsedSize = pLinkedImmOp[i]->usedSize;
            // special crutch for intel manual
            if (imms[i]==1)
            {
                iImmUsedSize = imms[i];
            }
            else
            {
                // DI_FLAG_CMD_SUPPORTS_IMM64
                if (pLinkedImmOp[i]->usedSize == 8)
                {
                    if (!(pResult->pInfo->m_flags & DI_FLAG_CMD_SUPPORTS_IMM64))
                    {
                        pLinkedImmOp[i]->usedSize = 4;
                        iImmUsedSize = 4;
                    }
                }
            }
        
            pLinkedImmOp[i]->iOffset = iCurOffset;
            iRes = DianaReadValueAsLong(readStream, 
                                        iImmUsedSize,
                                        &pLinkedImmOp[i]->value.imm,
                                        0);
            iCurOffset += iImmUsedSize;
        }
    }
    if (!iRes && pLinkedIs4Op)
    {
        // imm8[7:4]; bit 7 is ignored outside 64-bit mode
        int code = (int)((pLinkedIs4Op->value.imm >> 4) & (pContext->iAMD64Mode ? 0xF : 7));
        int size = pLinkedIs4Op->pInfo->m_size == 32 ? 32 : 16;
        pLinkedIs4Op->type = diana_register;
        pLinkedIs4Op->value.recognizedRegister = (DianaUnifiedRegister)((size == 32 ? reg_YMM0 : reg_XMM0) + code);
        pLinkedIs4Op->usedSize = (DI_CHAR)size;
    }
    return iRes;
}
