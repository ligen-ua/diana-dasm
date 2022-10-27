#include "test_or.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

static unsigned char or0[]= {0x0C, 1};  // AL,imm8        2         OR immediate byte to AL
static unsigned char or1[]= {0x0D, 1,0,0,0};  // EAX,imm32      2         OR immediate dword to EAX
static unsigned char or2[]= {0x80, 0x08, 0x01};  // r/m8,imm8      2/7       OR immediate byte to r/m byte

static void test_or_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    //static unsigned char or[]= {0x0C, 1};  // AL,imm8        2         OR immediate byte to AL
    int 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,or0, sizeof(or0), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "or")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_AL);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 1);
    }
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,or1, sizeof(or1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "or")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 1);
    }
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,or2, sizeof(or2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "or")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 1);
    }
    
}

void test_or()
{
    DIANA_TEST(test_or_impl());
}