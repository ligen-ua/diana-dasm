#include "test_arm64_cmds.h"
extern "C"
{
#include "diana_streams.h"
#include "diana_arm.h"
}

#include "test_common.h"
#include "string.h"

// One instruction per test, the way a client sees it: DianaParserResult, DianaLinkedOperand and its arm part.
// The words are little-endian in memory. Registers: reg_A64_X0 + n, reg_A64_W0 + n, reg_A64_V0 + n, ...
// Linked operands follow the raw encoding: aliases (cmp, mov, lsl) exist only in the text output.
// rel operands are relative to the next instruction, like x86: target = address + 4 + value.rel.m_value

// ---- Integer ------------------------------------------------------------

// add x0, x1, x2
// 8b020020: sf=1 op=0 S=0 shift=lsl Rm=2 imm6=0 Rn=1 Rd=0
static void test_a64_add_reg()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x00, 0x02, 0x8b};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "add") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_arm_shifted_reg);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_X0 + 2);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.opKind == DIANA_A64_OPK_SHREG_X);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.shiftType == DIANA_ARM_SHIFT_LSL);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.shiftAmount == 0);
    }
}

// add x0, sp, #0x10
// 910043e0: sf=1 op=0 S=0 sh=0 imm12=0x10 Rn=31 (sp) Rd=0
static void test_a64_add_sp_imm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xe0, 0x43, 0x00, 0x91};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "add") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.opKind == DIANA_A64_OPK_REG_XSP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_SP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_REG_XSP);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0x10);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.opKind == DIANA_A64_OPK_AIMM);
    }
}

// sub w3, w4, w5, lsl #2
// 4b050883: sf=0 op=1 S=0 shift=lsl Rm=5 imm6=2 Rn=4 Rd=3
static void test_a64_sub_shifted()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x83, 0x08, 0x05, 0x4b};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "sub") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_W0 + 3);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_W0 + 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_arm_shifted_reg);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_W0 + 5);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.opKind == DIANA_A64_OPK_SHREG_W);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.shiftType == DIANA_ARM_SHIFT_LSL);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.shiftAmount == 2);
    }
}

// and w0, w1, #0xff
// 12001c20: sf=0 opc=00 N=0 immr=0 imms=7 Rn=1 Rd=0, the bitmask immediate is decoded
static void test_a64_and_bitmask()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x1c, 0x00, 0x12};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "and") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_W0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.opKind == DIANA_A64_OPK_REG_WSP);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_W0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0xff);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.opKind == DIANA_A64_OPK_BIMM32);
    }
}

// movz x0, #0x1234, lsl #16
// d2a24680: sf=1 opc=10 hw=1 imm16=0x1234 Rd=0, the shift is in arm.shiftType/shiftAmount, imm is not shifted
static void test_a64_movz()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x80, 0x46, 0xa2, 0xd2};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movz") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x1234);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_MOVW);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.shiftType == DIANA_ARM_SHIFT_LSL);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.shiftAmount == 16);
    }
}

// madd x0, x1, x2, x3
// 9b020c20: sf=1 Rm=2 o0=0 Ra=3 Rn=1 Rd=0
static void test_a64_madd()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x0c, 0x02, 0x9b};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 4);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 4);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "madd") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_X0 + 2);
        DIANA_TEST_ASSERT(result.linkedOperands[3].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[3].value.recognizedRegister == reg_A64_X0 + 3);
        DIANA_TEST_ASSERT(result.linkedOperands[3].usedSize == 8);
    }
}

// csel x0, x1, x2, ne
// 9a821020: sf=1 Rm=2 cond=0001 (ne) Rn=1 Rd=0
static void test_a64_csel()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x10, 0x82, 0x9a};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 4);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 4);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "csel") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_X0 + 2);
        DIANA_TEST_ASSERT(result.linkedOperands[3].type == diana_arm_cond);
        DIANA_TEST_ASSERT(result.linkedOperands[3].arm.cond == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[3].arm.opKind == DIANA_A64_OPK_COND);
    }
}

