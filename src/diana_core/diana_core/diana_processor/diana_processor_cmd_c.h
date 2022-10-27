#ifndef DIANA_PROCESSOR_CMD_C_H
#define DIANA_PROCESSOR_CMD_C_H

#include "diana_processor_core_impl.h"

int Diana_Call_call(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_cbw(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_clc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_cld(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_cli(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_cmc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_cmova(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_cmovae(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

int Diana_Call_cmovb(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_cmovbe(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

int Diana_Call_cmove(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_cmovg(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_cmovge(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

int Diana_Call_cmovl(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_cmovle(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

int Diana_Call_cmovne(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

int Diana_Call_cmovno(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

int Diana_Call_cmovnp(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

int Diana_Call_cmovns(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);

int Diana_Call_cmovo(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_cmovp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_cmovs(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_cmp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_cmps(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);

int Diana_Call_cmpxchg(struct _dianaContext * pDianaContext,
                       DianaProcessor * pCallContext);

int Diana_Call_cwd(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_cmpxchg16b(struct _dianaContext * pDianaContext,
                          DianaProcessor * pCallContext);

int Diana_Call_cpuid(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
#endif