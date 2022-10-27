#include "test_x64.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

static void test_x64_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    
    int iRes = 0;

    static unsigned char mov0[] = {0x0f, 0xae, 0x59, 0x58}; //    stmxcsr dword ptr [rcx+0x58]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov0, sizeof(mov0), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "stmxcsr")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RCX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x58);
    }
    
    static unsigned char mov[] = {0x45, 0x8b, 0xc8};          // mov     r9d,r8d
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov, sizeof(mov), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_R9D);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_R8D);
    }

    
    static unsigned char mov2[] = {0x48, 0x8b, 0x49, 0x08};          // mov     rcx,[rcx+0x8]
     
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov2, sizeof(mov2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RCX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RCX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 8);

    }
    
    static unsigned char mov3[] = {0x66, 0x8b, 0x11};          // mov     dx,[rcx]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov3, sizeof(mov3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RCX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
    }

    static unsigned char mov4[] = {0xf0, 0x49, 0x0f, 0xba, 0x28, 0x00};          // lock    bts qword ptr [r8],0x0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov4, sizeof(mov4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iPrefix == DI_PREFIX_LOCK);
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bts")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_R8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0);
    }

    static unsigned char mov5[] = {0xf7, 0x04, 0x24, 0x00, 0x02, 0x00, 0x00};          // test    dword ptr [rsp],0x200
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov5, sizeof(mov5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "test")==0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RSP);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x200);
    }


    static unsigned char mov6[] = {0x48, 0x8d, 0x4d, 0x20};          // lea     rcx,[rbp+0x20]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov6, sizeof(mov6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lea")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RCX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x20);
    }


    static unsigned char mov7[] = {0x8a, 0x85, 0x20, 0x01, 0x00, 0x00};  // mov     al,[rbp+0x120]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov7, sizeof(mov7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_AL);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x120);
    }

    static unsigned char mov8[] = {0x48, 0x89, 0x6c, 0x24, 0x10};  // mov     [rsp+0x10],rbp
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov8, sizeof(mov8), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_RBP);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RSP);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x10);
    }

    static unsigned char mov9[] = {0x4d, 0x89, 0x5c, 0xd1, 0x28};  // mov     [r9+rdx*8+0x28],r11
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov9, sizeof(mov9), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_R9);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_RDX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x28);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_R11);
    }


    static unsigned char mov10[] = {0x8d, 0x4c, 0x6d, 0x00};  // lea     ecx,[rbp+rbp*2]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov10, sizeof(mov10), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lea")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_RBP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
    }

    static unsigned char mov11[] = {0x48, 0xb8, 0x00, 0xf0, 0xff, 0xff, 0xff, 0x0f, 0x00, 0x00};  // mov rax,0xffffffff000
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov11, sizeof(mov11), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0xffffffff000ULL);
    }

    
    static unsigned char mov12[] = {0x66, 0x8b, 0x8c, 0x24, 0x88, 0x00, 0x00, 0x00};  // mov     cx,[rsp+0x88]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov12, sizeof(mov12), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_CX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RSP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x88);
    }
 

    static unsigned char mov13[] = {0x66, 0x41, 0x3b, 0xc9};  // cmp     cx,r9w
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov13, sizeof(mov13), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cmp")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_CX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_R9W);    
    }

        
    static unsigned char mov14[] = {0x48, 0x81, 0xec, 0xc0, 0x01, 0x00, 0x00}; //    sub     rsp,0x1c0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov14, sizeof(mov14), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "sub")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RSP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x1c0);    
    }
    

    static unsigned char mov15[] = {0x48, 0x8b, 0x05, 0x11, 0xc6, 0xdc, 0xff}; //    mov rax,[nt!MmUserProbeAddress (fffff80002af4000)]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov15, sizeof(mov15), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RIP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == -2308591);
    }
 

    // test default operand size commands
    static unsigned char mov16[] = {0x57}; //    push    rdi
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov16, sizeof(mov16), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RDI);
    }

    // test default operand size commands
    static unsigned char mov17[] = {0x66, 0x57}; //    push    di
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov17, sizeof(mov17), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "push")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DI);
    }

    // test default operand size commands
    static unsigned char mov18[] = {0x48, 0x0f, 0x43, 0xc8}; //    cmovae  rcx,rax
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov18, sizeof(mov18), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cmovae")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RCX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_RAX);
    }
 
    // test FPU
    static unsigned char mov19[] = {0xdd, 0xa0, 0x09, 0xc8, 0x00, 0x1c}; //    dda009c8001c     frstor  [rax+0x1c00c809]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov19, sizeof(mov19), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "frstor")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1c00c809);
    }

    
    static unsigned char mov20[] = {0xd8, 0xc7}; //    d8c7             fadd    st,st(7)
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov20, sizeof(mov20), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fadd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_fpu_ST0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_fpu_ST7);
    }


    static unsigned char mov21[] = {0xd8, 0x28}; //    d828             fsubr   dword ptr [rax]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov21, sizeof(mov21), Diana_GetRootLine(), &result, &read);
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
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
    }
   
    // test MMX
    static unsigned char mov22[] = {0xf3, 0x0f, 0x6f, 0x06}; //    movdqu  xmm0,oword ptr [rsi]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov22, sizeof(mov22), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movdqu")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RSI);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
    }
 
    static unsigned char mov23[] = {0x4f, 0xf3, 0x0f, 0x6f, 0xf1};  // movdqu  xmm6,xmm1 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov23, sizeof(mov23), Diana_GetRootLine(), &result, &read);
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


    static unsigned char mov24[] = {0x66, 0x0f, 0x2a, 0xdf};  // cvtpi2pd xmm3,mm7
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov24, sizeof(mov24), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cvtpi2pd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM3);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_MM7);
    }


    static unsigned char mov25[] = {0x66, 0x0f, 0x50, 0xdf}; //         movmskpd bx,xmm7
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov25, sizeof(mov25), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movmskpd")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EBX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM7);
    }

   
    static unsigned char mov26[] = {0xf3, 0x0f, 0x7f, 0x84, 0x24, 0x88, 0x00, 0x00, 0x00};//    movdqu oword ptr [rsp+0x88],xmm0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov26, sizeof(mov26), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movdqu")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RSP);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x88);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM0);
    }
    
 
    static unsigned char mov27[] = {0x4a, 0x0f, 0xae, 0x5c, 0x58, 0xd9};//     stmxcsr dword ptr [rax+r11*2-0x27]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov27, sizeof(mov27), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "stmxcsr")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_R11);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == -0x27);
    }


    static unsigned char mov28[] = {0x66, 0x44, 0x0f, 0x7f, 0x81, 0x80, 0x00, 0x00, 0x00};//     movdqa oword ptr [rcx+0x80],xmm8
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov28, sizeof(mov28), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movdqa")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RCX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x80);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM8);
    }

  
    static unsigned char mov29[] = {0xf3, 0x36, 0x66, 0x44, 0x0f, 0x7f, 
                                    0x89, 0x90, 0x00, 0x00, 0x00};//     rep  movdqa oword ptr ss:[rcx+0x90],xmm9
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov29, sizeof(mov29), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movdqa")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_SS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RCX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x90);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM9);
    } 

    static unsigned char mov30[] = {0x48, 0x8b, 0xc4};//     mov     rax,rsp
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, mov30, sizeof(mov30), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_RSP);
    } 
    //fffff800`028c3306 66660f1f840000000000 nop

    static unsigned char mov31[] = {0x48, 0x8b, 0x22, 0, 0,0,0,0};//     mov     rax,[rdx]
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE64, mov31, sizeof(mov31), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RSP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RDX);
    } 

    static unsigned char mov32[] = {0x48, 0xBB, 0x11, 0x11, 0x11, 0x11, 0x01, 0x00, 0x00, 0x00}; // mov     rbx, 111111111h
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE64, mov32, sizeof(mov32), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mov")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RBX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x111111111);
    } 

    static unsigned char mov33[] = {0x6e}; // outsb
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE64, mov33, sizeof(mov33), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "outs")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RSI);
    } 

    static unsigned char mov34[] = {0xf, 0xbf, 0xc0}; // movsx eax, ax
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE32, mov34, sizeof(mov34), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movsx")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_AX);
    } 

    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE64, mov34, sizeof(mov34), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movsx")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_AX);
    } 


    static unsigned char mov35[] = {0x45, 0x0f, 0xbf, 0xc0}; // movsx   r8d,r8w
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE64, mov35, sizeof(mov35), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movsx")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_R8D);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_R8W);
    } 

    static unsigned char mov36[] = {0x0f, 0x00, 0xc2}; // sldt    edx
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE64, mov36, sizeof(mov36), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "sldt")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EDX);
    } 

    static unsigned char mov37[] = {0x0F, 0xAA}; // rsm
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE64, mov37, sizeof(mov37), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "rsm")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }

    static unsigned char mov38[] = {0x66, 0x0F, 0x6F, 0x00}; // movdqa
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE32, mov38, sizeof(mov38), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movdqa")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
    }
} 

void test_x64()
{
    DIANA_TEST(test_x64_impl());
}