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
}

void test_expressions()
{
    DIANA_TEST(test_expressions_names2());
    DIANA_TEST(test_expressions_names());
    DIANA_TEST(test_expressions_mult());
    DIANA_TEST(test_expressions_address());
    DIANA_TEST(test_expressions_summ());
    DIANA_TEST(test_expressions_invalid());
}