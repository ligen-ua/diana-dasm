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

void test_masm1()
{
    test_masm1_1();
    test_masm1_2();
    test_masm1_aesni();
    test_masm1_avx();
}