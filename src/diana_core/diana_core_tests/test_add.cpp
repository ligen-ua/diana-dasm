#include "test_add.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

static unsigned char add[] = {0x83, 0xC0, 0x08}; //          add         eax,8 
static unsigned char add1[] = {0x83, 0xC4, 0x10};//          add         esp,10h 
static unsigned char code33[] = {0x2e, 0x67, 0xf0, 0x48, 0x81, 0x84, 0x80, 0x23, 0xdf, 0x06, 0x7e, 0x89, 0xab, 0xcd, 0xef};
static unsigned char code52[] = {0xf0, 0x2e, 0x66, 0x67, 0x81, 0x84, 0x18, 0x67, 0x45, 0x23, 0x01, 0xef, 0xcd};
static unsigned char adc[]= {0x67, 0x45, 0x10, 0xe}; // adc byte ptr ds:[r14d] r9b
static unsigned char add2[]= {0x00, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00}; // add     [00000000],al

static int test_add_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    int 

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,add, sizeof(add), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "add")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 8);
    }
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,add1, sizeof(add1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "add")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ESP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x10);
    }

    // lock add qword cs:[eax + 4 * eax + 07e06df23h], 0efcdab89h
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,code33, sizeof(code33), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "add")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_CS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x07e06df23);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x0efcdab89);
    }
    // static unsigned char adc[]= {0x67, 0x45, 0x10, 0xe}; // adc byte ptr ds:[r14d] r9b
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,adc, sizeof(adc), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "adc")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_R14D);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);

        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_R9B);
    }
    
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,add2, sizeof(add2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "add")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_AL);
    }
    return 0;

}

void test_add()
{
    DIANA_TEST(test_add_impl());
}