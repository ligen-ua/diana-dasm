extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"
#include "diana_core_cpp.h"

#define STRTEST(mode, masmString, code, str)  DIANA_TEST(Test(DIANA_MODE##mode, masmString, code, sizeof(code), str));

static void Test(int iMode,
                 diana::CMasmString & masmString,
                 void * pBuffer,
                 size_t size,
                 const char * pLine)
{
    DianaParserResult result;
    size_t sizeRead = 0;
    DIANA_TEST_ASSERT(!Diana_ParseCmdOnBuffer_test(iMode, pBuffer, size, Diana_GetRootLine(), &result, &sizeRead));
    const char * pResultCmd = masmString.Assign(&result, 0);
    DIANA_TEST_ASSERT(strcmp(pResultCmd, pLine) == 0);
}


static unsigned char mov[] = {0x66, 0x8C, 0xCC};          // mov         sp,cs 
static unsigned char mov1[] = {0x66, 0x8C, 0xD3};         // mov         bx,ss 
static unsigned char mov2[] = {0x3E, 0xA2, 0x34, 0x12, 0x00, 0x00}; // mov         byte ptr ds:[00001234h],al 
static unsigned char mov3[] = {0x36, 0xA3, 0x34, 0x12, 0x00, 0x00}; // mov         dword ptr ss:[00001234h],eax 
static unsigned char mov4[] = {0x3E, 0xA0, 0x34, 0x12, 0x00, 0x00}; // mov         al,byte ptr ds:[00001234h] 
static unsigned char mov5[] = {0x36, 0xA1, 0x34, 0x12, 0x00, 0x00}; // mov         eax,dword ptr ss:[00001234h] 
static unsigned char mov6[] = {0x2E, 0xC6, 0x02, 0x34};         // mov         byte ptr cs:[edx],34h 
static unsigned char mov7[] = {0x3E, 0xC6, 0x44, 0xBE, 0x01, 0x34}; // mov         byte ptr ds:[esi+edi*4+1],34h 
static unsigned char mov8[] = {0x8C, 0xC8};                // mov         eax,cs 
static unsigned char mov9[] = {0x0f, 0xb7, 0x00};// movzx eax, [word eax]
static unsigned char mov10[] = {0x67, 0x45, 0xf, 0x43, 0x18}; // cmovnb r11d dword ptr ds:[r8d]
static unsigned char mov11[] = {0x8E, 0xD6}; //    mov ss,si 

static void test_masm1_1()
{
    diana::CMasmString masmString;
    STRTEST(32, masmString, mov, "mov SP, CS");
    STRTEST(32, masmString, mov1, "mov BX, SS");
    STRTEST(32, masmString, mov2, "mov byte ptr DS:[1234h], AL");
    STRTEST(32, masmString, mov3, "mov dword ptr SS:[1234h], EAX");
    STRTEST(32, masmString, mov4, "mov AL, byte ptr DS:[1234h]");
    STRTEST(32, masmString, mov5, "mov EAX, dword ptr SS:[1234h]"); 
    STRTEST(32, masmString, mov6, "mov byte ptr CS:[EDX], 34h");
    STRTEST(32, masmString, mov7, "mov byte ptr DS:[ESI+EDI*4+1], 34h");
    STRTEST(32, masmString, mov8, "mov EAX, CS");
    STRTEST(32, masmString, mov9, "movzx EAX, word ptr DS:[EAX]");
    STRTEST(64, masmString, mov10, "cmovae R11D, dword ptr DS:[R8D]");
    STRTEST(64, masmString, mov11, "mov SS, SI");
}

static void test_masm1_2()
{
    diana::CMasmString masmString;
    
    static unsigned char lea5[] = {0x66, 0x8D, 0xBD, 0x40, 0xFF, 0xFF, 0xFF}; // lea         di,[ebp-0C0h]     
    STRTEST(32, masmString, lea5, "lea DI, word ptr DS:[EBP-0C0h]");
    static unsigned char call[] = {0xE8, 0x83, 0xF9, 0xFF, 0xFF}; // call        @ILT+945(_main) (4113B6h) 
    STRTEST(32, masmString, call, "call 0FFFFF988h");
}

// ---- AES-NI legacy (66 0F 38/3A xx) ----------------------------------------