// cmp x1, #0x4 is subs xzr, x1, #0x4
// f100103f: sf=1 op=1 S=1 sh=0 imm12=4 Rn=1 Rd=31 (xzr, not sp: Rd of subs is not an SP operand)
static void test_a64_cmp_imm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x3f, 0x10, 0x00, 0xf1};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "subs") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_XZR);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.opKind == DIANA_A64_OPK_REG_X);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 4);
    }
}

// lsr w0, w1, #0 is ubfm w0, w1, #0, #31
// 53007c20: sf=0 opc=10 N=0 immr=0 imms=31 Rn=1 Rd=0
static void test_a64_ubfm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x7c, 0x00, 0x53};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 4);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 4);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ubfm") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_W0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_W0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.opKind == DIANA_A64_OPK_UIMM);
        DIANA_TEST_ASSERT(result.linkedOperands[3].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[3].value.imm == 31);
    }
}

// add x0, x1, w2, uxtw #2
// 8b224820: sf=1 op=0 S=0 Rm=2 option=010 (uxtw) imm3=2 Rn=1 Rd=0
static void test_a64_add_extended()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x48, 0x22, 0x8b};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "add") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_arm_shifted_reg);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_W0 + 2);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.opKind == DIANA_A64_OPK_EXTREG_X);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.shiftType == DIANA_ARM_EXTEND_UXTW);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.shiftAmount == 2);
    }
}

// ---- Loads and stores -----------------------------------------------------

// ldr x0, [x1, #8]!
// f8408c20: size=11 opc=01 imm9=8 pre-index Rn=1 Rt=0
static void test_a64_ldr_pre()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x8c, 0x40, 0xf8};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ldr") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_none);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedAddressSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_MEM_PRE);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.writeback == DIANA_ARM_WRITEBACK_PRE);
    }
}

// ldr x0, [x1], #8
// f8408420: size=11 opc=01 imm9=8 post-index Rn=1 Rt=0
static void test_a64_ldr_post()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x84, 0x40, 0xf8};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ldr") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_MEM_POST);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.writeback == DIANA_ARM_WRITEBACK_POST);
    }
}

// ldr w0, [x1, #16]
// b9401020: size=10 opc=01 imm12=4 (scaled by 4) Rn=1 Rt=0, dispValue is the byte offset
static void test_a64_ldr_uoff()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x10, 0x40, 0xb9};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ldr") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_W0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_MEM_UOFF);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.writeback == DIANA_ARM_WRITEBACK_NONE);
    }
}

// ldr x0, [x1, w2, sxtw #3]
// f862d820: size=11 opc=01 Rm=2 option=110 (sxtw) S=1 Rn=1 Rt=0
// rmIndex.index is the scale (1 << amount) like x86, arm.hasAmount says "#3" is encoded
static void test_a64_ldr_regoff()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0xd8, 0x62, 0xf8};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ldr") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.indexed_reg == reg_A64_W0 + 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.index == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_MEM_REGOFF);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.shiftType == DIANA_ARM_EXTEND_SXTW);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.shiftAmount == 3);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.hasAmount == 1);
    }
}

// stp x29, x30, [sp, #-16]!
// a9bf7bfd: opc=10 pre-index imm7=-2 (scaled by 8) Rt2=30 Rn=31 (sp) Rt=29
static void test_a64_stp_pre()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xfd, 0x7b, 0xbf, 0xa9};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "stp") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0 + 29);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 30);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.reg == reg_A64_SP);
        DIANA_TEST_ASSERT((long long)result.linkedOperands[2].value.rmIndex.dispValue == -16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.writeback == DIANA_ARM_WRITEBACK_PRE);
    }
}

