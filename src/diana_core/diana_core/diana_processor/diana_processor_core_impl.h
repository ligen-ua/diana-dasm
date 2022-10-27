#ifndef DIANA_PROCESSOR_CORE_IMPL_H
#define DIANA_PROCESSOR_CORE_IMPL_H

#include "diana_core.h"
#include "diana_proc_gen.h"
#include "diana_processor_streams.h"
#include "diana_uids.h"
#include "diana_core_gen_tags.h"
typedef struct _DianaRegInfo
{
    int m_size;
    int m_offset;
}DianaRegInfo;

// FLAGS
#define DI_FLAG_CF         0x000001
#define DI_FLAG_1          0x000002 // 1
#define DI_FLAG_PF         0x000004
#define DI_FLAG_3          0x000008 // 0
#define DI_FLAG_AF         0x000010
#define DI_FLAG_5          0x000020 // 0
#define DI_FLAG_ZF         0x000040
#define DI_FLAG_SF         0x000080
#define DI_FLAG_TF         0x000100
#define DI_FLAG_IF         0x000200
#define DI_FLAG_DF         0x000400
#define DI_FLAG_OF         0x000800
#define DI_FLAG_IOPL_0     0x001000
#define DI_FLAG_IOPL_1     0x002000    
#define DI_FLAG_NT         0x004000
#define DI_FLAG_15         0x008000 // 0
//EFLAGS
#define DI_FLAG_RF         0x010000
#define DI_FLAG_VM         0x020000
#define DI_FLAG_AC         0x040000
#define DI_FLAG_VIF        0x080000
#define DI_FLAG_VIP        0x100000
#define DI_FLAG_ID         0x200000

// FPU flags
#define DI_FPU_SW_BACKWARD        0x8000  //  backward compatibility 
#define DI_FPU_SW_C3             0x4000  //  condition bit 3 
#define DI_FPU_SW_TOP            0x3800  //  top of stack 
#define DI_FPU_SW_C2            0x0400  //  condition bit 2 
#define DI_FPU_SW_C1            0x0200  //  condition bit 1 
#define DI_FPU_SW_C0            0x0100  //  condition bit 0 
#define DI_FPU_SW_SUMMARY       0x0080  //  exception summary 
#define DI_FPU_SW_STACK_FAULT    0x0040  //  stack fault 
#define DI_FPU_SW_PRECISION      0x0020  //  loss of precision 
#define DI_FPU_SW_UNDERDLOW       0x0010  //  underflow 
#define DI_FPU_SW_OVERDLOW        0x0008  //  overflow 
#define DI_FPU_SW_ZERO_DIV        0x0004  //  divide by zero 
#define DI_FPU_SW_DENORMAL_OP   0x0002  //  denormalized operand 
#define DI_FPU_SW_INVALID        0x0001  //  invalid operation 
#define DI_FPU_SW_CC            (DI_FPU_SW_C0|DI_FPU_SW_C1|DI_FPU_SW_C2|DI_FPU_SW_C3)

#define DI_FPU_SW_NO_TOP           (~DI_FPU_SW_TOP)
#define DI_FPU_SW_ALL_EXCEPTIONS     0x027f


#define DI_FPU_CW_RESERVED_BITS      0xe0c0  // reserved bits 
#define DI_FPU_CW_INF                0x1000  // infinity control, legacy 
#define DI_FPU_CW_ROUNDING_CONTROL     0x0C00  // rounding control 
#define DI_FPU_CW_PRECISION_CONTROL  0x0300  // precision control 
#define DI_FPU_CW_RESERVED_40        0x0040  // reserved as 1
#define DI_FPU_CW_PRECISION          0x0020  // loss of precision mask 
#define DI_FPU_CW_UNDERFLOW          0x0010  // underflow mask 
#define DI_FPU_CW_OVERFLOW             0x0008  // overflow mask 
#define DI_FPU_CW_ZERO_DIV             0x0004  // divide by zero mask 
#define DI_FPU_CW_DENORMAL             0x0002  // denormalized operand mask 
#define DI_FPU_CW_INVALID             0x0001  // invalid operation mask 
#define DI_FPU_CW_ALL_EXCEPTIONS     0x003f  


