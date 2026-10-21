#ifndef DIANA_PROCESSOR_CORE_IMPL_XMM_H
#define DIANA_PROCESSOR_CORE_IMPL_XMM_H

#include "diana_processor_core_impl.h"


typedef union _DianaRegisterFPU
{
    DI_CHAR u8[10];
}DianaRegisterFPU_type;


typedef union _DianaRegisterXMM
{
    DI_CHAR             u8[ 16 ];
    DI_UINT16           u16[ 8 ];
    DI_UINT32           u32[ 4 ];
    OPERAND_SIZE        u64[ 2 ];
    DI_SIGNED_CHAR      s8[ 16 ];
    DI_INT16            s16[ 8 ];
    DI_INT32            s32[ 4 ];
    OPERAND_SIZE_SIGNED s64[ 2 ];
} DianaRegisterXMM_type;

typedef union _DianaRegisterYMM
{
    DI_CHAR               u8[ 32 ];
    DI_UINT16             u16[ 16 ];
    DI_UINT32             u32[ 8 ];
    OPERAND_SIZE          u64[ 4 ];
    DI_SIGNED_CHAR        s8[ 32 ];
    DI_INT16              s16[ 16 ];
    DI_INT32              s32[ 8 ];
    OPERAND_SIZE_SIGNED   s64[ 4 ];
    DianaRegisterXMM_type xmm[ 2 ];   // the 128-bit lanes
} DianaRegisterYMM_type;

typedef union _DianaRegisterZMM
{
    DI_CHAR               u8[ 64 ];
    DI_UINT16             u16[ 32 ];
    DI_UINT32             u32[ 16 ];
    OPERAND_SIZE          u64[ 8 ];
    DI_SIGNED_CHAR        s8[ 64 ];
    DI_INT16              s16[ 32 ];
    DI_INT32              s32[ 16 ];
    OPERAND_SIZE_SIGNED   s64[ 8 ];
    DianaRegisterXMM_type xmm[ 4 ];   // the 128-bit lanes
    DianaRegisterYMM_type ymm[ 2 ];
} DianaRegisterZMM_type;

// reg_YMMi for reg_XMMi or reg_YMMi
DianaUnifiedRegister DianaProcessor_YMM_Register(DianaUnifiedRegister reg);

// reg_ZMMi for any xmm, ymm, zmm register (0..31), reg_none for another register
DianaUnifiedRegister DianaProcessor_ZMM_Register(DianaUnifiedRegister reg);

// the 64 bytes of the ZMM register of reg (xmm, ymm or zmm)
DianaRegisterZMM_type DianaProcessor_ZMM_GetValue(DianaProcessor * pCallContext,
                                                  DianaUnifiedRegister reg);

void DianaProcessor_ZMM_SetValue(DianaProcessor * pCallContext,
                                 DianaUnifiedRegister reg,
                                 const DianaRegisterZMM_type * value);

// the bytes from "bytes" up to 63 of the ZMM register of reg: zero (a VEX or EVEX write of a narrower vector)
void DianaProcessor_ZMM_ClearAbove(DianaProcessor * pCallContext,
                                   DianaUnifiedRegister reg,
                                   int bytes);

// the 32 bytes of the YMM register of reg (reg_XMMi or reg_YMMi)
DianaRegisterYMM_type DianaProcessor_YMM_GetValue(DianaProcessor * pCallContext,
                                                  DianaUnifiedRegister reg);

void DianaProcessor_YMM_SetValue(DianaProcessor * pCallContext,
                                 DianaUnifiedRegister reg,
                                 const DianaRegisterYMM_type * value);

// bits 511:128 of the ZMM register of reg: zero (a VEX.128 write)
void DianaProcessor_YMM_ClearUpper(DianaProcessor * pCallContext,
                                   DianaUnifiedRegister reg);

// an operand of a VEX instruction: size bytes of an xmm/ymm register or of memory (alignment: 0 or the bytes the
// address must be a multiple of). A register is read from bit 0; a write of an xmm register clears bits 511:128,
// of a ymm register bits 511:256 (VEX), a write of a general register (vmovd r32) zero-extends it as a 32-bit write
// does.
int DianaProcessor_YMM_SetGetOperand(struct _dianaContext * pDianaContext,
                                     DianaProcessor * pCallContext,
                                     int opNumber,
                                     DianaRegisterYMM_type * pValue,
                                     int bSet,
                                     int size,
                                     int alignment);


DianaRegisterXMM_type DianaProcessor_XMM_GetValue(DianaProcessor * pCallContext,
                                                  DianaRegInfo * pReg);

DianaRegisterXMM_type DianaProcessor_XMM_GetValueEx(DianaProcessor * pCallContext,
                                                    DianaRegInfo * pReg,
                                                    int size);

void DianaProcessor_XMM_SetValue(DianaProcessor * pCallContext,
                                 DianaUnifiedRegister regId,
                                 DianaRegInfo * pReg,
                                 DianaRegisterXMM_type * value);

