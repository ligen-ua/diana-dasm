#include "test_new.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

void test_sal1()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    
    int iRes = 0;

    static unsigned char mov0[] = {0xd1, 0xe6}; //    shl esi,1
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, mov0, sizeof(mov0), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "shl")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

}

static void test_r32_64()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    
    int iRes = 0;

    static unsigned char bswap0[] = {0x0F, 0xC9}; //    bswap ecx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, bswap0, sizeof(bswap0), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bswap")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
    }
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, bswap0, sizeof(bswap0), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount ==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bswap")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_ECX);
    }
    
    static unsigned char bswap1[] = {0x48, 0x0f, 0xc9}; //    bswap rcx
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, bswap1, sizeof(bswap1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bswap")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RCX);
    }
    
    static unsigned char cvttsd2si[] = {0xf2, 0x48, 0x0f, 0x2c, 0}; //    cvtsd2si rdx,dword ptr [rax]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, cvttsd2si, sizeof(cvttsd2si), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cvttsd2si")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0x00);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0x0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0x0);
    }

    static unsigned char cvttsd2si2[] = {0xf2, 0x0f, 0x2c, 0}; //    cvttsd2si eax, QWORD ptr [rax]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, cvttsd2si2, sizeof(cvttsd2si2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cvttsd2si")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_EAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
    }
    
}

void test_nop_pause()
{
    DianaGroupInfo * pGroupInfo=0;
    DianaParserResult result;
    size_t read;
    
    int iRes = 0;

    static unsigned char pause[] = {0xF3, 0x90}; //    PAUSE 
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, pause, sizeof(pause), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==0);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pause")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char nop[] = {0x90}; //    nop
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, nop, sizeof(nop), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount==0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==0);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "nop")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }
}

void test_nop()
{
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char nop[] = {0x66, 0x90}; // NOP
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, nop, sizeof(nop), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==2);
        DIANA_TEST_ASSERT(result.iLinkedOpCount==0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==0);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "nop")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    static unsigned char nop2[] = {0x66, 0x90}; // NOP
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, nop2, sizeof(nop2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==2);
        DIANA_TEST_ASSERT(result.iLinkedOpCount==0);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount==0);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "nop")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }
}

void test_nops()
{
    // this form GAS, the GNU Assembler
    // nopl (%[re]ax)
    static unsigned char alt_3[] = {0x0f,0x1f,0x00};
    // nopl 0(%[re]ax)
    static unsigned char alt_4[] = {0x0f,0x1f,0x40,0x00};
    // nopl 0(%[re]ax,%[re]ax,1)
    static unsigned char alt_5[] = {0x0f,0x1f,0x44,0x00,0x00};
    // nopw 0(%[re]ax,%[re]ax,1)
    static unsigned char alt_6[] = {0x66,0x0f,0x1f,0x44,0x00,0x00};
    // nopl 0L(%[re]ax)
    static unsigned char alt_7[] = {0x0f,0x1f,0x80,0x00,0x00,0x00,0x00};
    // nopl 0L(%[re]ax,%[re]ax,1)
    static unsigned char alt_8[] = {0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    // nopw 0L(%[re]ax,%[re]ax,1)
    static unsigned char alt_9[] = {0x66,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    // nopw %cs:0L(%[re]ax,%[re]ax,1)
    static unsigned char alt_10[] = {0x66,0x2e,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    // nopw %cs:0L(%[re]ax,%[re]ax,1)
    static unsigned char alt_long_11[] = {0x66,0x66,0x2e,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    // nopw %cs:0L(%[re]ax,%[re]ax,1)
    static unsigned char alt_long_12[] = {0x66,0x66,0x66,0x2e,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    // nopw %cs:0L(%[re]ax,%[re]ax,1)
    static unsigned char alt_long_13[] = {0x66,0x66,0x66,0x66,0x2e,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    // nopw %cs:0L(%[re]ax,%[re]ax,1)
    static unsigned char alt_long_14[] = {0x66,0x66,0x66,0x66,0x66,0x2e,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    // nopw %cs:0L(%[re]ax,%[re]ax,1)
    static unsigned char alt_long_15[] = {0x66,0x66,0x66,0x66,0x66,0x66,0x2e,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    // nopl 0(%[re]ax,%[re]ax,1)
    static unsigned char alt_short_11[] = {0x0f,0x1f,0x44,0x00,0x00};
    // nopl 0L(%[re]ax)
    static unsigned char alt_short_14[] = {0x0f,0x1f,0x80,0x00,0x00,0x00,0x00};

    DianaParserResult result;
    size_t read;
    int iRes = 0;

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_3, sizeof(alt_3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==3);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_4, sizeof(alt_4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==4);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_5, sizeof(alt_5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==5);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_6, sizeof(alt_6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==6);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_7, sizeof(alt_7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==7);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_8, sizeof(alt_8), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==8);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_9, sizeof(alt_9), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==9);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_10, sizeof(alt_10), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==10);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_long_11, sizeof(alt_long_11), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==11);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_long_12, sizeof(alt_long_12), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==12);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_long_13, sizeof(alt_long_13), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==13);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_long_14, sizeof(alt_long_14), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==14);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_long_15, sizeof(alt_long_15), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==15);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_short_11, sizeof(alt_short_11), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==5);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, alt_short_14, sizeof(alt_short_14), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==7);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    // this form Intel manual
    static unsigned char intel1[] = {0x0f,0x1f,0x00};
    static unsigned char intel2[] = {0x0f,0x1f,0x40,0x00};
    static unsigned char intel3[] = {0x0f,0x1f,0x44,0x00,0x00};
    static unsigned char intel4[] = {0x66,0x0f,0x1f,0x44,0x00,0x00};
    static unsigned char intel5[] = {0x0f,0x1f,0x80,0x00,0x00,0x00,0x00};
    static unsigned char intel6[] = {0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};
    static unsigned char intel7[] = {0x66,0x0f,0x1f,0x84,0x00,0x00,0x00,0x00,0x00};

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, intel1, sizeof(intel1), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==3);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, intel2, sizeof(intel2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==4);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, intel3, sizeof(intel3), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==5);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, intel4, sizeof(intel4), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==6);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, intel5, sizeof(intel5), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==7);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, intel6, sizeof(intel6), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==8);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, intel7, sizeof(intel7), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==9);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "hint_nop")==0);
    }

}