#define DI_FPU_EX_PRECISION            0x0020  // loss of precision
#define DI_FPU_EX_UNDERFLOW         0x0010  // underflow
#define DI_FPU_EX_OVERFLOW          0x0008  // overflow
#define DI_FPU_EX_ZERO_DIV          0x0004  // divide by zero
#define DI_FPU_EX_DENORMAL          0x0002  // denormalized operand
#define DI_FPU_EX_INVALID           0x0001  // invalid operation
#define DI_FPU_EX_STACK_OVERFLOW    (0x0041|DI_FPU_SW_C1)     // stack overflow
#define DI_FPU_EX_STACK_UNDERFLOW   0x0041        // stack underflow


typedef union _DianaRegisterValue16
{
    struct _DianaRegisterValue16_impl
    {
        unsigned char l;
        unsigned char h;
    }impl;
    DI_UINT16 value;
} DianaRegisterValue16_type;

typedef union _DianaRegisterValue32
{
    struct _DianaRegisterValue32_impl
    {
        DianaRegisterValue16_type l;
        DI_UINT16 h;
    }impl;
    DI_UINT32 value;
} DianaRegisterValue32_type;


typedef union _DianaRegisterValue
{
    struct _DianaRegisterValue_impl
    {
        DianaRegisterValue32_type l;
        DI_UINT32 h;
    }impl;
    OPERAND_SIZE value;
} DianaRegisterValue_type;

typedef union _DianaRegisterValue128
{
    DI_CHAR value[16];
} DianaRegisterValue128_type;


// SIGNED
typedef union _DianaRegisterValue16_signed
{
    struct _DianaRegisterValue16_signed_impl
    {
        DI_SIGNED_CHAR l;
        DI_SIGNED_CHAR h;
    }impl;
    DI_INT16 value;
} DianaRegisterValue16_signed_type;

typedef union _DianaRegisterValue32_signed
{
    struct _DianaRegisterValue32_signed_impl
    {
        DianaRegisterValue16_signed_type l;
        DI_INT16 h;
    }impl;
    DI_INT32 value;
} DianaRegisterValue32_signed_type;


typedef union _DianaRegisterValue_signed
{
    struct _DianaRegisterValue_signed_impl
    {
        DianaRegisterValue32_signed_type l;
        DI_INT32 h;
    }impl;
    OPERAND_SIZE_SIGNED value;
} DianaRegisterValue_signed_type;


//RFLAGS
#define DI_PROC_STATE_TEMP_RIP_IS_VALID       0x1
#define DI_PROC_STATE_RIP_CHANGED             0x2
#define DI_PROC_STATE_UPDATE_FLAGS_PSZ        0x4 
#define DI_PROC_STATE_CMD_USES_NORMAL_REP     0x8 

// forward declaration
struct _dianaProcessorFirePoint;

typedef void (*FireAction_type)(struct _dianaProcessorFirePoint * pPoint,
                                struct _dianaProcessor * pProcessor);
typedef struct _dianaProcessorFirePoint
{
    void * pContext;
    OPERAND_SIZE address;
    FireAction_type action;
}DianaProcessorFirePoint;


#define DI_PROCESSOR_FPU_REGISTER_BUSY    1
typedef struct _dianaFPU
{
    DI_UINT16 controlWord; // control word
    DI_UINT16 statusWord; // status word
    DI_CHAR registerFlags[8];
}DianaFPU;

#define DIANA_PROCESSOR_MAX_FIRE_POINTS     10