static void test_masm1_aesni()
{
    diana::CMasmString masmString;

    // AESIMC xmm1, xmm2
    static unsigned char aesimc[]          = {0x66, 0x0F, 0x38, 0xDB, 0xCA};
    // AESENC xmm1, xmm2
    static unsigned char aesenc[]          = {0x66, 0x0F, 0x38, 0xDC, 0xCA};
    // AESENCLAST xmm1, xmm2
    static unsigned char aesenclast[]      = {0x66, 0x0F, 0x38, 0xDD, 0xCA};
    // AESDEC xmm1, xmm2
    static unsigned char aesdec[]          = {0x66, 0x0F, 0x38, 0xDE, 0xCA};
    // AESDECLAST xmm1, xmm2
    static unsigned char aesdeclast[]      = {0x66, 0x0F, 0x38, 0xDF, 0xCA};
    // AESKEYGENASSIST xmm1, xmm2, 0x01
    static unsigned char aeskeygenassist[] = {0x66, 0x0F, 0x3A, 0xDF, 0xCA, 0x01};
    // PCLMULQDQ xmm1, xmm2, 0x01
    static unsigned char pclmulqdq[]       = {0x66, 0x0F, 0x3A, 0x44, 0xCA, 0x01};

    STRTEST(64, masmString, aesimc,          "aesimc XMM1, XMM2");
    STRTEST(64, masmString, aesenc,          "aesenc XMM1, XMM2");
    STRTEST(64, masmString, aesenclast,      "aesenclast XMM1, XMM2");
    STRTEST(64, masmString, aesdec,          "aesdec XMM1, XMM2");
    STRTEST(64, masmString, aesdeclast,      "aesdeclast XMM1, XMM2");
    STRTEST(64, masmString, aeskeygenassist, "aeskeygenassist XMM1, XMM2, 1");
    STRTEST(64, masmString, pclmulqdq,       "pclmulqdq XMM1, XMM2, 1");

    // ---- VEX-encoded AES/CLMUL -------------------------------------------

    // VAESIMC xmm1, xmm2  (VEX.128.66.0F38.WIG DB /r)
    static unsigned char vaesimc[]          = {0xC4, 0xE2, 0x79, 0xDB, 0xCA};
    // VAESENC xmm1, xmm2, xmm3  (VEX.NDS.128.66.0F38.WIG DC /r)
    static unsigned char vaesenc[]          = {0xC4, 0xE2, 0x69, 0xDC, 0xCB};
    // VAESENCLAST xmm1, xmm2, xmm3  (VEX.NDS.128.66.0F38.WIG DD /r)
    static unsigned char vaesenclast[]      = {0xC4, 0xE2, 0x69, 0xDD, 0xCB};
    // VAESDEC xmm1, xmm2, xmm3  (VEX.NDS.128.66.0F38.WIG DE /r)
    static unsigned char vaesdec[]          = {0xC4, 0xE2, 0x69, 0xDE, 0xCB};
    // VAESDECLAST xmm1, xmm2, xmm3  (VEX.NDS.128.66.0F38.WIG DF /r)
    static unsigned char vaesdeclast[]      = {0xC4, 0xE2, 0x69, 0xDF, 0xCB};
    // VAESKEYGENASSIST xmm1, xmm2, 0x01  (VEX.128.66.0F3A.WIG DF /r ib)
    static unsigned char vaeskeygenassist[] = {0xC4, 0xE3, 0x79, 0xDF, 0xCA, 0x01};
    // VPCLMULQDQ xmm1, xmm2, xmm3, 0x01  (VEX.NDS.128.66.0F3A.WIG 44 /r ib)
    static unsigned char vpclmulqdq[]       = {0xC4, 0xE3, 0x69, 0x44, 0xCB, 0x01};

    STRTEST(64, masmString, vaesimc,          "vaesimc XMM1, XMM2");
    STRTEST(64, masmString, vaesenc,          "vaesenc XMM1, XMM2, XMM3");
    STRTEST(64, masmString, vaesenclast,      "vaesenclast XMM1, XMM2, XMM3");
    STRTEST(64, masmString, vaesdec,          "vaesdec XMM1, XMM2, XMM3");
    STRTEST(64, masmString, vaesdeclast,      "vaesdeclast XMM1, XMM2, XMM3");
    STRTEST(64, masmString, vaeskeygenassist, "vaeskeygenassist XMM1, XMM2, 1");
    STRTEST(64, masmString, vpclmulqdq,       "vpclmulqdq XMM1, XMM2, XMM3, 1");
}

// ---- AVX (VEX-encoded arithmetic/move) --------------------------------------

