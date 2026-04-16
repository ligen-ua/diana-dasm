extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
}
#include "test_common.h"

// ---- BMI1 ---------------------------------------------------------------

static void test_andn()
{
    // ANDN eax, ebx, ecx  (VEX.NDS.128.0F38.W0 F2 /r)
    // C4 E2 60 F2 C1
    // 0xE2 = map=0F38, no ext
    // 0x60 = W=0, vvvv=~1100=0011=EBX, L=0, pp=00
    // ModRM 0xC1 = 11_000_001 => reg=EAX(0), rm=ECX(1)
    static unsigned char buf[] = {0xC4, 0xE2, 0x60, 0xF2, 0xC1};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "andn") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_BMI1);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_blsr()
{
    // BLSR eax, ebx  (VEX.NDD.128.0F38.W0 F3 /1)
    // C4 E2 78 F3 CB
    // 0x78 = W=0, vvvv=~1111=0000=EAX, L=0, pp=00
    // ModRM 0xCB = 11_001_011 => reg=/1(opcode ext), rm=EBX(3)
    static unsigned char buf[] = {0xC4, 0xE2, 0x78, 0xF3, 0xCB};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "blsr") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_BMI1);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_blsmsk()
{
    // BLSMSK eax, ebx  (VEX.NDD.128.0F38.W0 F3 /2)
    // C4 E2 78 F3 D3
    // ModRM 0xD3 = 11_010_011 => reg=/2, rm=EBX(3)
    static unsigned char buf[] = {0xC4, 0xE2, 0x78, 0xF3, 0xD3};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "blsmsk") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_BMI1);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_blsi()
{
    // BLSI eax, ebx  (VEX.NDD.128.0F38.W0 F3 /3)
    // C4 E2 78 F3 DB
    // ModRM 0xDB = 11_011_011 => reg=/3, rm=EBX(3)
    static unsigned char buf[] = {0xC4, 0xE2, 0x78, 0xF3, 0xDB};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "blsi") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_BMI1);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

// ---- BMI2 ---------------------------------------------------------------

static void test_bzhi()
{
    // BZHI eax, ecx, ebx  (VEX.NDS.128.0F38.W0 F5 /r)
    // Intel: BZHI r32a, r/m32, r32b => dest=eax(reg), src=ecx(rm), ctrl=ebx(vvvv)
    // Spec order: (dest, ctrl/vvvv, src/rm) => eax, ebx, ecx
    // C4 E2 60 F5 C1 but vvvv=EBX(3): 0x60 = 0_1100_0_00 => vvvv=~1100=0011=EBX
    // ModRM 0xC1 = 11_000_001 => reg=EAX(0), rm=ECX(1)
    static unsigned char buf[] = {0xC4, 0xE2, 0x60, 0xF5, 0xC1};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "bzhi") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_BMI2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_rorx()
{
    // RORX eax, ebx, 1  (VEX.128.F2.0F3A.W0 F0 /r ib)
    // C4 E3 7B F0 C3 01
    // 0xE3 = map=0F3A
    // 0x7B = W=0, vvvv=~1111=0, L=0, pp=11(F2)
    // ModRM 0xC3 = 11_000_011 => reg=EAX(0), rm=EBX(3)
    static unsigned char buf[] = {0xC4, 0xE3, 0x7B, 0xF0, 0xC3, 0x01};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "rorx") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_BMI2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_sarx()
{
    // SARX eax, ecx, ebx  (VEX.NDS.128.F3.0F38.W0 F7 /r)
    // Intel: SARX r32a, r/m32, r32b => dest=eax(reg), src=ecx(rm), shift=ebx(vvvv)
    // C4 E2 62 F7 C1
    // 0x62 = W=0, vvvv=~1100=0011=EBX(3), L=0, pp=10(F3)
    // ModRM 0xC1 = 11_000_001 => reg=EAX(0), rm=ECX(1)
    static unsigned char buf[] = {0xC4, 0xE2, 0x62, 0xF7, 0xC1};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "sarx") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_BMI2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_pdep()
{
    // PDEP eax, ebx, ecx  (VEX.NDS.128.F2.0F38.W0 F5 /r)
    // C4 E2 63 F5 C1
    // 0x63 = W=0, vvvv=~1100=0011=EBX(3), L=0, pp=11(F2)
    // ModRM 0xC1 = 11_000_001 => reg=EAX(0), rm=ECX(1)
    static unsigned char buf[] = {0xC4, 0xE2, 0x63, 0xF5, 0xC1};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pdep") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_BMI2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_pext()
{
    // PEXT eax, ebx, ecx  (VEX.NDS.128.F3.0F38.W0 F5 /r)
    // C4 E2 62 F5 C1
    // 0x62 = W=0, vvvv=~1100=0011=EBX(3), L=0, pp=10(F3)
    // ModRM 0xC1 = 11_000_001 => reg=EAX(0), rm=ECX(1)
    static unsigned char buf[] = {0xC4, 0xE2, 0x62, 0xF5, 0xC1};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "pext") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_BMI2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

// ---- Standalone instructions -------------------------------------------

static void test_popcnt()
{
    // POPCNT eax, ebx  (F3 0F B8 /r)
    static unsigned char buf[] = {0xF3, 0x0F, 0xB8, 0xC3};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "popcnt") == 0);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_lzcnt()
{
    // LZCNT eax, ebx  (F3 0F BD /r)
    static unsigned char buf[] = {0xF3, 0x0F, 0xBD, 0xC3};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "lzcnt") == 0);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_tzcnt()
{
    // TZCNT eax, ebx  (F3 0F BC /r)
    static unsigned char buf[] = {0xF3, 0x0F, 0xBC, 0xC3};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "tzcnt") == 0);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_crc32()
{
    // CRC32 eax, ebx  (F2 0F 38 F1 /r)
    static unsigned char buf[] = {0xF2, 0x0F, 0x38, 0xF1, 0xC3};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "crc32") == 0);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_adcx()
{
    // ADCX eax, ebx  (66 0F 38 F6 /r)
    static unsigned char buf[] = {0x66, 0x0F, 0x38, 0xF6, 0xC3};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "adcx") == 0);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_adox()
{
    // ADOX eax, ebx  (F3 0F 38 F6 /r)
    static unsigned char buf[] = {0xF3, 0x0F, 0x38, 0xF6, 0xC3};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "adox") == 0);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_movbe()
{
    // MOVBE eax, [ecx]  (0F 38 F0 /r, memory source)
    static unsigned char buf[] = {0x0F, 0x38, 0xF0, 0x01};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "movbe") == 0);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_endbr64()
{
    // ENDBR64  (F3 0F 1E FA)
    static unsigned char buf[] = {0xF3, 0x0F, 0x1E, 0xFA};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "endbr64") == 0);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_endbr32()
{
    // ENDBR32  (F3 0F 1E FB)
    static unsigned char buf[] = {0xF3, 0x0F, 0x1E, 0xFB};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE32, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "endbr32") == 0);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

