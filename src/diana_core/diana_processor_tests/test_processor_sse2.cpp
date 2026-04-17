#include "test_processor_sse2.h"
#include "test_common.h"
#include "test_processor_impl.h"
#include <string.h>

/* paddb xmm0, xmm1: 66 0F FC C1 */
static void test_processor_sse2_paddb()
{
    unsigned char code[] = { 0x66, 0x0F, 0xFC, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    int i;
    for (i = 0; i < 16; ++i) xmm0.u8[i] = (DI_CHAR)(i + 1);  /* 1..16 */
    for (i = 0; i < 16; ++i) xmm1.u8[i] = 1;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* each byte incremented by 1: result bytes 2..17 */
    DIANA_TEST_ASSERT(xmm0.u64[0] == 0x0908070605040302ULL);
    DIANA_TEST_ASSERT(xmm0.u64[1] == 0x11100F0E0D0C0B0AULL);
}

/* paddw xmm0, xmm1: 66 0F FD C1 */
static void test_processor_sse2_paddw()
{
    unsigned char code[] = { 0x66, 0x0F, 0xFD, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u16[0] = 0x0100; xmm0.u16[1] = 0x0200;
    xmm0.u16[2] = 0x0300; xmm0.u16[3] = 0x0400;
    xmm0.u16[4] = 0x0500; xmm0.u16[5] = 0x0600;
    xmm0.u16[6] = 0x0700; xmm0.u16[7] = 0x0800;
    int i;
    for (i = 0; i < 8; ++i) xmm1.u16[i] = 1;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.u64[0] == 0x0401030102010101ULL);
    DIANA_TEST_ASSERT(xmm0.u64[1] == 0x0801070106010501ULL);
}

/* paddd xmm0, xmm1: 66 0F FE C1 */
static void test_processor_sse2_paddd()
{
    unsigned char code[] = { 0x66, 0x0F, 0xFE, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u32[0] = 1; xmm0.u32[1] = 2; xmm0.u32[2] = 3; xmm0.u32[3] = 4;
    xmm1.u32[0] = 10; xmm1.u32[1] = 20; xmm1.u32[2] = 30; xmm1.u32[3] = 40;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* {11, 22, 33, 44} */
    DIANA_TEST_ASSERT(xmm0.u64[0] == 0x000000160000000BULL);  /* 22<<32 | 11 */
    DIANA_TEST_ASSERT(xmm0.u64[1] == 0x0000002C00000021ULL);  /* 44<<32 | 33 */
}

/* psubd xmm0, xmm1: 66 0F FA C1 */
static void test_processor_sse2_psubd()
{
    unsigned char code[] = { 0x66, 0x0F, 0xFA, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u32[0] = 10; xmm0.u32[1] = 20; xmm0.u32[2] = 30; xmm0.u32[3] = 40;
    xmm1.u32[0] = 1;  xmm1.u32[1] = 2;  xmm1.u32[2] = 3;  xmm1.u32[3] = 4;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* {9, 18, 27, 36} */
    DIANA_TEST_ASSERT(xmm0.u64[0] == 0x0000001200000009ULL);  /* 18<<32 | 9 */
    DIANA_TEST_ASSERT(xmm0.u64[1] == 0x000000240000001BULL);  /* 36<<32 | 27 */
}

/* pcmpeqd xmm0, xmm1: 66 0F 76 C1 */
static void test_processor_sse2_pcmpeqd()
{
    unsigned char code[] = { 0x66, 0x0F, 0x76, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u32[0] = 1; xmm0.u32[1] = 2; xmm0.u32[2] = 3; xmm0.u32[3] = 4;
    xmm1.u32[0] = 1; xmm1.u32[1] = 5; xmm1.u32[2] = 3; xmm1.u32[3] = 7;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* dwords 0,2 equal → 0xFFFFFFFF; dwords 1,3 not equal → 0 */
    DIANA_TEST_ASSERT(xmm0.u32[0] == 0xFFFFFFFFU);
    DIANA_TEST_ASSERT(xmm0.u32[1] == 0x00000000U);
    DIANA_TEST_ASSERT(xmm0.u32[2] == 0xFFFFFFFFU);
    DIANA_TEST_ASSERT(xmm0.u32[3] == 0x00000000U);
}

/* pcmpgtb xmm0, xmm1: 66 0F 64 C1 */
static void test_processor_sse2_pcmpgtb()
{
    unsigned char code[] = { 0x66, 0x0F, 0x64, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    int i;
    for (i = 0; i < 16; ++i) xmm0.u8[i] = 5;
    for (i = 0; i < 16; i += 2) xmm1.u8[i] = 3;     /* even: 3 (5 > 3) */
    for (i = 1; i < 16; i += 2) xmm1.u8[i] = 6;     /* odd: 6 (5 < 6) */
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* even bytes → 0xFF (5>3), odd bytes → 0x00 (5!>6) */
    DIANA_TEST_ASSERT(xmm0.u64[0] == 0x00FF00FF00FF00FFULL);
    DIANA_TEST_ASSERT(xmm0.u64[1] == 0x00FF00FF00FF00FFULL);
}

/* pmullw xmm0, xmm1: 66 0F D5 C1 */
static void test_processor_sse2_pmullw()
{
    unsigned char code[] = { 0x66, 0x0F, 0xD5, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    int i;
    for (i = 0; i < 8; ++i) xmm0.u16[i] = (DI_UINT16)(i + 2);  /* 2..9 */
    for (i = 0; i < 8; ++i) xmm1.u16[i] = 3;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* {6, 9, 12, 15, 18, 21, 24, 27} */
    DIANA_TEST_ASSERT(xmm0.u16[0] == 6);
    DIANA_TEST_ASSERT(xmm0.u16[1] == 9);
    DIANA_TEST_ASSERT(xmm0.u16[2] == 12);
    DIANA_TEST_ASSERT(xmm0.u16[3] == 15);
    DIANA_TEST_ASSERT(xmm0.u16[4] == 18);
    DIANA_TEST_ASSERT(xmm0.u16[5] == 21);
    DIANA_TEST_ASSERT(xmm0.u16[6] == 24);
    DIANA_TEST_ASSERT(xmm0.u16[7] == 27);
}

/* pavgb xmm0, xmm1: 66 0F E0 C1 */
static void test_processor_sse2_pavgb()
{
    unsigned char code[] = { 0x66, 0x0F, 0xE0, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    int i;
    for (i = 0; i < 16; ++i) xmm0.u8[i] = (DI_CHAR)((i + 1) * 2);  /* 2,4,6,...,32 */
    /* xmm1 = 0 */
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* pavgb = (a + b + 1) >> 1, with b=0: (2+1)/2=1, (4+1)/2=2, ... */
    int i2;
    for (i2 = 0; i2 < 16; ++i2)
    {
        DI_UINT8 expected = (DI_UINT8)(((i2 + 1) * 2 + 1) >> 1);
        DIANA_TEST_ASSERT(xmm0.u8[i2] == expected);
    }
}

/* psllw xmm0, imm8=2: 66 0F 71 F0 02 */
static void test_processor_sse2_psllw()
{
    unsigned char code[] = { 0x66, 0x0F, 0x71, 0xF0, 0x02 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    int i;
    for (i = 0; i < 8; ++i) xmm0.u16[i] = (DI_UINT16)(i + 1);  /* 1..8 */
    SET_REG_XMM0(xmm0);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* {4, 8, 12, 16, 20, 24, 28, 32} */
    DIANA_TEST_ASSERT(xmm0.u16[0] == 4);
    DIANA_TEST_ASSERT(xmm0.u16[1] == 8);
    DIANA_TEST_ASSERT(xmm0.u16[2] == 12);
    DIANA_TEST_ASSERT(xmm0.u16[3] == 16);
    DIANA_TEST_ASSERT(xmm0.u16[4] == 20);
    DIANA_TEST_ASSERT(xmm0.u16[5] == 24);
    DIANA_TEST_ASSERT(xmm0.u16[6] == 28);
    DIANA_TEST_ASSERT(xmm0.u16[7] == 32);
}

/* psraw xmm0, imm8=2: 66 0F 71 E0 02 */
static void test_processor_sse2_psraw()
{
    unsigned char code[] = { 0x66, 0x0F, 0x71, 0xE0, 0x02 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    xmm0.s16[0] = 8;   xmm0.s16[1] = 16;
    xmm0.s16[2] = -8;  xmm0.s16[3] = -16;
    xmm0.s16[4] = 24;  xmm0.s16[5] = 32;
    xmm0.s16[6] = -24; xmm0.s16[7] = -32;
    SET_REG_XMM0(xmm0);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.s16[0] == 2);
    DIANA_TEST_ASSERT(xmm0.s16[1] == 4);
    DIANA_TEST_ASSERT(xmm0.s16[2] == -2);
    DIANA_TEST_ASSERT(xmm0.s16[3] == -4);
    DIANA_TEST_ASSERT(xmm0.s16[4] == 6);
    DIANA_TEST_ASSERT(xmm0.s16[5] == 8);
    DIANA_TEST_ASSERT(xmm0.s16[6] == -6);
    DIANA_TEST_ASSERT(xmm0.s16[7] == -8);
}

/* psrlw xmm0, imm8=2: 66 0F 71 D0 02 */
static void test_processor_sse2_psrlw()
{
    unsigned char code[] = { 0x66, 0x0F, 0x71, 0xD0, 0x02 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    int i;
    for (i = 0; i < 8; ++i) xmm0.u16[i] = (DI_UINT16)((i + 1) * 4);  /* 4,8,...,32 */
    SET_REG_XMM0(xmm0);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* {1, 2, 3, 4, 5, 6, 7, 8} */
    int i2;
    for (i2 = 0; i2 < 8; ++i2)
        DIANA_TEST_ASSERT(xmm0.u16[i2] == (DI_UINT16)(i2 + 1));
}

/* movq xmm0, xmm1 (F3 0F 7E C1): moves lo 64-bits, zeros hi */
static void test_processor_sse2_movq()
{
    unsigned char code[] = { 0xF3, 0x0F, 0x7E, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u64[0] = 0xAAAAAAAAAAAAAAAAULL;
    xmm0.u64[1] = 0xBBBBBBBBBBBBBBBBULL;
    xmm1.u64[0] = 0x1234567890ABCDEFULL;
    xmm1.u64[1] = 0xDEADBEEFCAFEBABEULL;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.u64[0] == 0x1234567890ABCDEFULL);
    DIANA_TEST_ASSERT(xmm0.u64[1] == 0x0000000000000000ULL);
}

/* addss xmm0, xmm1 (F3 0F 58 C1): add scalar single */
static void test_processor_sse2_addss()
{
    unsigned char code[] = { 0xF3, 0x0F, 0x58, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u32[0] = 0x3F800000U;  /* 1.0f */
    xmm0.u32[1] = 0xDEADBEEFU;  /* preserved */
    xmm0.u32[2] = 0xDEADBEEFU;
    xmm0.u32[3] = 0xDEADBEEFU;
    xmm1.u32[0] = 0x40000000U;  /* 2.0f */
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.u32[0] == 0x40400000U);  /* 3.0f */
    DIANA_TEST_ASSERT(xmm0.u32[1] == 0xDEADBEEFU);
    DIANA_TEST_ASSERT(xmm0.u32[2] == 0xDEADBEEFU);
    DIANA_TEST_ASSERT(xmm0.u32[3] == 0xDEADBEEFU);
}

/* addpd xmm0, xmm1 (66 0F 58 C1): add packed double */
static void test_processor_sse2_addpd()
{
    unsigned char code[] = { 0x66, 0x0F, 0x58, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u64[0] = 0x3FF0000000000000ULL;  /* 1.0 */
    xmm0.u64[1] = 0x4000000000000000ULL;  /* 2.0 */
    xmm1.u64[0] = 0x3FF0000000000000ULL;  /* 1.0 */
    xmm1.u64[1] = 0x3FF0000000000000ULL;  /* 1.0 */
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    DIANA_TEST_ASSERT(xmm0.u64[0] == 0x4000000000000000ULL);  /* 2.0 */
    DIANA_TEST_ASSERT(xmm0.u64[1] == 0x4008000000000000ULL);  /* 3.0 */
}

/* ucomisd xmm0, xmm1 (66 0F 2E C1) */
static void test_processor_sse2_ucomisd()
{
    /* xmm0 < xmm1: CF=1, ZF=0, PF=0 → flags low bits: 0x203 */
    {
        unsigned char code[] = { 0x66, 0x0F, 0x2E, 0xC1 };

        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
        DianaProcessor * pCallContext = proc.GetSelf();

        DianaRegisterXMM_type xmm0 = { { 0 } };
        DianaRegisterXMM_type xmm1 = { { 0 } };
        xmm0.u64[0] = 0x3FF8000000000000ULL;  /* 1.5 */
        xmm1.u64[0] = 0x4004000000000000ULL;  /* 2.5 */
        SET_REG_XMM0(xmm0);
        SET_REG_XMM1(xmm1);

        int res = proc.ExecOnce();
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.impl.l.impl.l.value == 0x203);
    }
    /* xmm0 > xmm1: CF=0, ZF=0, PF=0 → flags = 0x202 */
    {
        unsigned char code[] = { 0x66, 0x0F, 0x2E, 0xC1 };

        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
        DianaProcessor * pCallContext = proc.GetSelf();

        DianaRegisterXMM_type xmm0 = { { 0 } };
        DianaRegisterXMM_type xmm1 = { { 0 } };
        xmm0.u64[0] = 0x4004000000000000ULL;  /* 2.5 */
        xmm1.u64[0] = 0x3FF8000000000000ULL;  /* 1.5 */
        SET_REG_XMM0(xmm0);
        SET_REG_XMM1(xmm1);

        int res = proc.ExecOnce();
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.impl.l.impl.l.value == 0x202);
    }
    /* xmm0 == xmm1: ZF=1, CF=0, PF=0 → flags = 0x242 */
    {
        unsigned char code[] = { 0x66, 0x0F, 0x2E, 0xC1 };

        CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
        DianaProcessor * pCallContext = proc.GetSelf();

        DianaRegisterXMM_type xmm0 = { { 0 } };
        DianaRegisterXMM_type xmm1 = { { 0 } };
        xmm0.u64[0] = 0x3FF8000000000000ULL;  /* 1.5 */
        xmm1.u64[0] = 0x3FF8000000000000ULL;  /* 1.5 */
        SET_REG_XMM0(xmm0);
        SET_REG_XMM1(xmm1);

        int res = proc.ExecOnce();
        DIANA_TEST_ASSERT(res == DI_SUCCESS);
        DIANA_TEST_ASSERT(proc.GetSelf()->m_flags.impl.l.impl.l.value == 0x242);
    }
}

/* movshdup xmm0, xmm1 (F3 0F 16 C1): duplicate odd dwords */
static void test_processor_sse2_movshdup()
{
    unsigned char code[] = { 0xF3, 0x0F, 0x16, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm1.u32[0] = 0x3F800000U;  /* 1.0f */
    xmm1.u32[1] = 0x40000000U;  /* 2.0f */
    xmm1.u32[2] = 0x40400000U;  /* 3.0f */
    xmm1.u32[3] = 0x40800000U;  /* 4.0f */
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* result[0]=src[1], result[1]=src[1], result[2]=src[3], result[3]=src[3] */
    DIANA_TEST_ASSERT(xmm0.u32[0] == 0x40000000U);
    DIANA_TEST_ASSERT(xmm0.u32[1] == 0x40000000U);
    DIANA_TEST_ASSERT(xmm0.u32[2] == 0x40800000U);
    DIANA_TEST_ASSERT(xmm0.u32[3] == 0x40800000U);
}

/* unpcklps xmm0, xmm1 (0F 14 C1): interleave low dwords */
static void test_processor_sse2_unpcklps()
{
    unsigned char code[] = { 0x0F, 0x14, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u32[0] = 1; xmm0.u32[1] = 2; xmm0.u32[2] = 3; xmm0.u32[3] = 4;
    xmm1.u32[0] = 5; xmm1.u32[1] = 6; xmm1.u32[2] = 7; xmm1.u32[3] = 8;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* {xmm0[0], xmm1[0], xmm0[1], xmm1[1]} = {1, 5, 2, 6} */
    DIANA_TEST_ASSERT(xmm0.u32[0] == 1);
    DIANA_TEST_ASSERT(xmm0.u32[1] == 5);
    DIANA_TEST_ASSERT(xmm0.u32[2] == 2);
    DIANA_TEST_ASSERT(xmm0.u32[3] == 6);
}

/* shufps xmm0, xmm1, 0 (0F C6 C1 00) */
static void test_processor_sse2_shufps()
{
    unsigned char code[] = { 0x0F, 0xC6, 0xC1, 0x00 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u32[0] = 0x3F800000U;  /* 1.0f */
    xmm0.u32[1] = 0x40000000U;
    xmm0.u32[2] = 0x40400000U;
    xmm0.u32[3] = 0x40800000U;
    xmm1.u32[0] = 0x40A00000U;  /* 5.0f */
    xmm1.u32[1] = 0x40C00000U;
    xmm1.u32[2] = 0x40E00000U;
    xmm1.u32[3] = 0x41000000U;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* imm8=0: [0]=xmm0[0],[1]=xmm0[0],[2]=xmm1[0],[3]=xmm1[0] */
    DIANA_TEST_ASSERT(xmm0.u32[0] == 0x3F800000U);
    DIANA_TEST_ASSERT(xmm0.u32[1] == 0x3F800000U);
    DIANA_TEST_ASSERT(xmm0.u32[2] == 0x40A00000U);
    DIANA_TEST_ASSERT(xmm0.u32[3] == 0x40A00000U);
}

/* rdtsc (0F 31): sets EDX:EAX */
static void test_processor_sse2_rdtsc()
{
    unsigned char code[] = { 0x0F, 0x31 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* Just verify it executes without error; TSC value is non-deterministic */
}

/* mfence (0F AE F8): memory fence, no effect on registers */
static void test_processor_sse2_mfence()
{
    unsigned char code[] = { 0x0F, 0xAE, 0xF8 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
}

/* ud2 (0F 0B): must return DI_UNSUPPORTED_COMMAND */
static void test_processor_sse2_ud2()
{
    unsigned char code[] = { 0x0F, 0x0B };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_UNSUPPORTED_COMMAND);
}

/* emms (0F 77): no-op in this implementation, must return DI_SUCCESS */
static void test_processor_sse2_emms()
{
    unsigned char code[] = { 0x0F, 0x77 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
}

/* packuswb xmm0, xmm1 (66 0F 67 C1): pack 16→8 with unsigned saturation */
static void test_processor_sse2_packuswb()
{
    unsigned char code[] = { 0x66, 0x0F, 0x67, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    xmm0.u16[0] = 1;    xmm0.u16[1] = 2;
    xmm0.u16[2] = 300;  xmm0.u16[3] = 0;       /* 300 saturates to 255 */
    xmm0.u16[4] = 100;  xmm0.u16[5] = 200;
    xmm0.u16[6] = 0;    xmm0.u16[7] = 255;
    xmm1.u16[0] = 5;    xmm1.u16[1] = 10;
    xmm1.u16[2] = 400;  xmm1.u16[3] = 50;      /* 400 saturates to 255 */
    xmm1.u16[4] = 128;  xmm1.u16[5] = 63;
    xmm1.u16[6] = 7;    xmm1.u16[7] = 8;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* lo 8 bytes from xmm0 words, hi 8 bytes from xmm1 words */
    DIANA_TEST_ASSERT(xmm0.u8[0] == 1);
    DIANA_TEST_ASSERT(xmm0.u8[1] == 2);
    DIANA_TEST_ASSERT(xmm0.u8[2] == 255);  /* saturated */
    DIANA_TEST_ASSERT(xmm0.u8[3] == 0);
    DIANA_TEST_ASSERT(xmm0.u8[4] == 100);
    DIANA_TEST_ASSERT(xmm0.u8[5] == 200);
    DIANA_TEST_ASSERT(xmm0.u8[6] == 0);
    DIANA_TEST_ASSERT(xmm0.u8[7] == 255);
    DIANA_TEST_ASSERT(xmm0.u8[8] == 5);
    DIANA_TEST_ASSERT(xmm0.u8[9] == 10);
    DIANA_TEST_ASSERT(xmm0.u8[10] == 255); /* saturated */
    DIANA_TEST_ASSERT(xmm0.u8[11] == 50);
    DIANA_TEST_ASSERT(xmm0.u8[12] == 128);
    DIANA_TEST_ASSERT(xmm0.u8[13] == 63);
    DIANA_TEST_ASSERT(xmm0.u8[14] == 7);
    DIANA_TEST_ASSERT(xmm0.u8[15] == 8);
}

/* psadbw xmm0, xmm1 (66 0F F6 C1): sum of absolute differences */
static void test_processor_sse2_psadbw()
{
    unsigned char code[] = { 0x66, 0x0F, 0xF6, 0xC1 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    DianaRegisterXMM_type xmm1 = { { 0 } };
    /* lo 8 bytes: all 10 vs all 0 → sum = 80 */
    int i;
    for (i = 0; i < 8; ++i) xmm0.u8[i] = 10;
    /* hi 8 bytes: all 20 vs all 5 → sum = 8*15 = 120 */
    for (i = 8; i < 16; ++i) xmm0.u8[i] = 20;
    for (i = 8; i < 16; ++i) xmm1.u8[i] = 5;
    SET_REG_XMM0(xmm0);
    SET_REG_XMM1(xmm1);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    xmm0 = GET_REG_XMM0;
    /* result: u64[0] = 80, u64[1] = 120 */
    DIANA_TEST_ASSERT(xmm0.u64[0] == 80);
    DIANA_TEST_ASSERT(xmm0.u64[1] == 120);
}

/* pmovmskb eax, xmm0 (66 0F D7 C0): extract sign bits of 16 bytes */
static void test_processor_sse2_pmovmskb()
{
    /* 66 0F D7 C0: pmovmskb eax, xmm0 (ModRM C0 = mod=11, reg=0 [eax], rm=0 [xmm0]) */
    unsigned char code[] = { 0x66, 0x0F, 0xD7, 0xC0 };

    CTestProcessor proc(code, sizeof(code), 0, DIANA_MODE32);
    DianaProcessor * pCallContext = proc.GetSelf();

    DianaRegisterXMM_type xmm0 = { { 0 } };
    /* set high bits of bytes 0, 4, 8, 12 → mask = 0x1111 */
    xmm0.u8[0]  = 0x80;
    xmm0.u8[4]  = 0x80;
    xmm0.u8[8]  = 0x80;
    xmm0.u8[12] = 0x80;
    SET_REG_XMM0(xmm0);
    SET_REG_EAX(0);

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_EAX == 0x1111);
}

void test_processor_sse2()
{
    DIANA_TEST(test_processor_sse2_paddb());
    DIANA_TEST(test_processor_sse2_paddw());
    DIANA_TEST(test_processor_sse2_paddd());
    DIANA_TEST(test_processor_sse2_psubd());
    DIANA_TEST(test_processor_sse2_pcmpeqd());
    DIANA_TEST(test_processor_sse2_pcmpgtb());
    DIANA_TEST(test_processor_sse2_pmullw());
    DIANA_TEST(test_processor_sse2_pavgb());
    DIANA_TEST(test_processor_sse2_psllw());
    DIANA_TEST(test_processor_sse2_psraw());
    DIANA_TEST(test_processor_sse2_psrlw());
    DIANA_TEST(test_processor_sse2_movq());
    DIANA_TEST(test_processor_sse2_addss());
    DIANA_TEST(test_processor_sse2_addpd());
    DIANA_TEST(test_processor_sse2_ucomisd());
    DIANA_TEST(test_processor_sse2_movshdup());
    DIANA_TEST(test_processor_sse2_unpcklps());
    DIANA_TEST(test_processor_sse2_shufps());
    DIANA_TEST(test_processor_sse2_rdtsc());
    DIANA_TEST(test_processor_sse2_mfence());
    DIANA_TEST(test_processor_sse2_ud2());
    DIANA_TEST(test_processor_sse2_emms());
    DIANA_TEST(test_processor_sse2_packuswb());
    DIANA_TEST(test_processor_sse2_psadbw());
    DIANA_TEST(test_processor_sse2_pmovmskb());
}