// ALIGNMENT:
//  If alignment checking is enabled (CR0.AM = 1, RFLAGS.AC = 1, and CPL = 3), 
//    an alignment-check exception (#AC) may or may not be generated (depending on processor implementation) 
//    when the operand is not aligned on an 8-byte boundary.
// http://www.tptp.cc/mirrors/siyobik.info/instruction/MOVDQU.html
// ----
// NOTE: historically, emulator always performed the alignment check without any analysis of registers and flags
//       which is wrong. now it does not perform the check by default because of "may not be generated" statement
//       if you want the legacy check to be enabled use:
//            DianaProcessor_SetOptions + DIANA_PROCESSOR_OPTION_CHECK_ALIGNMENT_LEGACY_MODE
//       if you want the new, correct check to be enabled use:
//            DianaProcessor_SetOptions + DIANA_PROCESSOR_OPTION_CHECK_ALIGNMENT_STRICT_MODE 
//----
#define DIANA_PROCESSOR_OPTION_CHECK_ALIGNMENT_LEGACY_MODE       1
#define DIANA_PROCESSOR_OPTION_CHECK_ALIGNMENT_STRICT_MODE       2


typedef int (*DianaProcessorCustomCommandProvider_type)(struct _dianaProcessor * pProcessor,
                                                        void * pCustomProviderContext,
                                                        const DianaGroupInfo * pGroupInfo,
                                                        DianaProcessorCommand_type * ppCommand);


typedef struct _dianaProcessRandom_pcg32
{ 
    DI_UINT64 state;  
    DI_UINT64 inc;
}DianaProcessRandom;

void DianaProcessRandom_Init(DianaProcessRandom * pRandom);
DI_UINT64 DianaProcessRandom_Generate(DianaProcessRandom * pRandom);

typedef struct _dianaProcessor
{
    DianaBase m_base;
    int m_iLastRegistersOffset;
    char * m_pRegistersVector;
    int m_registersVectorSize;

    DianaRegInfo m_registers[count_of_DianaUnifiedRegister];
    DianaRandomReadWriteStream * m_pMemoryStream;
    Diana_Allocator * m_pAllocator;
    
    DianaRegisterValue_type m_flags;
    
    DianaReadStream m_readStream;
    DianaContext m_context;
    DianaParserResult m_result;

    int m_stateFlags;
    int m_stateFlagsToRemove;
    OPERAND_SIZE m_tempRIP;

    DianaProcessorFirePoint m_firePoints[DIANA_PROCESSOR_MAX_FIRE_POINTS];
    int m_firePointsCount;

    DianaFPU m_fpu;
    int m_options;
    DianaProcessorCustomCommandProvider_type m_customProvider;
    void * m_pCustomProviderContext;
    int m_initialDianaMode;
    OPERAND_SIZE m_lastCallRIP;
    OPERAND_SIZE m_lastCallRSP;
    DianaProcessRandom m_random;
}DianaProcessor;

#define UPDATE_PSZ(X, highMask) \
    if (pCallContext->m_stateFlags & DI_PROC_STATE_UPDATE_FLAGS_PSZ)\
    {\
        CLEAR_FLAG_PF;\
        CLEAR_FLAG_SF;\
        CLEAR_FLAG_ZF;\
        if ((X))\
        {\
            if ((X)&highMask)\
                SET_FLAG_SF;\
            if (IsParity((unsigned char)((X))))\
                SET_FLAG_PF;\
        }\
        else\
        {\
            SET_FLAG_ZF;\
            SET_FLAG_PF;\
        }\
    }


const char * Diana_GenerateStuff();

int DianaProcessor_RegisterFirePoint(DianaProcessor * pCallContext,
                                     const DianaProcessorFirePoint * pPoint);

void DianaProcessor_CmdUsesNormalRep(DianaProcessor * pCallContext);

int DianaProcessor_InitProcessorImpl(DianaProcessor * pThis);

OPERAND_SIZE DianaProcessor_GetValue(DianaProcessor * pCallContext,
                                     DianaRegInfo * pReg);

OPERAND_SIZE DianaProcessor_GetValueEx(DianaProcessor * pCallContext,
                                       DianaRegInfo * pReg,
                                       int size);

