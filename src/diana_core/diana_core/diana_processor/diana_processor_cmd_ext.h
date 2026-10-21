#ifndef DIANA_PROCESSOR_CMD_EXT_H
#define DIANA_PROCESSOR_CMD_EXT_H

#include "diana_processor_core_impl.h"

// general-register extensions: POPCNT, LZCNT, TZCNT, BMI1, BMI2, CRC32, MOVBE, ADX, endbr32/endbr64
int Diana_Call_endbr32(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_endbr64(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_popcnt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_lzcnt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_tzcnt(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_andn(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_bextr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_blsi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_blsmsk(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_blsr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_bzhi(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_mulx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pdep(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_pext(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_rorx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_sarx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_shlx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_shrx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_crc32(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_movbe(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_adcx(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_adox(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

#endif
