#ifndef DIANA_PROCESSOR_CMD_AVX512_H
#define DIANA_PROCESSOR_CMD_AVX512_H

#include "diana_processor_core_impl.h"

// every EVEX instruction (DianaProcessor_ExecOnce calls it when DianaParserResult::evex.isEvex): AVX-512F, BW, DQ,
// VL, CD; DI_UNSUPPORTED_COMMAND for the others (FP16, the Xeon Phi ones, the extensions)
int Diana_Call_evex(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// the opmask instructions (VEX)
void Diana_Avx512_RegisterCommands(DianaGroupInfo * p);

#endif