void DianaProcessor_SetValueEx(DianaProcessor * pCallContext,
                               DianaUnifiedRegister regId,
                               DianaRegInfo * pReg,
                               OPERAND_SIZE value,
                               int size);

// This function only for size <= sizeof(*pResult)
int DianaProcessor_GetMemValue(DianaProcessor * pThis,
                               OPERAND_SIZE selector,
                               OPERAND_SIZE offset,
                               OPERAND_SIZE size,
                               OPERAND_SIZE * pResult,
                               int flags,
                               DianaUnifiedRegister segReg);

// This function only for size <= sizeof(*pResult)
int DianaProcessor_SetMemValue(DianaProcessor * pThis,
                               OPERAND_SIZE selector,
                               OPERAND_SIZE offset,
                               OPERAND_SIZE size,
                               OPERAND_SIZE * pResult,
                               int flags,
                               DianaUnifiedRegister segReg);

//-------------
DianaRegInfo * DianaProcessor_QueryReg(DianaProcessor * pThis, 
                                       DianaUnifiedRegister reg);

OPERAND_SIZE DianaProcessor_GetSignMask(int sizeInBytes);

OPERAND_SIZE DianaProcessor_GetSignMaskSpecifyBit(OPERAND_SIZE sizeInBits);

void DianaProcessor_SetValue(DianaProcessor * pCallContext,
                             DianaUnifiedRegister regId,
                             DianaRegInfo * pReg,
                             OPERAND_SIZE value);

int  DianaProcessor_QueryFlag(DianaProcessor * pThis, 
                              OPERAND_SIZE flag);

void DianaProcessor_SetFlag(DianaProcessor * pThis, 
                            OPERAND_SIZE flag);

void DianaProcessor_ClearFlag(DianaProcessor * pThis, 
                              OPERAND_SIZE flag);

int DianaProcessor_SetGetOperand(struct _dianaContext * pDianaContext,
                                 DianaProcessor * pCallContext,
                                 int opNumber,
                                 OPERAND_SIZE * pResult,
                                 int bSet,
                                 int * pSizeOfOperand,
                                 int flags);

void DianaProcessor_ProcImplInit();

void DianaProcessor_UpdatePSZ(DianaProcessor * pCallContext,
                              OPERAND_SIZE value,
                              int opSize);

OPERAND_SIZE DianaProcessor_CutValue(OPERAND_SIZE value,
                                     int size);

int DianaProcessor_SetCOA_Add(struct _dianaContext * pDianaContext,
                              DianaProcessor * pCallContext,
                              const OPERAND_SIZE * pOldValue,
                              const OPERAND_SIZE * pNewValue,
                              const OPERAND_SIZE * pOperand,
                              int opSize,
                              int bSetCF
                              );

int DianaProcessor_SetCOA_AddCF(struct _dianaContext * pDianaContext,
                                DianaProcessor * pCallContext,
                                const OPERAND_SIZE * pOldValue,
                                const OPERAND_SIZE * pNewValue,
                                const OPERAND_SIZE * pOperand,
                                int opSize,
                                int bSetCF
                                );

int DianaProcessor_SetCOA_Sub(struct _dianaContext * pDianaContext,
                              DianaProcessor * pCallContext,
                              const OPERAND_SIZE * pOldValue,
                              const OPERAND_SIZE * pNewValue,
                              const OPERAND_SIZE * pOperand,
                              int opSize,
                              int bSetCF
                              );

int DianaProcessor_SetCOA_SubCF(struct _dianaContext * pDianaContext,
                                DianaProcessor * pCallContext,
                                const OPERAND_SIZE * pOldValue,
                                const OPERAND_SIZE * pNewValue,
                                const OPERAND_SIZE * pOperand,
                                int opSize,
                                int bSetCF
                                );

int DianaProcessor_SignExtend(OPERAND_SIZE * pVariable, 
                              int size, 
                              int newSize);

int DianaProcessor_CalcIndex(struct _dianaContext * pDianaContext,
                             DianaProcessor * pCallContext,
                             const DianaRmIndex * pIndex,
                             OPERAND_SIZE * pSelector,
                             OPERAND_SIZE * pAddress);

