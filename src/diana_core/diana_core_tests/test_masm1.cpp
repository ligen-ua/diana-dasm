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

void test_masm1()
{
    test_masm1_1();
    test_masm1_2();
}