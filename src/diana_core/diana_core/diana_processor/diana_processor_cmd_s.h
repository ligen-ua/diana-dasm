#ifndef DIANA_PROCESSOR_CMD_S_H
#define DIANA_PROCESSOR_CMD_S_H

#include "diana_processor_core_impl.h"


int Diana_Call_sahf(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_seta(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_setae(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_setb(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_setbe(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_sete(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_setg(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_setge(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_setl(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_setle(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_setne(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_setno(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_setnp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_setns(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_seto(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_setp(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_sets(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);



#endif