// ldp x0, x1, [sp], #16
// a8c107e0: opc=10 post-index imm7=2 (scaled by 8) Rt2=1 Rn=31 (sp) Rt=0
static void test_a64_ldp_post()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xe0, 0x07, 0xc1, 0xa8};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ldp") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.reg == reg_A64_SP);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.dispValue == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.writeback == DIANA_ARM_WRITEBACK_POST);
    }
}

// ldraa x0, [x1, #-4096]
// f8600420: M=0 S=1 imm9=0 W=0 Rn=1 Rt=0, the offset S:imm9 is scaled by 8
static void test_a64_ldraa()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x04, 0x60, 0xf8};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ldraa") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT((long long)result.linkedOperands[1].value.rmIndex.dispValue == -4096);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.writeback == DIANA_ARM_WRITEBACK_NONE);
    }
}

// casp x2, x3, x4, x5, [x6]
// 48227cc4: sz=1 L=0 Rs=2 o0=0 Rn=6 Rt=4, the second register of each pair is Rs+1 and Rt+1
static void test_a64_casp()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xc4, 0x7c, 0x22, 0x48};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 5);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "casp") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0 + 2);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_X0 + 4);
        DIANA_TEST_ASSERT(result.linkedOperands[3].value.recognizedRegister == reg_A64_X0 + 5);
        DIANA_TEST_ASSERT(result.linkedOperands[4].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[4].value.rmIndex.reg == reg_A64_X0 + 6);
    }
}

// cpyfp [x0]!, [x1]!, x2!
// 19010440: Rs=1 Rn=2 Rd=0; all three registers are updated: writeback on both addresses and on the size
static void test_a64_cpyfp()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x40, 0x04, 0x01, 0x19};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cpyfp") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rmIndex.reg == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.writeback == DIANA_ARM_WRITEBACK_PRE);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.writeback == DIANA_ARM_WRITEBACK_PRE);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_X0 + 2);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.writeback == DIANA_ARM_WRITEBACK_PRE);
    }
}

// sdot v0.4s, v1.16b, v18.4b[3]
// 4fb2e820: Q=1 size=10 L=1 M=1 Rm=2 H=1; the index counts groups of four bytes: Vm is M:Rm, index H:L
static void test_a64_sdot_element()
{
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0xe8, 0xb2, 0x4f};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.arrangement == DIANA_ARM_ARR_4S);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.arrangement == DIANA_ARM_ARR_16B);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_V0 + 18);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.arrangement == DIANA_ARM_ARR_GROUP(0, 2));
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.lane == 3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 4);
    }
}

// fmlal v0.2s, v1.2h, v2.2h; fcadd v0.4h, v1.4h, v2.4h, #90
// 0e22ec20: Q=0, the sources are half of Q (32 bits); 2e42e420: rot=0 is #90
static void test_a64_fmlal_fcadd()
{
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char fmlal[] = {0x20, 0xec, 0x22, 0x0e};
    static unsigned char fcadd[] = {0x20, 0xe4, 0x42, 0x2e};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, fmlal, sizeof(fmlal), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.arrangement == DIANA_ARM_ARR_2S);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.arrangement == DIANA_ARM_ARR_2H);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
    }
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, fcadd, sizeof(fcadd), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[3].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[3].value.imm == 90);
    }
}

// stilp x0, x1, [x2, #-16]!; mrrs x0, x1, ttbr0_el1
// d9010840: the pre-index is minus the access size, no offset field; d5782000: the pair is Rt, Rt+1
static void test_a64_rcpc3_d128()
{
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char stilp[] = {0x40, 0x08, 0x01, 0xd9};
    static unsigned char mrrs[] = {0x00, 0x20, 0x78, 0xd5};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, stilp, sizeof(stilp), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rmIndex.reg == reg_A64_X0 + 2);
        DIANA_TEST_ASSERT((long long)result.linkedOperands[2].value.rmIndex.dispValue == -16);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.writeback == DIANA_ARM_WRITEBACK_PRE);
    }
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, mrrs, sizeof(mrrs), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_arm_sysreg);
    }
}

