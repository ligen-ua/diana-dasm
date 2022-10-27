#include "test_cmp.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

unsigned char cmp[] = {0x3B, 0xF4};       //          cmp         esi,esp 

static int test_cmp1()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    int 

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,cmp, sizeof(cmp), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cmp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ESI);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_ESP);
    }

    return 0;
}


void test_cmpxchg8b()
{
    // f00fc74d00       lock    cmpxchg8b qword ptr [ebp]
    unsigned char code[] = {0xf0, 0x0f, 0xc7, 0x4d, 0x00};       
   
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    
    int iRes = 0;

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, code, sizeof(code), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cmpxchg16b")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedAddressSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
    }

}

int test_cmp()
{
    DIANA_TEST(test_cmp1());
    DIANA_TEST(test_cmpxchg8b());
    return 0;
}