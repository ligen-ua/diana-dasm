// AVX-512 state and the rules a state compare can't show: zmm0..31 and k0..7, the bits a legacy SSE (kept), a VEX
// (cleared above the register) and an EVEX write (cleared above the vector length) leave; vzeroupper and vzeroall
// leave zmm16..31; the masked-off elements of a memory operand are not accessed (no fault); a gather that faults
// keeps the elements loaded and the opmask bits of the rest (restartable); compress and expand touch only the
// selected count of elements in memory; the context API with and without
// DIANA_PROCESSOR_CONTEXT_ZMM_STATE_VALID. The instructions are checked against the CPU (tests/x86/cpu_state.cpp).
#include "test_common.h"
#include "test_processor_impl.h"
#include <string.h>

extern "C"
{
#include "diana_processor/diana_proc_gen.h"
#include "diana_processor/diana_processor_core_impl_xmm.h"
#include "diana_processor/diana_processor_context.h"
}

namespace
{

DianaRegisterZMM_type Bytes(unsigned char value)
{
    DianaRegisterZMM_type zmm;
    memset(&zmm, value, sizeof(zmm));
    return zmm;
}

void SetK(DianaProcessor * pCallContext, int k, OPERAND_SIZE value)
{
    DianaUnifiedRegister reg = (DianaUnifiedRegister)(reg_K0 + k);
    DianaProcessor_SetValue(pCallContext, reg, DianaProcessor_QueryReg(pCallContext, reg), value);
}

OPERAND_SIZE GetK(DianaProcessor * pCallContext, int k)
{
    return DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, (DianaUnifiedRegister)(reg_K0 + k)));
}

bool AllBytes(const unsigned char * p, int size, unsigned char value)
{
    for (int i = 0; i < size; ++i)
        if (p[i] != value)
            return false;
    return true;
}

