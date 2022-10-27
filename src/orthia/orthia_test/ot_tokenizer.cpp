#define _CRT_SECURE_NO_WARNINGS

#include "test_common.h"
#include "orthia_tokenizer.h"

static void test_empty_tokenizer()
{
    orthia::CStreamTokenFileSource sources[] = {"", "    ", "    \t    \t", "  //  hi there\n//", "\n", "\n\n"};
    size_t count = sizeof(sources)/sizeof(sources[0]);

    orthia::CTokenizerEnv env;
    for(size_t i = 0; i < count; ++i)
    {
        orthia::CStreamTokenFileSource & source = sources[i]; 
        env.ResetSource(&source);

        orthia::Token token;

        env.GetTokenizer().TestPopulateAllCaches();
        bool res = env.GetNextToken(&token);
        DIANA_TEST_ASSERT(res);
        DIANA_TEST_ASSERT(token.type == orthia::Token::ttEOF);

        env.GetTokenizer().TestPopulateAllCaches();
        res = env.GetNextToken(&token);
        DIANA_TEST_ASSERT(!res);

        env.GetTokenizer().TestPopulateAllCaches();
        res = env.GetNextToken(&token);
        DIANA_TEST_ASSERT(!res);
    }
}

struct PositionInfo
{
    int line;
    int column;
};

struct TokenTest
{
    orthia::CTokenizerEnv env;

    void TestTokens(orthia::CStreamTokenFileSource * sources, 
                               orthia::Token * tokens, 
                               size_t count)
    {
        
        for(size_t i = 0; i < count; ++i)
        {
            orthia::CStreamTokenFileSource & source = sources[i]; 
            env.ResetSource(&source);

            orthia::Token token;
            env.GetTokenizer().TestPopulateAllCaches();
            bool res = env.GetNextToken(&token);
            DIANA_TEST_ASSERT(res);

            const orthia::Token & token2compare = tokens[i];
            DIANA_TEST_ASSERT(token2compare == token);

            env.GetTokenizer().TestPopulateAllCaches();
            res = env.GetNextToken(&token);
            DIANA_TEST_ASSERT(res);
            DIANA_TEST_ASSERT(token.type == orthia::Token::ttEOF);
        }
    }

    void TestTokens(orthia::CStreamTokenFileSource & source, 
                       orthia::Token * tokens, 
                       size_t count)
    {
        env.ResetSource(&source);
        for(size_t i = 0; i < count; ++i)
        {
            orthia::Token token;

            env.GetTokenizer().TestPopulateAllCaches();
            bool res = env.GetNextToken(&token);
            DIANA_TEST_ASSERT(res);
            if (token.type == orthia::Token::ttReservedWord)
            {
                DIANA_TEST_ASSERT(token.reservedWordId);
                token.reservedWordId = 0;
            }
            const orthia::Token & token2compare = tokens[i];
            DIANA_TEST_ASSERT(token2compare == token);
        }
    }

    template <class NativeType>
    void TestValues(orthia::CStreamTokenFileSource * pSources,
                          const orthia::Token & prototypeToken, 
                          const NativeType * pData, 
                          size_t count)
    {
        orthia::CBinaryTokenStorage storage;
        std::vector<orthia::Token> tokens;
        tokens.reserve(count);
        for(size_t i = 0; i < count; ++i)
        {
            orthia::Token token = prototypeToken;
            storage.RegisterTokenData(&token, &pData[i], sizeof(NativeType));
            tokens.push_back(token);
        }
        TestTokens(pSources, &tokens[0], count);
    }


    template <class NativeType>
    void TestValues(orthia::CStreamTokenFileSource & source,
                          const orthia::Token & prototypeToken, 
                          const NativeType * pData, 
                          PositionInfo * pPositions,
                          size_t count)
    {
        orthia::CBinaryTokenStorage storage;
        std::vector<orthia::Token> tokens;
        tokens.reserve(count);
        for(size_t i = 0; i < count; ++i)
        {
            orthia::Token token = prototypeToken;
            token.column = pPositions[i].column;
            token.line = pPositions[i].line;
            storage.RegisterTokenData(&token, &pData[i], sizeof(NativeType));
            tokens.push_back(token);
        }
        TestTokens(source, &tokens[0], count);
    }

