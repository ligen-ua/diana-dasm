// YMM state: reg_XMMi is the low half of reg_YMMi, a legacy SSE write keeps bits 255:128, a VEX.128 write clears
// them; the context API with and without DIANA_PROCESSOR_CONTEXT_YMM_STATE_VALID; the alignment of the VEX moves.
// The instructions themselves are checked against the CPU (tests/x86/cpu_state.cpp, test_processor_cpu.cpp).
#include "test_common.h"
#include "test_processor_impl.h"
#include <string.h>
#include <memory>

extern "C"
{
#include "diana_processor/diana_proc_gen.h"
#include "diana_processor/diana_processor_core_impl_xmm.h"
#include "diana_processor/diana_processor_context.h"
}

namespace
{

DianaRegisterYMM_type Floats(unsigned low, unsigned high)
{
    DianaRegisterYMM_type value;
    for (int i = 0; i < 8; ++i)
        value.u32[i] = i < 4 ? low : high;
    return value;
}

bool IsFloats(const DianaRegisterYMM_type & value, unsigned low, unsigned high)
{
    DianaRegisterYMM_type expected = Floats(low, high);
    return !memcmp(&value, &expected, sizeof(value));
}

const unsigned kOne = 0x3f800000, kTwo = 0x40000000, kThree = 0x40400000, kFour = 0x40800000,
               kFive = 0x40a00000, kSeven = 0x40e00000;

void test_ymm_halves()
{
    // addps xmm0, xmm1; vaddps xmm0, xmm0, xmm1; vaddps ymm0, ymm0, ymm1
    unsigned char code[] = {0x0f, 0x58, 0xc1,
                            0xc5, 0xf8, 0x58, 0xc1,
                            0xc5, 0xfc, 0x58, 0xc1,
                            0x90, 0x90, 0x90, 0x90, 0x90};
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * p = proc.GetSelf();

    // the xmm register is the low half
    DianaRegisterYMM_type ymm0 = Floats(kOne, 0x55555555), ymm1 = Floats(kTwo, kThree);
    DianaProcessor_YMM_SetValue(p, reg_YMM0, &ymm0);
    DianaProcessor_YMM_SetValue(p, reg_XMM1, &ymm1);
    DianaRegisterXMM_type xmm0 = DianaProcessor_XMM_GetValue(p, DianaProcessor_QueryReg(p, reg_XMM0));
    DIANA_TEST_ASSERT(!memcmp(&xmm0, &ymm0.xmm[0], 16));
    DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p, reg_XMM1), kTwo, kThree));

    // legacy SSE: the upper half stays
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p, reg_YMM0), kThree, 0x55555555));

    // VEX.128: cleared
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p, reg_YMM0), kFive, 0));

    // VEX.256: both halves
    ymm0 = Floats(kFive, kOne);
    DianaProcessor_YMM_SetValue(p, reg_YMM0, &ymm0);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p, reg_YMM0), kSeven, kFour));
    DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p, reg_YMM1), kTwo, kThree));

    DianaProcessor_YMM_ClearUpper(p, reg_XMM1);
    DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p, reg_YMM1), kTwo, 0));
}

