#ifndef DIANA_PROCESSOR_CMD_P_H
#define DIANA_PROCESSOR_CMD_P_H

#include "diana_processor_core_impl.h"

int Diana_Call_push(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_pushf(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_pop(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_popa(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_pusha(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_popf(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_pusha32(struct _dianaContext * pDianaContext,
                       DianaProcessor * pCallContext);

int Diana_Call_pause(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

#endif