// DianaProcessor_XMM_SetGetOperand with the size in and out: an MMX register is 8 bytes whatever *pSize asks,
// *pSize is the size of the operand after it
int DianaProcessor_XMM_SetGetOperandEx(struct _dianaContext * pDianaContext,
                                       DianaProcessor * pCallContext,
                                       int opNumber,
                                       DianaRegisterXMM_type * pResult,
                                       int bSet,
                                       int * pSize,
                                       int flags,
                                       int setNewSizeLegacyMode);

void DianaProcessor_XMM_SetValueEx(DianaProcessor * pCallContext,
                                   DianaUnifiedRegister regId,
                                   DianaRegInfo * pReg,
                                   DianaRegisterXMM_type * value,
                                   int size);

int DianaProcessor_XMM_GetMemValue(DianaProcessor * pThis,
                                   OPERAND_SIZE selector,
                                   OPERAND_SIZE offset,
                                   OPERAND_SIZE size,
                                   DianaRegisterXMM_type * pResult,
                                   int flags,
                                   DianaUnifiedRegister segReg);

int DianaProcessor_XMM_SetMemValue(DianaProcessor * pThis,
                                   OPERAND_SIZE selector,
                                   OPERAND_SIZE offset,
                                   OPERAND_SIZE size,
                                   DianaRegisterXMM_type * pResult,
                                   int flags,
                                   DianaUnifiedRegister segReg);

int DianaProcessor_XMM_SetGetOperand(struct _dianaContext * pDianaContext,
                                     DianaProcessor * pCallContext,
                                     int opNumber,
                                     DianaRegisterXMM_type * pResult,
                                     int bSet,
                                     int sizeOfOperand,
                                     int flags,
                                     int setNewSizeLegacyMode);

int Diana_ProcessorSetGetOperand_XMM_index(struct _dianaContext * pDianaContext,
                                           DianaProcessor * pCallContext,
                                           int usedSize,
                                           DianaRegisterXMM_type * pResult,
                                           int bSet,
                                           DianaRmIndex * pRmIndex,
                                           int flags);

#define DI_DEF_LOCAL_XMM(X) \
    DianaRegisterXMM_type X = {0}; \
    int X##_size = 16;
//
#define DI_DEF_LOCAL_XMM_FPU(X) \
    DianaRegisterXMM_type X = {0}; \
    int X##_size = 8;
//
// the xmm operand macros: *_CHANGE_SIZE use variable##_size as the operand size (a scalar element of memory), the
// others the size of the operand; an MMX register is always 8 bytes. variable##_size is the size used after it.
#define DI_MEM_GET_XMM_CHANGE_SIZE(variable, number) \
    DI_CHECK(DianaProcessor_XMM_SetGetOperandEx(pDianaContext, \
                                                pCallContext, \
                                                number, \
                                                &variable, \
                                                0, \
                                                &variable##_size, \
                                                0, \
                                                1));

#define DI_MEM_SET_XMM_CHANGE_SIZE(variable, number) \
    DI_CHECK(DianaProcessor_XMM_SetGetOperandEx(pDianaContext, \
                                                pCallContext, \
                                                number, \
                                                &variable, \
                                                1, \
                                                &variable##_size, \
                                                0, \
                                                1));
#define DI_MEM_GET_XMM(variable, number) \
    DI_CHECK(DianaProcessor_XMM_SetGetOperandEx(pDianaContext, \
                                                pCallContext, \
                                                number, \
                                                &variable, \
                                                0, \
                                                &variable##_size, \
                                                0, \
                                                0));

#define DI_MEM_SET_XMM(variable, number) \
    DI_CHECK(DianaProcessor_XMM_SetGetOperandEx(pDianaContext, \
                                                pCallContext, \
                                                number, \
                                                &variable, \
                                                1, \
                                                &variable##_size, \
                                                0, \
                                                0));

#define DI_MEM_GET_SRC_XMM(variable)    DI_MEM_GET_XMM_CHANGE_SIZE(variable, 1)
#define DI_MEM_SET_SRC_XMM(variable)    DI_MEM_SET_XMM_CHANGE_SIZE(variable, 1)
#define DI_MEM_GET_DEST_XMM(variable)   DI_MEM_GET_XMM_CHANGE_SIZE(variable, 0)
#define DI_MEM_SET_DEST_XMM(variable)   DI_MEM_SET_XMM_CHANGE_SIZE(variable, 0)


#define DI_MEM_GET_SRC_ANY(variable)    DI_MEM_GET_XMM(variable, 1)
#define DI_MEM_SET_SRC_ANY(variable)    DI_MEM_SET_XMM(variable, 1)
#define DI_MEM_GET_DEST_ANY(variable)   DI_MEM_GET_XMM(variable, 0)
#define DI_MEM_SET_DEST_ANY(variable)   DI_MEM_SET_XMM(variable, 0)

// 8-bytes API
void DianaProcessor_FPU_SetSTRegister(DianaProcessor * pCallContext,
                                              DianaUnifiedRegister recognizedRegister,
                                              const OPERAND_SIZE * pValue);

OPERAND_SIZE DianaProcessor_FPU_GetSTRegister(DianaProcessor * pCallContext,
                                               DianaUnifiedRegister recognizedRegister);

#endif
