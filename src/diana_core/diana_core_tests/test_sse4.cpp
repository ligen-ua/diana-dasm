extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}
#include "test_common.h"

static void test_pmulld()
{
    // PMULLD xmm1, xmm2  (66 0F 38 40 /r)
    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0x40, 0xCA};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pmulld") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SSE41);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_pcmpeqq()
{
    // PCMPEQQ xmm1, xmm2  (66 0F 38 29 /r)
    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0x29, 0xCA};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pcmpeqq") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SSE41);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_roundpd()
{
    // ROUNDPD xmm1, xmm2, 1  (66 0F 3A 09 /r ib)
    static unsigned char buf[] = {0x66, 0x0F, 0x3A, 0x09, 0xCA, 0x01};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "roundpd") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SSE41);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 1);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_blendps()
{
    // BLENDPS xmm1, xmm2, 0x0F  (66 0F 3A 0C /r ib)
    static unsigned char buf[] = {0x66, 0x0F, 0x3A, 0x0C, 0xCA, 0x0F};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "blendps") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SSE41);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 0x0F);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_pmaxsb()
{
    // PMAXSB xmm1, xmm2  (66 0F 38 3C /r)
    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0x3C, 0xCA};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pmaxsb") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SSE41);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_pmovsxbw()
{
    // PMOVSXBW xmm1, xmm2  (66 0F 38 20 /r)
    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0x20, 0xCA};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pmovsxbw") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SSE41);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_ptest()
{
    // PTEST xmm1, xmm2  (66 0F 38 17 /r)
    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0x17, 0xCA};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "ptest") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SSE41);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_pcmpgtq()
{
    // PCMPGTQ xmm1, xmm2  (66 0F 38 37 /r) -- SSE4.2
    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0x37, 0xCA};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pcmpgtq") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SSE42);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_pcmpistri()
{
    // PCMPISTRI xmm1, xmm2, 1  (66 0F 3A 63 /r ib) -- SSE4.2
    static unsigned char buf[] = {0x66, 0x0F, 0x3A, 0x63, 0xCA, 0x01};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pcmpistri") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SSE42);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 1);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

void test_sse4()
{
    DIANA_TEST(test_pmulld());
    DIANA_TEST(test_pcmpeqq());
    DIANA_TEST(test_roundpd());
    DIANA_TEST(test_blendps());
    DIANA_TEST(test_pmaxsb());
    DIANA_TEST(test_pmovsxbw());
    DIANA_TEST(test_ptest());
    DIANA_TEST(test_pcmpgtq());
    DIANA_TEST(test_pcmpistri());
}