static void test_masm1_avx()
{
    diana::CMasmString masmString;

    // VADDPD xmm1, xmm2, xmm3  (VEX.NDS.128.66.0F.WIG 58 /r)
    static unsigned char vaddpd_xmm[]  = {0xC5, 0xE9, 0x58, 0xCB};
    // VADDPD ymm1, ymm2, ymm3  (VEX.NDS.256.66.0F.WIG 58 /r)
    static unsigned char vaddpd_ymm[]  = {0xC5, 0xED, 0x58, 0xCB};
    // VMOVAPD xmm0, xmm1  (VEX.128.66.0F.WIG 28 /r)
    static unsigned char vmovapd_xmm[] = {0xC5, 0xF9, 0x28, 0xC1};
    // VMOVUPS xmm2, xmm3  (VEX.128.0F.WIG 10 /r)
    static unsigned char vmovups_xmm[] = {0xC5, 0xF8, 0x10, 0xD3};
    // VMOVAPD ymm0, ymm1  (VEX.256.66.0F.WIG 28 /r)
    static unsigned char vmovapd_ymm[] = {0xC5, 0xFD, 0x28, 0xC1};
    // VMOVUPS ymm2, ymm3  (VEX.256.0F.WIG 10 /r)
    static unsigned char vmovups_ymm[] = {0xC5, 0xFC, 0x10, 0xD3};
    // VADDPD xmm1, xmm2, [rax]  (VEX.NDS.128.66.0F.WIG 58 /r, memory source)
    static unsigned char vaddpd_mem[]      = {0xC5, 0xE9, 0x58, 0x08};
    // VADDPD ymm1, ymm2, [rax]  (VEX.NDS.256.66.0F.WIG 58 /r, memory source)
    static unsigned char vaddpd_ymm_mem[]  = {0xC5, 0xED, 0x58, 0x08};

    STRTEST(64, masmString, vaddpd_xmm,     "vaddpd XMM1, XMM2, XMM3");
    STRTEST(64, masmString, vaddpd_ymm,     "vaddpd YMM1, YMM2, YMM3");
    STRTEST(64, masmString, vmovapd_xmm,    "vmovapd XMM0, XMM1");
    STRTEST(64, masmString, vmovapd_ymm,    "vmovapd YMM0, YMM1");
    STRTEST(64, masmString, vmovups_xmm,    "vmovups XMM2, XMM3");
    STRTEST(64, masmString, vmovups_ymm,    "vmovups YMM2, YMM3");
    STRTEST(64, masmString, vaddpd_mem,     "vaddpd XMM1, XMM2, xmmword ptr DS:[RAX]");
    STRTEST(64, masmString, vaddpd_ymm_mem, "vaddpd YMM1, YMM2, ymmword ptr DS:[RAX]");
}

// ---- SSE4.1/4.2 --------------------------------------------------------

static void test_masm1_sse4()
{
    diana::CMasmString masmString;

    // PMULLD xmm1, xmm2
    static unsigned char pmulld[] = {0x66, 0x0F, 0x38, 0x40, 0xCA};
    // PCMPEQQ xmm1, xmm2
    static unsigned char pcmpeqq[] = {0x66, 0x0F, 0x38, 0x29, 0xCA};
    // ROUNDPD xmm1, xmm2, 1
    static unsigned char roundpd[] = {0x66, 0x0F, 0x3A, 0x09, 0xCA, 0x01};
    // BLENDPS xmm1, xmm2, 0x0F
    static unsigned char blendps[] = {0x66, 0x0F, 0x3A, 0x0C, 0xCA, 0x0F};
    // PMAXSB xmm1, xmm2
    static unsigned char pmaxsb[] = {0x66, 0x0F, 0x38, 0x3C, 0xCA};
    // PTEST xmm1, xmm2
    static unsigned char ptest[] = {0x66, 0x0F, 0x38, 0x17, 0xCA};
    // PCMPGTQ xmm1, xmm2  (SSE4.2)
    static unsigned char pcmpgtq[] = {0x66, 0x0F, 0x38, 0x37, 0xCA};
    // PCMPISTRI xmm1, xmm2, 1  (SSE4.2)
    static unsigned char pcmpistri[] = {0x66, 0x0F, 0x3A, 0x63, 0xCA, 0x01};

    STRTEST(64, masmString, pmulld,    "pmulld XMM1, XMM2");
    STRTEST(64, masmString, pcmpeqq,   "pcmpeqq XMM1, XMM2");
    STRTEST(64, masmString, roundpd,   "roundpd XMM1, XMM2, 1");
    STRTEST(64, masmString, blendps,   "blendps XMM1, XMM2, 0Fh");
    STRTEST(64, masmString, pmaxsb,    "pmaxsb XMM1, XMM2");
    STRTEST(64, masmString, ptest,     "ptest XMM1, XMM2");
    STRTEST(64, masmString, pcmpgtq,   "pcmpgtq XMM1, XMM2");
    STRTEST(64, masmString, pcmpistri, "pcmpistri XMM1, XMM2, 1");
}

