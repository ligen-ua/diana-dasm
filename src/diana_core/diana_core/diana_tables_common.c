#include "diana_tables_common.h"

typedef struct _dianaRM16_4
{
    DianaUnifiedRegister regs[4];
}DianaRM16_4;

extern
DianaUnifiedRegister g_diana_regsAmd8[4];

static DianaRM16_4 table2[16]= 
{
    {{reg_EAX, reg_AX, reg_AL, reg_RAX}},
    {{reg_ECX, reg_CX, reg_CL, reg_RCX}},
    {{reg_EDX, reg_DX, reg_DL, reg_RDX}},
    {{reg_EBX, reg_BX, reg_BL, reg_RBX}},
    {{reg_ESP, reg_SP, reg_AH, reg_RSP}},
    {{reg_EBP, reg_BP, reg_CH, reg_RBP}},
    {{reg_ESI, reg_SI, reg_DH, reg_RSI}},
    {{reg_EDI, reg_DI, reg_BH, reg_RDI}},

    {{reg_R8D,  reg_R8W,  reg_R8B,   reg_R8}},
    {{reg_R9D,  reg_R9W,  reg_R9B,   reg_R9}},
    {{reg_R10D, reg_R10W, reg_R10B,  reg_R10}},
    {{reg_R11D, reg_R11W, reg_R11B,  reg_R11}},
    {{reg_R12D, reg_R12W, reg_R12B,  reg_R12}},
    {{reg_R13D, reg_R13W, reg_R13B,  reg_R13}},
    {{reg_R14D, reg_R14W, reg_R14B,  reg_R14}},
    {{reg_R15D, reg_R15W, reg_R15B,  reg_R15}}
};


int Diana_DispatchMod3(DianaLinkedOperand * pInfo,
                       int rm,
                       DI_CHAR old_true_rm,
                       int iRegSizeInBytes,
                       DianaUnifiedRegister *pOut,
                       int isRexPrefix)
{
    int iIndex = 0;

    old_true_rm;

    if (pInfo->pInfo->m_type == diana_orMemoryMMX)
    {
         pInfo->usedSize = 8;
         return DianaRecognizeMMX(( DI_CHAR )rm, pOut);
    }
    if (pInfo->pInfo->m_type == diana_orMemoryXMM)
    {
        pInfo->usedSize = 16;
        return DianaRecognizeXMM(( DI_CHAR )rm, pOut);
    }
        
    switch (iRegSizeInBytes)
    {
    default:
        return DI_ERROR;

    case 8:
        iIndex = 3;
        break;

    case 4:
        iIndex = 0;
        break;
    case 2:
        iIndex = 1;
        break;
    case 1: 
        iIndex = 2;
    };
    *pOut = table2[rm].regs[iIndex];
    if (iIndex == 2)
    {
        if (isRexPrefix && iIndex==2 && rm >= 4 && rm < 8)
        {
            // SIL, DIL fix 
            *pOut = g_diana_regsAmd8[rm-4];
        }
    }
    return DI_SUCCESS;
}
