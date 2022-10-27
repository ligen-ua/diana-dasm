#ifndef DIANA_PROCESSOR_CMD_INTERNAL_XMM_H
#define DIANA_PROCESSOR_CMD_INTERNAL_XMM_H

#include "diana_core.h"
#include "diana_processor_core_impl_xmm.h"

int diana_mov_xmm(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext);

int diana_mov_xmm8(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

#endif
