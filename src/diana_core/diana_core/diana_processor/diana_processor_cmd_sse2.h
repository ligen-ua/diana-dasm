#ifndef DIANA_PROCESSOR_CMD_SSE2_H
#define DIANA_PROCESSOR_CMD_SSE2_H

#include "diana_processor_core_impl.h"

/* Packed integer add */
int Diana_Call_paddb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_paddw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_paddd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_paddq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_paddsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_paddsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_paddusb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_paddusw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Packed integer subtract */
int Diana_Call_psubb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psubw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psubd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psubq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psubsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psubsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psubusb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psubusw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Packed compare */
int Diana_Call_pcmpeqb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpeqd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpeqw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpgtb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpgtd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpgtw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Packed multiply */
int Diana_Call_pmullw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmulhw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmulhuw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmuludq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmaddwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Packed average / min / max */
int Diana_Call_pavgb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pavgw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmaxsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmaxub(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pminsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pminub(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psadbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Mask move */
int Diana_Call_pmovmskb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Insert / extract word */
int Diana_Call_pinsrw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pextrw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Pack */
int Diana_Call_packssdw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_packsswb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_packuswb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Unpack high */
int Diana_Call_punpckhbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_punpckhdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_punpckhqdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_punpckhwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Shift */
int Diana_Call_psllw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pslld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psllq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pslldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psrlw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psrld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psrlq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psrldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psraw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psrad(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Shuffle */
int Diana_Call_pshufhw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pshuflw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Move quadword */
int Diana_Call_movq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_movq2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_movdq2q(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* MMX state management */
int Diana_Call_emms(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_femms(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Scalar float32 */
int Diana_Call_addss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_subss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_mulss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_divss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sqrtss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_comiss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_ucomiss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtsi2ss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtss2sd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtss2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvttss2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_maxss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_minss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Packed float32 */
int Diana_Call_addps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_subps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_mulps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_divps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sqrtps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_maxps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_minps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Packed float64 */
int Diana_Call_addpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_subpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_mulpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_divpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sqrtpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_maxpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_minpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Scalar float64 extra */
int Diana_Call_sqrtsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_ucomisd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_maxsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_minsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtsi2sd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtsd2ss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtsd2si(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Move with duplication */
int Diana_Call_movshdup(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_movsldup(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Unpack */
int Diana_Call_unpcklps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_unpckhps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_unpcklpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_unpckhpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* Shuffle */
int Diana_Call_shufps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_shufpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* MXCSR load */
int Diana_Call_ldmxcsr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

/* System no-ops */
int Diana_Call_rdtsc(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_rdtscp(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_mfence(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_lfence(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sfence(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_clflush(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_ud2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

#endif
