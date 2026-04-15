#include "test_aesni.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

// ---- Legacy AES-NI instructions (66 0F 38 xx /r) -------------------------

// AESIMC xmm1, xmm2
// 66 0F 38 DB CA
//   66      = operand-size prefix
//   0F 38   = 3-byte opcode escape
//   DB      = opcode
//   CA      = ModRM: mod=11 reg=xmm1=1 rm=xmm2=2
static void test_aesimc_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0xDB, 0xCA};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aesimc") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
    }
}

// AESENC xmm1, xmm2
// 66 0F 38 DC CA
static void test_aesenc_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0xDC, 0xCA};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aesenc") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
    }
}

// AESENCLAST xmm1, xmm2
// 66 0F 38 DD CA
static void test_aesenclast_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0xDD, 0xCA};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aesenclast") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
    }
}

// AESDEC xmm1, xmm2
// 66 0F 38 DE CA
static void test_aesdec_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0xDE, 0xCA};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aesdec") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
    }
}

// AESDECLAST xmm1, xmm2
// 66 0F 38 DF CA
static void test_aesdeclast_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0xDF, 0xCA};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aesdeclast") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
    }
}

// AESKEYGENASSIST xmm1, xmm2, 0x01
// 66 0F 3A DF CA 01
//   66      = operand-size prefix
//   0F 3A   = 3-byte opcode escape
//   DF      = opcode
//   CA      = ModRM: mod=11 reg=xmm1=1 rm=xmm2=2
//   01      = imm8
static void test_aeskeygenassist_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x66, 0x0F, 0x3A, 0xDF, 0xCA, 0x01};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aeskeygenassist") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0x01);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 1);
    }
}

// PCLMULQDQ xmm1, xmm2, 0x01
// 66 0F 3A 44 CA 01
static void test_pclmulqdq_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x66, 0x0F, 0x3A, 0x44, 0xCA, 0x01};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pclmulqdq") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0x01);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 1);
    }
}

// ---- VEX-encoded AES/CLMUL instructions ----------------------------------

// VAESIMC xmm1, xmm2
// VEX.128.66.0F38.WIG DB /r
// 3-byte VEX: C4 E2 79 DB CA
//   C4      = 3-byte VEX prefix
//   E2      = ~R=1,~X=1,~B=1, map=00010(0F38)
//   79      = W=0, ~vvvv=1111(unused), L=0, pp=01(66)
//   DB      = opcode
//   CA      = ModRM: mod=11 reg=xmm1=1 rm=xmm2=2
static void test_vaesimc_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xC4, 0xE2, 0x79, 0xDB, 0xCA};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vaesimc") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
    }
}

// VAESENC xmm1, xmm2, xmm3
// VEX.NDS.128.66.0F38.WIG DC /r
// 3-byte VEX: C4 E2 69 DC CB
//   E2      = ~R=1,~X=1,~B=1, map=00010(0F38)
//   69      = W=0, ~vvvv=~xmm2=1101, L=0, pp=01(66)
//   DC      = opcode
//   CB      = ModRM: mod=11 reg=xmm1=1 rm=xmm3=3
static void test_vaesenc_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xC4, 0xE2, 0x69, 0xDC, 0xCB};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vaesenc") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_XMM3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 16);
    }
}

// VAESENCLAST xmm1, xmm2, xmm3
// VEX.NDS.128.66.0F38.WIG DD /r
// 3-byte VEX: C4 E2 69 DD CB
static void test_vaesenclast_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xC4, 0xE2, 0x69, 0xDD, 0xCB};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vaesenclast") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_XMM3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 16);
    }
}

// VAESDEC xmm1, xmm2, xmm3
// VEX.NDS.128.66.0F38.WIG DE /r
// 3-byte VEX: C4 E2 69 DE CB
static void test_vaesdec_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xC4, 0xE2, 0x69, 0xDE, 0xCB};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vaesdec") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_XMM3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 16);
    }
}

// VAESDECLAST xmm1, xmm2, xmm3
// VEX.NDS.128.66.0F38.WIG DF /r
// 3-byte VEX: C4 E2 69 DF CB
static void test_vaesdeclast_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xC4, 0xE2, 0x69, 0xDF, 0xCB};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vaesdeclast") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_XMM3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 16);
    }
}

// VAESKEYGENASSIST xmm1, xmm2, 0x01
// VEX.128.66.0F3A.WIG DF /r ib
// 3-byte VEX: C4 E3 79 DF CA 01
//   E3      = ~R=1,~X=1,~B=1, map=00011(0F3A)
//   79      = W=0, ~vvvv=1111(unused), L=0, pp=01(66)
//   DF      = opcode
//   CA      = ModRM: mod=11 reg=xmm1=1 rm=xmm2=2
//   01      = imm8
static void test_vaeskeygenassist_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xC4, 0xE3, 0x79, 0xDF, 0xCA, 0x01};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vaeskeygenassist") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0x01);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 1);
    }
}

// VPCLMULQDQ xmm1, xmm2, xmm3, 0x01
// VEX.NDS.128.66.0F3A.WIG 44 /r ib
// 3-byte VEX: C4 E3 69 44 CB 01
//   E3      = ~R=1,~X=1,~B=1, map=00011(0F3A)
//   69      = W=0, ~vvvv=~xmm2=1101, L=0, pp=01(66)
//   44      = opcode
//   CB      = ModRM: mod=11 reg=xmm1=1 rm=xmm3=3
//   01      = imm8
static void test_vpclmulqdq_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xC4, 0xE3, 0x69, 0x44, 0xCB, 0x01};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 4);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 4);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vpclmulqdq") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_XMM3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[3].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[3].value.imm == 0x01);
        DIANA_TEST_ASSERT(result.linkedOperands[3].usedSize == 1);
    }
}

void test_aesni()
{
    // legacy
    DIANA_TEST(test_aesimc_xmm());
    DIANA_TEST(test_aesenc_xmm());
    DIANA_TEST(test_aesenclast_xmm());
    DIANA_TEST(test_aesdec_xmm());
    DIANA_TEST(test_aesdeclast_xmm());
    DIANA_TEST(test_aeskeygenassist_xmm());
    DIANA_TEST(test_pclmulqdq_xmm());
    // VEX
    DIANA_TEST(test_vaesimc_xmm());
    DIANA_TEST(test_vaesenc_xmm());
    DIANA_TEST(test_vaesenclast_xmm());
    DIANA_TEST(test_vaesdec_xmm());
    DIANA_TEST(test_vaesdeclast_xmm());
    DIANA_TEST(test_vaeskeygenassist_xmm());
    DIANA_TEST(test_vpclmulqdq_xmm());
}
