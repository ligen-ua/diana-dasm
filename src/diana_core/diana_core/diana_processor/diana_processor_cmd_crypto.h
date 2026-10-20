#ifndef DIANA_PROCESSOR_CMD_CRYPTO_H
#define DIANA_PROCESSOR_CMD_CRYPTO_H

#include "diana_processor_core_impl.h"
#include "diana_processor_core_impl_xmm.h"

// AES-NI, PCLMULQDQ, SHA (diana_processor_cmd_crypto.c)

// the operations on values (the VEX forms use them too)
typedef enum { DI_AESENC, DI_AESENCLAST, DI_AESDEC, DI_AESDECLAST } DiAesRound;
void Diana_AesRoundValue(DianaRegisterXMM_type * pState, const DianaRegisterXMM_type * pKey, DiAesRound round);
void Diana_AesImcValue(DianaRegisterXMM_type * pValue);
void Diana_AesKeyGenAssistValue(DianaRegisterXMM_type * pDest, const DianaRegisterXMM_type * pSrc, int rcon);
// the carry-less product of the qword imm[0] of a and the qword imm[4] of b
void Diana_PclmulValue(DianaRegisterXMM_type * pDest, const DianaRegisterXMM_type * pA,
                       const DianaRegisterXMM_type * pB, int imm);

int Diana_Call_aesenc(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_aesenclast(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_aesdec(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_aesdeclast(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_aesimc(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_aeskeygenassist(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pclmulqdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sha1rnds4(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sha1nexte(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sha1msg1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sha1msg2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sha256msg1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sha256msg2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sha256rnds2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

#endif