void test_ymm_context()
{
    unsigned char code[16] = {0x90};
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    CTestProcessor proc2(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * p = proc.GetSelf();
    DianaProcessor * p2 = proc2.GetSelf();

    for (int i = 0; i < 16; ++i)
    {
        DianaRegisterYMM_type value = Floats(0x01010101 * i, 0x10000000 + i);
        DianaProcessor_YMM_SetValue(p, (DianaUnifiedRegister)(reg_YMM0 + i), &value);
    }
    DIANA_AUTO_PTR<Diana_Processor_Registers_Context> context(new Diana_Processor_Registers_Context());
    DIANA_TEST_ASSERT(DianaProcessor_QueryContext(p, context.get()) == DI_SUCCESS);
    DIANA_TEST_ASSERT(context->fpuStateFlags & DIANA_PROCESSOR_CONTEXT_YMM_STATE_VALID);
    unsigned high = 0;
    memcpy(&high, context->reg_YMM_High[15].value + 12, 4);
    DIANA_TEST_ASSERT(high == 0x1000000f);
    memcpy(&high, context->reg_XMM15.value, 4);
    DIANA_TEST_ASSERT(high == 0x0f0f0f0f);

    // with the flag: the whole registers
    DIANA_TEST_ASSERT(DianaProcessor_InitContext(p2, context.get()) == DI_SUCCESS);
    for (int i = 0; i < 16; ++i)
        DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p2, (DianaUnifiedRegister)(reg_YMM0 + i)),
                                   0x01010101 * i, 0x10000000 + i));

    // without it (a Windows CONTEXT): the low halves, the upper halves stay
    DianaRegisterYMM_type other = Floats(0, 0x77777777);
    for (int i = 0; i < 16; ++i)
        DianaProcessor_YMM_SetValue(p2, (DianaUnifiedRegister)(reg_YMM0 + i), &other);
    context->fpuStateFlags &= ~DIANA_PROCESSOR_CONTEXT_YMM_STATE_VALID;
    DIANA_TEST_ASSERT(DianaProcessor_InitContext(p2, context.get()) == DI_SUCCESS);
    for (int i = 0; i < 16; ++i)
        DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p2, (DianaUnifiedRegister)(reg_YMM0 + i)),
                                   0x01010101 * i, 0x77777777));
}

void test_ymm_alignment()
{
    // vmovaps ymm0, [eax]; vmovups ymm1, [eax]; data at 0x20 .. 0x5f
    unsigned char code[0x60] = {0xc5, 0xfc, 0x28, 0x00,
                                0xc5, 0xfc, 0x10, 0x08};
    for (int i = 0x20; i < 0x60; ++i)
        code[i] = (unsigned char)i;

    // aligned: loaded
    {
        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
        DianaProcessor * pCallContext = proc.GetSelf();
        SET_REG_RAX(0x20);
        DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
        DianaRegisterYMM_type value = DianaProcessor_YMM_GetValue(pCallContext, reg_YMM0);
        DIANA_TEST_ASSERT(value.u8[0] == 0x20 && value.u8[31] == 0x3f);
    }
    // 16 bytes off: #GP with the alignment check (CTestProcessor: the legacy mode), vmovups loads it
    {
        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
        DianaProcessor * pCallContext = proc.GetSelf();
        SET_REG_RAX(0x30);
        DIANA_TEST_ASSERT(proc.ExecOnce() == DI_GP);
        DI_JUMP_TO_RIP(4);
        DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
        DianaRegisterYMM_type value = DianaProcessor_YMM_GetValue(pCallContext, reg_YMM1);
        DIANA_TEST_ASSERT(value.u8[0] == 0x30 && value.u8[31] == 0x4f);
    }
}

void test_ymm_zero_upper()
{
    // vzeroupper; vzeroall: ymm0..7 outside 64-bit mode, ymm8..15 keep their value
    unsigned char code[] = {0xc5, 0xf8, 0x77, 0xc5, 0xfc, 0x77, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    for (int mode = 0; mode < 2; ++mode)
    {
        CTestProcessor proc(code, sizeof(code), 0, mode ? DIANA_MODE64 : DIANA_MODE32);
        DianaProcessor * p = proc.GetSelf();
        const int count = mode ? 16 : 8;
        DianaRegisterYMM_type value = Floats(kOne, kTwo);
        for (int i = 0; i < 16; ++i)
            DianaProcessor_YMM_SetValue(p, (DianaUnifiedRegister)(reg_YMM0 + i), &value);
        DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
        for (int i = 0; i < 16; ++i)
            DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p, (DianaUnifiedRegister)(reg_YMM0 + i)),
                                       kOne, i < count ? 0 : kTwo));
        DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
        for (int i = 0; i < 16; ++i)
            DIANA_TEST_ASSERT(IsFloats(DianaProcessor_YMM_GetValue(p, (DianaUnifiedRegister)(reg_YMM0 + i)),
                                       i < count ? 0 : kOne, i < count ? 0 : kTwo));
    }
}