int DianaProcessor_GetAddress(struct _dianaContext * pDianaContext,
                              DianaProcessor * pCallContext,
                              DianaLinkedOperand * pLinkedOp,
                              OPERAND_SIZE * pSelector,
                              OPERAND_SIZE * pAddress);

int Diana_ProcessorGetOperand_index_ex2(struct _dianaContext * pDianaContext,
                                        DianaProcessor * pCallContext,
                                        int usedSize,
                                        OPERAND_SIZE * pAddress,
                                        OPERAND_SIZE * pResult,
                                        DianaRmIndex * pRmIndex,
                                        OPERAND_SIZE * pSelector);

int DianaProcessor_ReadMemory(DianaProcessor * pThis,
                              OPERAND_SIZE selector,
                              OPERAND_SIZE offset,
                              void * pBuffer, 
                              OPERAND_SIZE iBufferSize, 
                              OPERAND_SIZE * readBytes,
                              int flags,
                              DianaUnifiedRegister segReg);
int DianaProcessor_ReadMemory_Exact(DianaProcessor * pThis,
                                    OPERAND_SIZE selector,
                                    OPERAND_SIZE offset,
                                    void * pBuffer, 
                                    OPERAND_SIZE iBufferSize, 
                                    int flags,
                                    DianaUnifiedRegister segReg);
int DianaProcessor_WriteMemory(DianaProcessor * pThis,
                               OPERAND_SIZE selector,
                               OPERAND_SIZE offset,
                               void * pBuffer, 
                               OPERAND_SIZE iBufferSize, 
                               OPERAND_SIZE * readed,
                               int flags,
                               DianaUnifiedRegister segReg);

int Diana_ProcessorSetGetOperand_index(struct _dianaContext * pDianaContext,
                                       DianaProcessor * pCallContext,
                                       int usedSize,
                                       OPERAND_SIZE * pResult,
                                       int bSet,
                                       DianaRmIndex * pRmIndex,
                                       int flags);

int Diana_ProcessorGetOperand_index_ex(struct _dianaContext * pDianaContext,
                                       DianaProcessor * pCallContext,
                                       int usedSize,
                                       OPERAND_SIZE * pAddress,
                                       OPERAND_SIZE * pResult,
                                       DianaRmIndex * pRmIndex);

void DianaProcessor_SetResetDefaultFlags(DianaProcessor * pThis);

int DianaProcessor_Query64RegisterFor32(DianaUnifiedRegister registerIn,
                                        DianaUnifiedRegister * pUsedReg);

int DianaProcessor_QueryRaxRegister(int size, 
                                    DianaUnifiedRegister * pUsedReg);
int DianaProcessor_QueryRdxRegister(int size, 
                                    DianaUnifiedRegister * pUsedReg);
int DianaProcessor_QueryRcxRegister(int size, 
                                    DianaUnifiedRegister * pUsedReg);

#define DI_UPDATE_FLAGS_PSZ(X) \
    pCallContext->m_stateFlags |= DI_PROC_STATE_UPDATE_FLAGS_PSZ;\
    pCallContext->m_stateFlagsToRemove |= DI_PROC_STATE_UPDATE_FLAGS_PSZ;\
    X; \
    pCallContext->m_stateFlags &= ~DI_PROC_STATE_UPDATE_FLAGS_PSZ;

//----
// MEMORY ACCESS MACROSES
#define DI_DEF_LOCAL_1(X) \
    OPERAND_SIZE X = 0; \
    int X##_size = 0;
//---
#define DI_DEF_LOCAL(X) \
    OPERAND_SIZE oldDestValue = 0; \
    OPERAND_SIZE X = 0; \
    int X##_size = 0;
//---
#define DI_DEF_LOCALS(X, Y) \
    OPERAND_SIZE oldDestValue = 0;\
    OPERAND_SIZE X = 0, Y = 0;\
    int X##_size = 0, Y##_size = 0;
