#define _CRT_SECURE_NO_WARNINGS

#include "test_common.h"
#include "orthia_expressions.h"

static void test_expressions_address()
{
    auto resolver = std::make_shared< orthia::MapNameResolver>();
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR(" fffff806`b1458a9e "), resolver);
        DIANA_TEST_ASSERT(address == 0xfffff806b1458a9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR(" fffff806`b1458a9eh"), resolver);
        DIANA_TEST_ASSERT(address == 0xfffff806b1458a9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR(" + + + fffff806`b1458a9eh"), resolver);
        DIANA_TEST_ASSERT(address == 0xfffff806b1458a9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("+++fffff806`b1458a9eh"), resolver);
        DIANA_TEST_ASSERT(address == 0xfffff806b1458a9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR(" -  1"), resolver);
        DIANA_TEST_ASSERT(address == 0xffffffffffffffffULL);
    }

    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR(" -  0n15"), resolver);
        DIANA_TEST_ASSERT(address == 0xfffffffffffffff1ULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR(" - - 0xfffffffffffffffffh"), resolver);
        DIANA_TEST_ASSERT(address == 0xffffffffffffffffULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("--0xfffffffffffffffffh"), resolver);
        DIANA_TEST_ASSERT(address == 0xffffffffffffffffULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR(" + - + - 0"), resolver);
        DIANA_TEST_ASSERT(address == 0);
    }
}


static void test_expressions_summ()
{
    auto resolver = std::make_shared< orthia::MapNameResolver>();
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("0xfffffffffffffffffh + 2"), resolver);
        DIANA_TEST_ASSERT(address == 1);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("42 - 91"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFFFB1ULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("+ 42 + - 91"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFFFB1ULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("+ + 42 + - + 91"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFFFB1ULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("- 42 - - - 91"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFFF2DULL);
    }
}

static void test_expressions_mult()
{
    auto resolver = std::make_shared< orthia::MapNameResolver>();
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("0xffffffffffffffffh * 2"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFFFFEULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("91 / 42"), resolver);
        DIANA_TEST_ASSERT(address == 2);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("42 * - 91"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFda9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("42 * - 91 / 2"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FFFFFFFFFFFed4fULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("42 * - 91 / + 2"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FFFFFFFFFFFed4fULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("42 * - 91 / (1+1)"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FFFFFFFFFFFed4fULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("42 * - 91 / (1+1) * 2"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFda9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("1 + 2 * 3"), resolver);
        DIANA_TEST_ASSERT(address == 7);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("(1 + 2) * 3"), resolver);
        DIANA_TEST_ASSERT(address == 9);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("(1)"), resolver);
        DIANA_TEST_ASSERT(address == 1);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("9 * (1 + 2) * 4"), resolver);
        DIANA_TEST_ASSERT(address == 108);
    }
}

static void test_expressions_names()
{
    auto resolver = std::make_shared< orthia::MapNameResolver>();
    resolver->names[ORTHIA_TCSTR("t1")] = 0xffffffffffffffff;
    resolver->names[ORTHIA_TCSTR("t2")] = 0x2;
    resolver->names[ORTHIA_TCSTR("t3")] = 0x91;
    resolver->names[ORTHIA_TCSTR("t4")] = 0x42;
    resolver->names[ORTHIA_TCSTR("x")] = 0x1;
    resolver->names[ORTHIA_TCSTR("x3")] = 0x3;

    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t1 * t2"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFFFFEULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t3 / t4"), resolver);
        DIANA_TEST_ASSERT(address == 2);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4 * - t3"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFda9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4 * - t3 / t2"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FFFFFFFFFFFed4fULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4 * - t3 / + t2"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FFFFFFFFFFFed4fULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4 * - t3 / (x+x)"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FFFFFFFFFFFed4fULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4 * - t3 / (x+x) * t2"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFda9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("x + t2 * x3"), resolver);
        DIANA_TEST_ASSERT(address == 7);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("(x + t2) * x3"), resolver);
        DIANA_TEST_ASSERT(address == 9);
    }
}


