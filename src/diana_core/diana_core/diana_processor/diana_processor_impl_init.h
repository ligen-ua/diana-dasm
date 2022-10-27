#include "diana_processor_cmd_internal.h"

static OPERAND_SIZE g_maskBuffer[8];

static int GetSetBitsCount(int value)
{
    int mask = 0x1;
    int count = 0;
    int i = 0;

    for(; i < 8; ++i)
    {
        if (value & mask)
        {
            ++count;
        }
        mask<<=1;
    }
    return count;
}
OPERAND_SIZE DianaProcessor_GetSignMask(int sizeInBytes)
{
    return g_maskBuffer[sizeInBytes-1];
}
OPERAND_SIZE DianaProcessor_GetSignMaskSpecifyBit(OPERAND_SIZE sizeInBits)
{
    return g_maskBuffer[sizeInBits/8]>>(7-sizeInBits%8);
}

OPERAND_SIZE DianaProcessor_CutValue(OPERAND_SIZE value,
                                     int size)
{
    switch(size)
    {
    case 1:
        return (unsigned char)value;
    case 2:
        return (unsigned short)value;
    case 4:
        return (unsigned int)value;
    case 8:
        return value;    
    }
    Diana_FatalBreak();
    return 0;
}


void DianaProcessor_ProcImplInit()
{
    int i = 1;
    g_diana_parityBuffer[0] = 1; // WTF?
    for(; i<256; ++i)
    {
        g_diana_parityBuffer[i] = (GetSetBitsCount(i)%2)?0:1;
    }
    // init masks
    g_maskBuffer[0] = 0x80ULL;
    g_maskBuffer[1] = 0x8000ULL;
    g_maskBuffer[2] = 0x800000ULL;
    g_maskBuffer[3] = 0x80000000ULL;
    g_maskBuffer[4] = 0x8000000000ULL;
    g_maskBuffer[5] = 0x800000000000ULL;
    g_maskBuffer[6] = 0x80000000000000ULL;
    g_maskBuffer[7] = 0x8000000000000000ULL;
}


#define SIZE_reg_IP  4

#define SIZE_reg_AL  1
#define SIZE_reg_CL  1
#define SIZE_reg_DL  1
#define SIZE_reg_BL  1 
#define SIZE_reg_AH  1
#define SIZE_reg_CH  1
#define SIZE_reg_DH  1
#define SIZE_reg_BH  1

#define SIZE_reg_AX  2
#define SIZE_reg_CX  2
#define SIZE_reg_DX  2
#define SIZE_reg_BX  2
#define SIZE_reg_SP  2
#define SIZE_reg_BP  2
#define SIZE_reg_SI  2
#define SIZE_reg_DI  2

#define SIZE_reg_EAX  4  
#define SIZE_reg_ECX  4
#define SIZE_reg_EDX  4
#define SIZE_reg_EBX  4
#define SIZE_reg_ESP  4
#define SIZE_reg_EBP  4
#define SIZE_reg_ESI  4
#define SIZE_reg_EDI  4  

#define SIZE_reg_ES  2
#define SIZE_reg_CS  2
#define SIZE_reg_SS  2
#define SIZE_reg_DS  2
#define SIZE_reg_FS  2
#define SIZE_reg_GS  2


#define SIZE_reg_CR0    4
#define SIZE_reg_CR1    4
#define SIZE_reg_CR2    4
#define SIZE_reg_CR3    4
#define SIZE_reg_CR4    4
#define SIZE_reg_CR5    4
#define SIZE_reg_CR6    4
#define SIZE_reg_CR7    4

#define SIZE_reg_DR0    4
#define SIZE_reg_DR1    4
#define SIZE_reg_DR2    4
#define SIZE_reg_DR3    4
#define SIZE_reg_DR4    4
#define SIZE_reg_DR5    4
#define SIZE_reg_DR6    4
#define SIZE_reg_DR7    4

