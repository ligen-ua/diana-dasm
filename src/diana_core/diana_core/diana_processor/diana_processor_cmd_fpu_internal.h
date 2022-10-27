#ifndef DIANA_PROCESSOR_CMD_FPU_INTERNAL_H
#define DIANA_PROCESSOR_CMD_FPU_INTERNAL_H


#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"
#include "softfloat/softfloatx80.h"
#include "diana_processor_core_impl_xmm.h"



#define DI_FPU_START   DI_CHECK(Diana_FPU_CheckFPU(pCallContext, 0)); &pDianaContext;
#define DI_FPU_START_IGNORE_EXCEPTIONS   DI_CHECK(Diana_FPU_CheckFPU(pCallContext, 1)); &pDianaContext;
#define DI_FPU_CLEAR_C0 pCallContext->m_fpu.statusWord &= ~DI_FPU_SW_C0;
#define DI_FPU_CLEAR_C1 pCallContext->m_fpu.statusWord &= ~DI_FPU_SW_C1;
#define DI_FPU_CLEAR_C2 pCallContext->m_fpu.statusWord &= ~DI_FPU_SW_C2;
#define DI_FPU_CLEAR_C3 pCallContext->m_fpu.statusWord &= ~DI_FPU_SW_C3;


#define DI_FPU_SET_C0 pCallContext->m_fpu.statusWord |= DI_FPU_SW_C0;
#define DI_FPU_SET_C1 pCallContext->m_fpu.statusWord |= DI_FPU_SW_C1;
#define DI_FPU_SET_C2 pCallContext->m_fpu.statusWord |= DI_FPU_SW_C2;
#define DI_FPU_SET_C3 pCallContext->m_fpu.statusWord |= DI_FPU_SW_C3;

#define DI_FPU_SET_CC(cc)  pCallContext->m_fpu.statusWord =(pCallContext->m_fpu.statusWord & ~(DI_FPU_SW_CC))|((cc) & DI_FPU_SW_CC);


#define DI_FPU_REG_IS_EMPTY(Number) Diana_FPU_IsRegEmpty(pCallContext, Number)



#define DI_FPU_PR_32_BITS          0x000
#define DI_FPU_PR_RESERVED_BITS    0x100
#define DI_FPU_PR_64_BITS          0x200
#define DI_FPU_PR_80_BITS          0x300



float_status_t FPU_pre_exception_handling(DianaProcessor * pCallContext);
void Diana_FPU_SetStackTop(DianaProcessor * pCallContext, int stackTop);
int Diana_FPU_GetStackTop(DianaProcessor * pCallContext);
int Diana_FPU_IsRegEmpty(DianaProcessor * pCallContext, int number);
void Diana_FPU_MarkRegState(DianaProcessor * pCallContext, int number, int isEmpty);
void Diana_FPU_Push(DianaProcessor * pCallContext);
void Diana_FPU_Pop(DianaProcessor * pCallContext);


// 80-bit  API
void DianaProcessor_FPU_GetSTRegister_80(DianaProcessor * pCallContext,
                                         DianaUnifiedRegister recognizedRegister,
                                         floatx80_t * value);

void DianaProcessor_FPU_SetSTRegister_80(DianaProcessor * pCallContext,
                                         DianaUnifiedRegister recognizedRegister,
                                         const floatx80_t * value);

// 64-bit API
void DianaProcessor_FPU_SetSTRegister(DianaProcessor * pCallContext,
                                              DianaUnifiedRegister recognizedRegister,
                                              const OPERAND_SIZE * pValue);
OPERAND_SIZE DianaProcessor_FPU_GetSTRegister(DianaProcessor * pCallContext,
                                               DianaUnifiedRegister recognizedRegister);

//--------------------------------------
DI_UINT32 DI_FPU_ProcessException(DianaProcessor * pCallContext, DI_UINT32 in_exception);

void DI_FPU_Overflow(DianaProcessor * pCallContext);
int Diana_GlobalInitFPU();

int Diana_FPU_WriteFloatArgument(struct _dianaContext * pDianaContext,
                                 DianaProcessor * pCallContext,
                                 int number,
                                 floatx80_t * result);

int Diana_FPU_ReadFloatArgument_80(struct _dianaContext * pDianaContext,
                                   DianaProcessor * pCallContext,
                                   int number,
                                   floatx80_t * result);

int Diana_FPU_ReadFloatArgument(struct _dianaContext * pDianaContext,
                                   DianaProcessor * pCallContext,
                                   int number,
                                   float64 * result);

#endif