static void test_expressions_names2()
{
    auto resolver = std::make_shared< orthia::MapNameResolver>();
    resolver->names[ORTHIA_TCSTR("t1.dll!$entry")] = 0xffffffffffffffff;
    resolver->names[ORTHIA_TCSTR("t2!a")] = 0x2;
    resolver->names[ORTHIA_TCSTR("t3!a")] = 0x91;
    resolver->names[ORTHIA_TCSTR("t4!a")] = 0x42;
    resolver->names[ORTHIA_TCSTR("x.dll!@test")] = 0x1;
    resolver->names[ORTHIA_TCSTR("x3")] = 0x3;

    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t1.dll!$entry * t2!a"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFFFFEULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t3!a / t4!a"), resolver);
        DIANA_TEST_ASSERT(address == 2);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4!a * - t3!a"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFda9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4!a * - t3!a / t2!a"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FFFFFFFFFFFed4fULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4!a * - t3!a / + t2!a"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FFFFFFFFFFFed4fULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4!a * - t3!a / (x.dll!@test+x.dll!@test)"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FFFFFFFFFFFed4fULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("t4!a * - t3!a / (x.dll!@test+x.dll!@test) * t2!a"), resolver);
        DIANA_TEST_ASSERT(address == 0xFFFFFFFFFFFFda9eULL);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("x.dll!@test + t2!a * x3"), resolver);
        DIANA_TEST_ASSERT(address == 7);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("(x.dll!@test + t2!a) * x3"), resolver);
        DIANA_TEST_ASSERT(address == 9);
    }
}

static void test_expressions_invalid()
{
    auto resolver = std::make_shared< orthia::MapNameResolver>();
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR(""), resolver), orthia::NoTokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("1 2"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("()"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("1!"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("(1) !"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("1 * !"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("1 * !name"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("1 (1 + 2)"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("(1 + 2) 1"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("1 * * 2"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("(1 + 2) * 3 *"), resolver), orthia::NoTokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("(1 + 2) 3"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("1 +"), resolver), orthia::NoTokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("(1"), resolver), orthia::NoTokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("1)"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("(1))"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("DS:[7FF769486040h]]"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("DS::[7FF769486040h]"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("FS::[7FF769486040h]"), resolver), orthia::TokenError);
    }
    {
        // a plain expression never accepts a length
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("1 L4"), resolver), orthia::TokenError);
    }
}

static void test_expressions_range()
{
    auto resolver = std::make_shared< orthia::MapNameResolver>();
    resolver->names[ORTHIA_TCSTR("t1")] = 0x5000;
    resolver->names[ORTHIA_TCSTR("x3")] = 0x3;
    resolver->names[ORTHIA_TCSTR("lstrlenA")] = 0x6000;
    resolver->names[ORTHIA_TCSTR("LdrLoadDll")] = 0x7000;
    resolver->names[ORTHIA_TCSTR("kernel32!lstrlenA")] = 0x8000;
    resolver->addresses[0x7FF769486040] = 0x9000;

    auto check = [&](const orthia::PlatformString_type& text, orthia::Address_type address, std::optional<orthia::Address_type> length) {
        auto range = orthia::CaptureAddressRangeExp(text, resolver);
        DIANA_TEST_ASSERT(range.address == address);
        DIANA_TEST_ASSERT(range.length == length);
    };
    check(ORTHIA_TCSTR("1000"), 0x1000, std::nullopt);
    check(ORTHIA_TCSTR("1000 L4"), 0x1000, 4);
    check(ORTHIA_TCSTR("1000 l4"), 0x1000, 4);
    check(ORTHIA_TCSTR("1000 L 4"), 0x1000, 4);
    check(ORTHIA_TCSTR("1000 l 4"), 0x1000, 4);
    check(ORTHIA_TCSTR("1000 L10"), 0x1000, 0x10);
    check(ORTHIA_TCSTR("1000 l(2*4)"), 0x1000, 8);
    check(ORTHIA_TCSTR("1000 L0n10"), 0x1000, 10);
    check(ORTHIA_TCSTR("1000 l4+1"), 0x1000, 5);
    check(ORTHIA_TCSTR("1000 L x3 * 2"), 0x1000, 6);
    check(ORTHIA_TCSTR("t1 lx3"), 0x5000, 3);
    check(ORTHIA_TCSTR("t1+10 l4"), 0x5010, 4);
    check(ORTHIA_TCSTR("(1+2) L4"), 3, 4);
    check(ORTHIA_TCSTR("2*(1+2) L4"), 6, 4);
    check(ORTHIA_TCSTR("poi(7FF769486040h) l2"), 0x9000, 2);
    check(ORTHIA_TCSTR("DS:[7FF769486040h] l2"), 0x7FF769486040, 2);
    check(ORTHIA_TCSTR("fffff806`b1458a9e l2"), 0xfffff806b1458a9eULL, 2);

    // names that start with L are symbols, not lengths
    check(ORTHIA_TCSTR("lstrlenA"), 0x6000, std::nullopt);
    check(ORTHIA_TCSTR("LdrLoadDll"), 0x7000, std::nullopt);
    check(ORTHIA_TCSTR("LdrLoadDll l2"), 0x7000, 2);
    check(ORTHIA_TCSTR("kernel32!lstrlenA L2"), 0x8000, 2);

    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR(""), resolver), orthia::NoTokenError);
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("L4"), resolver), orthia::NameNotFound);
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("1000 L"), resolver), orthia::NoTokenError);
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("1000 L4 5"), resolver), orthia::TokenError);
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("1000 L4 l5"), resolver), orthia::TokenError);
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("1000 X4"), resolver), orthia::TokenError);
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("1000 5"), resolver), orthia::TokenError);
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("(1 L4)"), resolver), orthia::TokenError);
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("(1 L4"), resolver), orthia::NoTokenError);
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("poi(1 L4)"), resolver), orthia::TokenError);
    for (auto text : { ORTHIA_TCSTR("1000 L?4"), ORTHIA_TCSTR("1000 L-4"), ORTHIA_TCSTR("1000 l -4"), ORTHIA_TCSTR("1000 l-x3") })
    {
        DIANA_TEST_EXCEPTION2(orthia::CaptureAddressRangeExp(text, resolver), const std::runtime_error& e)
        {
            DIANA_TEST_ASSERT(std::string(e.what()).find("not supported") != std::string::npos);
        }
    }
    DIANA_TEST_EXCEPTION(orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("1000 L(4"), resolver), orthia::NoTokenError);
}
static void test_expressions_segment_prefix()
{
    auto resolver = std::make_shared< orthia::MapNameResolver>();
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("DS:[7FF769486040h]"), resolver);
        DIANA_TEST_ASSERT(address == 0x7FF769486040);
    }
}

