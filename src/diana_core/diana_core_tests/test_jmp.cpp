#include "test_jmp.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

static unsigned char jmp[]= {0xEB, 0x00}; // jmp $+0
static unsigned char jmp1[]= {0xE9, 0x00,0x00,0x00,0x00}; // jmp $+0
static unsigned char jmp2[]= {0x66, 0xE9, 0x00,0x00}; // jmp $+0
static unsigned char jmp3[]= {0x67, 0xE9, 0x00,0x00,0x00,0x00}; // jmp $+0
static unsigned char jmp4[] = {0xff, 0x20};     //jmp         dword ptr [eax] 
static unsigned char jmp5[] = {0xff, 0x28};     //jmp         fword ptr [eax] 
static unsigned char jmp6[] = {0xea, 0x01, 0x02,0x03,0x04,0x05, 0x06};     //jmp         0605:04030201 


static void test_jmp_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    int 

    //static unsigned char jmp[]= {0xEB, 0x00}; // jmp $+0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,jmp, sizeof(jmp), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jmp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 1);
    }

    //static unsigned char jmp1[]= {0xE9, 0x00,0x00,0x00,0x00}; // jmp $+0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,jmp1, sizeof(jmp1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jmp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 4);
    }

    //static unsigned char jmp2[]= {0x66, 0xE9, 0x00,0x00}; // jmp $+0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,jmp2, sizeof(jmp2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jmp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 2);
    }

    //static unsigned char jmp3[]= {0x67, 0xE9, 0x00,0x00,0x00,0x00}; // jmp $+0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,jmp3, sizeof(jmp3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jmp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 4);
    }
    
    //static unsigned char jmp4[] = {0xff, 0x20};     //jmp         dword ptr [eax] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,jmp4, sizeof(jmp4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jmp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);

    }
    //static unsigned char jmp5[] = {0xff, 0x28};     //jmp         fword ptr [eax] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,jmp5, sizeof(jmp5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jmp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispValue == 0);
    }
    //static unsigned char jmp6[] = {0xea, 0x01, 0x02,0x03,0x04,0x05, 0x06};     //jmp         0605:04030201 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,jmp6, sizeof(jmp6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jmp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_call_ptr);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_segment == 0x0605);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_address == 0x04030201);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_segment_size == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_address_size == 4);
    }
}

void test_jmp()
{
    DIANA_TEST(test_jmp_impl());
}