void test_ymm_mask_move()
{
    // vmaskmovps xmm0, xmm1, [eax]; vmaskmovps [eax], xmm1, xmm2: elements 2, 3 are past the end of the memory and
    // masked off: not read, not written, no fault
    unsigned char code[0x40] = {0xc4, 0xe2, 0x71, 0x2c, 0x00,
                                0xc4, 0xe2, 0x71, 0x2e, 0x10};
    for (int i = 0x30; i < 0x40; ++i)
        code[i] = (unsigned char)i;
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaRegisterYMM_type mask = Floats(0, 0), data = Floats(kSeven, kSeven);
    mask.u32[0] = mask.u32[1] = 0x80000000;
    DianaProcessor_YMM_SetValue(pCallContext, reg_YMM1, &mask);
    DianaProcessor_YMM_SetValue(pCallContext, reg_YMM2, &data);
    SET_REG_RAX(0x38);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    DianaRegisterYMM_type value = DianaProcessor_YMM_GetValue(pCallContext, reg_YMM0);
    DIANA_TEST_ASSERT(value.u32[0] == 0x3b3a3938 && value.u32[1] == 0x3f3e3d3c && !value.u64[1] && !value.u64[2]);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    DIANA_TEST_ASSERT(code[0x38] == 0x00 && code[0x3b] == 0x40 && code[0x3f] == 0x40);
    // an element that is not masked off faults
    mask.u32[2] = 0x80000000;
    DianaProcessor_YMM_SetValue(pCallContext, reg_YMM1, &mask);
    DI_JUMP_TO_RIP(0);
    DIANA_TEST_ASSERT(proc.ExecOnce() != DI_SUCCESS);
}

void test_f16c_rounding()
{
    // vcvtps2ph xmm0, xmm1, 0 (to nearest); vcvtps2ph xmm0, xmm1, 1 (down): a tie of a normal half to even
    // (softfloat rounded ties up), an exact half denormal (the jamming shift saw lost bits in every value)
    unsigned char code[] = {0xc4, 0xe3, 0x79, 0x1d, 0xc8, 0x00,
                            0xc4, 0xe3, 0x79, 0x1d, 0xc8, 0x01,
                            0x90, 0x90, 0x90, 0x90};
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * p = proc.GetSelf();
    for (int run = 0; run < 2; ++run)
    {
        DianaRegisterYMM_type value = Floats(0, 0);
        value.u32[0] = 0x40b65000;  // 5.697..: a tie, to even
        value.u32[1] = 0xb84e0000;  // a negative denormal half, exact
        value.u32[2] = 0x37408000;  // a denormal half tie
        DianaProcessor_YMM_SetValue(p, reg_YMM1, &value);
        DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
        DianaRegisterYMM_type result = DianaProcessor_YMM_GetValue(p, reg_YMM0);
        DIANA_TEST_ASSERT(result.u16[0] == 0x45b2 && result.u16[1] == 0x8338 && result.u16[2] == 0x00c0 &&
                          result.u16[3] == 0 && result.u64[1] == 0);
    }
}