static void test_expressions_poi()
{
    auto resolver = std::make_shared< orthia::MapNameResolver>();
    resolver->addresses[0x7FF769486040] = 0xfffff806b1458a9e;
    resolver->addresses[0x7FF769486040*2] = 0x42;

    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(7FF769486040h)"), resolver);
        DIANA_TEST_ASSERT(address == 0xfffff806b1458a9e);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("poi((0x7FF769486040))"), resolver);
        DIANA_TEST_ASSERT(address == 0xfffff806b1458a9e);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(2*(0x7FF769486040))"), resolver);
        DIANA_TEST_ASSERT(address == 0x42);
    }
    {
        auto address = orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(DS:[7FF769486040h])"), resolver);
        DIANA_TEST_ASSERT(address == 0xfffff806b1458a9e);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi 7FF769486040h)"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(7FF769486040h"), resolver), orthia::NoTokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(7FF769486040h, )"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION2(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(7FF769486040h, 1)"), resolver), const std::runtime_error& e)
        {
            DIANA_TEST_ASSERT(std::string(e.what()).find("expects 1 argument") != std::string::npos);
        }
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi"), resolver), orthia::NoTokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi("), resolver), orthia::NoTokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(7FF769486040h,"), resolver), orthia::NoTokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi()"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(,1)"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(7FF769486040h,,1)"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(7FF769486040h))"), resolver), orthia::TokenError);
    }
    {
        DIANA_TEST_EXCEPTION(orthia::CaptureAddressExp(ORTHIA_TCSTR("poi(7FF769486040h)(1)"), resolver), orthia::TokenError);
    }
    {
        auto range = orthia::CaptureAddressRangeExp(ORTHIA_TCSTR("poi(7FF769486040h) L4"), resolver);
        DIANA_TEST_ASSERT(range.address == 0xfffff806b1458a9e);
        DIANA_TEST_ASSERT(range.length == 4);
    }

}

void test_expressions()
{   
    DIANA_TEST(test_expressions_poi());
    DIANA_TEST(test_expressions_segment_prefix());
    DIANA_TEST(test_expressions_names());
    DIANA_TEST(test_expressions_names2());
    DIANA_TEST(test_expressions_mult());
    DIANA_TEST(test_expressions_address());
    DIANA_TEST(test_expressions_summ());
    DIANA_TEST(test_expressions_invalid());
    DIANA_TEST(test_expressions_range());
}