// ---- SHA extension -----------------------------------------------------

static void test_sha1nexte()
{
    // SHA1NEXTE xmm1, xmm2  (0F 38 C8 /r)
    static unsigned char buf[] = {0x0F, 0x38, 0xC8, 0xCA};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "sha1nexte") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SHA_EXT);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

static void test_sha1rnds4()
{
    // SHA1RNDS4 xmm1, xmm2, 1  (0F 3A CC /r ib)
    static unsigned char buf[] = {0x0F, 0x3A, 0xCC, 0xCA, 0x01};
    DianaParserResult result;
    size_t read = 0;
    int iRes = Diana_ParseCmdOnBuffer_test(DIANA_MODE64, buf, sizeof(buf),
                                           Diana_GetRootLine(), &result, &read);
    DIANA_TEST_ASSERT_IF(!iRes)
    {
        DianaGroupInfo * pGroupInfo = Diana_GetGroupInfo(result.pInfo->m_lGroupId);
        DIANA_TEST_ASSERT(pGroupInfo);
        DIANA_TEST_ASSERT(strcmp(pGroupInfo->m_pName, "sha1rnds4") == 0);
        DIANA_TEST_ASSERT(result.pInfo->m_flags & DI_FLAG_CMD_SHA_EXT);
        DIANA_TEST_ASSERT(result.linkedOperands[0].value.recognizedRegister == reg_XMM1);
        DIANA_TEST_ASSERT(result.linkedOperands[1].value.recognizedRegister == reg_XMM2);
        DIANA_TEST_ASSERT(result.linkedOperands[2].value.imm == 1);
        DIANA_TEST_ASSERT(read == sizeof(buf));
    }
}

void test_bmi()
{
    // BMI1
    DIANA_TEST(test_andn());
    DIANA_TEST(test_blsr());
    DIANA_TEST(test_blsmsk());
    DIANA_TEST(test_blsi());
    // BMI2
    DIANA_TEST(test_bzhi());
    DIANA_TEST(test_rorx());
    DIANA_TEST(test_sarx());
    DIANA_TEST(test_pdep());
    DIANA_TEST(test_pext());
    // Standalone
    DIANA_TEST(test_popcnt());
    DIANA_TEST(test_lzcnt());
    DIANA_TEST(test_tzcnt());
    DIANA_TEST(test_crc32());
    DIANA_TEST(test_adcx());
    DIANA_TEST(test_adox());
    DIANA_TEST(test_movbe());
    DIANA_TEST(test_endbr64());
    DIANA_TEST(test_endbr32());
    // SHA
    DIANA_TEST(test_sha1nexte());
    DIANA_TEST(test_sha1rnds4());
}