void test_gather_fault()
{
    // vpgatherdd xmm0, [eax+xmm1*4], xmm2: index 2 is past the end of the memory: the fault keeps elements 0, 1
    // loaded with their mask cleared, elements 2, 3 and their mask as they were (restartable); after the memory
    // grows the second run completes and clears the mask
    unsigned char code[0x40] = {0xc4, 0xe2, 0x69, 0x90, 0x04, 0x88};
    for (int i = 0x20; i < 0x40; ++i)
        code[i] = (unsigned char)i;
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaRegisterYMM_type index = Floats(0, 0), mask = Floats(0x80000000, 0x80000000), dest = Floats(7, 7);
    index.u32[0] = 0; index.u32[1] = 1; index.u32[2] = 100; index.u32[3] = 2;
    DianaProcessor_YMM_SetValue(pCallContext, reg_YMM1, &index);
    DianaProcessor_YMM_SetValue(pCallContext, reg_YMM2, &mask);
    DianaProcessor_YMM_SetValue(pCallContext, reg_YMM0, &dest);
    SET_REG_RAX(0x20);
    DIANA_TEST_ASSERT(proc.ExecOnce() != DI_SUCCESS);
    DianaRegisterYMM_type value = DianaProcessor_YMM_GetValue(pCallContext, reg_YMM0);
    DianaRegisterYMM_type maskAfter = DianaProcessor_YMM_GetValue(pCallContext, reg_YMM2);
    DIANA_TEST_ASSERT(value.u32[0] == 0x23222120 && value.u32[1] == 0x27262524 && value.u32[2] == 7 && value.u32[3] == 7);
    DIANA_TEST_ASSERT(maskAfter.u32[0] == 0 && maskAfter.u32[1] == 0 && maskAfter.u32[2] == 0x80000000 &&
                      maskAfter.u32[3] == 0x80000000);
    // index 2 now in the memory: completes, the mask is 0
    index.u32[2] = 3;
    DianaProcessor_YMM_SetValue(pCallContext, reg_YMM1, &index);
    DI_JUMP_TO_RIP(0);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    value = DianaProcessor_YMM_GetValue(pCallContext, reg_YMM0);
    maskAfter = DianaProcessor_YMM_GetValue(pCallContext, reg_YMM2);
    DIANA_TEST_ASSERT(value.u32[2] == 0x2f2e2d2c && value.u32[3] == 0x2b2a2928 && !value.u64[2] && !value.u64[3]);
    DIANA_TEST_ASSERT(!maskAfter.u64[0] && !maskAfter.u64[1] && !maskAfter.u64[2] && !maskAfter.u64[3]);
}

void test_fma_denormal_flag()
{
    // vfmadd213sd xmm0, xmm1, xmm2; vfmadd213ss xmm0, xmm1, xmm2: xmm1 (0) * xmm0 (a denormal) + xmm2 (1): DE
    // (a denormal factor of a zero product: softfloat's muladd didn't raise it), the result 1
    unsigned char code[] = {0xc4, 0xe2, 0xf1, 0xa9, 0xc2,
                            0xc4, 0xe2, 0x71, 0xa9, 0xc2,
                            0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * p = proc.GetSelf();
    for (int single = 0; single < 2; ++single)
    {
        DianaRegisterYMM_type zero = Floats(0, 0), denormal = Floats(0, 0), one = Floats(0, 0);
        if (single)
        {
            denormal.u32[0] = 1;
            one.u32[0] = kOne;
        }
        else
        {
            denormal.u64[0] = 1;
            one.u64[0] = 0x3ff0000000000000ULL;
        }
        DianaProcessor_YMM_SetValue(p, reg_YMM0, &denormal);
        DianaProcessor_YMM_SetValue(p, reg_YMM1, &zero);
        DianaProcessor_YMM_SetValue(p, reg_YMM2, &one);
        p->m_mxcsr = 0x1f80;
        DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
        DianaRegisterYMM_type result = DianaProcessor_YMM_GetValue(p, reg_YMM0);
        DIANA_TEST_ASSERT(p->m_mxcsr == 0x1f82);
        DIANA_TEST_ASSERT(single ? result.u32[0] == kOne : result.u64[0] == 0x3ff0000000000000ULL);
    }
}

}

void test_processor_avx()
{
    DIANA_TEST(test_ymm_halves());
    DIANA_TEST(test_ymm_context());
    DIANA_TEST(test_ymm_alignment());
    DIANA_TEST(test_ymm_zero_upper());
    DIANA_TEST(test_ymm_mask_move());
    DIANA_TEST(test_f16c_rounding());
    DIANA_TEST(test_gather_fault());
    DIANA_TEST(test_fma_denormal_flag());
}
