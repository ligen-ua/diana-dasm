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
// SSE: the softfloat status of MXCSR (rounding, DAZ, FTZ, the masks), the x87 state is not used
float_status_t Diana_SSE_Status(DianaProcessor * pCallContext);
// SSE: the exception flags of the status into MXCSR; DI_INTERRUPT (#XM) when one is unmasked, the result is not
// written then
int Diana_SSE_Exceptions(DianaProcessor * pCallContext, const float_status_t * pStatus);

// the SSE cores of the legacy and the VEX forms (diana_processor_cmd_sse2.c, _simd.c, _avx.c): values in, values out,
// the operands are the callers'
typedef enum
{
    DI_SSE_ADD, DI_SSE_SUB, DI_SSE_MUL, DI_SSE_DIV, DI_SSE_SQRT, DI_SSE_MIN, DI_SSE_MAX
} DiSseOperation;
float32 Diana_SSE_Float32(DiSseOperation operation, float32 a, float32 b, float_status_t * pStatus);
float64 Diana_SSE_Float64(DiSseOperation operation, float64 a, float64 b, float_status_t * pStatus);
// comis*, ucomis*: the relation of element 0; ZF PF CF from it, OF SF AF cleared
int Diana_SSE_CompareScalar(const DianaRegisterXMM_type * pA, const DianaRegisterXMM_type * pB, int elementSize,
                            int quiet, float_status_t * pStatus);
void Diana_SSE_SetCompareFlags(DianaProcessor * pCallContext, int relation);
// cvt*2si: element 0 to an integer of size bytes (4 or 8); cvtsi2*: an integer of size bytes to element 0
OPERAND_SIZE Diana_SSE_ToInteger(const DianaRegisterXMM_type * pSrc, int elementSize, int size, int truncate,
                                 float_status_t * pStatus);
void Diana_SSE_FromInteger(DianaRegisterXMM_type * pDest, int elementSize, OPERAND_SIZE value, int size,
                           float_status_t * pStatus);
// cmp*: the predicate imm8[4:0] (the legacy forms: 0..7) of count elements of pA and pB, all ones or zeros to pA
void Diana_SSE_Compare(void * pA, const void * pB, int elementSize, int count, int predicate,
                       float_status_t * pStatus);
// addsub*, hadd*, hsub*: one 128-bit lane
typedef enum { DI_ADDSUB, DI_HADD, DI_HSUB } DiHorizontal;
void Diana_SSE_Horizontal(DianaRegisterXMM_type * pResult, const DianaRegisterXMM_type * pA,
                          const DianaRegisterXMM_type * pB, int elementSize, DiHorizontal operation,
                          float_status_t * pStatus);
// the packed conversions: count elements of pSrc to pResult
typedef enum { DI_I32, DI_F32, DI_F64 } DiElement;
void Diana_SSE_Convert(void * pResult, const void * pSrc, DiElement from, DiElement to, int count, int truncate,
                       float_status_t * pStatus);
// round*: count elements of pSrc to pResult, imm8 the rounding control
void Diana_SSE_Round(void * pResult, const void * pSrc, int elementSize, int count, int imm,
                     float_status_t * pStatus);
// dpps, dppd: one 128-bit lane, the result to pA
void Diana_SSE_DotProduct(DianaRegisterXMM_type * pA, const DianaRegisterXMM_type * pB, int elementSize, int imm,
                          float_status_t * pStatus);
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
// isStore: a memory destination (an unmasked overflow or underflow writes nothing)
DI_UINT32 DI_FPU_ProcessExceptionEx(DianaProcessor * pCallContext, DI_UINT32 in_exception, int isStore);
// a stack underflow of an instruction writing st(stnr): IE and SF; masked, the default NaN in st(stnr) and the pop
void DI_FPU_StackUnderflow(DianaProcessor * pCallContext, int stnr, int pop);
// C3 C2 C0 of a compare result (float_relation_*)
void DI_FPU_SetCompareFlags(DianaProcessor * pCallContext, int relation);

// after an instruction with an MMX register operand: TOP 0, every x87 register valid, 0xFFFF in bits 79:64 of the
// MMX register written
void Diana_FPU_MmxTransition(DianaProcessor * pCallContext);
void DI_FPU_Overflow(DianaProcessor * pCallContext);
int Diana_GlobalInitFPU();

#endif