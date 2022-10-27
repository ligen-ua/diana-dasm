#ifndef DIANA_PROCESSOR_CMD_I_H
#define DIANA_PROCESSOR_CMD_I_H

#include "diana_processor_core_impl.h"

int Diana_Call_idiv(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_imul(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_int(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

#endif