// ---- BMI1/BMI2 ---------------------------------------------------------

static void test_masm1_bmi()
{
    diana::CMasmString masmString;

    // POPCNT eax, ebx  (F3 0F B8 /r)
    static unsigned char popcnt[] = {0xF3, 0x0F, 0xB8, 0xC3};
    // LZCNT eax, ebx  (F3 0F BD /r)
    static unsigned char lzcnt[] = {0xF3, 0x0F, 0xBD, 0xC3};
    // TZCNT eax, ebx  (F3 0F BC /r)
    static unsigned char tzcnt[] = {0xF3, 0x0F, 0xBC, 0xC3};
    // CRC32 eax, ebx  (F2 0F 38 F1 /r)
    static unsigned char crc32[] = {0xF2, 0x0F, 0x38, 0xF1, 0xC3};
    // SHA1NEXTE xmm1, xmm2  (0F 38 C8 /r)
    static unsigned char sha1nexte[] = {0x0F, 0x38, 0xC8, 0xCA};

    STRTEST(32, masmString, popcnt,    "popcnt EAX, EBX");
    STRTEST(32, masmString, lzcnt,     "lzcnt EAX, EBX");
    STRTEST(32, masmString, tzcnt,     "tzcnt EAX, EBX");
    STRTEST(32, masmString, crc32,     "crc32 EAX, EBX");
    STRTEST(64, masmString, sha1nexte, "sha1nexte XMM1, XMM2");
}

