#include "test_analyze.h"
#include "test_analyze_common.h"
extern "C"
{
#include "diana_pe_analyzer.h"
}
#include <vector>


void test_analyzer1()
{
    unsigned char code[] = { 0x48, 0x8b, 0xc4 //mov     rax,rsp :0
                            , 0x45, 0x33, 0xf6//    xor     r14d,r14d   :3
                            , 0xe8, 0x12, 0x00, 0x00, 0x00//    call proc1   :6
                            , 0xe8, 0x03, 0x00, 0x00, 0x00//    call proc2   :11
                            , 0x75, 0x15                  //    jne :16
                            , 0xc3                        //    ret :18
                            , 0x66, 0x44, 0x0f, 0x7f, 0xa1, 0xc0, 0x00, 0x00, 0x00//    movdqa oword ptr [rcx+0xc0],xmm12 :19
                            , 0xc3                                               //    ret :28
                            , 0x48, 0x89, 0x71, 0x20 //    mov     [rcx+0x20],rsi :29
                            , 0xe9, 0x01, 0x00, 0x00, 0x00 // jmp to next :33
                            , 0x00                   //    db 0  :38 // should never have come here
                            , 0x48, 0xb8, 50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00                   //    mov rax, 1  :39
                            , 0xC3       // ret :49
                            , 0x33, 0xd2 // xor :50
                            , 0xc5                   //    db c5  :52 
                            };

    IntructionInfo instructions[] =   
                        {{0,               0,                           0},
                         {3,               0,                           0},
                         {6,               "29",                        0},
                         {11,              "19",                        0},
                         {16,              "39",                        0},
                         {18,              0,                           0},
                         {19,              "$192",                      "11"},
                         {28,              0,                           0},
                         {29,              0,                           "6"},
                         {33,              "39",                        0},
                         // mov rax, 50: the route at 50 is rolled back, the reference stays as data
                         {39,              "50;#50",                    "16;33"},
                         {49,              0,                           0}
    };

    const int instructionsCount = sizeof(instructions)/sizeof(instructions[0]);

    size_t minOffset = 0;
    size_t maxOffset = sizeof(code);
    size_t start = 0;

    TestAnalyzeEnvironment env((OPERAND_SIZE)code, start, sizeof(code));
    Diana_InstructionsOwner owner;
    DIANA_TEST_ASSERT(DI_SUCCESS == Diana_InstructionsOwner_Init(&owner, maxOffset- minOffset, 0));
    DIANA_TEST_ASSERT(DI_SUCCESS == Diana_AnalyzeCode(&owner,
                                                &env.observer,
                                                DIANA_MODE64,
                                                start,
                                                maxOffset));

    DIANA_TEST_ASSERT(owner.m_actualSize == instructionsCount);
    // verify found instructions
    for(int i = 0; i < instructionsCount; ++i)
    {
        const IntructionInfo * pInfo = instructions + i;
        Diana_Instruction * pInstruction = owner.m_ppPresenceVec[pInfo->offset];
        DIANA_TEST_ASSERT(pInstruction);
        DIANA_TEST_ASSERT(pInstruction->m_offset == pInfo->offset);

        VerifyREF(pInfo->pXrefsFrom, &pInstruction->m_referencesToThisInstruction, 0);
        VerifyREF(pInfo->pXrefsTo, &pInstruction->m_referencesFromThisInstruction, 1);
    }

    Diana_InstructionsOwner_Free(&owner);
}


