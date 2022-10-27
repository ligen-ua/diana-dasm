#include "test_call.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

// not implemented yet
// 32 bit segment?

static unsigned char call[] = {0xE8, 0x83, 0xF9, 0xFF, 0xFF};      // call        @ILT+945(_main) (4113B6h) 
static unsigned char call1[] = {0x66, 0xE8, 0x83, 0xF9};  // call        000074C7 
static unsigned char call2[] = {0xFF, 0x55, 0x00};             // call        dword ptr [ebp] 
static unsigned char call3[] = {0x67, 0xFF, 0x55, 0x01};          // call        dword ptr [di+1] 
static unsigned char call4[] = {0x66, 0x9A, 0x8D, 0xBD, 0x40, 0xFF};    // call        FF40:BD8D
static unsigned char call5[] = {0x9A, 0x8D, 0xBD, 0x40, 0xFF, 0x00, 0x00}; // call        0000:FF40BD8D 
static unsigned char call6[] = {0xFF, 0x18}; // call        fword ptr [eax] 
static unsigned char call7[] = {0x3E, 0xFF, 0x5C, 0xBE, 0x01}; // call        fword ptr ds:[esi+edi*4+1] 
static unsigned char call8[] = {0xFF, 0xD0};     //          call        eax  
static unsigned char call9[] = {0x64, 0xFF, 0x15, 0xC0, 0x00, 0x00, 0x00}; // call        dword ptr fs:[0C0h] 

static void test_call_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    int 

    //static unsigned char call[] = {0xE8, 0x83, 0xF9, 0xFF, 0xFF};      // call        @ILT+945(_main) (4113B6h) 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call, sizeof(call), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == -0x67D);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 4);
    }

    //static unsigned char call1[] = {0x66, 0xE8, 0x83, 0xF9};  // call        000074C7 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call1, sizeof(call1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == -0x67D);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 2);
    }

    //static unsigned char call2[] = {0xFF, 0x55, 0x00};             // call        dword ptr [ebp] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call2, sizeof(call2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);

    }
    //static unsigned char call3[] = {0x67, 0xFF, 0x55, 0x01};          // call        dword ptr [di+1] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call3, sizeof(call3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_DI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 1);
    }
    //static unsigned char call4[] = {0x66, 0x9A, 0x8D, 0xBD, 0x40, 0xFF};    // call        FF40:BD8D
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call4, sizeof(call4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_call_ptr);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_segment == 0xFF40);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_address == 0xBD8D);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_segment_size == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_address_size == 2);
    }
    //static unsigned char call5[] = {0x9A, 0x8D, 0xBD, 0x40, 0xFF, 0x00, 0x00}; // call        0000:FF40BD8D 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call5, sizeof(call5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_call_ptr);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_segment == 0x0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_address == 0xFF40BD8D);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_segment_size == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.ptr.m_address_size == 4);
    }

    //static unsigned char call6[] = {0xFF, 0x18}; // call        fword ptr [eax] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call6, sizeof(call6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispValue == 0);
    }
    //static unsigned char call7[] = {0x3E, 0xFF, 0x5C, 0xBE, 0x01}; // call        fword ptr ds:[esi+edi*4+1] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call7, sizeof(call7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.reg == reg_ESI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.indexed_reg == reg_EDI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.index == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispValue == 1);
    }

    //static unsigned char call7[] = {0x3E, 0xFF, 0x5C, 0xBE, 0x01}; // call        fword ptr ds:[esi+edi*4+1] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call7, sizeof(call7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.reg == reg_ESI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.indexed_reg == reg_EDI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.index == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispValue == 1);
    }

    static unsigned char call8[] = {0xFF, 0xD0};     //          call        eax  
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call8, sizeof(call8), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister  == reg_EAX);
    }

    static unsigned char call9[] = {0x64, 0xFF, 0x15, 0xC0, 0x00, 0x00, 0x00}; // call        dword ptr fs:[0C0h] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,call9, sizeof(call9), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "call")==0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0xC0);
    }
}

void test_call()
{
    DIANA_TEST(test_call_impl());
}

