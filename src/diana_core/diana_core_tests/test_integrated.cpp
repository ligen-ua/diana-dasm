#include "test_integrated.h"

extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"
/* TEST 1 
8195e9d7 f00fb116         lock    cmpxchg [esi],edx
81881dc5 c1e902           shr     ecx,0x2
81881db0 f3a5             rep     movsd
*/
void test_integrated()
{
    unsigned char buf[] = {0xf0, 0x0f, 0xb1, 0x16,
                           0xc1, 0xe9, 0x02,
                           0xf3, 0xa5};

    DianaParserResult result;
    DianaGroupInfo * pGroupInfo = 0;
    size_t cmdSize = 0;
    int iRes = 0;
        
    DianaMemoryStream stream;
    DianaContext context;

    Diana_InitContext(&context, DIANA_MODE32);
    Diana_InitMemoryStream(&stream, buf, sizeof(buf));

    // 1
    iRes = Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent,  &result);
    cmdSize = stream.curSize - context.cacheSize;

    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cmpxchg")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_ESI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EDX);
        DIANA_TEST_ASSERT(result.iPrefix == DI_PREFIX_LOCK);
   }

    // 2
    iRes = Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent,  &result);
    cmdSize = stream.curSize - context.cacheSize;

    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "shr")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 2);
        DIANA_TEST_ASSERT(result.iPrefix == 0);
   }

   // 3
   iRes = Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent,  &result);
   cmdSize = stream.curSize - context.cacheSize;

   DIANA_TEST_ASSERT_IF(!iRes)
   {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movs")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_reserved_reg);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_reserved_reg);
        DIANA_TEST_ASSERT(result.iPrefix == DI_PREFIX_REP);
   }

   iRes = Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent,  &result);
   DIANA_TEST_ASSERT(iRes == DI_END);
}