    template<class CharType>
    void TestStrings(orthia::CStreamTokenFileSource & source,
                          const orthia::Token & prototypeToken, 
                          const std::basic_string<CharType> * pData, 
                          PositionInfo * pPositions,
                          size_t count,
                          bool addZero = true)
    {
        orthia::CBinaryTokenStorage storage;
        std::vector<orthia::Token> tokens;
        tokens.reserve(count);
        const int extraChar = addZero?1:0;
        for(size_t i = 0; i < count; ++i)
        {
            orthia::Token token = prototypeToken;
            token.column = pPositions[i].column;
            token.line = pPositions[i].line;
            storage.RegisterTokenData(&token, 
                                      (const char*)pData[i].c_str(), 
                                      (pData[i].size() + extraChar) * sizeof(CharType));
            tokens.push_back(token);
        }
        TestTokens(source, &tokens[0], count);
    }
};

struct PositionsBuilderAddSpace
{
};
struct PositionsBuilder
{
    std::vector<PositionInfo> positions;
    orthia::CStreamTokenFileSource & source;
    size_t col;
    size_t line;

    PositionsBuilder(orthia::CStreamTokenFileSource & source_in)
        :
            source(source_in),
            col(0),
            line(0)
    {
    }
    PositionsBuilder & operator << (const PositionsBuilderAddSpace & )
    {
        ++col;
        source.GetStream()<<" ";
        return *this;
    }
    PositionsBuilder & operator << (const std::string & str)
    {
        RegTestItem(str);
        return *this;
    }
    void RegTestItem(const std::string & str)
    {
        const int minCapacity = 100;
        if (positions.capacity() < minCapacity)
        {
            positions.reserve(minCapacity);
        }

        PositionInfo pos;
        pos.column = (int)col;
        pos.line = (int)line;
        positions.push_back(pos);

        source.GetStream()<<str;
        col += str.size();
    }
};

// --- test int --
static void test_int_tokenizer()
{
    char maxBin[1024];
    _i64toa(MAXINT, maxBin, 2);

    char maxOct[100];
    _i64toa(MAXINT, maxOct, 8);
    
    orthia::CStreamTokenFileSource sources[] = {"42", 
                                                "0x42", 
                                                "042", 
                                                "0b101011010",
                                                "0000000000000000000000000000000000000000000001",
                                                orthia::ObjectToString_Ansi(MAXINT),
                                                "0x" + orthia::ToAnsiStringAsHex(MAXINT),
                                                std::string("0") + maxOct,
                                                std::string("0b") + maxBin,
                                                "0"
    };
    
    size_t count = sizeof(sources)/sizeof(sources[0]);

    int values[] = {42,
                    0x42,
                    042,
                    0x15A,
                    1,
                    MAXINT,
                    MAXINT,
                    MAXINT,
                    MAXINT,
                    0
    };

    orthia::Token token = {orthia::Token::ttLiteral, orthia::Token::ttLiteralInt, 0, true, 0, 0, 0, 0, 0, 0};

    TokenTest test;
    test.TestValues(sources, token, values, count);
}

static void test_uint_tokenizer()
{
    char maxBin[1024];
    _i64toa(MAXUINT, maxBin, 2);

    char maxOct[100];
    _i64toa(MAXUINT, maxOct, 8);
    
    orthia::CStreamTokenFileSource source;
    
    PositionsBuilder positionsBuilder(source);
    positionsBuilder<<"42U"
                      <<"0x42U"
                      <<"042U"
                      <<"0b101011010U"
                      <<"0000000000000000000000000000000000000000000001U"
                      <<orthia::ObjectToString_Ansi(MAXUINT) + "U"
                      <<"0x" + orthia::ToAnsiStringAsHex(MAXUINT)+ "U"
                      <<std::string("0") + maxOct + "U"
                      <<std::string("0b") + maxBin + "U"
                      <<"0U";

    unsigned int values[] = {42,
                    0x42,
                    042,
                    0x15A,
                    1,
                    MAXUINT,
                    MAXUINT,
                    MAXUINT,
                    MAXUINT,
                    0
    };

    size_t count = sizeof(values)/sizeof(values[0]);

    orthia::Token token = {orthia::Token::ttLiteral, orthia::Token::ttLiteralInt, 0, false, 0, 0, 0, 0, 0, 0};
    
    TokenTest test;
    test.TestValues(source, 
               token, 
               values, 
               &positionsBuilder.positions[0], 
               count);
}