#define SIZE_reg_TR0    4
#define SIZE_reg_TR1    4
#define SIZE_reg_TR2    4
#define SIZE_reg_TR3    4
#define SIZE_reg_TR4    4
#define SIZE_reg_TR5    4
#define SIZE_reg_TR6    4
#define SIZE_reg_TR7    4


#define SIZE_reg_RAX    8
#define SIZE_reg_RCX    8
#define SIZE_reg_RDX    8
#define SIZE_reg_RBX    8
#define SIZE_reg_RSP    8
#define SIZE_reg_RBP    8
#define SIZE_reg_RSI    8
#define SIZE_reg_RDI    8

#define SIZE_reg_R8    8
#define SIZE_reg_R9    8
#define SIZE_reg_R10   8
#define SIZE_reg_R11   8
#define SIZE_reg_R12   8
#define SIZE_reg_R13   8
#define SIZE_reg_R14   8
#define SIZE_reg_R15   8

#define SIZE_reg_RIP   8

#define SIZE_reg_MM0   8
#define SIZE_reg_MM1   8
#define SIZE_reg_MM2   8
#define SIZE_reg_MM3   8
#define SIZE_reg_MM4   8
#define SIZE_reg_MM5   8
#define SIZE_reg_MM6   8
#define SIZE_reg_MM7   8

#define SIZE_reg_MM_Internal0   10
#define SIZE_reg_MM_Internal1   10
#define SIZE_reg_MM_Internal2   10
#define SIZE_reg_MM_Internal3   10
#define SIZE_reg_MM_Internal4   10
#define SIZE_reg_MM_Internal5   10
#define SIZE_reg_MM_Internal6   10
#define SIZE_reg_MM_Internal7   10

#define SIZE_reg_XMM0  16
#define SIZE_reg_XMM1  16
#define SIZE_reg_XMM2  16
#define SIZE_reg_XMM3  16
#define SIZE_reg_XMM4  16
#define SIZE_reg_XMM5  16
#define SIZE_reg_XMM6  16
#define SIZE_reg_XMM7  16
#define SIZE_reg_XMM8  16
#define SIZE_reg_XMM9  16
#define SIZE_reg_XMM10  16
#define SIZE_reg_XMM11  16
#define SIZE_reg_XMM12  16
#define SIZE_reg_XMM13  16
#define SIZE_reg_XMM14  16
#define SIZE_reg_XMM15  16

#define SIZE_reg_fpu_ST0     8
#define SIZE_reg_fpu_ST1     8
#define SIZE_reg_fpu_ST2     8
#define SIZE_reg_fpu_ST3     8
#define SIZE_reg_fpu_ST4     8
#define SIZE_reg_fpu_ST5     8
#define SIZE_reg_fpu_ST6     8
#define SIZE_reg_fpu_ST7     8

#define SIZE_reg_R8D      4
#define SIZE_reg_R9D      4
#define SIZE_reg_R10D     4
#define SIZE_reg_R11D     4
#define SIZE_reg_R12D     4
#define SIZE_reg_R13D     4
#define SIZE_reg_R14D     4
#define SIZE_reg_R15D     4

#define SIZE_reg_R8W      2
#define SIZE_reg_R9W      2
#define SIZE_reg_R10W     2
#define SIZE_reg_R11W     2
#define SIZE_reg_R12W     2
#define SIZE_reg_R13W     2
#define SIZE_reg_R14W     2
#define SIZE_reg_R15W     2

#define SIZE_reg_R8B     1
#define SIZE_reg_R9B     1
#define SIZE_reg_R10B     1
#define SIZE_reg_R11B     1
#define SIZE_reg_R12B     1
#define SIZE_reg_R13B     1
#define SIZE_reg_R14B     1
#define SIZE_reg_R15B     1

#define SIZE_reg_SIL      1
#define SIZE_reg_DIL      1
#define SIZE_reg_BPL      1
#define SIZE_reg_SPL      1