void test_analyzer2()
{
    unsigned char code[] = {  0xbe, 23, 0x00, 0x00, 0x00 //    mov     esi,14 :0
                            , 0xbe, 24, 0x00, 0x00, 0x00 //    mov     esi,15 :5
                            , 0xbe, 25, 0x00, 0x00, 0x00 //    mov     esi,16 :10
                            , 0x75, 0x5                  //    jne            :15
                            , 0xbe, 26, 0x00, 0x00, 0x00 //    mov     esi,17 :17
                            , 0xC3                       //    ret            :22
                            , 0xbe, 55, 0x00, 0x00, 0x00 //    mov     esi,55 :23
                            , 0xc5                       //    db c5          :28
                            , 0xc5                       //    db c5          :29
                            , 0xc5                       //    db c5          :30
                            , 0xc5                       //    db c5          :31
                            , 0xc5                       //    db c5          :32
                            , 0xc5                       //    db c5          :33
                            , 0xc5                       //    db c5          :34
                            };

    IntructionInfo instructions[] =   
                        {{0,         "23;#23",                        0},     // the route at 23 is rolled back
                         {5,      "24;#24",                        0},     // 24 is aaa, not in 64-bit
                         {10,      "25;#25",                        0},     // the route at 25 too
                         {15,           "22",                        0},
                         {17,      "26;#26",                        0},     // and at 26
                         {22,              0,                     "15"},
                         {23,              0,                        0},
    };

    const int instructionsCount = sizeof(instructions)/sizeof(instructions[0]);

    size_t minOffset = 0;
    size_t maxOffset = sizeof(code);
    size_t start = 0;

    TestAnalyzeEnvironment env((OPERAND_SIZE)code, start, sizeof(code));
    Diana_InstructionsOwner owner;
    DIANA_TEST_ASSERT(DI_SUCCESS == Diana_InstructionsOwner_Init(&owner, maxOffset- minOffset, 0));
    DIANA_TEST_ASSERT(DI_SUCCESS == Diana_AnalyzeCode(&owner,
                                                &env.observer,
                                                DIANA_MODE64,
                                                start,
                                                maxOffset));

    // 23 is listed but rolled back; the failed route at 24 isn't counted either
    DIANA_TEST_ASSERT(owner.m_actualSize == instructionsCount - 1);
    // verify found instructions
    for(int i = 0; i < instructionsCount; ++i)
    {
        const IntructionInfo * pInfo = instructions + i;
        Diana_Instruction * pInstruction = owner.m_ppPresenceVec[pInfo->offset];
        DIANA_TEST_ASSERT(pInstruction);
        DIANA_TEST_ASSERT(pInstruction->m_offset == pInfo->offset);

        VerifyREF(pInfo->pXrefsFrom, &pInstruction->m_referencesToThisInstruction, 0);
        VerifyREF(pInfo->pXrefsTo, &pInstruction->m_referencesFromThisInstruction, 1);
    }

    Diana_InstructionsOwner_Free(&owner);
}


void test_analyzer3()
{
    unsigned char code[] = {  0x75, 0x5 //    jne +0 - out of scope
                            };
    const int instructionsCount = 1;

    size_t minOffset = 0;
    size_t maxOffset = sizeof(code);
    size_t start = 0;

    TestAnalyzeEnvironment env((OPERAND_SIZE)code, start, sizeof(code));
    Diana_InstructionsOwner owner;
    DIANA_TEST_ASSERT(DI_SUCCESS == Diana_InstructionsOwner_Init(&owner, maxOffset- minOffset, 0));
    DIANA_TEST_ASSERT(DI_SUCCESS == Diana_AnalyzeCode(&owner,
                                                &env.observer,
                                                DIANA_MODE64,
                                                start,
                                                maxOffset));

    DIANA_TEST_ASSERT(owner.m_actualSize == 1);
    Diana_InstructionsOwner_Free(&owner);
}

// mov eax, [rip+X] reads data: X is never analyzed as code, even if its bytes decode
static void test_analyzer_data_access_x64()
{
    unsigned char code[] = {  0x8b, 0x05, 0x01, 0x00, 0x00, 0x00  // mov eax, [rip+1] -> 7   :0
                            , 0xc3                                // ret                     :6
                            , 0x90, 0xc3                          // data: nop; ret          :7
                            };
    AnalyzedCode analyzed(code, sizeof(code), DIANA_MODE64);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 2);
    DIANA_TEST_ASSERT(analyzed.At(0) && analyzed.At(6));
    DIANA_TEST_ASSERT(!analyzed.At(7) && !analyzed.At(8));
    Diana_XRef * pXRef = FindXRefFrom(analyzed.At(0), 7, DI_XREF_DATA);
    DIANA_TEST_ASSERT(pXRef != 0);
    DIANA_TEST_ASSERT(pXRef && (pXRef->m_subrefs[1].m_pInstruction->m_flags & DI_INSTRUCTION_DATA));
    DIANA_TEST_ASSERT(pXRef && !(pXRef->m_flags & DI_XREF_INVALID));
}

// mov eax, [abs32] in 32-bit code: the same, the address is absolute
static void test_analyzer_data_access_x86()
{
    unsigned char code[] = {  0x8b, 0x05, 0x07, 0x00, 0x00, 0x00  // mov eax, [7]             :0
                            , 0xc3                                // ret                     :6
                            , 0x90, 0xc3                          // data: nop; ret          :7
                            };
    AnalyzedCode analyzed(code, sizeof(code), DIANA_MODE32);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 2);
    DIANA_TEST_ASSERT(!analyzed.At(7));
    DIANA_TEST_ASSERT(FindXRefFrom(analyzed.At(0), 7, DI_XREF_DATA) != 0);
}

