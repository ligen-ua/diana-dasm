#ifndef DIANA_PROCESSOR_CMD_J_H
#define DIANA_PROCESSOR_CMD_J_H

#include "diana_processor_core_impl.h"


int Diana_Call_ja(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int Diana_Call_jae(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_jb(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int Diana_Call_jbe(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_je(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int Diana_Call_jg(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int Diana_Call_jge(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_jl(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int Diana_Call_jle(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_jne(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_jno(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_jnp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_jns(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

int Diana_Call_jo(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int Diana_Call_jp(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int Diana_Call_js(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int Diana_Call_jecxz(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

int Diana_Call_jmp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

#endif