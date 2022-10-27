#include "test_imul.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

unsigned char imul[] = {0x0F, 0xAF, 0xFE};                         // imul        edi,esi 
unsigned char imul1[] = {0x6B, 0x7D, 0xF8, 0x0F};                  // imul        edi,dword ptr [c],0Fh 
unsigned char imul2[] = {0x69, 0x7D, 0xF8, 0x78, 0x56, 0x34, 0x12};// imul        edi,dword ptr [c],12345678h 

static int test_imul_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    int 

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,imul, sizeof(imul), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "imul")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EDI);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_ESI);
    }
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,imul1, sizeof(imul1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "imul")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EDI);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == -8);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0xF);
    }
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,imul2, sizeof(imul2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "imul")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EDI);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == -8);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0x12345678);
    }

    return 0;
}

void test_imul()
{
    DIANA_TEST(test_imul_impl());
}