// --- test int64 --
static void test_int64_tokenizer()
{
    char maxBin[1024];
    _i64toa(MAXINT64, maxBin, 2);

    char maxOct[100];
    _i64toa(MAXINT64, maxOct, 8);
    
    orthia::CStreamTokenFileSource sources[] = {"42LL", 
                                                "0x42LL", 
                                                "042LL", 
                                                "0b101011010LL",
                                                "0000000000000000000000000000000000000000000001LL",
                                                orthia::ObjectToString_Ansi(MAXINT64) + "LL",
                                                "0x" + orthia::ToAnsiStringAsHex(MAXINT64) + "LL",
                                                std::string("0") + maxOct + "LL",
                                                std::string("0b") + maxBin + "LL",
                                                "0LL"
    };
    
    size_t count = sizeof(sources)/sizeof(sources[0]);

    __int64 values[] = {42,
                    0x42,
                    042,
                    0x15A,
                    1,
                    MAXINT64,
                    MAXINT64,
                    MAXINT64,
                    MAXINT64,
                    0
    };

    orthia::Token token = {orthia::Token::ttLiteral, orthia::Token::ttLiteralInt, 0, true, 0, 0, 0, 0, 0, 0};
    
    TokenTest test;
    test.TestValues(sources, token, values, count);
}

static void test_uint64_tokenizer()
{
    char maxBin[1024];
    _i64toa(MAXUINT64, maxBin, 2);

    char maxOct[100];
    _i64toa(MAXUINT64, maxOct, 8);
    
    orthia::CStreamTokenFileSource source;
    
    PositionsBuilder positionsBuilder(source);
    positionsBuilder<<"42ULL"
                      <<"0x42ULL"
                      <<"042ULL"
                      <<"0b101011010ULL"
                      <<"0000000000000000000000000000000000000000000001ULL"
                      <<orthia::ObjectToString_Ansi(MAXUINT64) + "ULL"
                      <<"0x" + orthia::ToAnsiStringAsHex(MAXUINT64)+ "ULL"
                      <<std::string("0") + maxOct + "ULL"
                      <<std::string("0b") + maxBin + "ULL"
                      <<"0ULL";

    unsigned __int64 values[] = {42,
                    0x42,
                    042,
                    0x15A,
                    1,
                    MAXUINT64,
                    MAXUINT64,
                    MAXUINT64,
                    MAXUINT64,
                    0
    };

    size_t count = sizeof(values)/sizeof(values[0]);

    orthia::Token token = {orthia::Token::ttLiteral, orthia::Token::ttLiteralInt, 0, false, 0, 0, 0, 0, 0, 0};
    
    TokenTest test;
    test.TestValues(source, 
               token, 
               values, 
               &positionsBuilder.positions[0], 
               count);
}

// --- test string --
static void test_string_tokenizer()
{
    orthia::CStreamTokenFileSource source;
    
    PositionsBuilder positionsBuilder(source);
    positionsBuilder<<"\"\""
                    <<"\"test\""
                    <<"\"\\1234\""
                    <<"\"\\x123\""
                    <<"\"\\a\\b\\f\\r\\n\\t\\v\"";

    std::string values[] = {"",
                            "test",
                            "\1234",
                            "\x12""3", // cpp produces warning, orthia tokenizer produces error, that's ok
                            "\a\b\f\r\n\t\v"
    };

    size_t count = sizeof(values)/sizeof(values[0]);

    orthia::Token token = {orthia::Token::ttLiteral, orthia::Token::ttLiteralString, 0, false, 0, 0, 0, 0, 0, 0};
    
    TokenTest test;
    test.TestStrings(source, 
               token, 
               values, 
               &positionsBuilder.positions[0], 
               count);
}