// addg x0, x1, #0x10, #0x3
// 91810c20: imm6=1 (scaled by 16) imm4=3 Rn=1 Rd=0
static void test_a64_addg()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x0c, 0x81, 0x91};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 4);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "addg") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[3].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[3].value.imm == 3);
    }
}

// ldr x0, <address + 8>
// 58000040: opc=01 imm19=2 (scaled by 4) Rt=0
// the literal is [pc + disp], relative to the next instruction like x86 rip: disp = 8 - 4
static void test_a64_ldr_literal()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x40, 0x00, 0x00, 0x58};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ldr") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_A64_PC);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_LITERAL);
    }
}

// ldrb w0, [x1]
// 39400020: size=00 opc=01 imm12=0 Rn=1 Rt=0
static void test_a64_ldrb()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x00, 0x40, 0x39};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ldrb") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_W0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_A64_X0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
    }
}

// ---- Control flow -----------------------------------------------------------

// b <address + 0x100>
// 14000040: imm26=0x40 (scaled by 4), rel = 0x100 - 4
static void test_a64_b()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x40, 0x00, 0x00, 0x14};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "b") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0x100 - 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.opKind == DIANA_A64_OPK_LABEL);
    }
}

// bl <address + 0x40>
// 94000010: imm26=0x10 (scaled by 4)
static void test_a64_bl()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x10, 0x00, 0x00, 0x94};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bl") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0x40 - 4);
    }
}

// b.ne <address + 0x10>
// 54000081: imm19=4 (scaled by 4) cond=0001 (ne); the group is "b.cond", the condition is the second operand
static void test_a64_b_cond()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x81, 0x00, 0x00, 0x54};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "b.cond") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.rel.m_value == 0x10 - 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_arm_cond);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.cond == 1);
    }
}

// cbz x0, <address + 0xc>
// b4000060: sf=1 op=0 imm19=3 (scaled by 4) Rt=0
static void test_a64_cbz()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x60, 0x00, 0x00, 0xb4};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "cbz") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rel.m_value == 0xc - 4);
    }
}

// tbnz w0, #3, <address + 0xc>
// 37180060: b5=0 op=1 b40=3 imm14=3 (scaled by 4) Rt=0
static void test_a64_tbnz()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x60, 0x00, 0x18, 0x37};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "tbnz") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_W0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 3);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_TBIT);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_rel);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.rel.m_value == 0xc - 4);
    }
}

// br x16
// d61f0200: opc=0000 Rn=16
static void test_a64_br()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x00, 0x02, 0x1f, 0xd6};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "br") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0 + 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
    }
}

// ret
// d65f03c0: opc=0010 Rn=30, the default x30 is an operand
static void test_a64_ret()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xc0, 0x03, 0x5f, 0xd6};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ret") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0 + 30);
    }
}

// adrp x0, <page of address + 0x3000>
// f0000000: op=1 immlo=3 immhi=0 Rd=0
// the target depends on the instruction address, so the operand is diana_arm_special with the page delta in imm
static void test_a64_adrp()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x00, 0x00, 0x00, 0xf0};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "adrp") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_arm_special);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x3000);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_ADRP);
    }
}

// ---- System -------------------------------------------------------------------

// mrs x0, tpidr_el0
// d53bd040: L=1 o0=1 op1=011 CRn=1101 CRm=0000 op2=010 Rt=0
// the system register is the raw o0:op1:CRn:CRm:op2 field (bits 5..19)
static void test_a64_mrs()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x40, 0xd0, 0x3b, 0xd5};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "mrs") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_arm_sysreg);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x5e82);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_SYSREG);
    }
}

