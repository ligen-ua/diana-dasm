#ifndef DIANA_PROCESSOR_CMD_S2_H
#define DIANA_PROCESSOR_CMD_S2_H

#include "diana_processor_core_impl.h"

int Diana_Call_shl(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_sar(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_sub(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_sbb(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_stc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_std(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_sti(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_shld(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_shr(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_shrd(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_scas(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_stos(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

#endif