// --- test wstring --
static void test_wstring_tokenizer()
{
    orthia::CStreamTokenFileSource source;
    
    wchar_t buf[] = L"te\0st";
    std::wstring testZero(buf, buf + sizeof(buf)/sizeof(buf[0]) - 1);
    PositionsBuilder positionsBuilder(source);
    positionsBuilder<<"L\"\""
                    <<"L\"test\""
                    <<"L\"\\1234567\""
                    <<"L\"\\x12345\""
                    <<"L\"\\a\\b\\f\\r\\n\\t\\v\""
                    <<"L\"te\\0st\"";

    std::wstring values[] = {L"",
                            L"test",
                            L"\1234567",
                            L"\x1234" L"5", // cpp produces warning, orthia tokenizer produces error, that's ok
                            L"\a\b\f\r\n\t\v",
                            testZero
    };

    size_t count = sizeof(values)/sizeof(values[0]);

    orthia::Token token = {orthia::Token::ttLiteral, orthia::Token::ttLiteralWideString, 0, false, 0, 0, 0, 0, 0, 0};
    
    TokenTest test;
    test.TestStrings(source, 
               token, 
               values, 
               &positionsBuilder.positions[0], 
               count);
}

// --- test char --
static void test_char_tokenizer()
{
    orthia::CStreamTokenFileSource source;
    
    PositionsBuilder positionsBuilder(source);
    positionsBuilder<<"\'\\0\'"
                    <<"\'t\'"
                    <<"\'\\123\'"
                    <<"\'\\x12\'"
                    <<"\'\\a\'"
                    <<"\'\\b\'"
                    <<"\'\\f\'"
                    <<"\'\\r\'"
                    <<"\'\\n\'"
                    <<"\'\\t\'"
                    <<"\'\\v\'";

    const char values[] = {'\0',
                           't',
                           '\123',
                           '\x12', // cpp produces warning, orthia tokenizer produces error, that's ok
                           '\a',
                           '\b',
                           '\f',
                           '\r',
                           '\n',
                           '\t',
                           '\v'
    };

    size_t count = sizeof(values)/sizeof(values[0]);

    orthia::Token token = {orthia::Token::ttLiteral, orthia::Token::ttLiteralChar, 0, false, 0, 0, 0, 0, 0, 0};
    
    TokenTest test;
    test.TestValues(source, 
               token, 
               values, 
               &positionsBuilder.positions[0], 
               count);
}

// --- test wchar_t --
static void test_wchar_tokenizer()
{
    orthia::CStreamTokenFileSource source;
    
    PositionsBuilder positionsBuilder(source);
    positionsBuilder<<"L\'\\0\'"
                    <<"L\'t\'"
                    <<"L\'\\123\'"
                    <<"L\'\\x1234\'"
                    <<"L\'\\a\'"
                    <<"L\'\\b\'"
                    <<"L\'\\f\'"
                    <<"L\'\\r\'"
                    <<"L\'\\n\'"
                    <<"L\'\\t\'"
                    <<"L\'\\v\'";

    const wchar_t values[] = {L'\0',
                           L't',
                           L'\123',
                           L'\x1234', // cpp produces warning, orthia tokenizer produces error, that's ok
                           L'\a',
                           L'\b',
                           L'\f',
                           L'\r',
                           L'\n',
                           L'\t',
                           L'\v'
    };

    size_t count = sizeof(values)/sizeof(values[0]);

    orthia::Token token = {orthia::Token::ttLiteral, orthia::Token::ttLiteralWideChar, 0, false, 0, 0, 0, 0, 0, 0};
    
    TokenTest test;
    test.TestValues(source, 
               token, 
               values, 
               &positionsBuilder.positions[0], 
               count);
}


// --- test name --
static void test_name_tokenizer()
{
    orthia::CStreamTokenFileSource source;
    
    PositionsBuilder positionsBuilder(source);
    positionsBuilder<<"hello"<<PositionsBuilderAddSpace()
                    <<"world";

    std::string values[] = {"hello",
                            "world"
    };

    size_t count = sizeof(values)/sizeof(values[0]);

    orthia::Token token = {orthia::Token::ttReservedWord, orthia::Token::ttLiteralNone, 0, false, 0, 0, 0, 0, 0, 0};
    
    TokenTest test;
    test.env.GetReservedWordsStorage().AddReservedWord("hello");
    test.env.GetReservedWordsStorage().AddReservedWord("world");
    test.TestStrings(source, 
               token, 
               values, 
               &positionsBuilder.positions[0], 
               count,
               false);
}


