#include "test_push.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

static unsigned char mov[] = {0x8B, 0x85, 0x44, 0xFF, 0xFF, 0xFF}; //mov         eax,dword ptr [pLinkedOp] 
static unsigned char mov1[] = {0x8B, 0xF4};      //         mov         esi,esp  
static unsigned char mov2[] = {0x8B, 0x4D, 0x0C};//         mov         ecx,dword ptr [readStream]
static unsigned char mov3[] = {0x8A, 0x55, 0xD4};//         mov         dl,byte ptr [PostByte] 
static unsigned char mov4[] = {0x8A, 0x85, 0x2F, 0xFF, 0xFF, 0xFF};//  mov         al,byte ptr [opSizeUsed] 
static unsigned char mov5[] = {0x8B, 0x04, 0x85, 0x03, 0x00, 0x00, 0x00};//    mov eax, [4*eax+3]
static unsigned char mov6[] = {0x89, 0x74, 0x85, 0x03}; //      mov         dword ptr [ebp+eax*4+3],esi 
static unsigned char mov7[] = {0x88, 0xB4, 0xD5, 0x09, 0x03, 0x00, 0x00}; //mov         byte ptr [ebp+edx*8+309h],dh 
static unsigned char mov8[] = {0x66, 0x66, 0x8B, 0xD1}; // mov dx, cx
static unsigned char mov9[] = {0xB9, 0x78, 0x56, 0x34, 0x12}; // mov ecx, 0x12345678
static unsigned char mov10[] = {0xBD, 0x78, 0x56, 0x34, 0x12}; // mov ebp, 0x12345678
static unsigned char mov11[] = {0x66, 0x0f, 0xb7, 0x00}; //movzx ax,[word eax]
static unsigned char mov12[] = {0x66, 0x0f, 0xbf, 0x00}; //movsx ax,[word eax]
static unsigned char mov13[] = {0x67, 0x41, 0x63, 0x1b}; // :movsxd rbx dword ptr ds:[r11d]
static unsigned char mov14[] = {0x40, 0xb6, 0x01}; // mov sil,1
static unsigned char mov15[] = {0xb6, 0x01};             // mov     dh,1

static void test_mov_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    //unsigned char mov[] = {0x8B, 0x85, 0x44, 0xFF, 0xFF, 0xFF}; //mov         eax,dword ptr [pLinkedOp] 
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
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == -188);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
    }

    // unsigned char mov1[] = {0x8B, 0xF4};      //         mov         esi,esp  
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov1, sizeof(mov1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ESI);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_ESP);
    }

    //unsigned char mov2[] = {0x8B, 0x4D, 0x0C};//         mov         ecx,dword ptr [readStream]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov2, sizeof(mov2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x0C);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
    }

    //unsigned char mov3[] = {0x8A, 0x55, 0xD4};//         mov         dl,byte ptr [PostByte] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov3, sizeof(mov3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DL);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == -44);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
    }

    //unsigned char mov4[] = {0x8A, 0x85, 0x2F, 0xFF, 0xFF, 0xFF};//  mov         al,byte ptr [opSizeUsed] 
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
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == -209);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
    }

    //unsigned char mov5[] = {0x8B, 0x04, 0x85, 0x03, 0x00, 0x00, 0x00};//    __asm mov eax, [4*eax+3]
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
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x3);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 4);
    }

    //unsigned char mov6[] = {0x89, 0x74, 0x85, 0x03}; //      mov         dword ptr [ebp+eax*4+3],esi 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov6, sizeof(mov6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x3);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0x1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_ESI);
    }

    //unsigned char mov7[] = {0x88, 0xB4, 0xC5, 0x09, 0x03, 0x00, 0x00}; //mov         byte ptr [ebp+eax*8+309h],dh 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov7, sizeof(mov7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x309);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0x4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_EDX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_DH);
    }


     //unsigned char mov8[] = {0x66, 0x66, 0x8B, 0xD1}; // mov dx, cx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov8, sizeof(mov8), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_CX);
    }

   //unsigned char mov9[] = {0xB9, 0x78, 0x56, 0x34, 0x12}; // mov ecx, 0x12345678
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov9, sizeof(mov9), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x12345678);
    }

    //unsigned char mov10[] = {0xBD, 0x78, 0x56, 0x34, 0x12}; // mov ebp, 0x12345678
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov10, sizeof(mov10), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x12345678);
    }


    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov11, sizeof(mov11), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movzx")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_AX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,mov12, sizeof(mov12), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movsx")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_AX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
    }

    //static unsigned char mov13[] = {0x67, 0x41, 0x63, 0x1b}; // :movsxd rbx dword ptr ds:[r11d]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,mov13, sizeof(mov13), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movsxd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RBX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_R11D);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
    }

    //static unsigned char mov14[] = {0x40, 0xb6, 0x01}; // mov sil,1
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,mov14, sizeof(mov14), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_SIL);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 1);
    }

    //static unsigned char mov15[] = {0xb6, 0x01};             // mov     dh,1
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,mov15, sizeof(mov15), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DH);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 1);
    }
    

}
void test_mov()
{
    DIANA_TEST(test_mov_impl());
}