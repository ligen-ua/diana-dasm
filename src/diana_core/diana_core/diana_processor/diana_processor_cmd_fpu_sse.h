#ifndef DIANA_PROCESSOR_CMD_FPU_SSE_H
#define DIANA_PROCESSOR_CMD_FPU_SSE_H

#include "diana_processor_core_impl.h"

int Diana_Call_addsd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_subsd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_divsd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_mulsd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_cvttsd2si(struct _dianaContext * pDianaContext,
                         DianaProcessor * pCallContext);
int Diana_Call_comisd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_xgetbv(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);
int Diana_Call_pshufd(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

int Diana_Call_punpcklbw(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);
int Diana_Call_punpcklwd(struct _dianaContext * pDianaContext,
                         DianaProcessor * pCallContext);
int Diana_Call_punpckldq(struct _dianaContext * pDianaContext,
                         DianaProcessor * pCallContext);
int Diana_Call_punpcklqdq(struct _dianaContext * pDianaContext,
                         DianaProcessor * pCallContext);
int Diana_Call_stmxcsr(struct _dianaContext * pDianaContext,
                       DianaProcessor * pCallContext);
#endif