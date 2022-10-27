#include "test_fpu_mmx32.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"


static void test_fpu_mmx32_impl()
{
    int iRes = 0;
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;

    static unsigned char mov00[] = {0x0f, 0xae, 0xc0};//           fxsave  eax
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, mov00, sizeof(mov00), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fxsave")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
    } 

    static unsigned char mov01[] = {0xf3, 0x36, 0x66, 0x44};//           rep     inc sp
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, mov01, sizeof(mov01), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iRexPrefix == 0);
        DIANA_TEST_ASSERT(result.iPrefix == DI_PREFIX_REP);
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "inc")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_SP);
    } 

    static unsigned char mov02[] = {0x26, 0x66, 0x0f, 0x66, 0x44, 0x0f, 0x66};//           pcmpgtd xmm0,oword ptr es:[edi+ecx+0x66]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, mov02, sizeof(mov02), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pcmpgtd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_ES);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EDI);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x66);
    } 

    static unsigned char mov03[] = {0x67, 0xf3, 0x0f, 0x6f, 0xf1};       // movdqu  xmm6,xmm1  
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, mov03, sizeof(mov03), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movdqu")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM6);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM1);
    }

    static unsigned char mov04[] = {0xde, 0xe7};  // fsubrp  st(7),st 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, mov04, sizeof(mov04), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fsubrp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_fpu_ST7);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_fpu_ST0);
    }

    static unsigned char mov05[] = {0x67, 0xd8, 0x2f};  //    fsubr   dword ptr [bx]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, mov05, sizeof(mov05), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fsubr")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_BX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
    }

    static unsigned char data[] = {0xdf, 0xe0};  //    fstsw ax
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, data, sizeof(data), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fnstsw")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_AX);
    }

    static unsigned char data2[] = {0x66, 0x0f, 0x6e, 0xc3};  // movd xmm0,ebx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, data2, sizeof(data2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EBX);
    }

    static unsigned char data3[] = {0x66, 0x0f, 0x6e, 0xc3};  // movd mm0,ebx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, data3, sizeof(data3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EBX);
    }
 
    static unsigned char data4[] = {0x0f,0x6e, 0xc3};  // movd mm0,ebx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, data4, sizeof(data4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_MM0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EBX);
    }

    static unsigned char data5[] = {0xD8, 0xD1};  // fcom
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, data5, sizeof(data5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fcom")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_fpu_ST1);
    }
}

static void test_xmm()
{
    int iRes = 0;
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;

    static unsigned char mov00[] = { 0x66, 0x0F, 0x6E, 0xC0 };  //           movd    xmm0, eax
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, mov00, sizeof(mov00), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movd") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EAX);
    }

    static unsigned char mov01[] = { 0x66, 0x0F, 0x70, 0xC0, 0x00 };  //           pshufd    xmm0, xmm0, 0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, mov01, sizeof(mov01), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pshufd") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM0);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0);
    }
}

void test_fpu_mmx32()
{
    DIANA_TEST(test_xmm());
    DIANA_TEST(test_fpu_mmx32_impl());
}