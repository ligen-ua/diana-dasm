#ifndef DIANA_PROCESSOR_CMD_SIMD_H
#define DIANA_PROCESSOR_CMD_SIMD_H

#include "diana_processor_core_impl.h"
#include "diana_processor_core_impl_xmm.h"

// SSE, SSE2, SSE3, SSE4.1, SSE4.2 instructions (diana_processor_cmd_simd.c)
int Diana_Call_cmpps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cmpss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cmppd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cmpsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_addsubps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_addsubpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_haddps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_haddpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_hsubps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_hsubpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtdq2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtdq2pd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtps2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvttps2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtpd2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvttpd2dq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtps2pd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtpd2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtps2pi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvttps2pi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtpd2pi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvttpd2pi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtpi2ps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_cvtpi2pd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_roundps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_roundpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_roundss(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_roundsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_dpps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_dppd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_movmskps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_movmskpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_movhpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_movntq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_movntdqa(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_maskmovdqu(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_maskmovq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_blendps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_blendpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pblendw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_extractps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pextrb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pextrd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pinsrb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pinsrd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_insertps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovsxbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovsxbd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovsxwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovsxwq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovsxdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovzxbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovzxbd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovzxwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovzxwq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovzxdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pminsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pminsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pminuw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pminud(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmaxsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmaxsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmaxuw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmaxud(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpeqq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpgtq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmuldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmulld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_packusdw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_phminposuw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_mpsadbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_ptest(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpestri(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpestrm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpistri(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pcmpistrm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

// SSSE3, the SSE4.1 rest
int Diana_Call_palignr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pblendvb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_blendvps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_blendvpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pshufb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_phaddw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_phaddd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_phaddsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmaddubsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_phsubw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_phsubd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_phsubsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psignb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psignw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_psignd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmulhrsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pabsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pabsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pabsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovsxbq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pmovzxbq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

#endif