void test_zmm_upper_bits()
{
    // addps xmm1, xmm2; vaddps xmm1, xmm2, xmm3; {evex} vaddps ymm1, ymm2, ymm3; vzeroupper; vzeroall
    unsigned char code[] = {0x0f, 0x58, 0xca,
                            0xc5, 0xe8, 0x58, 0xcb,
                            0x62, 0xf1, 0x6c, 0x28, 0x58, 0xcb,
                            0xc5, 0xf8, 0x77,
                            0xc5, 0xfc, 0x77,
                            0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * p = proc.GetSelf();
    DianaRegisterZMM_type pattern = Bytes(0x55), zero = Bytes(0);
    DianaProcessor_ZMM_SetValue(p, reg_ZMM1, &pattern);
    DianaProcessor_ZMM_SetValue(p, reg_ZMM2, &zero);
    DianaProcessor_ZMM_SetValue(p, reg_ZMM3, &zero);

    // legacy SSE: bits 511:128 stay
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    DianaRegisterZMM_type zmm1 = DianaProcessor_ZMM_GetValue(p, reg_ZMM1);
    DIANA_TEST_ASSERT(AllBytes(zmm1.u8 + 16, 48, 0x55));

    // VEX.128: 511:128 zero
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    zmm1 = DianaProcessor_ZMM_GetValue(p, reg_ZMM1);
    DIANA_TEST_ASSERT(AllBytes(zmm1.u8 + 16, 48, 0));

    // EVEX.256: 511:256 zero
    DianaProcessor_ZMM_SetValue(p, reg_ZMM1, &pattern);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    zmm1 = DianaProcessor_ZMM_GetValue(p, reg_ZMM1);
    DIANA_TEST_ASSERT(AllBytes(zmm1.u8, 32, 0) && AllBytes(zmm1.u8 + 32, 32, 0));

    // vzeroupper: zmm1 511:128 zero, zmm17 stays
    DianaProcessor_ZMM_SetValue(p, reg_ZMM1, &pattern);
    DianaProcessor_ZMM_SetValue(p, reg_ZMM17, &pattern);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    zmm1 = DianaProcessor_ZMM_GetValue(p, reg_ZMM1);
    DIANA_TEST_ASSERT(AllBytes(zmm1.u8, 16, 0x55) && AllBytes(zmm1.u8 + 16, 48, 0));
    DianaRegisterZMM_type zmm17 = DianaProcessor_ZMM_GetValue(p, reg_ZMM17);
    DIANA_TEST_ASSERT(AllBytes(zmm17.u8, 64, 0x55));

    // vzeroall: zmm0..15 zero, zmm17 stays
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    zmm1 = DianaProcessor_ZMM_GetValue(p, reg_ZMM1);
    DIANA_TEST_ASSERT(AllBytes(zmm1.u8, 64, 0));
    zmm17 = DianaProcessor_ZMM_GetValue(p, reg_ZMM17);
    DIANA_TEST_ASSERT(AllBytes(zmm17.u8, 64, 0x55));

    // the register vector: xmm1 is the low part of ymm1 and zmm1, xmm17 of zmm17
    DianaProcessor_ZMM_SetValue(p, reg_ZMM17, &zero);
    DianaRegisterXMM_type xmm17;
    memset(&xmm17, 0x33, sizeof(xmm17));
    DianaProcessor_XMM_SetValue(p, reg_XMM17, DianaProcessor_QueryReg(p, reg_XMM17), &xmm17);
    zmm17 = DianaProcessor_ZMM_GetValue(p, reg_ZMM17);
    DIANA_TEST_ASSERT(AllBytes(zmm17.u8, 16, 0x33) && AllBytes(zmm17.u8 + 16, 48, 0));
}

void test_evex_masked_memory()
{
    // vmovdqu32 zmm0{k1}, [eax]; vmovdqu32 zmm0{k1}, [eax]; vmovdqu8 [eax]{k1}, zmm0: the memory ends 12 dwords
    // after eax
    unsigned char code[0x70] = {0x62, 0xf1, 0x7e, 0x49, 0x6f, 0x00,
                                0x62, 0xf1, 0x7e, 0x49, 0x6f, 0x00,
                                0x62, 0xf1, 0x7f, 0x49, 0x7f, 0x00};
    for (int i = 0x40; i < 0x70; ++i)
        code[i] = (unsigned char)i;
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaRegisterZMM_type pattern = Bytes(0x77);
    DianaProcessor_ZMM_SetValue(pCallContext, reg_ZMM0, &pattern);
    SET_REG_EAX(0x40);

    // the elements of the opmask are in the memory: the rest isn't read and keeps its value
    SetK(pCallContext, 1, 0x0fff);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    DianaRegisterZMM_type zmm0 = DianaProcessor_ZMM_GetValue(pCallContext, reg_ZMM0);
    DIANA_TEST_ASSERT(!memcmp(zmm0.u8, code + 0x40, 48) && AllBytes(zmm0.u8 + 48, 16, 0x77));

    // element 15 past the end: a fault, zmm0 as it was
    DianaProcessor_ZMM_SetValue(pCallContext, reg_ZMM0, &pattern);
    SetK(pCallContext, 1, 0x8000);
    DIANA_TEST_ASSERT(proc.ExecOnce() != DI_SUCCESS);
    zmm0 = DianaProcessor_ZMM_GetValue(pCallContext, reg_ZMM0);
    DIANA_TEST_ASSERT(AllBytes(zmm0.u8, 64, 0x77));

    // a masked store: only the bytes of the opmask (one past the end masked off: no fault)
    DI_JUMP_TO_RIP(12);
    SET_REG_EAX(0x6f);
    SetK(pCallContext, 1, 0x1);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    DIANA_TEST_ASSERT(code[0x6f] == 0x77 && code[0x6e] == 0x6e);
}

void test_evex_gather_fault()
{
    // vpgatherdd zmm0{k1}, [eax+zmm1*4]: index 2 is past the end of the memory: elements 0, 1 loaded and their
    // opmask bits cleared, the rest of zmm0 and of k1 as they were; with the index in the memory it completes, k1 0
    unsigned char code[0x40] = {0x62, 0xf2, 0x7d, 0x49, 0x90, 0x04, 0x88};
    for (int i = 0x20; i < 0x40; ++i)
        code[i] = (unsigned char)i;
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaRegisterZMM_type index = Bytes(0), dest = Bytes(0x77);
    for (int i = 0; i < 16; ++i)
        index.u32[i] = i < 2 ? i : i == 2 ? 100 : 3;
    DianaProcessor_ZMM_SetValue(pCallContext, reg_ZMM1, &index);
    DianaProcessor_ZMM_SetValue(pCallContext, reg_ZMM0, &dest);
    SetK(pCallContext, 1, 0xffff);
    SET_REG_EAX(0x20);
    DIANA_TEST_ASSERT(proc.ExecOnce() != DI_SUCCESS);
    DianaRegisterZMM_type value = DianaProcessor_ZMM_GetValue(pCallContext, reg_ZMM0);
    DIANA_TEST_ASSERT(value.u32[0] == 0x23222120 && value.u32[1] == 0x27262524 && value.u32[2] == 0x77777777);
    DIANA_TEST_ASSERT(GetK(pCallContext, 1) == 0xfffc);
    index.u32[2] = 4;
    DianaProcessor_ZMM_SetValue(pCallContext, reg_ZMM1, &index);
    DI_JUMP_TO_RIP(0);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    value = DianaProcessor_ZMM_GetValue(pCallContext, reg_ZMM0);
    DIANA_TEST_ASSERT(value.u32[2] == 0x33323130 && value.u32[15] == 0x2f2e2d2c);
    DIANA_TEST_ASSERT(GetK(pCallContext, 1) == 0);
}

void test_evex_compress_expand()
{
    // vpcompressd [eax]{k1}, zmm0: the selected elements to consecutive dwords, nothing after them written (the
    // memory ends 2 dwords after eax); vexpandps zmm0{k1}{z}, [eax]: consecutive dwords to the selected elements
    unsigned char code[0x48] = {0x62, 0xf2, 0x7d, 0x49, 0x8b, 0x00,
                                0x62, 0xf2, 0x7d, 0xc9, 0x88, 0x00};
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaRegisterZMM_type value = Bytes(0);
    for (int i = 0; i < 16; ++i)
        value.u32[i] = 0x1000 + i;
    DianaProcessor_ZMM_SetValue(pCallContext, reg_ZMM0, &value);
    SET_REG_EAX(0x40);
    SetK(pCallContext, 1, 0x8004);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    DI_UINT32 stored[2];
    memcpy(stored, code + 0x40, 8);
    DIANA_TEST_ASSERT(stored[0] == 0x1002 && stored[1] == 0x100f);

    SetK(pCallContext, 1, 0x0110);
    DIANA_TEST_ASSERT(proc.ExecOnce() == DI_SUCCESS);
    value = DianaProcessor_ZMM_GetValue(pCallContext, reg_ZMM0);
    for (int i = 0; i < 16; ++i)
        DIANA_TEST_ASSERT(value.u32[i] == (i == 4 ? 0x1002u : i == 8 ? 0x100fu : 0u));
}

void test_zmm_context()
{
    unsigned char code[] = {0x90, 0x90, 0x90, 0x90};
    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE64);
    DianaProcessor * p = proc.GetSelf();
    Diana_Processor_Registers_Context context;
    DIANA_TEST_ASSERT(DianaProcessor_QueryContext(p, &context) == DI_SUCCESS);
    DIANA_TEST_ASSERT(context.fpuStateFlags & DIANA_PROCESSOR_CONTEXT_ZMM_STATE_VALID);
    context.reg_K[3].value = 0x1234;
    memset(context.reg_ZMM_High[2].value, 0x42, 32);
    memset(context.reg_ZMM16_31[5].value, 0x21, 64);
    DIANA_TEST_ASSERT(DianaProcessor_InitContext(p, &context) == DI_SUCCESS);
    DIANA_TEST_ASSERT(GetK(p, 3) == 0x1234);
    DianaRegisterZMM_type zmm2 = DianaProcessor_ZMM_GetValue(p, reg_ZMM2);
    DIANA_TEST_ASSERT(AllBytes(zmm2.u8 + 32, 32, 0x42));
    DianaRegisterZMM_type zmm21 = DianaProcessor_ZMM_GetValue(p, reg_ZMM21);
    DIANA_TEST_ASSERT(AllBytes(zmm21.u8, 64, 0x21));

    // without the flag (a Win32 CONTEXT): the AVX-512 state stays
    context.fpuStateFlags &= ~DIANA_PROCESSOR_CONTEXT_ZMM_STATE_VALID;
    context.reg_K[3].value = 0;
    memset(context.reg_ZMM16_31[5].value, 0, 64);
    DIANA_TEST_ASSERT(DianaProcessor_InitContext(p, &context) == DI_SUCCESS);
    DIANA_TEST_ASSERT(GetK(p, 3) == 0x1234);
    zmm21 = DianaProcessor_ZMM_GetValue(p, reg_ZMM21);
    DIANA_TEST_ASSERT(AllBytes(zmm21.u8, 64, 0x21));
}

} // namespace

void test_processor_avx512()
{
    DIANA_TEST(test_zmm_upper_bits());
    DIANA_TEST(test_evex_masked_memory());
    DIANA_TEST(test_evex_gather_fault());
    DIANA_TEST(test_evex_compress_expand());
    DIANA_TEST(test_zmm_context());
}
