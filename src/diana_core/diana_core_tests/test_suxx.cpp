#include "test_suxx.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

static void test_suxx_impl()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char suxx1[] = {0x37};//        AAA            4         ASCII adjust AL after addition
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx1, sizeof(suxx1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aaa") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx2[] = {0xD5, 0x0A}; //     AAD            19        ASCII adjust AX before division
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx2, sizeof(suxx2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aad") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx3[] = {0xD4, 0x0A}; //     AAM            17        ASCII adjust AX after multiply
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx3, sizeof(suxx3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aam") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx4[] = {0x3F};//        AAS            4         ASCII adjust AL after subtraction
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx4, sizeof(suxx4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aas") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx5[] = {0x14, 0xFE};//       ADC AL,imm8      2         Add with carry immediate byte to AL
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx5, sizeof(suxx5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "adc") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_AL);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0xFE);
    }
    
    static unsigned char suxx6[] = {0x14, 0xFE};//       ADC AL,imm8      2         Add with carry immediate byte to AL
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx6, sizeof(suxx6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "adc") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_AL);

        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0xFE);
    }

    static unsigned char suxx7[] = {0x00,0x80,0x01,0x01,0x01,0x01}; //add         byte ptr [eax+1010101h],al };  //     ADD r/m8,r8          2/7      Add byte register to r/m byte
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx7, sizeof(suxx7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "add") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1010101);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_AL);
    }

    static unsigned char suxx8[] = {0x20,0x80,0x01,0x01,0x01,0x01}; //and         byte ptr [eax+1010101h],al };  //     ADD r/m8,r8          2/7      Add byte register to r/m byte
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx8, sizeof(suxx8), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "and") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1010101);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_AL);
    }

    static unsigned char suxx9[] = {0x63,0x80,0x01,0x01,0x01,0x01}; // arpl        word ptr [eax+1010101h],ax 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx9, sizeof(suxx9), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "arpl") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1010101);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_AX);
    }

    static unsigned char suxx10[] = {0x62,0x80,0x01,0x01,0x01,0x01}; // 62 /r     BOUND r32,m32        10        Check if r32 is within bounds
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx10, sizeof(suxx10), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bound") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispValue == 0x1010101);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }

    static unsigned char suxx11[] = {0x0F, 0xBC,0x80,0x01,0x01,0x01,0x01}; // 0F  BC    BSF r16,r/m16        10+3n     Bit scan forward on r/m word
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx11, sizeof(suxx11), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bsf") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x1010101);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }

    static unsigned char suxx12[] = {0x0F, 0xBD, 0x80, 0x01, 0x01, 0x01, 0x01}; // 0F  BC    BSR r16,r/m16        10Bit scan reverse on r/m word
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx12, sizeof(suxx12), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bsr") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x1010101);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }

    static unsigned char suxx13[] = {0x0F, 0xA3,0x80,0x01,0x01,0x01,0x01}; // BT r/m16,r16    3/12      Save bit in carry flag
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx13, sizeof(suxx13), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bt") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1010101);
    }

    static unsigned char suxx14[] = {0x0F, 0xBB,0x80,0x01,0x01,0x01,0x01}; // BTC r/m16,r16   6/13    Save bit in carry flag and complement
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx14, sizeof(suxx14), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "btc") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1010101);
    }

    static unsigned char suxx15[] = {0x0F, 0xB3,0x80,0x01,0x01,0x01,0x01}; // 0F  B3 /r     BTR r/m32,r32   6/13    Save bit in carry flag and reset
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx15, sizeof(suxx15), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "btr") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1010101);
    }

    static unsigned char suxx16[] = {0x0F, 0xAB, 0x80, 0x01, 0x01, 0x01, 0x01}; // 0F  AB /r     BTS r/m32,r32   6/13    Save bit in carry flag and set
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx16, sizeof(suxx16), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bts") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x1010101);
    }

    static unsigned char suxx17[] = {0x98};//        98        CBW             3               AX := sign-extend of AL
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx17, sizeof(suxx17), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cbw") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx18[] = {0xf8};//        F8        CLC             2               Clear carry flag
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx18, sizeof(suxx18), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "clc") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx19[] = {0xfc};//        FC        CLD             2        Clear direction flag; 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx19, sizeof(suxx19), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cld") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx20[] = {0xfa};//        FA        CLI            3        Clear interrupt flag; interrupts disabled
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx20, sizeof(suxx20), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cli") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx21[] = {0x0F, 0x06}; // CLTS           5        Clear task-switched flag
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx21, sizeof(suxx21), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "clts") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx22[] = {0xF5}; //        CMC            2        Complement carry flag
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx22, sizeof(suxx22), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cmc") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx23[] = {0xA6};//        CMPS m8,m8         10       Compare bytes ES:[(E)DI] (second
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx23, sizeof(suxx23), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cmps") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_reserved_reg);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_reserved_reg);
    }

    static unsigned char suxx24[] = {0x99};//        CWD                2        DX:AX := sign-extend of AX
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx24, sizeof(suxx24), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cwd") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }
    
    static unsigned char suxx25[] = {0x27};//        DAA                4        Decimal adjust AL after addition
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx25, sizeof(suxx25), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "daa") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char suxx26[] = {0x2F};//        DAS                4        Decimal adjust AL after subtraction
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx26, sizeof(suxx26), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "das") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }
    
    static unsigned char suxx27[] = {0xFF, 0xC8}; // dec eax
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx27, sizeof(suxx27), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "dec") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
    }

    static unsigned char suxx28[] = {0xF7,0xF1};//            div         eax,ecx 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx28, sizeof(suxx28), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "div") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
    }

    static unsigned char suxx29[] = {0xC8, 0x01, 0x00, 0x02};//      enter       1,2  
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx29, sizeof(suxx29), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "enter") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.imm == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 2);
    }

    static unsigned char suxx30[] = {0xF4};//      F4          HLT                5          Halt    
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx30, sizeof(suxx30), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "hlt") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }


    static unsigned char suxx31[] = {0xF7,0xF9};//            idiv         ecx 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx31, sizeof(suxx31), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "idiv") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
    }

    static unsigned char suxx32[] = {0xED};//            in         eax,dx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx32, sizeof(suxx32), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "in") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_DX);
    }

    static unsigned char suxx33[] = {0xE5, 0x05};//            in         eax, 5
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx33, sizeof(suxx33), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "in") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 5);
    }

    static unsigned char suxx34[] = {0xFF, 0xC0}; // FF  /0      INC r/m32                      Increment r/m dword by 1
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx34, sizeof(suxx34), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "inc") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
    }

    static unsigned char suxx35[] = {0x40}; // //40 + rw     INC r16                        Increment word register by 1
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx35, sizeof(suxx35), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "inc") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
    }

    static unsigned char suxx36[] = {0x6D};//            ins         edi, dx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx36, sizeof(suxx36), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ins") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_ES);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EDI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type  ==  diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_DX);
    }

    static unsigned char suxx37[] = {0xCC};//        INT 3        33              Interrupt 3--trap to debugger
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx37, sizeof(suxx37), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "int") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.imm == 3);
    }

    static unsigned char suxx38[] = {0xCE};//        CE        INTO         Fail:3,pm=3;
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx38, sizeof(suxx38), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "into") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }

    static unsigned char suxx39[] = {0xCD, 5};//        CD ib     INT imm8     37              Interrupt numbered by immediate
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx39, sizeof(suxx39), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "int") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.imm == 5);
    }

    static unsigned char suxx40[] = {0xCF};      //IRET         22,pm=38     Interrupt return (far return and pop
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx40, sizeof(suxx40), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "iret") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }

    static unsigned char suxx41[] = {0x70, 0xFF};      //JO rel8           7+m,3    Jump short if overflow (OF=1)
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx41, sizeof(suxx41), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jo") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == -1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 1);
    }
    
    static unsigned char suxx42[] = {0x0F, 0x80, 0x05, 0x00, 0x00, 0x00};      //0F  80 cw/cd   JO rel16/32       7+m,3    Jump near if overflow (OF=1)
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx42, sizeof(suxx42), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jo") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 5);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 4);
    }

    static unsigned char suxx43[] = {0x7F, 0xFF};      //JG rel8           7+m,3    Jump short if overflow (OF=1)
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx43, sizeof(suxx43), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jg") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == -1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 1);
    }
    
    static unsigned char suxx44[] = {0x0F, 0x8F, 0x05, 0x00, 0x00, 0x00};      //0F  8G cw/cd   JG rel16/32       7+m,3    Jump near if overflow (OF=1)
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx44, sizeof(suxx44), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "jg") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 5);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 4);
    }

    static unsigned char suxx45[] = {0x9F};//      LAHF          2        Load: AH := flags SF ZF xx AF xx PF xx CF
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx45, sizeof(suxx45), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lahf") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }

    static unsigned char suxx46[] = {0x0F, 0x02, 0x00};//         lar         eax,dword ptr [eax] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx46, sizeof(suxx46), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lar") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
    }

    static unsigned char suxx47[] = {0xc9};//      LEAVE        4       Set SP to BP, then pop BP
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx47, sizeof(suxx47), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "leave") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }

    static unsigned char suxx48[] = {0x0F, 0x01, 0x10};//         lgdt        fword ptr [eax] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx48, sizeof(suxx48), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lgdt") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
    }

    static unsigned char suxx49[] = {0x0F, 0x01, 0x18};//         lidt        fword ptr [eax] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx49, sizeof(suxx49), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lidt") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
    }

    static unsigned char suxx50[] = {0xC4, 0x00};//            les         eax,fword ptr [eax]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx50, sizeof(suxx50), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "les") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }

    static unsigned char suxx51[] = {0xC5, 0x00};//            lds         eax,fword ptr [eax] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx51, sizeof(suxx51), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lds") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }

    static unsigned char suxx52[] = {0x0F, 0xB5, 0x00};//         lgs         eax,fword ptr [eax]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx52, sizeof(suxx52), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lgs") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }

    static unsigned char suxx53[] = {0x0F, 0xB4, 0x00};//         lfs         eax,fword ptr [eax] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx53, sizeof(suxx53), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lfs") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }

    static unsigned char suxx54[] = {0x0F, 0xB2, 0x00};//         lss         eax,fword ptr [eax] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx54, sizeof(suxx54), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lss") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.memory.m_index.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }

    static unsigned char suxx55[] = {0x0F,  0x00, 0x10}; // /2   LLDT r/m16       20       Load selector r/m16 into LDTR
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx55, sizeof(suxx55), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lldt") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
    }

    
    static unsigned char suxx56[] = {0x0F,  0x01, 0x30}; // 0F  01 /6   LMSW r/m16       10/13    Load r/m16 in machine status word
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx56, sizeof(suxx56), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lmsw") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
    }
    
    static unsigned char suxx57[] = {0xAC};//      LODS m8       5        Load byte [(E)SI] into AL
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx57, sizeof(suxx57), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lods") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_reserved_reg);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 1);
    }

    static unsigned char suxx58[] = {0xE2, 0x13};//  cb   LOOP rel8    11+m    DEC count; jump short if count <> 0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx58, sizeof(suxx58), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "loop") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0x13);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 1);
    }

    static unsigned char suxx59[] = {0xE1, 0x13};  // cb   LOOPE rel8   11+m    DEC count; jump short if count <> 0 and ZF=1
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx59, sizeof(suxx59), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "loope") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0x13);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 1);
    }

    static unsigned char suxx60[] = {0xE0, 0x13};  // LOOPNE rel8  11+m    DEC count; jump short if count <> 0 and ZF=0
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx60, sizeof(suxx60), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "loopne") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0x13);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_size == 1);
    }

    static unsigned char suxx61[] = {0x0F, 0x03, 0x00}; // /r    LSL r16,r/m16    pm=20/21    Load: r16 := segment limit,
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx61, sizeof(suxx61), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lsl") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
    }

    static unsigned char suxx62[] = {0x0F, 0x00, 0x18}; // /3    LTR r/m16      pm=23/27  Load EA word into task register
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx62, sizeof(suxx62), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ltr") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
    }

    static unsigned char suxx63[] = {0xA5}; // MOVS m16,m16     7        Move word [(E)SI] to ES:[(E)DI]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx63, sizeof(suxx63), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movs") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_reserved_reg);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_reserved_reg);
    }

    static unsigned char suxx64[] = {0x0F, 0xBE, 0x00};// /r  MOVSX r16,r/m8     3/6      Move byte to word with sign-extend
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx64, sizeof(suxx64), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movsx") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 1);
    }

    static unsigned char suxx65[] = {0x0F, 0xBF, 0};// /r  MOVSX r32,r/m16    3/6      Move word to dword, sign-extend
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx65, sizeof(suxx65), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movsx") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);
    }
        
    static unsigned char suxx66[] = {0x0F, 0xB6, 0x00};//  0F  B6 /r   MOVZX r16,r/m8     3/6      Move byte to word with sign-extend
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx66, sizeof(suxx66), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movzx") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 1);
    }

    static unsigned char suxx67[] = {0x0F, 0xB7, 0};// /r  MOVZX r32,r/m16    3/6      Move word to dword, sign-extend
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx67, sizeof(suxx67), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movzx") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);
    }

    
    static unsigned char suxx68[] = {0xF7, 0x20};//  /4  MUL EAX,r/m32
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx68, sizeof(suxx68), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mul") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
    }

    static unsigned char suxx69[] = {0xF7, 0x18}; // F7  /3  NEG r/m16     2/6       Two's complement negate r/m word
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx69, sizeof(suxx69), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "neg") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
    }

    static unsigned char suxx70[] = {0xF7, 0x10}; // F7   /2   NOT r/m16      2/6      Reverse each bit of r/m word
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx70, sizeof(suxx70), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "not") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
    }


    static unsigned char suxx71[] = {0xEE};//            out         DX,AL
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx71, sizeof(suxx71), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "out") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_AL);
    }

    static unsigned char suxx72[] = {0xE7, 0x05};//            out         5, eax
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx72, sizeof(suxx72), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "out") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.imm == 5);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_EAX);
    }

    static unsigned char suxx73[] = {0x2E, 0x6F};//            outs        dx,dword ptr cs:[esi] 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx73, sizeof(suxx73), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "outs") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_CS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_ESI);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
    }


    static unsigned char suxx74[] = {0xC3};      //RET         C3             
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx74, sizeof(suxx74), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ret") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }

    static unsigned char suxx75[] = {0xC2, 0x0C, 0x00};//         ret         0Ch  
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx75, sizeof(suxx75), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ret") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.imm == 0xC);
    }

    static unsigned char suxx76[] = {0x0f, 0x92, 0xc0};//         setb    al
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx76, sizeof(suxx76), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "setb") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_AL);
    }

    static unsigned char suxx77[] = {0xF6, 0xFA};//         idiv dl
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx77, sizeof(suxx77), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "idiv") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_DL);
    }

    static unsigned char suxx78[] = {0x67, 0x66, 0x45, 0xf, 0x2, 0x38};//         lar r15w word ptr ds:[r8d]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,suxx78, sizeof(suxx78), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lar") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_R15W);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_R8D);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 2);
    }    


    static unsigned char suxx79[] = {0x67, 0x41, 0xf, 0x0, 0x10};//         lldt word ptr ds:[r8d]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,suxx79, sizeof(suxx79), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lldt") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_R8D);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
    }    


    /*static unsigned char suxx80[] = {0x0f, 0xae, 0x3b };//         clflush [rbx]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,suxx80, sizeof(suxx80), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "clflush") == 0);
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RBX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 1);
    }*/

    
    static unsigned char suxx81[] = {0xf, 0x1, 0xDF};//         INVLPGA RAX, ECX | 0F 01 DF
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,suxx81, sizeof(suxx81), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "invlpga") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }
    
    static unsigned char suxx82[] = {0x40, 0xf6, 0xfd}; // idiv bpl
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,suxx82, sizeof(suxx82), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "idiv") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_BPL);
    }

    static unsigned char suxx83[] = {0xf, 0x1, 0xf8};//         swapgs
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,suxx83, sizeof(suxx83), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "swapgs") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }

    static unsigned char suxx85[] = {0x48, 0xf, 0x1, 0xDF};//         INVLPGA RAX, ECX | 0F 01 DF
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,suxx85, sizeof(suxx85), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "invlpga") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_ECX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }


    static unsigned char suxx86[] = {0x48, 0x0f, 0x01, 0x3f};//         invlpg  [rdi]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64,suxx86, sizeof(suxx86), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "invlpg") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RDI);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
    }

    static unsigned char suxx87[] = {0xD4, 0x01}; //     AAM            17        ASCII adjust AX after multiply
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx87, sizeof(suxx87), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aam") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.imm == 1);
    }

    static unsigned char suxx88[] = {0xD6}; // salc
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32,suxx88, sizeof(suxx88), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "salc") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 0);
        DIANA_TEST_ASSERT((result.pInfo->m_flags & DI_FLAG_CMD_UNDOCUMENTED) == DI_FLAG_CMD_UNDOCUMENTED);
    }

    static unsigned char suxx89[] = {0x0F, 0x01, 0x20}; // smsw        [eax]
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE32, suxx89, sizeof(suxx89), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "smsw") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
    }

    static unsigned char suxx90[] = {0x0F, 0x01, 0x58, 0x00}; // lidt        [eax][0]
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE32, suxx90, sizeof(suxx90), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lidt") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED == (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
    }

    static unsigned char suxx91[] = {0xDF, 0x10}; // fist word [eax]
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE32, suxx91, sizeof(suxx91), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fist") == 0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
    }

    static unsigned char suxx92[] = {0x0F, 0x1E, 0x84, 0xC0, 0x90, 0x90, 0x90, 0x90}; // nop dword ptr [eax+eax*8-0x6f6f6f70]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, suxx92, sizeof(suxx92), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "hint_nop") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0xffffffff90909090);
    }

    // 1. spec cmd test
    static unsigned char suxx93[] = {0x66, 0x0F, 0xFE}; // special test command
    iRes = Diana_ParseCmdOnBuffer_testmode(DIANA_MODE32, suxx93, sizeof(suxx93), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "test_cmd") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
        DIANA_TEST_ASSERT(result.iFullCmdSize == 2);
    }

    // 2. with finalizer
    static unsigned char suxx94[] = {0x66, 0x0F}; 
    iRes = Diana_ParseCmdOnBuffer_testmode(DIANA_MODE32, suxx94, sizeof(suxx94), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "test_cmd") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
        DIANA_TEST_ASSERT(result.iFullCmdSize == 2);
    }

    // 3. without flag
    iRes = Diana_ParseCmdOnBuffer(DIANA_MODE32, suxx94, sizeof(suxx94), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT(iRes);


    // xrstor
    static unsigned char suxx95[] = {0x0f, 0xae, 0x28};  // xrstor  [rax]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, suxx95, sizeof(suxx95), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "xrstor") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0);
    }

    // lfence 
    static unsigned char suxx96[] = {0x0f, 0xae, 0xe8};  // lfence
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, suxx96, sizeof(suxx96), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lfence") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }

    // mfence 
    static unsigned char suxx97[] = {0x0f, 0xae, 0xf0};  // mfence
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, suxx97, sizeof(suxx97), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mfence") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 0);
    }

    static unsigned char suxx98[] = {0x0f, 0xae, 0x68, 0x10}; // xrstor  [rax+10h]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, suxx98, sizeof(suxx98), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "xrstor") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispSize == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.dispValue == 0x10);
    }

    
    static unsigned char suxx99[] = {0x0f, 0xae, 0xb8, 0x0c, 0x10, 0x00, 0x0f}; //  clflush [rax+0F00100Ch]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, suxx99, sizeof(suxx99), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "clflush") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_memory);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.memory.m_index.dispValue == 0x0F00100C);
    }
}

void test_suxx()
{
    DIANA_TEST(test_suxx_impl());
}