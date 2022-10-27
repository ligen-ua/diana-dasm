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


DianaRegisterXMM_type DianaProcessor_XMM_GetValue(DianaProcessor * pCallContext,
                                                  DianaRegInfo * pReg);

DianaRegisterXMM_type DianaProcessor_XMM_GetValueEx(DianaProcessor * pCallContext,
                                                    DianaRegInfo * pReg,
                                                    int size);

void DianaProcessor_XMM_SetValue(DianaProcessor * pCallContext,
                                 DianaUnifiedRegister regId,
                                 DianaRegInfo * pReg,
                                 DianaRegisterXMM_type * value);

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
#define DI_MEM_GET_XMM_CHANGE_SIZE(variable, number) \
    DI_CHECK(DianaProcessor_XMM_SetGetOperand(pDianaContext, \
                                              pCallContext, \
                                              number, \
                                              &variable, \
                                              0, \
                                              variable##_size, \
                                              0, \
                                              1));

#define DI_MEM_SET_XMM_CHANGE_SIZE(variable, number) \
    DI_CHECK(DianaProcessor_XMM_SetGetOperand(pDianaContext, \
                                              pCallContext, \
                                              number, \
                                              &variable, \
                                              1, \
                                              variable##_size, \
                                              0, \
                                              1));
#define DI_MEM_GET_XMM(variable, number) \
    DI_CHECK(DianaProcessor_XMM_SetGetOperand(pDianaContext, \
                                              pCallContext, \
                                              number, \
                                              &variable, \
                                              0, \
                                              variable##_size, \
                                              0, \
                                              0));

#define DI_MEM_SET_XMM(variable, number) \
    DI_CHECK(DianaProcessor_XMM_SetGetOperand(pDianaContext, \
                                              pCallContext, \
                                              number, \
                                              &variable, \
                                              1, \
                                              variable##_size, \
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
