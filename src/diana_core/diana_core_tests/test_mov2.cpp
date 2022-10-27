#include "test_mov2.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

#include "diana_core_cpp.h"
static unsigned char mov[] = {0x66, 0x8C, 0xCC};          // mov         sp,cs 
static unsigned char mov1[] = {0x66, 0x8C, 0xD3};         // mov         bx,ss 
static unsigned char mov2[] = {0x3E, 0xA2, 0x34, 0x12, 0x00, 0x00}; // mov         byte ptr ds:[00001234h],al 
static unsigned char mov3[] = {0x36, 0xA3, 0x34, 0x12, 0x00, 0x00}; // mov         dword ptr ss:[00001234h],eax 
static unsigned char mov4[] = {0x3E, 0xA0, 0x34, 0x12, 0x00, 0x00}; // mov         al,byte ptr ds:[00001234h] 
static unsigned char mov5[] = {0x36, 0xA1, 0x34, 0x12, 0x00, 0x00}; // mov         eax,dword ptr ss:[00001234h] 
static unsigned char mov6[] = {0x2E, 0xC6, 0x02, 0x34};         // mov         byte ptr cs:[edx],34h 
static unsigned char mov7[] = {0x3E, 0xC6, 0x44, 0xBE, 0x01, 0x34}; // mov         byte ptr ds:[esi+edi*4+1],34h 
static unsigned char mov8[] = {0x8C, 0xC8};                // mov         eax,cs 
static unsigned char mov9[] = {0x0f, 0xb7, 0x00};// movzx eax, [word eax]
static unsigned char mov10[] = {0x67, 0x45, 0xf, 0x43, 0x18}; // cmovnb r11d dword ptr ds:[r8d]
static unsigned char mov11[] = {0x8E, 0xD6}; //    mov ss,si 

static void test_mov2_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    //static unsigned char mov[] = {0x66, 0x8C, 0xCC};          // mov         sp,cs
    int 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov, sizeof(mov), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_SP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_CS);
    }

    //static unsigned char mov1[] = {0x66, 0x8C, 0xD3};         // mov         bx,ss
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov1, sizeof(mov1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_BX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_SS);
    }

    //static unsigned char mov2[] = {0x3E, 0xA2, 0x34, 0x12, 0x00, 0x00}; // mov         byte ptr ds:[00001234h],al 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov2, sizeof(mov2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS)
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1234);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_AL);
    }
    //static unsigned char mov3[] = {0x36, 0xA3, 0x34, 0x12, 0x00, 0x00}; // mov         dword ptr ss:[00001234h],eax 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov3, sizeof(mov3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_SS)
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1234);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EAX);
    }

    //static unsigned char mov4[] = {0x3E, 0xA0, 0x34, 0x12, 0x00, 0x00}; // mov         al,byte ptr ds:[00001234h] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov4, sizeof(mov4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_AL);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS)
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x1234);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
    }

    //static unsigned char mov5[] = {0x36, 0xA1, 0x34, 0x12, 0x00, 0x00}; // mov         eax,dword ptr ss:[00001234h] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov5, sizeof(mov5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);    
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_SS)
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x1234);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
    }


    //static unsigned char mov6[] = {0x2E, 0xC6, 0x02, 0x34};         // mov         byte ptr cs:[edx],34h 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov6, sizeof(mov6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_CS)
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EDX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x34);
    }

    //static unsigned char mov7[] = {0x3E, 0xC6, 0x44, 0xBE, 0x01, 0x34}; // mov         byte ptr ds:[esi+edi*4+1],34h 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov7, sizeof(mov7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS)
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_ESI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_EDI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x34);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov8, sizeof(mov8), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);    
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_CS);

    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov9, sizeof(mov9), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movzx")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);    
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS)
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);
    }
    //static unsigned char mov10[] = {0x67, 0x45, 0xf, 0x43, 0x18}; // cmovnb r11d dword ptr ds:[r8d]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,mov10, sizeof(mov10), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cmovae")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_R11D);    
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS)
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_R8D);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }

    //static unsigned char mov11[] = {0x8E, 0xD6}; //    mov ss,si 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov11, sizeof(mov11), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_SS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_SI);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);    
    }
}

void test_mov2()
{
    DIANA_TEST(test_mov2_impl());
}