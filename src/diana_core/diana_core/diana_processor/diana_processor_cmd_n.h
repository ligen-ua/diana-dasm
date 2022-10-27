#ifndef DIANA_PROCESSOR_CMD_N_H
#define DIANA_PROCESSOR_CMD_N_H

#include "diana_processor_core_impl.h"

int Diana_Call_neg(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_not(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_nop(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_hint_nop(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext);
#endif