void test_ret()
{
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    // ret 0
    unsigned char code[] = {0xC2, 0x00, 0x00};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, code, sizeof(code), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==3);
        DIANA_TEST_ASSERT(result.iLinkedOpCount==1);
        DIANA_TEST_ASSERT(strcmp(result.pInfo->m_pGroupInfo->m_pName, "ret")==0);
        DIANA_TEST_ASSERT(DI_FLAG_CMD_PRIVILEGED != (result.pInfo->m_flags & DI_FLAG_CMD_PRIVILEGED));
    }

}

void test_nop_toobig()
{
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char nop[] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x90}; // NOP
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, nop, sizeof(nop), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iFullCmdSize==15);
        DIANA_TEST_ASSERT(result.pInfo->m_pGroupInfo->m_commandId==diana_cmd_nop);
    }
    static unsigned char nop2[] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x90}; // NOP
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, nop2, sizeof(nop2), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT(iRes)
}
void test_rdrand()
{
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char rdrand[] = {0x0f, 0xc7, 0xf6}; // rdrand  esi
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, rdrand, sizeof(rdrand), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_rdrand);
        DIANA_TEST_ASSERT (result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.recognizedRegister == reg_ESI);
    }
    static unsigned char rdseed[] = {0x0f, 0xc7, 0xf8}; // rdseed  eax
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, rdseed, sizeof(rdseed), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_rdseed);
        DIANA_TEST_ASSERT (result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.recognizedRegister == reg_EAX);
    }
    // test also the same opcode
    static unsigned char vmptrld[] = {0x0f, 0xc7, 0x36}; // vmptrld qword ptr [esi]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, vmptrld, sizeof(vmptrld), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_vmptrld);
        DIANA_TEST_ASSERT (result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.reg == reg_ESI);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispSize == 0);
    }
    static unsigned char vmptrst[] = {0x0f, 0xc7, 0x39}; // vmptrst qword ptr [ecx]
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, vmptrst, sizeof(vmptrst), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT (result.pInfo->m_pGroupInfo->m_commandId == diana_cmd_vmptrst);
        DIANA_TEST_ASSERT (result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT (result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.reg == reg_ECX);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT (result.linkedOperands[0].value.rmIndex.dispSize == 0);
    }
}

void test_new()
{
    DIANA_TEST(test_rdrand());
    DIANA_TEST(test_nop_pause());
    DIANA_TEST(test_nop());
    DIANA_TEST(test_nops());
    DIANA_TEST(test_r32_64());
    DIANA_TEST(test_sal1());
    DIANA_TEST(test_ret());
    DIANA_TEST(test_nop_toobig());
}