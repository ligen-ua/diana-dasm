#ifndef DIANA_PROCESSOR_CMD_D_H
#define DIANA_PROCESSOR_CMD_D_H

#include "diana_processor_core_impl.h"

int Diana_Call_daa(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_das(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_dec(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_div(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

#endif