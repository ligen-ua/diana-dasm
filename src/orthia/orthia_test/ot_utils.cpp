#include "test_common.h"
#include "orthia_utils.h"

void test_expand_variable()
{
    DIANA_TEST_VAR(orthia::ExpandVariable(ORTHIA_TCSTR("hello")) == ORTHIA_TCSTR("hello"));
    DIANA_TEST_VAR(orthia::ExpandVariable(ORTHIA_TCSTR("%3DFB3833-C5F1-48e0-9499-BD8DBA2A0AD7%")) == ORTHIA_TCSTR(""));

    orthia::PlatformString_type res = orthia::ExpandVariable(ORTHIA_TCSTR("%SystemRoot%"));
    DIANA_TEST_ASSERT(!res.empty());
    DIANA_TEST_ASSERT(res.find(ORTHIA_TCSTR("%")) == res.npos);
}
static void test_trim()
{
    DIANA_TEST_VAR(orthia::Trim(ORTHIA_TCSTR("")) == ORTHIA_TCSTR(""));
    DIANA_TEST_VAR(orthia::Trim(ORTHIA_TCSTR("    ")) == ORTHIA_TCSTR(""));
    DIANA_TEST_VAR(orthia::Trim(ORTHIA_TCSTR("A")) == ORTHIA_TCSTR("A"));
    DIANA_TEST_VAR(orthia::Trim(ORTHIA_TCSTR(" Hello")) == ORTHIA_TCSTR("Hello"));
    DIANA_TEST_VAR(orthia::Trim(ORTHIA_TCSTR("Hello  ")) == ORTHIA_TCSTR("Hello"));
}

#ifdef WIN32
static void test_convert()
{
    DIANA_TEST_VAR(orthia::ToWideString("somestring") == ORTHIA_TCSTR("somestring"));
    DIANA_TEST_VAR(orthia::ToWideString("") == ORTHIA_TCSTR(""));
    DIANA_TEST_VAR(orthia::ToWideString(0x172620af25bb) == ORTHIA_TCSTR("172620af25bb"));
    DIANA_TEST_VAR(orthia::ToWideString(0) == ORTHIA_TCSTR("0"));
}
#endif
static void test_split()
{
    std::vector<orthia::PlatformString_type> res;
    orthia::Split(orthia::PlatformString_type(ORTHIA_TCSTR("word1|word2")), &res, ORTHIA_TCSTR('|'));
    DIANA_TEST_ASSERT(res.size() == 2);
    DIANA_TEST_ASSERT(res[0] == ORTHIA_TCSTR("word1"));
    DIANA_TEST_ASSERT(res[1] == ORTHIA_TCSTR("word2"));

    res.clear();
    orthia::Split(orthia::PlatformString_type(ORTHIA_TCSTR("|")), &res, ORTHIA_TCSTR('|'));
    DIANA_TEST_ASSERT(res.empty());

    res.clear();
    orthia::Split(orthia::PlatformString_type(ORTHIA_TCSTR("   1    ")), &res);
    DIANA_TEST_ASSERT(res.size() == 1);
    DIANA_TEST_ASSERT(res[0] == ORTHIA_TCSTR("1"));

    res.clear();
    orthia::Split(orthia::PlatformString_type(ORTHIA_TCSTR("   1        2")), &res);
    DIANA_TEST_ASSERT(res.size() == 2);
    DIANA_TEST_ASSERT(res[0] == ORTHIA_TCSTR("1"));
    DIANA_TEST_ASSERT(res[1] == ORTHIA_TCSTR("2"));

    res.clear();
    orthia::Split(orthia::PlatformString_type(ORTHIA_TCSTR("a b c")), &res);
    DIANA_TEST_ASSERT(res.size() == 3);
    DIANA_TEST_ASSERT(res[0] == ORTHIA_TCSTR("a"));
    DIANA_TEST_ASSERT(res[1] == ORTHIA_TCSTR("b"));
    DIANA_TEST_ASSERT(res[2] == ORTHIA_TCSTR("c"));

    res.clear();
    orthia::Split(orthia::PlatformString_type(ORTHIA_TCSTR("")), &res);
    DIANA_TEST_ASSERT(res.empty());
}
void test_utils()
{
    DIANA_TEST(test_split())
    DIANA_TEST_WIN32(test_convert())
    DIANA_TEST_WIN32(test_expand_variable())
    DIANA_TEST(test_trim())
}