static void test_masm1_sse2()
{
    diana::CMasmString masmString;

    static unsigned char addps[]      = {0x0F, 0x58, 0xCA};
    static unsigned char mulps[]      = {0x0F, 0x59, 0xCA};
    static unsigned char mulpd[]      = {0x66, 0x0F, 0x59, 0xCA};
    static unsigned char mulss[]      = {0xF3, 0x0F, 0x59, 0xCA};
    static unsigned char subpd[]      = {0x66, 0x0F, 0x5C, 0xCA};
    static unsigned char subps[]      = {0x0F, 0x5C, 0xCA};
    static unsigned char subss[]      = {0xF3, 0x0F, 0x5C, 0xCA};
    static unsigned char divpd[]      = {0x66, 0x0F, 0x5E, 0xCA};
    static unsigned char divps[]      = {0x0F, 0x5E, 0xCA};
    static unsigned char divss[]      = {0xF3, 0x0F, 0x5E, 0xCA};
    static unsigned char sqrtpd[]     = {0x66, 0x0F, 0x51, 0xCA};
    static unsigned char sqrtps[]     = {0x0F, 0x51, 0xCA};
    static unsigned char sqrtsd[]     = {0xF2, 0x0F, 0x51, 0xCA};
    static unsigned char sqrtss[]     = {0xF3, 0x0F, 0x51, 0xCA};
    static unsigned char maxpd[]      = {0x66, 0x0F, 0x5F, 0xCA};
    static unsigned char maxps[]      = {0x0F, 0x5F, 0xCA};
    static unsigned char maxsd[]      = {0xF2, 0x0F, 0x5F, 0xCA};
    static unsigned char maxss[]      = {0xF3, 0x0F, 0x5F, 0xCA};
    static unsigned char minpd[]      = {0x66, 0x0F, 0x5D, 0xCA};
    static unsigned char minps[]      = {0x0F, 0x5D, 0xCA};
    static unsigned char minsd[]      = {0xF2, 0x0F, 0x5D, 0xCA};
    static unsigned char minss[]      = {0xF3, 0x0F, 0x5D, 0xCA};
    static unsigned char paddq[]      = {0x66, 0x0F, 0xD4, 0xCA};
    static unsigned char psubq[]      = {0x66, 0x0F, 0xFB, 0xCA};
    static unsigned char paddsb[]     = {0x66, 0x0F, 0xEC, 0xCA};
    static unsigned char paddsw[]     = {0x66, 0x0F, 0xED, 0xCA};
    static unsigned char paddusb[]    = {0x66, 0x0F, 0xDC, 0xCA};
    static unsigned char paddusw[]    = {0x66, 0x0F, 0xDD, 0xCA};
    static unsigned char psubb[]      = {0x66, 0x0F, 0xF8, 0xCA};
    static unsigned char psubw[]      = {0x66, 0x0F, 0xF9, 0xCA};
    static unsigned char psubsb[]     = {0x66, 0x0F, 0xE8, 0xCA};
    static unsigned char psubsw[]     = {0x66, 0x0F, 0xE9, 0xCA};
    static unsigned char psubusb[]    = {0x66, 0x0F, 0xD8, 0xCA};
    static unsigned char psubusw[]    = {0x66, 0x0F, 0xD9, 0xCA};
    static unsigned char pcmpeqb[]    = {0x66, 0x0F, 0x74, 0xCA};
    static unsigned char pcmpeqw[]    = {0x66, 0x0F, 0x75, 0xCA};
    static unsigned char pcmpgtd[]    = {0x66, 0x0F, 0x66, 0xCA};
    static unsigned char pcmpgtw[]    = {0x66, 0x0F, 0x65, 0xCA};
    static unsigned char pmaxsw[]     = {0x66, 0x0F, 0xEE, 0xCA};
    static unsigned char pmaxub[]     = {0x66, 0x0F, 0xDE, 0xCA};
    static unsigned char pminsw[]     = {0x66, 0x0F, 0xEA, 0xCA};
    static unsigned char pminub[]     = {0x66, 0x0F, 0xDA, 0xCA};
    static unsigned char pmulhuw[]    = {0x66, 0x0F, 0xE4, 0xCA};
    static unsigned char pmulhw[]     = {0x66, 0x0F, 0xE5, 0xCA};
    static unsigned char pmuludq[]    = {0x66, 0x0F, 0xF4, 0xCA};
    static unsigned char pmaddwd[]    = {0x66, 0x0F, 0xF5, 0xCA};
    static unsigned char pavgw[]      = {0x66, 0x0F, 0xE3, 0xCA};
    static unsigned char packssdw[]   = {0x66, 0x0F, 0x6B, 0xCA};
    static unsigned char packsswb[]   = {0x66, 0x0F, 0x63, 0xCA};
    static unsigned char punpckhbw[]  = {0x66, 0x0F, 0x68, 0xCA};
    static unsigned char punpckhwd[]  = {0x66, 0x0F, 0x69, 0xCA};
    static unsigned char punpckhdq[]  = {0x66, 0x0F, 0x6A, 0xCA};
    static unsigned char punpckhqdq[] = {0x66, 0x0F, 0x6D, 0xCA};
    static unsigned char unpckhpd[]   = {0x66, 0x0F, 0x15, 0xCA};
    static unsigned char unpckhps[]   = {0x0F, 0x15, 0xCA};
    static unsigned char unpcklpd[]   = {0x66, 0x0F, 0x14, 0xCA};
    static unsigned char comiss[]     = {0x0F, 0x2F, 0xCA};
    static unsigned char ucomiss[]    = {0x0F, 0x2E, 0xCA};
    static unsigned char movsldup[]   = {0xF3, 0x0F, 0x12, 0xCA};
    static unsigned char shufpd[]     = {0x66, 0x0F, 0xC6, 0xCA, 0x01};
    static unsigned char pshufhw[]    = {0xF3, 0x0F, 0x70, 0xC1, 0xE4};
    static unsigned char pshuflw[]    = {0xF2, 0x0F, 0x70, 0xC1, 0xE4};
    static unsigned char sfence[]     = {0x0F, 0xAE, 0xF8};
    static unsigned char lfence[]     = {0x0F, 0xAE, 0xE8};
    static unsigned char femms[]      = {0x0F, 0x0E};
    static unsigned char rdtscp[]     = {0x0F, 0x01, 0xF9};

    /* immediate shift forms (rm=xmm0) */
    static unsigned char pslld[]   = {0x66, 0x0F, 0x72, 0xF0, 0x02};
    static unsigned char psllq[]   = {0x66, 0x0F, 0x73, 0xF0, 0x03};
    static unsigned char psrld[]   = {0x66, 0x0F, 0x72, 0xD0, 0x01};
    static unsigned char psrlq[]   = {0x66, 0x0F, 0x73, 0xD0, 0x01};
    static unsigned char psrad[]   = {0x66, 0x0F, 0x72, 0xE0, 0x01};
    static unsigned char pslldq[]  = {0x66, 0x0F, 0x73, 0xF8, 0x04};
    static unsigned char psrldq[]  = {0x66, 0x0F, 0x73, 0xD8, 0x04};

    /* conversion */
    static unsigned char cvtsi2ss[]  = {0xF3, 0x0F, 0x2A, 0xC0};
    static unsigned char cvtsi2sd[]  = {0xF2, 0x0F, 0x2A, 0xC0};
    static unsigned char cvtss2sd[]  = {0xF3, 0x0F, 0x5A, 0xC1};
    static unsigned char cvtsd2ss[]  = {0xF2, 0x0F, 0x5A, 0xC1};
    static unsigned char cvtss2si[]  = {0xF3, 0x0F, 0x2D, 0xC0};
    static unsigned char cvtsd2si[]  = {0xF2, 0x0F, 0x2D, 0xC0};
    static unsigned char cvttss2si[] = {0xF3, 0x0F, 0x2C, 0xC0};
    static unsigned char pextrw[]    = {0x66, 0x0F, 0xC5, 0xC0, 0x00};
    static unsigned char pinsrw[]    = {0x66, 0x0F, 0xC4, 0xC0, 0x00};

    STRTEST(32, masmString, addps,      "addps XMM1, XMM2");
    STRTEST(32, masmString, mulps,      "mulps XMM1, XMM2");
    STRTEST(32, masmString, mulpd,      "mulpd XMM1, XMM2");
    STRTEST(32, masmString, mulss,      "mulss XMM1, XMM2");
    STRTEST(32, masmString, subpd,      "subpd XMM1, XMM2");
    STRTEST(32, masmString, subps,      "subps XMM1, XMM2");
    STRTEST(32, masmString, subss,      "subss XMM1, XMM2");
    STRTEST(32, masmString, divpd,      "divpd XMM1, XMM2");
    STRTEST(32, masmString, divps,      "divps XMM1, XMM2");
    STRTEST(32, masmString, divss,      "divss XMM1, XMM2");
    STRTEST(32, masmString, sqrtpd,     "sqrtpd XMM1, XMM2");
    STRTEST(32, masmString, sqrtps,     "sqrtps XMM1, XMM2");
    STRTEST(32, masmString, sqrtsd,     "sqrtsd XMM1, XMM2");
    STRTEST(32, masmString, sqrtss,     "sqrtss XMM1, XMM2");
    STRTEST(32, masmString, maxpd,      "maxpd XMM1, XMM2");
    STRTEST(32, masmString, maxps,      "maxps XMM1, XMM2");
    STRTEST(32, masmString, maxsd,      "maxsd XMM1, XMM2");
    STRTEST(32, masmString, maxss,      "maxss XMM1, XMM2");
    STRTEST(32, masmString, minpd,      "minpd XMM1, XMM2");
    STRTEST(32, masmString, minps,      "minps XMM1, XMM2");
    STRTEST(32, masmString, minsd,      "minsd XMM1, XMM2");
    STRTEST(32, masmString, minss,      "minss XMM1, XMM2");
    STRTEST(32, masmString, paddq,      "paddq XMM1, XMM2");
    STRTEST(32, masmString, psubq,      "psubq XMM1, XMM2");
    STRTEST(32, masmString, paddsb,     "paddsb XMM1, XMM2");
    STRTEST(32, masmString, paddsw,     "paddsw XMM1, XMM2");
    STRTEST(32, masmString, paddusb,    "paddusb XMM1, XMM2");
    STRTEST(32, masmString, paddusw,    "paddusw XMM1, XMM2");
    STRTEST(32, masmString, psubb,      "psubb XMM1, XMM2");
    STRTEST(32, masmString, psubw,      "psubw XMM1, XMM2");
    STRTEST(32, masmString, psubsb,     "psubsb XMM1, XMM2");
    STRTEST(32, masmString, psubsw,     "psubsw XMM1, XMM2");
    STRTEST(32, masmString, psubusb,    "psubusb XMM1, XMM2");
    STRTEST(32, masmString, psubusw,    "psubusw XMM1, XMM2");
    STRTEST(32, masmString, pcmpeqb,    "pcmpeqb XMM1, XMM2");
    STRTEST(32, masmString, pcmpeqw,    "pcmpeqw XMM1, XMM2");
    STRTEST(32, masmString, pcmpgtd,    "pcmpgtd XMM1, XMM2");
    STRTEST(32, masmString, pcmpgtw,    "pcmpgtw XMM1, XMM2");
    STRTEST(32, masmString, pmaxsw,     "pmaxsw XMM1, XMM2");
    STRTEST(32, masmString, pmaxub,     "pmaxub XMM1, XMM2");
    STRTEST(32, masmString, pminsw,     "pminsw XMM1, XMM2");
    STRTEST(32, masmString, pminub,     "pminub XMM1, XMM2");
    STRTEST(32, masmString, pmulhuw,    "pmulhuw XMM1, XMM2");
    STRTEST(32, masmString, pmulhw,     "pmulhw XMM1, XMM2");
    STRTEST(32, masmString, pmuludq,    "pmuludq XMM1, XMM2");
    STRTEST(32, masmString, pmaddwd,    "pmaddwd XMM1, XMM2");
    STRTEST(32, masmString, pavgw,      "pavgw XMM1, XMM2");
    STRTEST(32, masmString, packssdw,   "packssdw XMM1, XMM2");
    STRTEST(32, masmString, packsswb,   "packsswb XMM1, XMM2");
    STRTEST(32, masmString, punpckhbw,  "punpckhbw XMM1, XMM2");
    STRTEST(32, masmString, punpckhwd,  "punpckhwd XMM1, XMM2");
    STRTEST(32, masmString, punpckhdq,  "punpckhdq XMM1, XMM2");
    STRTEST(32, masmString, punpckhqdq, "punpckhqdq XMM1, XMM2");
    STRTEST(32, masmString, unpckhpd,   "unpckhpd XMM1, XMM2");
    STRTEST(32, masmString, unpckhps,   "unpckhps XMM1, XMM2");
    STRTEST(32, masmString, unpcklpd,   "unpcklpd XMM1, XMM2");
    STRTEST(32, masmString, comiss,     "comiss XMM1, XMM2");
    STRTEST(32, masmString, ucomiss,    "ucomiss XMM1, XMM2");
    STRTEST(32, masmString, movsldup,   "movsldup XMM1, XMM2");
    STRTEST(32, masmString, shufpd,     "shufpd XMM1, XMM2, 1");
    STRTEST(32, masmString, pshufhw,    "pshufhw XMM0, XMM1, 0E4h");
    STRTEST(32, masmString, pshuflw,    "pshuflw XMM0, XMM1, 0E4h");
    STRTEST(32, masmString, sfence,     "sfence");
    STRTEST(32, masmString, lfence,     "lfence");
    STRTEST(32, masmString, femms,      "femms");
    STRTEST(32, masmString, rdtscp,     "rdtscp");

    STRTEST(32, masmString, pslld,   "pslld XMM0, 2");
    STRTEST(32, masmString, psllq,   "psllq XMM0, 3");
    STRTEST(32, masmString, psrld,   "psrld XMM0, 1");
    STRTEST(32, masmString, psrlq,   "psrlq XMM0, 1");
    STRTEST(32, masmString, psrad,   "psrad XMM0, 1");
    STRTEST(32, masmString, pslldq,  "pslldq XMM0, 4");
    STRTEST(32, masmString, psrldq,  "psrldq XMM0, 4");

    STRTEST(32, masmString, cvtsi2ss,  "cvtsi2ss XMM0, EAX");
    STRTEST(32, masmString, cvtsi2sd,  "cvtsi2sd XMM0, EAX");
    STRTEST(32, masmString, cvtss2sd,  "cvtss2sd XMM0, XMM1");
    STRTEST(32, masmString, cvtsd2ss,  "cvtsd2ss XMM0, XMM1");
    STRTEST(32, masmString, cvtss2si,  "cvtss2si EAX, XMM0");
    STRTEST(32, masmString, cvtsd2si,  "cvtsd2si EAX, XMM0");
    STRTEST(32, masmString, cvttss2si, "cvttss2si EAX, XMM0");
    STRTEST(32, masmString, pextrw,    "pextrw EAX, XMM0, 0");
    STRTEST(32, masmString, pinsrw,    "pinsrw XMM0, EAX, 0");
}