//---
#define DI_DEF_LOCALS2(X, Y, Z) \
    OPERAND_SIZE oldDestValue = 0;\
    OPERAND_SIZE X = 0, Y = 0, Z = 0;\
    int X##_size = 0, Y##_size, Z##_size = 0;
//---
#define DI_MEM_GET(variable, number)  \
    DI_CHECK(DianaProcessor_SetGetOperand(pDianaContext, \
                                          pCallContext, \
                                          number, \
                                          &variable, \
                                          0, \
                                          &variable##_size, \
                                          0));

#define DI_MEM_SET(variable, number)  \
    DI_CHECK(DianaProcessor_SetGetOperand(pDianaContext, \
                                         pCallContext, \
                                         number, \
                                         &variable, \
                                         1,  \
                                         &variable##_size, \
                                         0));

#define DI_MEM_GET_SRC2(variable)    DI_MEM_GET(variable, 2)
#define DI_MEM_SET_SRC2(variable)    DI_MEM_SET(variable, 2)
#define DI_MEM_GET_SRC(variable)    DI_MEM_GET(variable, 1)
#define DI_MEM_SET_SRC(variable)    DI_MEM_SET(variable, 1)
#define DI_MEM_GET_DEST(variable)   DI_MEM_GET(variable, 0)
#define DI_MEM_SET_DEST(variable)   DI_MEM_SET(variable, 0)
#define DI_MEM_GET_DEST_SIZE(variable)   DI_CHECK(DianaProcessor_GetOperandSize(pCallContext, 0, &variable##_size));

#define DI_PROC_END \
    return DI_SUCCESS;

#define DI_VAR_SIZE(variable)   (variable##_size)

//---
#define DI_START_UPDATE_COA_FLAGS(dest) \
    CLEAR_FLAG_AF;\
    CLEAR_FLAG_OF;\
    CLEAR_FLAG_CF;\
    oldDestValue = dest;
//-
#define DI_END_UPDATE_COA_FLAGS_ADD(dest, src) \
    DI_CHECK(DianaProcessor_SetCOA_Add(pDianaContext, \
                                       pCallContext,\
                                       &oldDestValue,\
                                       &(dest),\
                                       &(src),\
                                       DI_VAR_SIZE(dest), \
                                       DI_FLAG_CF|DI_FLAG_OF|DI_FLAG_AF));

#define DI_END_UPDATE_COA_FLAGS_ADDCF(dest, src) \
    DI_CHECK(DianaProcessor_SetCOA_AddCF(pDianaContext, \
                                         pCallContext,\
                                         &oldDestValue,\
                                         &(dest),\
                                         &(src),\
                                         DI_VAR_SIZE(dest), \
                                         DI_FLAG_CF|DI_FLAG_OF|DI_FLAG_AF));
//-
#define DI_END_UPDATE_COA_FLAGS_SUB(dest, src) \
    DI_CHECK(DianaProcessor_SetCOA_Sub(pDianaContext, \
                                       pCallContext,\
                                       &oldDestValue,\
                                       &(dest),\
                                       &(src),\
                                       DI_VAR_SIZE(dest), \
                                       DI_FLAG_CF|DI_FLAG_OF|DI_FLAG_AF));

#define DI_END_UPDATE_COA_FLAGS_SUBCF(dest, src) \
    DI_CHECK(DianaProcessor_SetCOA_SubCF(pDianaContext, \
                                         pCallContext,\
                                         &oldDestValue,\
                                         &(dest),\
                                         &(src),\
                                         DI_VAR_SIZE(dest), \
                                         DI_FLAG_CF|DI_FLAG_OF|DI_FLAG_AF));

// OA
#define DI_START_UPDATE_OA_FLAGS(dest) \
    CLEAR_FLAG_AF;\
    CLEAR_FLAG_OF;\
    oldDestValue = dest;

