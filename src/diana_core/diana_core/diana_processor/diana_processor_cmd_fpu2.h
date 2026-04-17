#ifndef DIANA_PROCESSOR_CMD_FPU2_H
#define DIANA_PROCESSOR_CMD_FPU2_H

#include "diana_processor_core_impl.h"

// Exchange
int Diana_Call_fxch(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Sign/Abs
int Diana_Call_fabs(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fchs(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Test/Examine
int Diana_Call_ftst(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fxam(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Unordered compare (quiet)
int Diana_Call_fucom(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fucomp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fucompp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Compare and set EFLAGS
int Diana_Call_fcomi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fcomip(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fucomi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fucomip(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Integer compare
int Diana_Call_ficom(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_ficomp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Load constants
int Diana_Call_fld1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fldz(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fldpi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fldl2e(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fldl2t(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fldlg2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fldln2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Stack management
int Diana_Call_fnop(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fdecstp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fincstp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_ffree(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_ffreep(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fninit(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Arithmetic
int Diana_Call_fimul(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_frndint(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fscale(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fxtract(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_f2xm1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fyl2x(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fyl2xp1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fpatan(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fptan(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fprem(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fprem1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fsin(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fcos(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fsincos(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Store integer truncated
int Diana_Call_fisttp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Conditional float move
int Diana_Call_fcmovb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fcmove(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fcmovbe(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fcmovu(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fcmovnb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fcmovne(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fcmovnbe(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fcmovnu(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// Complex state (stubs)
int Diana_Call_fbld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fbstp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fldenv(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fnstenv(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_frstor(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fnsave(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fxsave(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_fxrstor(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

#endif