static void test_masm1_fpu2()
{
    diana::CMasmString masmString;

    static unsigned char fxch[]     = {0xD9, 0xC9};
    static unsigned char fabs[]     = {0xD9, 0xE1};
    static unsigned char fchs[]     = {0xD9, 0xE0};
    static unsigned char fldl2t[]   = {0xD9, 0xE9};
    static unsigned char fldl2e[]   = {0xD9, 0xEA};
    static unsigned char fldlg2[]   = {0xD9, 0xEC};
    static unsigned char fldln2[]   = {0xD9, 0xED};
    static unsigned char f2xm1[]    = {0xD9, 0xF0};
    static unsigned char fyl2x[]    = {0xD9, 0xF1};
    static unsigned char fptan[]    = {0xD9, 0xF2};
    static unsigned char fpatan[]   = {0xD9, 0xF3};
    static unsigned char fxtract[]  = {0xD9, 0xF4};
    static unsigned char fprem1[]   = {0xD9, 0xF5};
    static unsigned char fdecstp[]  = {0xD9, 0xF6};
    static unsigned char fincstp[]  = {0xD9, 0xF7};
    static unsigned char fprem[]    = {0xD9, 0xF8};
    static unsigned char fyl2xp1[]  = {0xD9, 0xF9};
    static unsigned char fsqrt[]    = {0xD9, 0xFA};
    static unsigned char fsincos[]  = {0xD9, 0xFB};
    static unsigned char frndint[]  = {0xD9, 0xFC};
    static unsigned char fscale[]   = {0xD9, 0xFD};
    static unsigned char fsin[]     = {0xD9, 0xFE};
    static unsigned char fcos[]     = {0xD9, 0xFF};
    static unsigned char fucom[]    = {0xDD, 0xE1};
    static unsigned char fucomp[]   = {0xDD, 0xE9};
    static unsigned char fucompp[]  = {0xDA, 0xE9};
    static unsigned char fcomi[]    = {0xDB, 0xF1};
    static unsigned char fcomip[]   = {0xDF, 0xF1};
    static unsigned char fucomi[]   = {0xDB, 0xE9};
    static unsigned char fucomip[]  = {0xDF, 0xE9};
    static unsigned char fcmovb[]   = {0xDA, 0xC1};
    static unsigned char fcmovbe[]  = {0xDA, 0xD1};
    static unsigned char fcmove[]   = {0xDA, 0xC9};
    static unsigned char fcmovnb[]  = {0xDB, 0xC1};
    static unsigned char fcmovnbe[] = {0xDB, 0xD1};
    static unsigned char fcmovne[]  = {0xDB, 0xC9};
    static unsigned char fcmovu[]   = {0xDA, 0xD9};
    static unsigned char fcmovnu[]  = {0xDB, 0xD9};
    static unsigned char ffree[]    = {0xDD, 0xC0};
    static unsigned char ffreep[]   = {0xDF, 0xC0};
    static unsigned char fninit[]   = {0xDB, 0xE3};
    static unsigned char fnop[]     = {0xD9, 0xD0};

    STRTEST(32, masmString, fxch,    "fxch fpu_ST1");
    STRTEST(32, masmString, fabs,    "fabs");
    STRTEST(32, masmString, fchs,    "fchs");
    STRTEST(32, masmString, fldl2t,  "fldl2t");
    STRTEST(32, masmString, fldl2e,  "fldl2e");
    STRTEST(32, masmString, fldlg2,  "fldlg2");
    STRTEST(32, masmString, fldln2,  "fldln2");
    STRTEST(32, masmString, f2xm1,   "f2xm1");
    STRTEST(32, masmString, fyl2x,   "fyl2x");
    STRTEST(32, masmString, fptan,   "fptan");
    STRTEST(32, masmString, fpatan,  "fpatan");
    STRTEST(32, masmString, fxtract, "fxtract");
    STRTEST(32, masmString, fprem1,  "fprem1");
    STRTEST(32, masmString, fdecstp, "fdecstp");
    STRTEST(32, masmString, fincstp, "fincstp");
    STRTEST(32, masmString, fprem,   "fprem");
    STRTEST(32, masmString, fyl2xp1, "fyl2xp1");
    STRTEST(32, masmString, fsqrt,   "fsqrt");
    STRTEST(32, masmString, fsincos, "fsincos");
    STRTEST(32, masmString, frndint, "frndint");
    STRTEST(32, masmString, fscale,  "fscale");
    STRTEST(32, masmString, fsin,    "fsin");
    STRTEST(32, masmString, fcos,    "fcos");
    STRTEST(32, masmString, fucom,   "fucom fpu_ST1");
    STRTEST(32, masmString, fucomp,  "fucomp fpu_ST1");
    STRTEST(32, masmString, fucompp, "fucompp");
    STRTEST(32, masmString, fcomi,   "fcomi fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fcomip,  "fcomip fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fucomi,  "fucomi fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fucomip, "fucomip fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fcmovb,  "fcmovb fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fcmovbe, "fcmovbe fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fcmove,  "fcmove fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fcmovnb, "fcmovnb fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fcmovnbe,"fcmovnbe fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fcmovne, "fcmovne fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fcmovu,  "fcmovu fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, fcmovnu, "fcmovnu fpu_ST0, fpu_ST1");
    STRTEST(32, masmString, ffree,   "ffree fpu_ST0");
    STRTEST(32, masmString, ffreep,  "ffreep fpu_ST0");
    STRTEST(32, masmString, fninit,  "fninit");
    STRTEST(32, masmString, fnop,    "fnop");
}

void test_masm1()
{
    test_masm1_1();
    test_masm1_2();
    test_masm1_aesni();
    test_masm1_avx();
    test_masm1_sse4();
    test_masm1_bmi();
    test_masm1_sse2();
    test_masm1_fpu2();
}