// dmb ish
// d5033bbf: CRm=1011 (ish) op2=101
static void test_a64_dmb()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xbf, 0x3b, 0x03, 0xd5};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "dmb") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_arm_special);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.imm == 0xb);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.opKind == DIANA_A64_OPK_BARRIER);
    }
}

// ---- FP and SIMD ------------------------------------------------------------

// fadd s0, s1, s2
// 1e222820: ftype=00 (single) Rm=2 Rn=1 Rd=0
static void test_a64_fadd_scalar()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x28, 0x22, 0x1e};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fadd") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_S0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.opKind == DIANA_A64_OPK_FPREG);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.arrangement == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_S0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_S0 + 2);
        DIANA_TEST_ASSERT(result.linkedOperands[2].usedSize == 4);
    }
}

// fadd v0.4s, v1.4s, v2.4s
// 4e22d420: Q=1 sz=0 Rm=2 Rn=1 Rd=0
static void test_a64_fadd_vector()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0xd4, 0x22, 0x4e};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fadd") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_V0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.opKind == DIANA_A64_OPK_VREG);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.arrangement == DIANA_ARM_ARR_4S);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_V0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.arrangement == DIANA_ARM_ARR_4S);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.recognizedRegister == reg_A64_V0 + 2);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.arrangement == DIANA_ARM_ARR_4S);
    }
}

// aese v0.16b, v1.16b
// 4e284820: size=00 opcode=00100 Rn=1 Rd=0 (FEAT_AES)
static void test_a64_aese()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x48, 0x28, 0x4e};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "aese") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_V0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.arrangement == DIANA_ARM_ARR_16B);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_V0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.arrangement == DIANA_ARM_ARR_16B);
    }
}

// ld1 {v0.16b-v3.16b}, [x0]
// 4c402000: Q=1 L=1 opcode=0010 (four registers) size=00 Rn=0 Rt=0
// a list is one operand: the first register and arm.regCount
static void test_a64_ld1_list()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x00, 0x20, 0x40, 0x4c};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ld1") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_arm_reglist);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_V0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 16);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.opKind == DIANA_A64_OPK_VLIST);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.regCount == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.arrangement == DIANA_ARM_ARR_16B);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_index);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.reg == reg_A64_X0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.rmIndex.dispValue == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_MEM_BASE);
    }
}

// mov v0.s[1], w1 is ins v0.s[1], w1
// 4e0c1c20: Q=1 imm5=01100 (S, index 1) Rn=1 Rd=0
static void test_a64_ins_element()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x1c, 0x0c, 0x4e};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ins") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_V0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.opKind == DIANA_A64_OPK_VELEM);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.arrangement == DIANA_ARM_ARR_S);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.lane == 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_W0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].usedSize == 4);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_GPREG_IMM5);
    }
}

// fmov d0, #1.0
// 1e6e1000: ftype=01 (double) imm8=0x70 Rd=0, imm is the IEEE bit pattern of the expanded value
static void test_a64_fmov_imm()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x00, 0x10, 0x6e, 0x1e};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "fmov") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_D0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].usedSize == 8);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0x3ff0000000000000ULL);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_FPIMM);
    }
}

// movi v0.4s, #0xff
// 4f0707e0: Q=1 op=0 abc=000 cmode=0000 defgh=11111 Rd=0
static void test_a64_movi()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xe0, 0x07, 0x07, 0x4f};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 2);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 2);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movi") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_V0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.arrangement == DIANA_ARM_ARR_4S);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.imm == 0xff);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.opKind == DIANA_A64_OPK_MODIMM);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.shiftType == DIANA_ARM_SHIFT_LSL);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.shiftAmount == 0);
    }
}

