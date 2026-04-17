#include "test_processor_fpu2.h"
#include "test_common.h"
#include "test_processor_impl.h"

/* Data helpers: 64-bit IEEE 754 doubles as little-endian byte sequences */
/* 1.1  = 0x3FF199999999999A */
#define DATA_1_1   0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0x3f
/* 1.2  = 0x3FF3333333333333 */
#define DATA_1_2   0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f
/* -1.1 = 0xBFF199999999999A */
#define DATA_NEG1_1  0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xf1, 0xbf
/* 1.7  = 0x3FFB333333333333 */
#define DATA_1_7   0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xfb, 0x3f
/* 0.5  = 0x3FE0000000000000 */
#define DATA_0_5   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x3f
/* zero-filled result buffer (8 bytes) */
#define DATA_ZERO  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

/* 80-bit mantissa values for known doubles */
#define MANT_1_1   0x8CCCCCCCCCCCD000ULL
#define MANT_1_2   0x9999999999999800ULL

/* fld qword ptr [disp32] = DD 05 <disp32-LE> (6 bytes) */
#define FLD_QWORD(offset) \
    0xDD, 0x05, (unsigned char)((offset) & 0xFF), \
    (unsigned char)(((offset)>>8) & 0xFF), 0x00, 0x00

/* -----------------------------------------------------------------------*/
/* fld1 : push 1.0                                                        */
/* -----------------------------------------------------------------------*/
static void test_processor_fld1()
{
    /* fld1  D9 E8 */
    unsigned char code[] = { 0xD9, 0xE8 };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT((pFpu->statusWord & 0x3800) == 0x3800); /* TOP=7 */
    DIANA_TEST_ASSERT(GET_REG_MM7 == 0x8000000000000000ULL);  /* 1.0 mantissa */
    DIANA_TEST_ASSERT(GET_REG_FPU_ST0 == 0x3FF0000000000000ULL); /* 1.0 as float64 */
}

/* -----------------------------------------------------------------------*/
/* fldz : push 0.0                                                        */
/* -----------------------------------------------------------------------*/
static void test_processor_fldz()
{
    /* fldz  D9 EE */
    unsigned char code[] = { 0xD9, 0xEE };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT((pFpu->statusWord & 0x3800) == 0x3800); /* TOP=7 */
    DIANA_TEST_ASSERT(GET_REG_FPU_ST0 == 0x0000000000000000ULL); /* 0.0 */
}

/* -----------------------------------------------------------------------*/
/* fldpi : push pi                                                        */
/* -----------------------------------------------------------------------*/
static void test_processor_fldpi()
{
    /* fldpi  D9 EB */
    unsigned char code[] = { 0xD9, 0xEB };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT((pFpu->statusWord & 0x3800) == 0x3800);
    /* pi mantissa = 0xC90FDAA22168C235 (from constant definition) */
    DIANA_TEST_ASSERT(GET_REG_MM7 == 0xC90FDAA22168C235ULL);
}

/* -----------------------------------------------------------------------*/
/* fabs : ST(0) = |ST(0)|                                                */
/* -----------------------------------------------------------------------*/
static void test_processor_fabs()
{
    /* fld qword [-1.1] from offset 8, then fabs */
    unsigned char code[] = {
        FLD_QWORD(8),   /* 6 bytes: load -1.1 */
        0xD9, 0xE1,     /* fabs (2 bytes) */
        DATA_NEG1_1     /* 8 bytes at offset 8 */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* After fabs(-1.1) = +1.1 */
    DIANA_TEST_ASSERT(GET_REG_FPU_ST0 == 0x3FF199999999999AULL); /* +1.1 as float64 */
    DIANA_TEST_ASSERT(GET_REG_MM7 == MANT_1_1);
}

/* -----------------------------------------------------------------------*/
/* fchs : ST(0) = -ST(0)                                                 */
/* -----------------------------------------------------------------------*/
static void test_processor_fchs()
{
    /* fld qword [+1.1] from offset 10, then fchs, then ftst */
    unsigned char code[] = {
        FLD_QWORD(10),  /* 6 bytes [0..5] */
        0xD9, 0xE0,     /* fchs (2 bytes) [6..7] */
        0xD9, 0xE4,     /* ftst (2 bytes) [8..9] */
        DATA_1_1        /* 8 bytes at offset 10 */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    /* Load +1.1 */
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_FPU_ST0 == 0x3FF199999999999AULL); /* +1.1 */

    /* fchs -> -1.1 */
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_FPU_ST0 == 0xBFF199999999999AULL); /* -1.1 */

    /* ftst: -1.1 < 0.0 => C0=1, C2=0, C3=0 */
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C0 == 1);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C2 == 0);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C3 == 0);
}

