#ifndef DIANA_PROCESSOR_CMD_R_H
#define DIANA_PROCESSOR_CMD_R_H

#include "diana_processor_core_impl.h"

int Diana_Call_rcl(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_rcr(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_rol(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_ror(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_rdrand(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);
int Diana_Call_rdseed(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);
#endif