// lea rcx, [rip+X] takes an address: X is tried as code. Bytes that don't decode keep the reference as data
static void test_analyzer_address_taken_data()
{
    unsigned char code[] = {  0x48, 0x8d, 0x0d, 0x01, 0x00, 0x00, 0x00  // lea rcx, [rip+1] -> 8 :0
                            , 0xc3                                      // ret                  :7
                            , 0x06, 0x06, 0x06                          // push es: not in 64-bit :8
                            };
    AnalyzedCode analyzed(code, sizeof(code), DIANA_MODE64);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 2);
    DIANA_TEST_ASSERT(!analyzed.At(8));
    Diana_XRef * pXRef = FindXRefFrom(analyzed.At(0), 8, DI_XREF_DATA|DI_XREF_ADDRESS_TAKEN);
    DIANA_TEST_ASSERT(pXRef != 0);
    DIANA_TEST_ASSERT(pXRef && !(pXRef->m_flags & DI_XREF_INVALID));
}

// lea rcx, [rip+X] to a function: X is code, the xref is a code xref
static void test_analyzer_address_taken_code()
{
    unsigned char code[] = {  0x48, 0x8d, 0x0d, 0x01, 0x00, 0x00, 0x00  // lea rcx, [rip+1] -> 8 :0
                            , 0xc3                                      // ret                  :7
                            , 0x31, 0xc0                                // xor eax, eax         :8
                            , 0xc3                                      // ret                  :10
                            };
    AnalyzedCode analyzed(code, sizeof(code), DIANA_MODE64);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 4);
    DIANA_TEST_ASSERT(analyzed.At(8) && analyzed.At(10));
    Diana_XRef * pXRef = FindXRefFrom(analyzed.At(0), 8, DI_XREF_ADDRESS_TAKEN);
    DIANA_TEST_ASSERT(pXRef && !(pXRef->m_flags & (DI_XREF_DATA|DI_XREF_INVALID)));
}

// lea rcx, [rip+X] past the module: an external address-taken xref, as the observer reports it
static void test_analyzer_address_taken_external()
{
    unsigned char code[] = {  0x48, 0x8d, 0x0d, 0x00, 0x01, 0x00, 0x00  // lea rcx, [rip+100h] -> 107h :0
                            , 0xc3                                      // ret                         :7
                            };
    AnalyzedCode analyzed(code, sizeof(code), DIANA_MODE64);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 2);
    Diana_XRef * pXRef = FindXRefFrom(analyzed.At(0), 0x107, DI_XREF_EXTERNAL|DI_XREF_ADDRESS_TAKEN);
    DIANA_TEST_ASSERT(pXRef && !(pXRef->m_flags & (DI_XREF_DATA|DI_XREF_INVALID)));
}