/* -----------------------------------------------------------------------*/
/* fxch : exchange ST(0) and ST(1)                                       */
/* -----------------------------------------------------------------------*/
static void test_processor_fxch()
{
    /* fld qword [1.1] at offset 14
       fld qword [1.2] at offset 22
       fxch st(1) */
    unsigned char code[] = {
        FLD_QWORD(14),  /* 6 bytes: load 1.1 */
        FLD_QWORD(22),  /* 6 bytes: load 1.2 */
        0xD9, 0xC9,     /* fxch st(1) */
        DATA_1_1,       /* 8 bytes at offset 14 */
        DATA_1_2        /* 8 bytes at offset 22 */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    /* fld 1.1 -> phys[7]=1.1, TOP=7 */
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_MM7 == MANT_1_1);

    /* fld 1.2 -> phys[6]=1.2, phys[7]=1.1, TOP=6 */
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_MM6 == MANT_1_2); /* 1.2 at phys[6]=ST(0) */
    DIANA_TEST_ASSERT(GET_REG_MM7 == MANT_1_1); /* 1.1 at phys[7]=ST(1) */

    /* fxch st(1) -> swap phys[6] and phys[7] */
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_MM6 == MANT_1_1); /* 1.1 now at phys[6]=ST(0) */
    DIANA_TEST_ASSERT(GET_REG_MM7 == MANT_1_2); /* 1.2 now at phys[7]=ST(1) */
}

/* -----------------------------------------------------------------------*/
/* frndint: round ST(0) to integer                                       */
/* -----------------------------------------------------------------------*/
static void test_processor_frndint()
{
    /* fld qword [1.7] at offset 8, frndint */
    unsigned char code[] = {
        FLD_QWORD(8),   /* 6 bytes */
        0xD9, 0xFC,     /* frndint */
        DATA_1_7        /* 8 bytes at offset 8 */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F; /* round-to-nearest mode */

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* round-to-nearest: 1.7 -> 2.0; float64(2.0) = 0x4000000000000000 */
    DIANA_TEST_ASSERT(GET_REG_FPU_ST0 == 0x4000000000000000ULL);
}

/* -----------------------------------------------------------------------*/
/* fsin : sin(ST(0))                                                      */
/* -----------------------------------------------------------------------*/
static void test_processor_fsin()
{
    /* fld qword [0.5] at offset 8, fsin */
    unsigned char code[] = {
        FLD_QWORD(8),   /* 6 bytes */
        0xD9, 0xFE,     /* fsin */
        DATA_0_5        /* 8 bytes at offset 8 */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* sin(0.5) ≈ 0.4794; verify it is positive and less than 1.0 */
    OPERAND_SIZE val = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT((val & 0x8000000000000000ULL) == 0);    /* positive */
    DIANA_TEST_ASSERT(val < 0x3FF0000000000000ULL);             /* < 1.0 */
}

/* -----------------------------------------------------------------------*/
/* fcos : cos(ST(0))                                                      */
/* -----------------------------------------------------------------------*/
static void test_processor_fcos()
{
    /* fld qword [0.5] at offset 8, fcos */
    unsigned char code[] = {
        FLD_QWORD(8),   /* 6 bytes */
        0xD9, 0xFF,     /* fcos */
        DATA_0_5        /* 8 bytes at offset 8 */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* cos(0.5) ≈ 0.8776; verify it is positive and less than 1.0 */
    OPERAND_SIZE val = GET_REG_FPU_ST0;
    DIANA_TEST_ASSERT((val & 0x8000000000000000ULL) == 0);    /* positive */
    DIANA_TEST_ASSERT(val < 0x3FF0000000000000ULL);             /* < 1.0 */
}

/* -----------------------------------------------------------------------*/
/* ftst : compare ST(0) with 0.0                                         */
/* -----------------------------------------------------------------------*/
static void test_processor_ftst()
{
    /* fld qword [1.1] at offset 8, ftst => ST(0) > 0.0 => C0=0,C2=0,C3=0 */
    unsigned char code[] = {
        FLD_QWORD(8),   /* 6 bytes */
        0xD9, 0xE4,     /* ftst */
        DATA_1_1        /* 8 bytes at offset 8 */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* 1.1 > 0: C0=0, C2=0, C3=0 */
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C0 == 0);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C2 == 0);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C3 == 0);
}

/* -----------------------------------------------------------------------*/
/* fucom / fucomp : unordered compare (without EFLAGS modification)      */
/* -----------------------------------------------------------------------*/
static void test_processor_fucom()
{
    /* fld [1.1] then fld [1.2]: ST(0)=1.2, ST(1)=1.1
       fucom st(1): compare ST(0)=1.2 with ST(1)=1.1 => 1.2 > 1.1
       => C0=0, C2=0, C3=0 */
    unsigned char code[] = {
        FLD_QWORD(14),  /* 6 bytes */
        FLD_QWORD(22),  /* 6 bytes */
        0xDD, 0xE1,     /* fucom st(1) */
        DATA_1_1,       /* offset 14 */
        DATA_1_2        /* offset 22 */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(3);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C0 == 0);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C2 == 0);
    DIANA_TEST_ASSERT(DI_FPU_GET_SW_C3 == 0);
}

