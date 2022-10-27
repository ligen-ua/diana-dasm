#include "test_common.h"
#include "orthia_utils.h"

void test_expand_variable()
{
    DIANA_TEST_VAR(orthia::ExpandVariable(L"hello") == L"hello");
    DIANA_TEST_VAR(orthia::ExpandVariable(L"%3DFB3833-C5F1-48e0-9499-BD8DBA2A0AD7%") == L"");

    std::wstring res = orthia::ExpandVariable(L"%SystemRoot%");
    DIANA_TEST_ASSERT(!res.empty());
    DIANA_TEST_ASSERT(res.find(L"%") == res.npos);
}
static void test_trim()
{
    DIANA_TEST_VAR(orthia::Trim(L"") == L"");
    DIANA_TEST_VAR(orthia::Trim(L"    ") == L"");
    DIANA_TEST_VAR(orthia::Trim(L"A") == L"A");
    DIANA_TEST_VAR(orthia::Trim(L" Hello") == L"Hello");
    DIANA_TEST_VAR(orthia::Trim(L"Hello  ") == L"Hello");
}
static void test_convert()
{
    DIANA_TEST_VAR(orthia::ToWideString("somestring") == L"somestring");
    DIANA_TEST_VAR(orthia::ToWideString("") == L"");
    DIANA_TEST_VAR(orthia::ToWideString(0x172620af25bb) == L"172620af25bb");
    DIANA_TEST_VAR(orthia::ToWideString(0) == L"0");
}
static void test_split()
{
    std::vector<std::wstring> res;
    orthia::Split(std::wstring(L"word1|word2"), &res, L'|');
    DIANA_TEST_ASSERT(res.size() == 2);
    DIANA_TEST_ASSERT(res[0] == L"word1");
    DIANA_TEST_ASSERT(res[1] == L"word2");

    res.clear();
    orthia::Split(std::wstring(L"|"), &res, L'|');
    DIANA_TEST_ASSERT(res.empty());

    res.clear();
    orthia::Split(std::wstring(L"   1    "), &res);
    DIANA_TEST_ASSERT(res.size() == 1);
    DIANA_TEST_ASSERT(res[0] == L"1");

    res.clear();
    orthia::Split(std::wstring(L"   1        2"), &res);
    DIANA_TEST_ASSERT(res.size() == 2);
    DIANA_TEST_ASSERT(res[0] == L"1");
    DIANA_TEST_ASSERT(res[1] == L"2");

    res.clear();
    orthia::Split(std::wstring(L"a b c"), &res);
    DIANA_TEST_ASSERT(res.size() == 3);
    DIANA_TEST_ASSERT(res[0] == L"a");
    DIANA_TEST_ASSERT(res[1] == L"b");
    DIANA_TEST_ASSERT(res[2] == L"c");

    res.clear();
    orthia::Split(std::wstring(L""), &res);
    DIANA_TEST_ASSERT(res.empty());
}
void test_utils()
{
    DIANA_TEST(test_split())
    DIANA_TEST(test_convert())
    DIANA_TEST(test_expand_variable())
    DIANA_TEST(test_trim())
}