#define REGISTER_REGISTER(Name) \
    Diana_RegisterRegister(pThis, Name, SIZE_##Name); 

#define REGISTER_SUB_REGISTER(Name, ParentName, Offset) \
    Diana_RegisterSubRegister(pThis, Name, SIZE_##Name, ParentName, Offset);

#define DI_REG_INIT_VALUE 1024

static
int Diana_RegisterSubRegister(DianaProcessor * pThis,
                              DianaUnifiedRegister reg,
                              int size,
                              DianaUnifiedRegister parentReg,
                              int offset)
{
    int parentOffset  = pThis->m_registers[parentReg].m_offset;

    if (offset + size > pThis->m_iLastRegistersOffset)
    {
        Diana_FatalBreak();
    }

    if (parentOffset + offset + size > pThis->m_iLastRegistersOffset)
    {
        Diana_FatalBreak();
    }

    pThis->m_registers[reg].m_offset = parentOffset + offset;
    pThis->m_registers[reg].m_size = size;
    return 0;
}

static
int Diana_RegisterRegister(DianaProcessor * pThis,
                           DianaUnifiedRegister reg,
                           int size)
{
    if (pThis->m_iLastRegistersOffset + size >  pThis->m_registersVectorSize)
    {
        // need realloc
        int newSize = (pThis->m_iLastRegistersOffset + size) * 2;
        char * pRegistersVector = pThis->m_pAllocator->m_alloc(pThis->m_pAllocator, newSize);
        char * pOldRegistersVector = 0;
        DI_CHECK_ALLOC(pRegistersVector);

        // set new vector
        pOldRegistersVector = pThis->m_pRegistersVector;

        DIANA_MEMCPY(pRegistersVector, pThis->m_pRegistersVector, pThis->m_registersVectorSize);
        pThis->m_pAllocator->m_free(pThis->m_pAllocator, pThis->m_pRegistersVector);
        pThis->m_pRegistersVector = pRegistersVector;
        pThis->m_registersVectorSize = newSize;

        pThis->m_pAllocator->m_free(pThis->m_pAllocator, pOldRegistersVector);
    }

    pThis->m_registers[reg].m_offset = pThis->m_iLastRegistersOffset;
    pThis->m_registers[reg].m_size = size;

    pThis->m_iLastRegistersOffset += size;
    return 0;
}

void DianaProcessor_SetResetDefaultFlags(DianaProcessor * pThis)
{
    DianaProcessor_SetFlag(pThis, DI_FLAG_1);
    DianaProcessor_SetFlag(pThis, DI_FLAG_ID); // allow CPUID
    DianaProcessor_SetFlag(pThis, DI_FLAG_IF); // interrupts enabled

    DianaProcessor_ClearFlag(pThis, DI_FLAG_3);
    DianaProcessor_ClearFlag(pThis, DI_FLAG_5);
    DianaProcessor_ClearFlag(pThis, DI_FLAG_15);
}

void DianaProcessor_ResetFPU(DianaProcessor * pThis)
{
    DIANA_MEMSET(&pThis->m_fpu, 0, sizeof(pThis->m_fpu));
    pThis->m_fpu.controlWord = 0x037F;
}
int DianaProcessor_InitProcessorImpl(DianaProcessor * pThis)
{
    // tune rflags
    DianaProcessor_SetResetDefaultFlags(pThis);

    DianaProcessor_ResetFPU(pThis);
    pThis->m_registersVectorSize = DI_REG_INIT_VALUE;
    pThis->m_pRegistersVector = pThis->m_pAllocator->m_alloc(pThis->m_pAllocator, DI_REG_INIT_VALUE);
    DI_CHECK_ALLOC(pThis->m_pRegistersVector);

    // x64 part
    REGISTER_REGISTER(reg_RAX);
    REGISTER_REGISTER(reg_RCX);
    REGISTER_REGISTER(reg_RDX);
    REGISTER_REGISTER(reg_RBX);
    REGISTER_REGISTER(reg_RSP);
    REGISTER_REGISTER(reg_RBP);
    REGISTER_REGISTER(reg_RSI);
    REGISTER_REGISTER(reg_RDI);

    REGISTER_REGISTER(reg_R8);
    REGISTER_REGISTER(reg_R9);
    REGISTER_REGISTER(reg_R10);
    REGISTER_REGISTER(reg_R11);
    REGISTER_REGISTER(reg_R12);
    REGISTER_REGISTER(reg_R13);
    REGISTER_REGISTER(reg_R14);
    REGISTER_REGISTER(reg_R15);

    REGISTER_REGISTER(reg_RIP);

    REGISTER_REGISTER(reg_XMM0);
    REGISTER_REGISTER(reg_XMM1);
    REGISTER_REGISTER(reg_XMM2);
    REGISTER_REGISTER(reg_XMM3);
    REGISTER_REGISTER(reg_XMM4);
    REGISTER_REGISTER(reg_XMM5);
    REGISTER_REGISTER(reg_XMM6);
    REGISTER_REGISTER(reg_XMM7);
    REGISTER_REGISTER(reg_XMM8);
    REGISTER_REGISTER(reg_XMM9);
    REGISTER_REGISTER(reg_XMM10);
    REGISTER_REGISTER(reg_XMM11);
    REGISTER_REGISTER(reg_XMM12);
    REGISTER_REGISTER(reg_XMM13);
    REGISTER_REGISTER(reg_XMM14);
    REGISTER_REGISTER(reg_XMM15);

    REGISTER_REGISTER(reg_MM_Internal0);
    REGISTER_REGISTER(reg_MM_Internal1);
    REGISTER_REGISTER(reg_MM_Internal2);
    REGISTER_REGISTER(reg_MM_Internal3);
    REGISTER_REGISTER(reg_MM_Internal4);
    REGISTER_REGISTER(reg_MM_Internal5);
    REGISTER_REGISTER(reg_MM_Internal6);
    REGISTER_REGISTER(reg_MM_Internal7);


    // fpu sub
    REGISTER_SUB_REGISTER(reg_MM0, reg_MM_Internal0, 0);
    REGISTER_SUB_REGISTER(reg_MM1, reg_MM_Internal1, 0);
    REGISTER_SUB_REGISTER(reg_MM2, reg_MM_Internal2, 0);
    REGISTER_SUB_REGISTER(reg_MM3, reg_MM_Internal3, 0);
    REGISTER_SUB_REGISTER(reg_MM4, reg_MM_Internal4, 0);
    REGISTER_SUB_REGISTER(reg_MM5, reg_MM_Internal5, 0);
    REGISTER_SUB_REGISTER(reg_MM6, reg_MM_Internal6, 0);
    REGISTER_SUB_REGISTER(reg_MM7, reg_MM_Internal7, 0);

    REGISTER_SUB_REGISTER(reg_R8D, reg_R8, 0);
    REGISTER_SUB_REGISTER(reg_R9D, reg_R9, 0);
    REGISTER_SUB_REGISTER(reg_R10D, reg_R10, 0);
    REGISTER_SUB_REGISTER(reg_R11D, reg_R11, 0);
    REGISTER_SUB_REGISTER(reg_R12D, reg_R12, 0);
    REGISTER_SUB_REGISTER(reg_R13D, reg_R13, 0);
    REGISTER_SUB_REGISTER(reg_R14D, reg_R14, 0);
    REGISTER_SUB_REGISTER(reg_R15D, reg_R15, 0);

    REGISTER_SUB_REGISTER(reg_R8W, reg_R8, 0);
    REGISTER_SUB_REGISTER(reg_R9W, reg_R9, 0);
    REGISTER_SUB_REGISTER(reg_R10W, reg_R10, 0);
    REGISTER_SUB_REGISTER(reg_R11W, reg_R11, 0);
    REGISTER_SUB_REGISTER(reg_R12W, reg_R12, 0);
    REGISTER_SUB_REGISTER(reg_R13W, reg_R13, 0);
    REGISTER_SUB_REGISTER(reg_R14W, reg_R14, 0);
    REGISTER_SUB_REGISTER(reg_R15W, reg_R15, 0);

    REGISTER_SUB_REGISTER(reg_R8B, reg_R8, 0);
    REGISTER_SUB_REGISTER(reg_R9B, reg_R9, 0);
    REGISTER_SUB_REGISTER(reg_R10B, reg_R10, 0);
    REGISTER_SUB_REGISTER(reg_R11B, reg_R11, 0);
    REGISTER_SUB_REGISTER(reg_R12B, reg_R12, 0);
    REGISTER_SUB_REGISTER(reg_R13B, reg_R13, 0);
    REGISTER_SUB_REGISTER(reg_R14B, reg_R14, 0);
    REGISTER_SUB_REGISTER(reg_R15B, reg_R15, 0);

    REGISTER_SUB_REGISTER(reg_SIL, reg_RSI, 0);  
    REGISTER_SUB_REGISTER(reg_DIL, reg_RDI, 0);  
    REGISTER_SUB_REGISTER(reg_BPL, reg_RBP, 0);  
    REGISTER_SUB_REGISTER(reg_SPL, reg_RSP, 0);

    // registers
    REGISTER_REGISTER(reg_ES)
    REGISTER_REGISTER(reg_CS)
    REGISTER_REGISTER(reg_SS)
    REGISTER_REGISTER(reg_DS)
    REGISTER_REGISTER(reg_FS)
    REGISTER_REGISTER(reg_GS)

    REGISTER_REGISTER(reg_CR0)
    REGISTER_REGISTER(reg_CR1)
    REGISTER_REGISTER(reg_CR2)
    REGISTER_REGISTER(reg_CR3)
    REGISTER_REGISTER(reg_CR4)
    REGISTER_REGISTER(reg_CR5)
    REGISTER_REGISTER(reg_CR6)
    REGISTER_REGISTER(reg_CR7)

    REGISTER_REGISTER(reg_DR0)
    REGISTER_REGISTER(reg_DR1)
    REGISTER_REGISTER(reg_DR2)
    REGISTER_REGISTER(reg_DR3)
    REGISTER_REGISTER(reg_DR4)
    REGISTER_REGISTER(reg_DR5)
    REGISTER_REGISTER(reg_DR6)
    REGISTER_REGISTER(reg_DR7)

    REGISTER_REGISTER(reg_TR0)
    REGISTER_REGISTER(reg_TR1)
    REGISTER_REGISTER(reg_TR2)
    REGISTER_REGISTER(reg_TR3)
    REGISTER_REGISTER(reg_TR4)
    REGISTER_REGISTER(reg_TR5)
    REGISTER_REGISTER(reg_TR6)
    REGISTER_REGISTER(reg_TR7)

    // sub registers
    REGISTER_SUB_REGISTER(reg_IP, reg_RIP, 0);

    REGISTER_SUB_REGISTER(reg_SP, reg_RSP, 0);
    REGISTER_SUB_REGISTER(reg_BP, reg_RBP, 0);
    REGISTER_SUB_REGISTER(reg_SI, reg_RSI, 0);
    REGISTER_SUB_REGISTER(reg_DI, reg_RDI, 0);

    REGISTER_SUB_REGISTER(reg_EAX, reg_RAX, 0);
    REGISTER_SUB_REGISTER(reg_ECX, reg_RCX, 0);
    REGISTER_SUB_REGISTER(reg_EDX, reg_RDX, 0);
    REGISTER_SUB_REGISTER(reg_EBX, reg_RBX, 0);
    REGISTER_SUB_REGISTER(reg_ESP, reg_RSP, 0);
    REGISTER_SUB_REGISTER(reg_EBP, reg_RBP, 0);
    REGISTER_SUB_REGISTER(reg_ESI, reg_RSI, 0);
    REGISTER_SUB_REGISTER(reg_EDI, reg_RDI, 0);

    // sub-sub registers
    REGISTER_SUB_REGISTER(reg_AX, reg_EAX, 0)
    REGISTER_SUB_REGISTER(reg_CX, reg_ECX, 0)
    REGISTER_SUB_REGISTER(reg_DX, reg_EDX, 0)
    REGISTER_SUB_REGISTER(reg_BX, reg_EBX, 0)

    REGISTER_SUB_REGISTER(reg_AL, reg_AX, 0)
    REGISTER_SUB_REGISTER(reg_CL, reg_CX, 0)
    REGISTER_SUB_REGISTER(reg_DL, reg_DX, 0)
    REGISTER_SUB_REGISTER(reg_BL, reg_BX, 0)

    REGISTER_SUB_REGISTER(reg_AH, reg_AX, 1)
    REGISTER_SUB_REGISTER(reg_CH, reg_CX, 1)
    REGISTER_SUB_REGISTER(reg_DH, reg_DX, 1)
    REGISTER_SUB_REGISTER(reg_BH, reg_BX, 1)

    // zero all registers
    DIANA_MEMSET(pThis->m_pRegistersVector, 0, pThis->m_registersVectorSize);
    return 0;
}
 


#define DIANA_GENERATE_IMPL(X2, X) \
    "#define GET_" #X2 " ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, " #X ")))\n" \
    "#define SET_" #X2 "(Y) DianaProcessor_SetValue(pCallContext, " #X ", DianaProcessor_QueryReg(pCallContext, " #X "), Y)\n" 

#define DIANA_GENERATE_FLAG(X2, X) \
    "#define GET_" #X2 " (DianaProcessor_QueryFlag(pCallContext, " #X " ))\n" \
    "#define SET_" #X2 " (DianaProcessor_SetFlag(pCallContext,  " #X " ))\n"  \
    "#define CLEAR_" #X2 " (DianaProcessor_ClearFlag(pCallContext,  " #X " ))\n"  


const char * Diana_GenerateStuff()
{
    const char * pData = 

    "// !!!!  AUTOGENERATED, DO NOT MODIFY  !!!!\n"

    DIANA_GENERATE_IMPL(REG_AL, reg_AL)
    DIANA_GENERATE_IMPL(REG_CL, reg_CL)
    DIANA_GENERATE_IMPL(REG_DL, reg_DL)
    DIANA_GENERATE_IMPL(REG_BL, reg_BL)
    DIANA_GENERATE_IMPL(REG_AH, reg_AH)
    DIANA_GENERATE_IMPL(REG_CH, reg_CH)
    DIANA_GENERATE_IMPL(REG_DH, reg_DH)
    DIANA_GENERATE_IMPL(REG_BH, reg_BH)
    DIANA_GENERATE_IMPL(REG_AX, reg_AX)
    DIANA_GENERATE_IMPL(REG_CX, reg_CX)
    DIANA_GENERATE_IMPL(REG_DX, reg_DX)
    DIANA_GENERATE_IMPL(REG_BX, reg_BX)
    DIANA_GENERATE_IMPL(REG_SP, reg_SP)
    DIANA_GENERATE_IMPL(REG_BP, reg_BP)
    DIANA_GENERATE_IMPL(REG_SI, reg_SI)
    DIANA_GENERATE_IMPL(REG_DI, reg_DI)
    DIANA_GENERATE_IMPL(REG_EAX, reg_EAX)
    DIANA_GENERATE_IMPL(REG_ECX, reg_ECX)
    DIANA_GENERATE_IMPL(REG_EDX, reg_EDX)
    DIANA_GENERATE_IMPL(REG_EBX, reg_EBX)
    DIANA_GENERATE_IMPL(REG_ESP, reg_ESP)
    DIANA_GENERATE_IMPL(REG_EBP, reg_EBP)
    DIANA_GENERATE_IMPL(REG_ESI, reg_ESI)
    DIANA_GENERATE_IMPL(REG_EDI, reg_EDI)
    DIANA_GENERATE_IMPL(REG_ES, reg_ES)
    DIANA_GENERATE_IMPL(REG_CS, reg_CS)
    DIANA_GENERATE_IMPL(REG_SS, reg_SS)
    DIANA_GENERATE_IMPL(REG_DS, reg_DS)
    DIANA_GENERATE_IMPL(REG_FS, reg_FS)
    DIANA_GENERATE_IMPL(REG_GS, reg_GS)
    DIANA_GENERATE_IMPL(REG_CR0, reg_CR0)
    DIANA_GENERATE_IMPL(REG_CR1, reg_CR1)
    DIANA_GENERATE_IMPL(REG_CR2, reg_CR2)
    DIANA_GENERATE_IMPL(REG_CR3, reg_CR3)
    DIANA_GENERATE_IMPL(REG_CR4, reg_CR4)
    DIANA_GENERATE_IMPL(REG_CR5, reg_CR5)
    DIANA_GENERATE_IMPL(REG_CR6, reg_CR6)
    DIANA_GENERATE_IMPL(REG_CR7, reg_CR7)
    DIANA_GENERATE_IMPL(REG_DR0, reg_DR0)
    DIANA_GENERATE_IMPL(REG_DR1, reg_DR1)
    DIANA_GENERATE_IMPL(REG_DR2, reg_DR2)
    DIANA_GENERATE_IMPL(REG_DR3, reg_DR3)
    DIANA_GENERATE_IMPL(REG_DR4, reg_DR4)
    DIANA_GENERATE_IMPL(REG_DR5, reg_DR5)
    DIANA_GENERATE_IMPL(REG_DR6, reg_DR6)
    DIANA_GENERATE_IMPL(REG_DR7, reg_DR7)
    DIANA_GENERATE_IMPL(REG_TR0, reg_TR0)
    DIANA_GENERATE_IMPL(REG_TR1, reg_TR1)
    DIANA_GENERATE_IMPL(REG_TR2, reg_TR2)
    DIANA_GENERATE_IMPL(REG_TR3, reg_TR3)
    DIANA_GENERATE_IMPL(REG_TR4, reg_TR4)
    DIANA_GENERATE_IMPL(REG_TR5, reg_TR5)
    DIANA_GENERATE_IMPL(REG_TR6, reg_TR6)
    DIANA_GENERATE_IMPL(REG_TR7, reg_TR7)
    DIANA_GENERATE_IMPL(REG_IP, reg_IP)
    DIANA_GENERATE_IMPL(REG_RAX, reg_RAX)
    DIANA_GENERATE_IMPL(REG_RCX, reg_RCX)
    DIANA_GENERATE_IMPL(REG_RDX, reg_RDX)
    DIANA_GENERATE_IMPL(REG_RBX, reg_RBX)
    DIANA_GENERATE_IMPL(REG_RSP, reg_RSP)
    DIANA_GENERATE_IMPL(REG_RBP, reg_RBP)
    DIANA_GENERATE_IMPL(REG_RSI, reg_RSI)
    DIANA_GENERATE_IMPL(REG_RDI, reg_RDI)
    DIANA_GENERATE_IMPL(REG_SIL, reg_SIL)
    DIANA_GENERATE_IMPL(REG_DIL, reg_DIL)
    DIANA_GENERATE_IMPL(REG_BPL, reg_BPL)
    DIANA_GENERATE_IMPL(REG_SPL, reg_SPL)
    DIANA_GENERATE_IMPL(REG_R8, reg_R8)
    DIANA_GENERATE_IMPL(REG_R9, reg_R9)
    DIANA_GENERATE_IMPL(REG_R10, reg_R10)
    DIANA_GENERATE_IMPL(REG_R11, reg_R11)
    DIANA_GENERATE_IMPL(REG_R12, reg_R12)
    DIANA_GENERATE_IMPL(REG_R13, reg_R13)
    DIANA_GENERATE_IMPL(REG_R14, reg_R14)
    DIANA_GENERATE_IMPL(REG_R15, reg_R15)
    DIANA_GENERATE_IMPL(REG_R8D, reg_R8D)
    DIANA_GENERATE_IMPL(REG_R9D, reg_R9D)
    DIANA_GENERATE_IMPL(REG_R10D, reg_R10D)
    DIANA_GENERATE_IMPL(REG_R11D, reg_R11D)
    DIANA_GENERATE_IMPL(REG_R12D, reg_R12D)
    DIANA_GENERATE_IMPL(REG_R13D, reg_R13D)
    DIANA_GENERATE_IMPL(REG_R14D, reg_R14D)
    DIANA_GENERATE_IMPL(REG_R15D, reg_R15D)
    DIANA_GENERATE_IMPL(REG_R8W, reg_R8W)
    DIANA_GENERATE_IMPL(REG_R9W, reg_R9W)
    DIANA_GENERATE_IMPL(REG_R10W, reg_R10W)
    DIANA_GENERATE_IMPL(REG_R11W, reg_R11W)
    DIANA_GENERATE_IMPL(REG_R12W, reg_R12W)
    DIANA_GENERATE_IMPL(REG_R13W, reg_R13W)
    DIANA_GENERATE_IMPL(REG_R14W, reg_R14W)
    DIANA_GENERATE_IMPL(REG_R15W, reg_R15W)
    DIANA_GENERATE_IMPL(REG_R8B, reg_R8B)
    DIANA_GENERATE_IMPL(REG_R9B, reg_R9B)
    DIANA_GENERATE_IMPL(REG_R10B, reg_R10B)
    DIANA_GENERATE_IMPL(REG_R11B, reg_R11B)
    DIANA_GENERATE_IMPL(REG_R12B, reg_R12B)
    DIANA_GENERATE_IMPL(REG_R13B, reg_R13B)
    DIANA_GENERATE_IMPL(REG_R14B, reg_R14B)
    DIANA_GENERATE_IMPL(REG_R15B, reg_R15B)
    DIANA_GENERATE_IMPL(REG_RIP, reg_RIP)
    DIANA_GENERATE_IMPL(REG_MM0, reg_MM0)
    DIANA_GENERATE_IMPL(REG_MM1, reg_MM1)
    DIANA_GENERATE_IMPL(REG_MM2, reg_MM2)
    DIANA_GENERATE_IMPL(REG_MM3, reg_MM3)
    DIANA_GENERATE_IMPL(REG_MM4, reg_MM4)
    DIANA_GENERATE_IMPL(REG_MM5, reg_MM5)
    DIANA_GENERATE_IMPL(REG_MM6, reg_MM6)

    DIANA_GENERATE_IMPL(REG_FPU_ST0, reg_fpu_ST0)
    DIANA_GENERATE_IMPL(REG_FPU_ST1, reg_fpu_ST1)
    DIANA_GENERATE_IMPL(REG_FPU_ST2, reg_fpu_ST2)
    DIANA_GENERATE_IMPL(REG_FPU_ST3, reg_fpu_ST3)
    DIANA_GENERATE_IMPL(REG_FPU_ST4, reg_fpu_ST4)
    DIANA_GENERATE_IMPL(REG_FPU_ST5, reg_fpu_ST5)
    DIANA_GENERATE_IMPL(REG_FPU_ST6, reg_fpu_ST6)
    
    ;
    return pData;
}
