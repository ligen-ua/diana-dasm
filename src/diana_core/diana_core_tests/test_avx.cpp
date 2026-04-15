#include "test_avx.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}

#include "test_common.h"
#include "string.h"

// VADDPD xmm1, xmm2, xmm3
// VEX.NDS.128.66.0F.WIG 58 /r
// 2-byte VEX: C5 E9 58 CB
//   byte2 = ~R=1, ~vvvv=~xmm2=0xD, L=0, pp=01(66h) => 0xE9
//   ModRM: mod=11 reg=xmm1=1 rm=xmm3=3 => 0xCB
static void test_vaddpd_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char vaddpd_xmm[] = {0xC5, 0xE9, 0x58, 0xCB};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, vaddpd_xmm, sizeof(vaddpd_xmm), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vaddpd") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        // operand[0]: dest = xmm1 (ModRM.reg)
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        // operand[1]: src1 = xmm2 (vvvv/NDS)
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        // operand[2]: src2 = xmm3 (ModRM.rm)
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_XMM3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 16);
    }
}

// VADDPD ymm1, ymm2, ymm3
// VEX.NDS.256.66.0F.WIG 58 /r
// 2-byte VEX: C5 ED 58 CB
//   byte2 = ~R=1, ~vvvv=0xD, L=1(256-bit), pp=01(66h) => 0xED
//   ModRM: mod=11 reg=ymm1=1 rm=ymm3=3 => 0xCB
static void test_vaddpd_ymm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char vaddpd_ymm[] = {0xC5, 0xED, 0x58, 0xCB};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, vaddpd_ymm, sizeof(vaddpd_ymm), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vaddpd") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1);
        // operand[0]: dest = ymm1
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_YMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 32);
        // operand[1]: src1 = ymm2 (vvvv/NDS)
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_YMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 32);
        // operand[2]: src2 = ymm3
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_YMM3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 32);
    }
}

// VMOVAPD xmm0, xmm1
// VEX.128.66.0F.WIG 28 /r  (no NDS)
// 2-byte VEX: C5 F9 28 C1
//   byte2 = ~R=1, ~vvvv=0xF(unused), L=0, pp=01(66h) => 0xF9
//   ModRM: mod=11 reg=xmm0=0 rm=xmm1=1 => 0xC1
static void test_vmovapd_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char vmovapd_xmm[] = {0xC5, 0xF9, 0x28, 0xC1};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, vmovapd_xmm, sizeof(vmovapd_xmm), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vmovapd") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        // operand[0]: dest = xmm0
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        // operand[1]: src = xmm1
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
    }
}

// VMOVUPS xmm2, xmm3
// VEX.128.0F.WIG 10 /r  (no pp, no NDS)
// 2-byte VEX: C5 F8 10 D3
//   byte2 = ~R=1, ~vvvv=0xF(unused), L=0, pp=00(none) => 0xF8
//   ModRM: mod=11 reg=xmm2=2 rm=xmm3=3 => 0xD3
static void test_vmovups_xmm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char vmovups_xmm[] = {0xC5, 0xF8, 0x10, 0xD3};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, vmovups_xmm, sizeof(vmovups_xmm), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vmovups") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        DIANA_TEST_ASSERT(!(result.pInfo->m_flags & DI_FLAG_CMD_VEX_L1));
        // operand[0]: dest = xmm2
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        // operand[1]: src = xmm3
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM3);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
    }
}

// VADDPD xmm1, xmm2, [rax]
// VEX.NDS.128.66.0F.WIG 58 /r  (memory source)
// 2-byte VEX: C5 E9 58 08
//   byte2 = ~R=1, ~vvvv=0xD(xmm2), L=0, pp=01(66h) => 0xE9
//   ModRM: mod=00 reg=xmm1=1 rm=rax=0 => 0x08
static void test_vaddpd_mem()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char vaddpd_mem[] = {0xC5, 0xE9, 0x58, 0x08};
    iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, vaddpd_mem, sizeof(vaddpd_mem), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "vaddpd") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_VEX_ENCODED);
        // operand[0]: dest = xmm1 (ModRM.reg)
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        // operand[1]: src1 = xmm2 (vvvv/NDS)
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        // operand[2]: src2 = [rax] (memory)
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.reg == reg_RAX);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.seg_reg == reg_DS);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.index == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.dispSize == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.dispValue == 0);
    }
}

void test_avx()
{
    DIANA_TEST(test_vaddpd_xmm());
    DIANA_TEST(test_vaddpd_ymm());
    DIANA_TEST(test_vmovapd_xmm());
    DIANA_TEST(test_vmovups_xmm());
    DIANA_TEST(test_vaddpd_mem());
}