//-
#define DI_END_UPDATE_OA_FLAGS_ADD(dest, src) \
    DI_CHECK(DianaProcessor_SetCOA_Add(pDianaContext, \
                                   pCallContext,\
                                   &oldDestValue,\
                                   &(dest),\
                                   &(src),\
                                   DI_VAR_SIZE(dest), \
                                   DI_FLAG_OF|DI_FLAG_AF));
//-
#define DI_END_UPDATE_OA_FLAGS_SUB(dest, src) \
    DI_CHECK(DianaProcessor_SetCOA_Sub(pDianaContext, \
                                   pCallContext,\
                                   &oldDestValue,\
                                   &(dest),\
                                   &(src),\
                                   DI_VAR_SIZE(dest), \
                                   DI_FLAG_OF|DI_FLAG_AF));

// A
#define DI_START_UPDATE_O_FLAGS(dest) \
    CLEAR_FLAG_OF;\
    oldDestValue = dest;

//-
#define DI_END_UPDATE_O_FLAGS_ADD(dest, src) \
    DI_CHECK(DianaProcessor_SetCOA_Add(pDianaContext, \
                                   pCallContext,\
                                   &oldDestValue,\
                                   &(dest),\
                                   &(src),\
                                   DI_VAR_SIZE(dest), \
                                   DI_FLAG_OF));
//-
#define DI_END_UPDATE_O_FLAGS_SUB(dest, src) \
    DI_CHECK(DianaProcessor_SetCOA_Sub(pDianaContext, \
                                   pCallContext,\
                                   &oldDestValue,\
                                   &(dest),\
                                   &(src),\
                                   DI_VAR_SIZE(dest), \
                                   DI_FLAG_OF));


#define DI_SIGN_EXTEND(variable, newSize) \
    DI_CHECK(DianaProcessor_SignExtend(&variable, DI_VAR_SIZE(variable), newSize));


int DianaProcessor_GetOperandSize(DianaProcessor * pCallContext,
                                  int opNumber,
                                  int * pSizeOfOperand);

int Diana_QueryAddress(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext, 
                           int argument,
                           OPERAND_SIZE * pSelector,
                           OPERAND_SIZE * pAddress,
                           DianaUnifiedRegister * pSeg_reg);

int Diana_WriteRawBufferToArgMem(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext, 
                           int argumentNumber,
                           void * pBuffer,
                           OPERAND_SIZE size,
                           OPERAND_SIZE * doneBytes,
                           int flags);

int Diana_ReadRawBufferFromArgMem(struct _dianaContext * pDianaContext,
                           DianaProcessor * pCallContext, 
                           int argument,
                           void * pBuffer,
                           OPERAND_SIZE size,
                           OPERAND_SIZE * doneBytes,
                           int flags);

int Diana_FPU_CheckExceptions(DianaProcessor * pCallContext);
DI_UINT16 Diana_FPU_QueryStatusWord(DianaProcessor * pCallContext);
int Diana_FPU_CheckFPU(DianaProcessor * pCallContext, int bIgnoreExceptions);
DianaRegInfo * DianaProcessor_FPU_QueryReg(DianaProcessor * pThis, 
                                           DianaUnifiedRegister reg);
void DianaProcessor_SetOptions(DianaProcessor * pThis, int optionsToSet, int optionsToRemove);

int Diana_FPU_GetStackTop(DianaProcessor * pCallContext);
void Diana_FPU_SetStackTop(DianaProcessor * pCallContext, int stackTop);
int Diana_FPU_GetStackTop(DianaProcessor * pCallContext);



#define DI_FPU_GET_SW_C3  ((pCallContext->m_fpu.statusWord & DI_FPU_SW_C3)?1:0)
#define DI_FPU_GET_SW_C2  ((pCallContext->m_fpu.statusWord & DI_FPU_SW_C2)?1:0)
#define DI_FPU_GET_SW_C1  ((pCallContext->m_fpu.statusWord & DI_FPU_SW_C1)?1:0)
#define DI_FPU_GET_SW_C0  ((pCallContext->m_fpu.statusWord & DI_FPU_SW_C0)?1:0)

#endif