static const TestEntry_Name g_Names[] = {
{"mov",  2,  0}, //    mov     ecx,eax             ; 0
{"inc",  1,  0}, //    inc     edx
{"xadd",  4,  0}, //    lock    xadd [ecx],edx
{"cmp",  3,  0}, //    cmp     byte ptr [ebx],0x0
{"jne",  2,  0}, //    jne     nt!IoAcquireRemoveLockEx+0x85 (8266d525)
{"cmp",  4,  0}, //    cmp     dword ptr [ebp+0x18],0x58
{"jne",  2,  0}, //    jne     nt!IoAcquireRemoveLockEx+0x81 (8266d521)
{"push",  1,  0}, //    push    esi
{"push",  3,  0}, //    push    dword ptr [ebx+0x28]
{"push",  2,  0}, //    push    0x18
{"push",  2,  0}, //    push    0x0                 ; 10
{"call",  5,  0}, //    call    nt!ExAllocatePoolWithTag (82721bab)
{"mov",  2,  0}, //    mov     esi,eax
{"xor",  2,  0}, //    xor     eax,eax
{"test",  2,  0}, //    test    esi,esi
{"jne",  2,  0}, //    jne     nt!IoAcquireRemoveLockEx+0x3f (8266d4df)
{"add",  3,  0}, //    add     ebx,0x38
{"inc",  1,  0}, //    inc     eax
{"xadd",  4,  0}, //    lock    xadd [ebx],eax
{"jmp",  2,  0}, //    jmp     nt!IoAcquireRemoveLockEx+0x80 (8266d520)
{"push",  1,  0}, //    push    edi
{"push",  2,  0}, //    push    0x6
{"pop",  1,  0}, //    pop     ecx
{"mov",  2,  0}, //    mov     edi,esi
{"stos",  2,  0}, //    rep     stosd
{"mov",  3,  0}, //    mov     eax,[ebp+0xc]
{"mov",  3,  0}, //    mov     [esi+0x4],eax
{"mov",  3,  0}, //    mov     eax,[ebp+0x10]
{"mov",  3,  0}, //    mov     [esi+0x10],eax
{"mov",  3,  0}, //    mov     eax,[ebp+0x14]
{"mov",  3,  0}, //    mov     [esi+0x14],eax
{"lea",  3,  0}, //    lea     eax,[esi+0x8]
{"push",  1,  0}, //    push    eax
{"call",  5,  0}, //    call    nt!KeQueryTickCount (8266d54a)
{"lea",  3,  0}, //    lea     edi,[ebx+0x34]
{"mov",  2,  0}, //    mov     ecx,edi
{"call",  6,  0}, //    call    dword ptr [nt+0x1188 (82603188)]
{"mov",  3,  0}, //    mov     ecx,[ebx+0x50]
{"mov",  2,  0}, //    mov     [esi],ecx
{"mov",  2,  0}, //    mov     dl,al
{"mov",  2,  0}, //    mov     ecx,edi
{"mov",  3,  0}, //    mov     [ebx+0x50],esi
{"call",  6,  0}, //    call    dword ptr [nt+0x1184 (82603184)]
{"pop",  1,  0}, //    pop     edi
{"pop",  1,  0}, //    pop     esi
{"xor",  2,  0}, //    xor     eax,eax
{"jmp",  2,  0}, //    jmp     nt!IoAcquireRemoveLockEx+0xa0 (8266d540)
{"or",  3,  0}, //    or      ecx,0xffffffff
{"xadd",  4,  0}, //    lock    xadd [eax],ecx
{"jne",  2,  0}, //    jne     nt!IoAcquireRemoveLockEx+0x9b (8266d53b)
{"push",  2,  0}, //    push    0x0
{"push",  2,  0}, //    push    0x0
{"add",  3,  0}, //    add     ebx,0x8
{"push",  1,  0}, //    push    ebx
{"call",  5,  0}, //    call    nt!KeSetEvent (826696c5)
{"mov",  5,  0}, //    mov     eax,0xc0000056
{"pop",  1,  0}, //    pop     ebx
{"pop",  1,  0}, //    pop     ebp
{"ret",  3,  0}, //    ret     0x14
{"int",  1,  DI_FLAG_CMD_PRIVILEGED}, //    int     3
{"rdtsc",  2,  DI_FLAG_CMD_PRIVILEGED}, //    RDTSC 
{"rdtscp",  3,  DI_FLAG_CMD_PRIVILEGED}, //    RDTSCP
{"rdpmc",  2,  DI_FLAG_CMD_PRIVILEGED}, //    RDPMC 
{"rdmsr",  2,  DI_FLAG_CMD_PRIVILEGED}, //    RDMSR
{"wrmsr",  2,  DI_FLAG_CMD_PRIVILEGED}, //    WRMSR
{"", 0}};
static const unsigned char g_Data[] = {
 0x8b, 0xc8//    mov     ecx,eax
, 0x42//    inc     edx
, 0xf0, 0x0f, 0xc1, 0x11//    lock    xadd [ecx],edx
, 0x80, 0x3b, 0x00//    cmp     byte ptr [ebx],0x0
, 0x75, 0x6b//    jne     nt!IoAcquireRemoveLockEx+0x85 (8266d525)
, 0x83, 0x7d, 0x18, 0x58//    cmp     dword ptr [ebp+0x18],0x58
, 0x75, 0x61//    jne     nt!IoAcquireRemoveLockEx+0x81 (8266d521)
, 0x56//    push    esi
, 0xff, 0x73, 0x28//    push    dword ptr [ebx+0x28]
, 0x6a, 0x18//    push    0x18
, 0x6a, 0x00//    push    0x0
, 0xe8, 0xde, 0x46, 0x0b, 0x00//    call    nt!ExAllocatePoolWithTag (82721bab)
, 0x8b, 0xf0//    mov     esi,eax
, 0x33, 0xc0//    xor     eax,eax
, 0x85, 0xf6//    test    esi,esi
, 0x75, 0x0a//    jne     nt!IoAcquireRemoveLockEx+0x3f (8266d4df)
, 0x83, 0xc3, 0x38//    add     ebx,0x38
, 0x40//    inc     eax
, 0xf0, 0x0f, 0xc1, 0x03//    lock    xadd [ebx],eax
, 0xeb, 0x41//    jmp     nt!IoAcquireRemoveLockEx+0x80 (8266d520)
, 0x57//    push    edi
, 0x6a, 0x06//    push    0x6
, 0x59//    pop     ecx
, 0x8b, 0xfe//    mov     edi,esi
, 0xf3, 0xab//    rep     stosd
, 0x8b, 0x45, 0x0c//    mov     eax,[ebp+0xc]
, 0x89, 0x46, 0x04//    mov     [esi+0x4],eax
, 0x8b, 0x45, 0x10//    mov     eax,[ebp+0x10]
, 0x89, 0x46, 0x10//    mov     [esi+0x10],eax
, 0x8b, 0x45, 0x14//    mov     eax,[ebp+0x14]
, 0x89, 0x46, 0x14//    mov     [esi+0x14],eax
, 0x8d, 0x46, 0x08//    lea     eax,[esi+0x8]
, 0x50//    push    eax
, 0xe8, 0x48, 0x00, 0x00, 0x00//    call    nt!KeQueryTickCount (8266d54a)
, 0x8d, 0x7b, 0x34//    lea     edi,[ebx+0x34]
, 0x8b, 0xcf//    mov     ecx,edi
, 0xff, 0x15, 0x88, 0x31, 0x60, 0x82//    call    dword ptr [nt+0x1188 (82603188)]
, 0x8b, 0x4b, 0x50//    mov     ecx,[ebx+0x50]
, 0x89, 0x0e//    mov     [esi],ecx
, 0x8a, 0xd0//    mov     dl,al
, 0x8b, 0xcf//    mov     ecx,edi
, 0x89, 0x73, 0x50//    mov     [ebx+0x50],esi
, 0xff, 0x15, 0x84, 0x31, 0x60, 0x82//    call    dword ptr [nt+0x1184 (82603184)]
, 0x5f//    pop     edi
, 0x5e//    pop     esi
, 0x33, 0xc0//    xor     eax,eax
, 0xeb, 0x1b//    jmp     nt!IoAcquireRemoveLockEx+0xa0 (8266d540)
, 0x83, 0xc9, 0xff//    or      ecx,0xffffffff
, 0xf0, 0x0f, 0xc1, 0x08//    lock    xadd [eax],ecx
, 0x75, 0x0d//    jne     nt!IoAcquireRemoveLockEx+0x9b (8266d53b)
, 0x6a, 0x00//    push    0x0
, 0x6a, 0x00//    push    0x0
, 0x83, 0xc3, 0x08//    add     ebx,0x8
, 0x53//    push    ebx
, 0xe8, 0x8a, 0xc1, 0xff, 0xff//    call    nt!KeSetEvent (826696c5)
, 0xb8, 0x56, 0x00, 0x00, 0xc0//    mov     eax,0xc0000056
, 0x5b//    pop     ebx
, 0x5d//    pop     ebp
, 0xc2, 0x14, 0x00//    ret     0x14
, 0xcc //    int     3
, 0x0F, 0x31 // RDTSC 
, 0x0F, 0x01, 0xF9// RDTSCP 
, 0x0F, 0x33 //RDPMC 
, 0x0F, 0x32 //RDMSR 
, 0x0F, 0x30//WRMSR 
};

void test_x32_integrated_impl(const unsigned char * pData, 
                              size_t dataSize,
                              const TestEntry_Name * pNames)
{
    DianaParserResult result;
    
    int iRes = 0;

    DianaMemoryStream stream;
    DianaContext context;

    Diana_InitContext(&context, DIANA_MODE32);
    Diana_InitMemoryStream(&stream, (void*)pData, dataSize);

    // 1
    for(int i =0; ; ++i)
    {
        iRes = Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent,  &result);
        if (iRes == DI_END)
            break;

        if (iRes)
        {
            Diana_FatalBreak();
            break;
        }

        if( ( result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED ) != pNames[i].iPrivileged )
        {
            Diana_FatalBreak();
            break;
        }

        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        
        if (strcmp(pNames[i].pCmdName,  pGroupInfo->m_pName))
        {
            Diana_FatalBreak();
            break;
        }

        if (pNames[i].iCmdSize != result.iFullCmdSize)
        {
            Diana_FatalBreak();
        }
    }

}

void test_integrated2()
{
    DIANA_TEST(test_x32_integrated_impl(g_Data, sizeof(g_Data), g_Names));
}