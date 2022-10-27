#include "test_mov_spec.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"


// IDA says:
static unsigned char mov[] = {0x0F, 0x20, 0xC0};        // mov     eax, cr0
static unsigned char mov1[] = {0x0F, 0x20, 0xFF&~7|1};  // mov     ecx, cr7
static unsigned char mov2[] = {0x0F, 0x22, 0xFF&~7|1};  // mov     cr7, ecx
static unsigned char mov3[] = {0x0F, 0x21, 0xFF&~7|1};  // mov     dr7, ecx
static unsigned char mov4[] = {0x0F, 0x24, 0xFF&~7|1};  // mov     ecx, tr7

static unsigned char mov5[] = {0x48, 0x63, 0x4C, 0x24, 0x04};  // movsxd  rcx, [rsp+4]
static unsigned char mov6[] = {0x66, 0x63, 0x68, 0x4c};  // movsxd  rbp,[rax+0x4c]
static unsigned char mov7[] = {0xf, 0x22, 0xf9};         // mov cr7, rcx

static unsigned char mov0_64[] = {0x45, 0x0F, 0x20, 0xC7};  // MOV r15,CR8

static void test_mov_spec_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    //static unsigned char mov[] = {0x0F, 0x20, 0xC0};  // MOV EAX,CR0
    int 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov, sizeof(mov), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
///        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_CR0);
    }

//static unsigned char mov0_64[] = {0x45, 0x0F, 0x20, 0xC7};  // MOV r15,CR8
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,mov0_64, sizeof(mov0_64), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
///        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_R15);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_CR8);
    }

    //static unsigned char mov1[] = {0x0F, 0x20, 0xFF&~7|1};  // mov     ecx, cr7
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov1, sizeof(mov1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
///        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_CR7);
    }

    //static unsigned char mov2[] = {0x0F, 0x22, 0xFF&~7|1};  // mov     cr7, ecx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov2, sizeof(mov2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
///        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_CR7);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_ECX);
    }
    //static unsigned char mov3[] = {0x0F, 0x21, 0xFF&~7|1};  // mov     ecx, dr7
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov3, sizeof(mov3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
///        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_DR7);
    }
    //static unsigned char mov4[] = {0x0F, 0x24, 0xFF&~7|1};  // mov     ecx, tr7
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov4, sizeof(mov4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
///        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_TR7);
    }

    //static unsigned char mov5[] = {0x48, 0x63, 0x4C, 0x24, 0x04};  // movsxd  rcx, [rsp+428h+var_424]
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE64,mov5, sizeof(mov5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movsxd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iFullCmdSize == sizeof(mov5));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RCX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RSP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0x00);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x01);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x0000000000000004);
    }

    // check arpl below, it has the same opcode, ooooo how I love you guys from intel :)
    // just skip prefix
    // 63 4C 24 04      arpl        word ptr [esp+4],cx 
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE32, mov5+1, sizeof(mov5)-1, Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "arpl")==0);
        DIANA_TEST_ASSERT(result.iFullCmdSize == sizeof(mov5)-1);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_ESP);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0x00);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0x01);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x0000000000000004);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_CX);
    }

    // very nice command, back to 64
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE64, mov6, sizeof(mov6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movsxd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iFullCmdSize == sizeof(mov6));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0x00);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x01);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x04C);
    }

    //static unsigned char mov7[] = {0xf, 0x22, 0xf9};         // mov cr7, rcx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,mov7, sizeof(mov7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
///        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_CR7);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_RCX);
    }
}

void test_mov_spec()
{
    DIANA_TEST(test_mov_spec_impl());
}