#ifndef DIANA_PROCESSOR_CMD_L_H
#define DIANA_PROCESSOR_CMD_L_H

#include "diana_processor_core_impl.h"

int Diana_Call_lahf(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_lea(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_leave(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_lods(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_loop(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_loope(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_loopne(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

#endif