// various
static void test_various_tokenizer()
{
    orthia::CStreamTokenFileSource source;
    source.GetStream()<<"/* this \n is \n comment */ \n ; // and this \n  a/**/b 1/***/2  c//*b\n/*\n*/d*/";
    
    orthia::CTokenizerEnv env;
    env.ResetSource(&source);

    orthia::Token tokens[] = {
        {orthia::Token::ttSpecialSign, orthia::Token::ttLiteralNone, ';', false, 3,  1, 0, 0, 0, 0},
        {orthia::Token::ttName,        orthia::Token::ttLiteralNone,   0, false, 4,  2, 0, 0, 0, 0},
        {orthia::Token::ttName,        orthia::Token::ttLiteralNone,   0, false, 4,  7, 0, 0, 0, 0},
        {orthia::Token::ttLiteral,     orthia::Token::ttLiteralInt,    0, true,  4,  9, 0, 0, 0, 0},
        {orthia::Token::ttLiteral,     orthia::Token::ttLiteralInt,    0, true,  4, 15, 0, 0, 0, 0},
        {orthia::Token::ttName,        orthia::Token::ttLiteralNone,   0, false, 4, 18, 0, 0, 0, 0},
        {orthia::Token::ttName,        orthia::Token::ttLiteralNone,   0, false, 6,  2, 0, 0, 0, 0},
        {orthia::Token::ttSpecialSign, orthia::Token::ttLiteralNone, '*', false, 6,  3, 0, 0, 0, 0},
        {orthia::Token::ttSpecialSign, orthia::Token::ttLiteralNone, '/', false, 6,  4, 0, 0, 0, 0},
        {orthia::Token::ttEOF,         orthia::Token::ttLiteralNone,   0, false, 6,  5, 0, 0, 0, 0},
    };
    
    env.GetBinaryStorage().RegisterTokenData(&tokens[1], "a", 1);
    env.GetBinaryStorage().RegisterTokenData(&tokens[2], "b", 1);
    env.GetBinaryStorage().RegisterTokenData(&tokens[5], "c", 1);
    env.GetBinaryStorage().RegisterTokenData(&tokens[6], "d", 1);

    const __int32 value1 = 1;
    const __int32 value2 = 2;
    env.GetBinaryStorage().RegisterTokenData(&tokens[3], &value1, sizeof(value1));
    env.GetBinaryStorage().RegisterTokenData(&tokens[4], &value2, sizeof(value2));

    size_t count = sizeof(tokens)/sizeof(tokens[0]);
    for(size_t i = 0; i < count; ++i)
    {
        orthia::Token token;
        env.GetTokenizer().TestPopulateAllCaches();
        bool res = env.GetNextToken(&token);
        DIANA_TEST_ASSERT(res);
            
        const orthia::Token & token2compare = tokens[i];        
        DIANA_TEST_ASSERT(token2compare == token);    }
}

static void test_tokenizer_errors()
{
    unsigned long long maxIntPlus1 = MAXINT64;
    ++maxIntPlus1;
    
    char incorrectIntBin[1024];
    _i64toa(maxIntPlus1, incorrectIntBin, 2);

    char incorrectIntOct[100];
    _i64toa(maxIntPlus1, incorrectIntOct, 8);

    char incorrectIntHex[100];
    _i64toa(maxIntPlus1, incorrectIntHex, 16);

    orthia::CStreamTokenFileSource sources[] = {
        "L\"\\xxx\"",
        "L\"\\s\"",
        "/*hi",
        "184467440737095516160ULL",
        "5ULULU",
        std::string("0b") + incorrectIntBin,
        std::string("0") + incorrectIntOct,
        std::string("0x") + incorrectIntHex,
        
    };
    size_t count = sizeof(sources)/sizeof(sources[0]);

    orthia::CTokenizerEnv env;
    for(size_t i = 0; i < count; ++i)
    {
        orthia::CStreamTokenFileSource & source = sources[i]; 
        env.ResetSource(&source);
        
        orthia::Token token;
        DIANA_TEST_EXCEPTION( env.GetNextToken(&token), std::exception );
    }
}

