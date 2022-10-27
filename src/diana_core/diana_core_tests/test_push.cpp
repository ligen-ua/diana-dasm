#include "test_push.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

static unsigned char nop[] = {0x90};
static unsigned char nop2[] = {0x66, 0x67, 0x90};
static unsigned char push[] = {0x50};            //         push        eax  
static unsigned char push1[] = {0x51};           //         push        ecx  
static unsigned char push2[] = {0x52};           //         push        edx  
static unsigned char push3[] = {0x6A, 0x01};     //         push        1    
static unsigned char push4[] = {0xFF, 0x75, 0xFC};     //         push        ttt    
static unsigned char push5[]= {0xFF, 0x74, 0x71, 0x05};//      push        dword ptr [ecx+esi*2+5] 
static unsigned char push6[]= {0xFF, 0x34, 0x75, 0x05, 0x00, 0x00, 0x00}; // push        dword ptr [esi*2+5] 
static unsigned char push7[]= {0xFF, 0xB4, 0x71, 0xF6, 0x00, 0x00, 0x00}; // push        dword ptr [ecx+esi*2+0F6h] 
static unsigned char push8[]= {0x0E};//         PUSH CS       2        Push CS
static unsigned char push9[]= {0x16};//         PUSH SS       2        Push SS
static unsigned char push10[]= {0x1E};//         PUSH DS       2        Push DS
static unsigned char push11[]= {0x06};//         PUSH ES       2        Push ES
static unsigned char push12[]= {0x0F, 0xA0};//   PUSH FS       2        Push FS
static unsigned char push13[]= {0x0F, 0xA8};//   PUSH GS       2        Push GS
static unsigned char push14[]= {0x68,0x08,0xaf,0x98,0xbf};//   push    offset win32k!`string'+0x168 (bf98af08)


static int test_push_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    //unsigned char nop[1] = {0x90};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,nop, sizeof(nop), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==0);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "nop")==0);
    }

    // static unsigned char nop2[] = {0x66, 0x67, 0x90};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,nop2, sizeof(nop2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==0);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "nop")==0);
    }

    //unsigned char push[] = {0x50};            //         push        eax  
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push, sizeof(push), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
    }

    //unsigned char push1[] = {0x51};           //         push        ecx  
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push1, sizeof(push1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
    }

    //unsigned char push2[] = {0x52};           //         push        edx  
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push2, sizeof(push2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.recognizedRegister == reg_EDX);
    }

    //unsigned char push3[] = {0x6A, 0x01};     //         push        1    
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push3, sizeof(push3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_imm);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.imm == 1);
    }
        
    // unsigned char push4[] = {0xFF, 0x75, 0xFC};     //         push        ttt    
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push4, sizeof(push4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispValue == -4);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispSize == 0x1);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.index == 0);
    }

    //unsigned char push5[]= {0xFF, 0x74, 0x71, 0x05};//      push        dword ptr [ecx+esi*2+5] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push5, sizeof(push5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.reg == reg_ECX);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispValue == 0x5);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispSize == 0x1);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.indexed_reg == reg_ESI);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.index == 2);
    }

    //unsigned char push6[]= {0xFF, 0x34, 0x75, 0x05, 0x00, 0x00, 0x00}; // push        dword ptr [esi*2+5] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push6, sizeof(push6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.reg == reg_none);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispValue == 0x5);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.indexed_reg == reg_ESI);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.index == 2);
    }

    //unsigned char push7[]= {0xFF, 0xB4, 0x71, 0xF6, 0x00, 0x00, 0x00}; // push        dword ptr [ecx+esi*2+0F6h]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push7, sizeof(push7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.reg == reg_ECX);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispValue == 0xf6);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.indexed_reg == reg_ESI);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.index == 2);
    }
 
    //unsigned char push8[]= {0x0E};//         PUSH CS       2        Push CS
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push8, sizeof(push8), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_CS);
   }

    //unsigned char push9[]= {0x16};//         PUSH SS       2        Push SS
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push9, sizeof(push9), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_SS);
   }
    //unsigned char push10[]= {0x1E};//         PUSH DS       2        Push DS
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push10, sizeof(push10), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DS);
   }
   //unsigned char push11[]= {0x06};//         PUSH ES       2        Push ES
   iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push11, sizeof(push11), Diana_GetRootLine(), &result, &read);
   DIANA_TEST_ASSERT_IF(!iRes)
   {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ES);
   }
   //unsigned char push12[]= {0x0F, 0xA0};//   PUSH FS       2        Push FS
   iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push12, sizeof(push12), Diana_GetRootLine(), &result, &read);
   DIANA_TEST_ASSERT_IF(!iRes)
   {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_FS);
   }
   //unsigned char push13[]= {0x0F, 0xA8};//   PUSH GS       2        Push GS
   iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push13, sizeof(push13), Diana_GetRootLine(), &result, &read);
   DIANA_TEST_ASSERT_IF(!iRes)
   {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_GS);
   }

   //unsigned char push14[]= {0x68,0x08,0xaf,0x98,0xbf};//   push    offset win32k!`string'+0x168 (bf98af08)
   iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,push14, sizeof(push14), Diana_GetRootLine(), &result, &read);
   DIANA_TEST_ASSERT_IF(!iRes)
   {
       DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
       DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
       DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
       DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
       DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
       DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_imm);
       DIANA_TEST_ASSERT (result.linkedOperands[0].value.imm == 0xbf98af08);
   }
   return 0;
}

void test_push()
{
    DIANA_TEST(test_push_impl());
}