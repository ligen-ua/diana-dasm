#ifndef DIANA_PROCESSOR_CMD_FPU_H
#define DIANA_PROCESSOR_CMD_FPU_H

#include "diana_processor_core_impl.h"

int Diana_GlobalInitFPU();

int Diana_Call_fistp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
int Diana_Call_fist(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
int Diana_Call_wait(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);
int Diana_Call_fnstcw(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fnclex(struct _dianaContext * pDianaContext,
                      DianaProcessor * pCallContext);
int Diana_Call_fldcw(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fild(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
int Diana_Call_fmul(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
int Diana_Call_fmulp(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext);
int Diana_Call_fstp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
int Diana_Call_fst(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
int Diana_Call_fld(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
int Diana_Call_fnstsw(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

// fcomp
int Diana_Call_fcom(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
int Diana_Call_fcomp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);
int Diana_Call_fcompp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);

// sub
int Diana_Call_fsub(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fsubp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fsubr(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fsubrp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fisub(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fisubr(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

// fadd
int Diana_Call_fadd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_faddp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fiadd(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);

// other
int Diana_Call_fsqrt(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext);


// fdiv
int Diana_Call_fdiv(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fdivp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fidiv(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fdivr(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fdivrp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);
int Diana_Call_fidivr(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext);


#endif