// ushr v0.4s, v1.4s, #3
// 6f3d0420: Q=1 U=1 immh=0111 immb=101 (shift = 64 - 61) Rn=1 Rd=0
static void test_a64_ushr()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0x20, 0x04, 0x3d, 0x6f};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 3);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 3);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ushr") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_A64_V0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].arm.arrangement == DIANA_ARM_ARR_4S);
        DIANA_TEST_ASSERT(result.linkedOperands[1].type == diana_register);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_A64_V0 + 1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].arm.arrangement == DIANA_ARM_ARR_4S);
        DIANA_TEST_ASSERT(result.linkedOperands[2].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 3);
        DIANA_TEST_ASSERT(result.linkedOperands[2].arm.opKind == DIANA_A64_OPK_SHIFTIMM);
    }
}

// ---- Undefined --------------------------------------------------------------

// .inst 0xffffffff: an undefined word is still a 4-byte instruction, so a disassembler can go on
static void test_a64_undefined()
{
    DianaGroupInfo * pGroupInfo = 0;
    DianaParserResult result;
    size_t read;
    int iRes = 0;

    static unsigned char buf[] = {0xff, 0xff, 0xff, 0xff};
    iRes = Diana_ParseCmdOnBufferEx_test(DIANA_MODE64, DIANA_ARCH_AARCH64, buf, sizeof(buf), Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DIANA_TEST_ASSERT(DianaA64_IsUndefined(&result));
        DIANA_TEST_ASSERT(result.iLinkedOpCount == 1);
        DIANA_TEST_ASSERT(result.pInfo->m_operandCount == 1);
        DIANA_TEST_ASSERT(pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId));
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, ".inst") == 0);
        DIANA_TEST_ASSERT(result.linkedOperands[0].type == diana_imm);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.imm == 0xffffffff);
    }
}

void test_arm64_cmds()
{
    DIANA_TEST(test_a64_add_reg());
    DIANA_TEST(test_a64_add_sp_imm());
    DIANA_TEST(test_a64_sub_shifted());
    DIANA_TEST(test_a64_and_bitmask());
    DIANA_TEST(test_a64_movz());
    DIANA_TEST(test_a64_madd());
    DIANA_TEST(test_a64_csel());
    DIANA_TEST(test_a64_cmp_imm());
    DIANA_TEST(test_a64_ubfm());
    DIANA_TEST(test_a64_add_extended());

    DIANA_TEST(test_a64_ldr_pre());
    DIANA_TEST(test_a64_ldr_post());
    DIANA_TEST(test_a64_ldr_uoff());
    DIANA_TEST(test_a64_ldr_regoff());
    DIANA_TEST(test_a64_stp_pre());
    DIANA_TEST(test_a64_ldp_post());
    DIANA_TEST(test_a64_ldraa());
    DIANA_TEST(test_a64_addg());
    DIANA_TEST(test_a64_casp());
    DIANA_TEST(test_a64_cpyfp());
    DIANA_TEST(test_a64_sdot_element());
    DIANA_TEST(test_a64_fmlal_fcadd());
    DIANA_TEST(test_a64_rcpc3_d128());
    DIANA_TEST(test_a64_ldr_literal());
    DIANA_TEST(test_a64_ldrb());

    DIANA_TEST(test_a64_b());
    DIANA_TEST(test_a64_bl());
    DIANA_TEST(test_a64_b_cond());
    DIANA_TEST(test_a64_cbz());
    DIANA_TEST(test_a64_tbnz());
    DIANA_TEST(test_a64_br());
    DIANA_TEST(test_a64_ret());
    DIANA_TEST(test_a64_adrp());

    DIANA_TEST(test_a64_mrs());
    DIANA_TEST(test_a64_dmb());

    DIANA_TEST(test_a64_fadd_scalar());
    DIANA_TEST(test_a64_fadd_vector());
    DIANA_TEST(test_a64_aese());
    DIANA_TEST(test_a64_ld1_list());
    DIANA_TEST(test_a64_ins_element());
    DIANA_TEST(test_a64_fmov_imm());
    DIANA_TEST(test_a64_movi());
    DIANA_TEST(test_a64_ushr());

    DIANA_TEST(test_a64_undefined());
}
