#ifndef DIANA_PROCESSOR_CMD_AVX2_H
#define DIANA_PROCESSOR_CMD_AVX2_H

#include "diana_processor_core_impl.h"
#include "diana_processor_core_impl_xmm.h"

// the integer element cores of AVX/AVX2 (diana_processor_cmd_avx2.c): bytes the vector length, size the element
typedef enum
{
    DI_INT_ADD, DI_INT_SUB, DI_INT_ADDS, DI_INT_SUBS, DI_INT_ADDUS, DI_INT_SUBUS, DI_INT_AVG, DI_INT_CMPEQ,
    DI_INT_CMPGT, DI_INT_MIN, DI_INT_MAX, DI_INT_MINU, DI_INT_MAXU, DI_INT_MULLO, DI_INT_MULHI, DI_INT_MULHU,
    DI_INT_MULHRS, DI_INT_SIGN, DI_INT_AND, DI_INT_ANDN, DI_INT_OR, DI_INT_XOR
} DiIntOperation;
typedef enum { DI_SHIFT_LEFT, DI_SHIFT_RIGHT, DI_SHIFT_ARITHMETIC } DiIntShift;
typedef enum { DI_SHUFFLE_D, DI_SHUFFLE_HW, DI_SHUFFLE_LW } DiIntShuffle;

void Diana_Int_Elementwise(void * r, const void * a, const void * b, int bytes, int size, DiIntOperation operation);
void Diana_Int_Abs(void * r, const void * a, int bytes, int size);
void Diana_Int_Shift(void * r, const void * a, int bytes, int size, DiIntShift shift, OPERAND_SIZE count);
void Diana_Int_ShiftVariable(void * r, const void * a, const void * b, int bytes, int size, DiIntShift shift);
void Diana_Int_Pack(void * r, const void * a, const void * b, int bytes, int fromSize, int toUnsigned);
void Diana_Int_Unpack(void * r, const void * a, const void * b, int bytes, int size, int high);
void Diana_Int_Horizontal(void * r, const void * a, const void * b, int bytes, int size, int subtract, int saturate);
void Diana_Int_ShuffleBytes(void * r, const void * a, const void * b, int bytes);
void Diana_Int_ShuffleImm(void * r, const void * a, int bytes, DiIntShuffle shuffle, int imm);
void Diana_Int_Align(void * r, const void * a, const void * b, int bytes, int imm);
void Diana_Int_ShiftBytes(void * r, const void * a, int bytes, int left, int imm);
void Diana_Int_MultiplyAdd(void * r, const void * a, const void * b, int bytes, int bytesAsUnsigned);
void Diana_Int_MultiplyEven(void * r, const void * a, const void * b, int bytes, int isSigned);
void Diana_Int_SumAbsoluteDifferences(void * r, const void * a, const void * b, int bytes);
void Diana_Int_MultipleSad(void * r, const void * a, const void * b, int bytes, int imm);
void Diana_Int_Blend(void * r, const void * a, const void * b, const void * mask, int bytes, int size, int imm);
void Diana_Int_Extend(void * r, const void * a, int bytes, int fromSize, int toSize, int isSigned);

// the handlers (VEX.128 and VEX.256)
int Diana_Call_vpslldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsrldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpabsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpabsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpabsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpshufd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpshufhw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpshuflw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpalignr(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpblendw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpblendd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vmpsadbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovmskb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpbroadcastb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpbroadcastw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpbroadcastd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpbroadcastq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpermd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpermps(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpermq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpermpd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpblendvb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpaddsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpaddsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpaddusb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpaddusw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsubsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsubsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsubusb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsubusw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpavgb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpavgw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpcmpeqb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpcmpeqw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpcmpeqd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpcmpeqq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpcmpgtb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpcmpgtw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpcmpgtd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpcmpgtq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmaxsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmaxsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmaxsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmaxub(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmaxuw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmaxud(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpminsb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpminsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpminsd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpminub(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpminuw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpminud(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmullw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmulld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmulhw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmulhuw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmulhrsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsignb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsignw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsignd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpacksswb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpackssdw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpackuswb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpackusdw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpunpcklbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpunpcklwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpunpckldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpunpcklqdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpunpckhbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpunpckhwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpunpckhdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpunpckhqdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vphaddw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vphaddd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vphaddsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vphsubw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vphsubd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vphsubsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpshufb(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmaddwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmaddubsw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmuludq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmuldq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsadbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsllvd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsllvq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsrlvd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsrlvq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsravd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsllw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpslld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsllq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsrlw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsrld(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsrlq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsraw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpsrad(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovsxbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovsxbd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovsxbq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovsxwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovsxwq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovsxdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovzxbw(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovzxbd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovzxbq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovzxwd(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovzxwq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);
int Diana_Call_vpmovzxdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext);

#endif
