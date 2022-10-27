#ifndef DIANA_PROCESSOR_CMD_A_H
#define DIANA_PROCESSOR_CMD_A_H

#include "diana_processor_core_impl.h"

int Diana_Call_aaa(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_aad(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_aam(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_aas(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
    
int Diana_Call_adc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_add(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_and(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

#endif