// a minimal PE32+ x64 image in memory (module layout): .text at 0x1000, .pdata at 0x2000
static void put16(unsigned char * p, size_t offset, DI_UINT32 value) 
{ 
	p[offset] = (unsigned char)value; 
	p[offset + 1] = (unsigned char)(value >> 8); 
};
static void put32(unsigned char * p, size_t offset, DI_UINT32 value) 
{ 
	put16(p, offset, value & 0xFFFF); 
	put16(p, offset + 2, value >> 16); 
};
static std::vector<unsigned char> MakePe64(const unsigned char * pCode, size_t codeSize,
                                           const DI_UINT32 * pPdata, size_t pdataCount)
{
    std::vector<unsigned char> image(0x3000);
    unsigned char * p = &image[0];
    p[0] = 'M'; p[1] = 'Z';
    put32(p, 0x3c, 0x40);
    p[0x40] = 'P'; p[0x41] = 'E';
    // file header
    put16(p, 0x44, 0x8664);            // AMD64
    put16(p, 0x46, 2);                 // sections
    put16(p, 0x54, 0xF0);              // SizeOfOptionalHeader
    put16(p, 0x56, 0x2022);            // executable, large address aware, DLL
    // optional header
    const size_t o = 0x58;
    put16(p, o + 0, 0x20B);
    put32(p, o + 16, 0x1000);          // AddressOfEntryPoint
    put32(p, o + 20, 0x1000);          // BaseOfCode
    put32(p, o + 28, 0x1);             // ImageBase 0x100000000
    put32(p, o + 32, 0x1000);          // SectionAlignment
    put32(p, o + 36, 0x200);           // FileAlignment
    put16(p, o + 40, 6);               // OS version
    put16(p, o + 48, 6);               // subsystem version
    put32(p, o + 56, 0x3000);          // SizeOfImage
    put32(p, o + 60, 0x200);           // SizeOfHeaders
    put16(p, o + 68, 3);               // console
    put32(p, o + 108, 16);             // NumberOfRvaAndSizes
    put32(p, o + 112 + 3 * 8, 0x2000); // exception directory
    put32(p, o + 112 + 3 * 8 + 4, (DI_UINT32)(pdataCount * 12));
    // sections
    size_t s = o + 0xF0;
    const char * names[] = {".text", ".pdata"};
    const DI_UINT32 rvas[] = {0x1000, 0x2000};
    const DI_UINT32 flags[] = {0x60000020, 0x40000040};
    for (int i = 0; i < 2; ++i, s += 40)
    {
        memcpy(p + s, names[i], strlen(names[i]));
        put32(p, s + 8, 0x1000);       // VirtualSize
        put32(p, s + 12, rvas[i]);
        put32(p, s + 16, 0x200);       // SizeOfRawData
        put32(p, s + 20, rvas[i]);     // PointerToRawData
        put32(p, s + 36, flags[i]);
    }
    memcpy(p + 0x1000, pCode, codeSize);
    for (size_t i = 0; i < pdataCount * 3; ++i)
        put32(p, 0x2000 + i * 4, pPdata[i]);
    return image;
}

// .pdata lists every function with unwind data: a function nothing calls is still analyzed, as a root
static void test_analyzer_pdata_x64()
{
    unsigned char code[0x20] = {  0xc3 };                            // entry: ret            :0x1000
    code[0x10] = 0x31; code[0x11] = 0xc0;                            // xor eax, eax          :0x1010
    code[0x12] = 0xc3;                                               // ret                   :0x1012
    const DI_UINT32 pdata[] = {0x1000, 0x1001, 0, 0x1010, 0x1013, 0};
    std::vector<unsigned char> image = MakePe64(code, sizeof(code), pdata, 2);

    Diana_InstructionsOwner owner;
    DIANA_TEST_ASSERT(Diana_PE_AnalyzePEInMemory(&image[0], image.size(), &owner, 0) == DI_SUCCESS);
    DIANA_TEST_ASSERT(owner.m_ppPresenceVec[0x1000] != 0);
    Diana_Instruction * pFunction = owner.m_ppPresenceVec[0x1010];
    DIANA_TEST_ASSERT(pFunction && !(pFunction->m_flags & DI_INSTRUCTION_INVALID));
    DIANA_TEST_ASSERT(pFunction && (pFunction->m_flags & DI_INSTRUCTION_ROOT));
    DIANA_TEST_ASSERT(owner.m_ppPresenceVec[0x1012] != 0);
    Diana_InstructionsOwner_Free(&owner);
}

// x86: the scan starts at every byte no route reached
static void test_analyzer_scan_through_x86()
{
    unsigned char code[] = {  0xc3          // ret: the root route         :0
                            , 0x31, 0xc0    // xor eax, eax: nothing calls :1
                            , 0xc3          // ret                         :3
                            };
    AnalyzedCode analyzed(code, sizeof(code), DIANA_MODE64);
    DIANA_TEST_ASSERT(analyzed.owner.m_actualSize == 1);
    DianaAnalyzeParams params;
    DianaAnalyzeParams_Init(&params, DIANA_ARCH_X86);
    analyzed.AnalyzeScanThrough(DIANA_MODE64, &params);
    DIANA_TEST_ASSERT(analyzed.At(1) && analyzed.At(3));
}

void test_analyze()
{
    DIANA_TEST(test_analyzer1());
    DIANA_TEST(test_analyzer2());
    DIANA_TEST(test_analyzer3());
    DIANA_TEST(test_analyzer_data_access_x64());
    DIANA_TEST(test_analyzer_data_access_x86());
    DIANA_TEST(test_analyzer_address_taken_data());
    DIANA_TEST(test_analyzer_address_taken_code());
    DIANA_TEST(test_analyzer_address_taken_external());
    DIANA_TEST(test_analyzer_pdata_x64());
    DIANA_TEST(test_analyzer_scan_through_x86());
}