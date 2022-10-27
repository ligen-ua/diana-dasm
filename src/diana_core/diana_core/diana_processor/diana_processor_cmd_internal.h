#ifndef DIANA_PROCESSOR_CMD_INTERNAL_H
#define DIANA_PROCESSOR_CMD_INTERNAL_H

#include "diana_core.h"
#include "diana_processor_core_impl.h"

extern char g_diana_parityBuffer[256];

int IsParity(unsigned char value);

void mul64(DI_UINT64 *plow, DI_UINT64 *phigh, DI_UINT64 a, DI_UINT64 b);

void imul64(DI_UINT64 *plow, DI_UINT64 *phigh, DI_INT64 a, DI_INT64 b);

int div64(DI_UINT64 *plow, DI_UINT64 *phigh, DI_UINT64 b);

int idiv64(DI_UINT64 *plow, DI_UINT64 *phigh, DI_INT64 b);

int Di_CheckZeroExtends(DianaProcessor * pCallContext,
                        OPERAND_SIZE * pDest,
                        int src_size,
                        int * dest_size);

int Di_CheckZeroExtends2(DianaProcessor * pCallContext,
                         OPERAND_SIZE * pDest,
                         int src_size,
                         int * dest_size);

int diana_internal_pop(DianaProcessor * pCallContext,
                       OPERAND_SIZE * pValue);
int diana_internal_push(DianaProcessor * pCallContext,
                        OPERAND_SIZE * pValue);

int Diana_Call_jcc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext,
                   int opCount,
                   int relOp);

int Di_PrepareSIDI_regs(DianaProcessor * pCallContext,
                        DianaRmIndex * pSrcIndex, 
                        DianaRmIndex * pDestIndex);

void Di_UpdateSIDI(DianaProcessor * pCallContext,
                   OPERAND_SIZE * pSrcRegAddress, 
                   OPERAND_SIZE * pDestRegAddress);

int Diana_Call_internal_ret(struct _dianaContext * pDianaContext,
                            DianaProcessor * pCallContext,
                            int bPopCS);

#endif