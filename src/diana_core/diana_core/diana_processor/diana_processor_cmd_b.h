#ifndef DIANA_PROCESSOR_CMD_B_H
#define DIANA_PROCESSOR_CMD_B_H

#include "diana_processor_core_impl.h"


int Diana_Call_bsf(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_bsr(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_bt(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int Diana_Call_btc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_btr(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_bts(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_bswap(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

#endif