/* -----------------------------------------------------------------------*/
/* fucompp : unordered compare and pop twice                             */
/* -----------------------------------------------------------------------*/
static void test_processor_fucompp()
{
    /* fld [1.1] then fld [1.2]: ST(0)=1.2, ST(1)=1.1, TOP=6
       fucompp: compare then pop twice => TOP = 0 */
    unsigned char code[] = {
        FLD_QWORD(14),
        FLD_QWORD(22),
        0xDA, 0xE9,     /* fucompp */
        DATA_1_1,
        DATA_1_2
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(3);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* Two pops: TOP was 6 -> 7 -> 0 */
    DIANA_TEST_ASSERT((pFpu->statusWord & 0x3800) == 0x0000);
}

/* -----------------------------------------------------------------------*/
/* fcomi : compare and set EFLAGS (1.2 > 1.1 => CF=0, ZF=0)             */
/* -----------------------------------------------------------------------*/
static void test_processor_fcomi()
{
    /* fld [1.1] then fld [1.2]: ST(0)=1.2, ST(1)=1.1
       fcomi st(0),st(1): 1.2 > 1.1 => CF=0, ZF=0 */
    unsigned char code[] = {
        FLD_QWORD(14),
        FLD_QWORD(22),
        0xDB, 0xF1,     /* fcomi st(0),st(1) */
        DATA_1_1,
        DATA_1_2
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(3);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_FLAG_CF == 0);
    DIANA_TEST_ASSERT(GET_FLAG_ZF == 0);
}

/* -----------------------------------------------------------------------*/
/* fcomi with equal values (ZF=1)                                        */
/* -----------------------------------------------------------------------*/
static void test_processor_fcomi_equal()
{
    /* fld [1.1] twice: ST(0)=ST(1)=1.1
       fcomi st(0),st(1): equal => CF=0, ZF=1 */
    unsigned char code[] = {
        FLD_QWORD(14),
        FLD_QWORD(14),  /* same offset = same value */
        0xDB, 0xF1,
        DATA_1_1,
        DATA_1_2        /* not used */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(3);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_FLAG_CF == 0);
    DIANA_TEST_ASSERT(GET_FLAG_ZF == 1);
}

/* -----------------------------------------------------------------------*/
/* fisttp dword [eax] : truncate ST(0) to integer and store              */
/* -----------------------------------------------------------------------*/
static void test_processor_fisttp()
{
    /* fld qword [1.7] from offset 8
       fisttp dword ptr [eax] : DB 08
       result buffer at offset 16 (EAX=16) */
    unsigned char code[] = {
        FLD_QWORD(8),   /* [0..5]: load 1.7 */
        0xDB, 0x08,     /* [6..7]: fisttp dword [eax] */
        DATA_1_7,       /* [8..15]: 1.7 double data */
        DATA_ZERO       /* [16..23]: 4-byte result buffer */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    SET_REG_EAX(16); /* point EAX to result buffer */

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* fisttp truncates toward zero: trunc(1.7) = 1 */
    int result_val = *(int*)(code + 16);
    DIANA_TEST_ASSERT(result_val == 1);
    /* Stack should be popped: TOP was 7, now back to 0 */
    DIANA_TEST_ASSERT((pFpu->statusWord & 0x3800) == 0x0000);
}

/* -----------------------------------------------------------------------*/
/* fnop : no operation                                                   */
/* -----------------------------------------------------------------------*/
static void test_processor_fnop()
{
    unsigned char code[] = { 0xD9, 0xD0 }; /* fnop */
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
}

/* -----------------------------------------------------------------------*/
/* fninit : initialize FPU state                                         */
/* -----------------------------------------------------------------------*/
static void test_processor_fninit()
{
    unsigned char code[] = { 0xDB, 0xE3 }; /* fninit */
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x1234;
    pFpu->statusWord  = 0x5678;

    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* After fninit: CW = 0x037F, SW = 0 */
    DIANA_TEST_ASSERT(pFpu->controlWord == 0x037F);
    DIANA_TEST_ASSERT(pFpu->statusWord  == 0x0000);
}

/* -----------------------------------------------------------------------*/
/* fdecstp / fincstp : decrement / increment TOP                         */
/* -----------------------------------------------------------------------*/
static void test_processor_fdecstp_fincstp()
{
    unsigned char code[] = { 0xD9, 0xF6, 0xD9, 0xF7 }; /* fdecstp, fincstp */
    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    /* fdecstp: TOP: 0 -> 7 */
    int res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT((pFpu->statusWord & 0x3800) == 0x3800); /* TOP=7 */

    /* fincstp: TOP: 7 -> 0 */
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT((pFpu->statusWord & 0x3800) == 0x0000); /* TOP=0 */
}

/* -----------------------------------------------------------------------*/
/* fld_log_consts : fldl2e / fldl2t / fldlg2 / fldln2                   */
/* -----------------------------------------------------------------------*/
static void test_processor_fld_log_consts()
{
    unsigned char code[] = {
        0xD9, 0xEA,  /* fldl2e */
        0xD9, 0xE9,  /* fldl2t */
        0xD9, 0xEC,  /* fldlg2 */
        0xD9, 0xED   /* fldln2 */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(4);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* All four constants were pushed; TOP = 4 (0b100) */
    DIANA_TEST_ASSERT((pFpu->statusWord & 0x3800) == (4 << 11));
}

/* -----------------------------------------------------------------------*/
/* ffree st(0) : mark register as free                                   */
/* -----------------------------------------------------------------------*/
static void test_processor_ffree()
{
    unsigned char code[] = {
        0xD9, 0xE8,  /* fld1 */
        0xDD, 0xC0   /* ffree st(0) */
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
}

/* -----------------------------------------------------------------------*/
/* fcmovb : conditional float move if CF=1                               */
/* -----------------------------------------------------------------------*/
static void test_processor_fcmovb()
{
    /* fld [1.1] then fld [1.2]: ST(0)=1.2, ST(1)=1.1, TOP=6
       fcmovb st(0),st(1): if CF=1, ST(0) <- ST(1)=1.1 */
    unsigned char code[] = {
        FLD_QWORD(14),
        FLD_QWORD(22),
        0xDA, 0xC1,     /* fcmovb st(0),st(1) */
        DATA_1_1,
        DATA_1_2
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    SET_FLAG_CF;
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* ST(0) = phys[6] should now be 1.1 */
    DIANA_TEST_ASSERT(GET_REG_MM6 == MANT_1_1);
}

/* -----------------------------------------------------------------------*/
/* fcmovnb : conditional float move if CF=0                              */
/* -----------------------------------------------------------------------*/
static void test_processor_fcmovnb()
{
    unsigned char code[] = {
        FLD_QWORD(14),
        FLD_QWORD(22),
        0xDB, 0xC1,     /* fcmovnb st(0),st(1) */
        DATA_1_1,
        DATA_1_2
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    CLEAR_FLAG_CF;
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    /* CF=0 => ST(0) <- ST(1)=1.1 */
    DIANA_TEST_ASSERT(GET_REG_MM6 == MANT_1_1);
}

/* -----------------------------------------------------------------------*/
/* fcmove : conditional float move if ZF=1                               */
/* -----------------------------------------------------------------------*/
static void test_processor_fcmove()
{
    unsigned char code[] = {
        FLD_QWORD(14),
        FLD_QWORD(22),
        0xDA, 0xC9,     /* fcmove st(0),st(1) */
        DATA_1_1,
        DATA_1_2
    };

    CTestProcessor proc(code, sizeof(code));
    DianaProcessor * pCallContext = proc.GetSelf();
    DianaFPU * pFpu = &pCallContext->m_fpu;
    pFpu->controlWord = 0x027F;

    int res = proc.Exec(2);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);

    SET_FLAG_ZF;
    res = proc.ExecOnce();
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DIANA_TEST_ASSERT(GET_REG_MM6 == MANT_1_1);
}

void test_processor_fpu2()
{
    DIANA_TEST(test_processor_fld1());
    DIANA_TEST(test_processor_fldz());
    DIANA_TEST(test_processor_fldpi());
    DIANA_TEST(test_processor_fabs());
    DIANA_TEST(test_processor_fchs());
    DIANA_TEST(test_processor_fxch());
    DIANA_TEST(test_processor_frndint());
    DIANA_TEST(test_processor_fsin());
    DIANA_TEST(test_processor_fcos());
    DIANA_TEST(test_processor_ftst());
    DIANA_TEST(test_processor_fucom());
    DIANA_TEST(test_processor_fucompp());
    DIANA_TEST(test_processor_fcomi());
    DIANA_TEST(test_processor_fcomi_equal());
    DIANA_TEST(test_processor_fisttp());
    DIANA_TEST(test_processor_fnop());
    DIANA_TEST(test_processor_fninit());
    DIANA_TEST(test_processor_fdecstp_fincstp());
    DIANA_TEST(test_processor_fld_log_consts());
    DIANA_TEST(test_processor_ffree());
    DIANA_TEST(test_processor_fcmovb());
    DIANA_TEST(test_processor_fcmovnb());
    DIANA_TEST(test_processor_fcmove());
}