// operator
static void AddOperatorToken(std::vector<orthia::Token> & tokens, 
                             const std::string & op)
{
    orthia::Token token;
    memset(&token, 0, sizeof(token));
    token.type = orthia::Token::ttSpecialSign;
    if (op.size() >= orthia::TokenMaxOperatorSize)
    {
        throw std::runtime_error("Incorrect test");
    }
    memcpy(token.multiByteChar, op.c_str(), op.size());
    tokens.push_back(token);
}
static void test_operator_tokenizer()
{
    orthia::CStreamTokenFileSource source;
    source.GetStream()<<"?|^><%*/+-~!&.[]()=:,{},;!===>=<=||&&->--++<<>>|=^=&=%=/=*=-=+=<<=>>=...";

    orthia::CTokenizerEnv env;
    env.ResetSource(&source);

    std::vector<orthia::Token> tokens;
    AddOperatorToken(tokens, "?");
    AddOperatorToken(tokens, "|");
    AddOperatorToken(tokens, "^");
    AddOperatorToken(tokens, ">");
    AddOperatorToken(tokens, "<");
    AddOperatorToken(tokens, "%");
    AddOperatorToken(tokens, "*");
    AddOperatorToken(tokens, "/");
    AddOperatorToken(tokens, "+");
    AddOperatorToken(tokens, "-");
    AddOperatorToken(tokens, "~");
    AddOperatorToken(tokens, "!");
    AddOperatorToken(tokens, "&");
    AddOperatorToken(tokens, ".");
    AddOperatorToken(tokens, "[");
    AddOperatorToken(tokens, "]");
    AddOperatorToken(tokens, "(");
    AddOperatorToken(tokens, ")");
    AddOperatorToken(tokens, "=");
    AddOperatorToken(tokens, ":");
    AddOperatorToken(tokens, ",");
    AddOperatorToken(tokens, "{");
    AddOperatorToken(tokens, "}");
    AddOperatorToken(tokens, ",");
    AddOperatorToken(tokens, ";");
    AddOperatorToken(tokens, "!=");
    AddOperatorToken(tokens, "==");
    AddOperatorToken(tokens, ">=");
    AddOperatorToken(tokens, "<=");
    AddOperatorToken(tokens, "||");
    AddOperatorToken(tokens, "&&");
    AddOperatorToken(tokens, "->");
    AddOperatorToken(tokens, "--");
    AddOperatorToken(tokens, "++");
    AddOperatorToken(tokens, "<<");
    AddOperatorToken(tokens, ">>");
    AddOperatorToken(tokens, "|=");
    AddOperatorToken(tokens, "^=");
    AddOperatorToken(tokens, "&=");
    AddOperatorToken(tokens, "%=");
    AddOperatorToken(tokens, "/=");
    AddOperatorToken(tokens, "*=");
    AddOperatorToken(tokens, "-=");
    AddOperatorToken(tokens, "+=");
    AddOperatorToken(tokens, "<<=");
    AddOperatorToken(tokens, ">>=");
    AddOperatorToken(tokens, "...");

    size_t count = tokens.size();
    for(size_t i = 0; i < count; ++i)
    {
        orthia::Token token;
        env.GetTokenizer().TestPopulateAllCaches();
        bool res = env.GetNextToken(&token);
        DIANA_TEST_ASSERT(res);
            
        const orthia::Token & token2compare = tokens[i];        
        DIANA_TEST_ASSERT(token2compare.type == token.type);
        DIANA_TEST_ASSERT(token2compare.operatorValue == token.operatorValue);
    }
}


void test_tokenizer()
{
    DIANA_TEST(test_operator_tokenizer());
    DIANA_TEST(test_empty_tokenizer());
    DIANA_TEST(test_int_tokenizer());
    DIANA_TEST(test_uint_tokenizer());
    DIANA_TEST(test_int64_tokenizer());
    DIANA_TEST(test_uint64_tokenizer());
    DIANA_TEST(test_char_tokenizer());
    DIANA_TEST(test_wchar_tokenizer());
    DIANA_TEST(test_string_tokenizer());
    DIANA_TEST(test_wstring_tokenizer());    
    DIANA_TEST(test_name_tokenizer());
    DIANA_TEST(test_various_tokenizer());
    DIANA_TEST(test_tokenizer_errors());
}