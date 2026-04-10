#include "orthia_tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <cinttypes>
namespace orthia
{   
struct InvalidUtf8Exception :public std::runtime_error
{
    InvalidUtf8Exception()
        :
        std::runtime_error("Invalid UTF8")
    {
    }
};


static
bool isValidUTF8(const char* data, int size, size_t& ascii_count) {
    size_t i = 0;
    ascii_count = 0;

    while (i < size) {
        unsigned char byte = data[i];

        // ASCII (0-127)
        if (byte <= 0x7F) {
            ascii_count++;
            i++;
        }
        // 2-byte UTF-8 sequence (110xxxxx 10xxxxxx)
        else if ((byte & 0xE0) == 0xC0) {
            if (i + 1 >= size || (data[i + 1] & 0xC0) != 0x80) {
                return false;
            }
            i += 2;
        }
        // 3-byte UTF-8 sequence (1110xxxx 10xxxxxx 10xxxxxx)
        else if ((byte & 0xF0) == 0xE0) {
            if (i + 2 >= size ||
                (data[i + 1] & 0xC0) != 0x80 ||
                (data[i + 2] & 0xC0) != 0x80) {
                return false;
            }
            i += 3;
        }
        // 4-byte UTF-8 sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
        else if ((byte & 0xF8) == 0xF0) {
            if (i + 3 >= size ||
                (data[i + 1] & 0xC0) != 0x80 ||
                (data[i + 2] & 0xC0) != 0x80 ||
                (data[i + 3] & 0xC0) != 0x80) {
                return false;
            }
            i += 4;
        }
        else {
            return false;
        }
    }

    return true;
}


static bool IsUTF8SpecialByte(char t) {
    
    size_t asciiCount = 0;
    return isValidUTF8(&t, 1, asciiCount) == false;
}

orthia::PlatformString_type ReadString(const Token& token)
{
    orthia::PlatformString_type res;
    if (!token.pBinaryTokenStorage)
    {
        return res;
    }
    if (token.type != Token::ttName)
    {
        return res;
    }
    auto buffer = (char*)token.pBinaryTokenStorage->QueryData(token.tokenOffset, token.tokenSize);
    std::string utf8(buffer, buffer + token.tokenSize);
    return Utf8ToPlatformString(utf8);
}
bool operator == (const Token & token1, const Token & token2)
{
    bool commonPropsAreEqual = 
            token1.type == token2.type &&
            token1.literalType == token2.literalType &&
            token1.signCharCode == token2.signCharCode &&
            token1.signedValue == token2.signedValue &&
            token1.line == token2.line &&
            token1.column == token2.column &&
            token1.reservedWordId == token2.reservedWordId &&
            token1.tokenSize == token2.tokenSize;
    if (!commonPropsAreEqual)
    {
        return false;
    }
    if (!token1.pBinaryTokenStorage && !token2.pBinaryTokenStorage)
    {
        return true;
    }
    if (token1.pBinaryTokenStorage && token2.pBinaryTokenStorage)
    {
        const void * pData1 = token1.pBinaryTokenStorage->QueryData(token1.tokenOffset, token1.tokenSize);
        const void * pData2 = token2.pBinaryTokenStorage->QueryData(token2.tokenOffset, token1.tokenSize);
        return memcmp(pData1, pData2, token1.tokenSize) == 0;
    }
    return false;
}

// CBinaryTokenStorage
CBinaryTokenStorage::CBinaryTokenStorage(size_t hintToReserve)
    :
        m_lastPos(0)
{
    m_storage.reserve(hintToReserve);
}

void CBinaryTokenStorage::Clear()
{
    m_lastPos = 0;
    m_storage.clear();
}
const void * CBinaryTokenStorage::QueryData(size_t offset, size_t size)
{
    size_t endSize = offset + size;
    if (endSize < offset || endSize < size)
    {
        throw std::runtime_error("Overflow");
    }
    if (endSize > m_storage.size())
    {
        throw std::runtime_error("Token overflow");
    }
    if (m_storage.empty())
    {
        return 0;
    }
    return &m_storage[offset];
}
size_t CBinaryTokenStorage::RegisterTokenData(Token * pToken, const void * pRawData, size_t size)
{
    size_t offset = m_storage.size();
    const char * pBegin = (const char * )pRawData;
    const char * pEnd = pBegin + size;
    if (m_storage.size() - m_storage.capacity() < size)
    {
        m_storage.reserve(m_storage.capacity()*2); 
    }
    m_storage.insert(m_storage.end(), pBegin, pEnd);

    pToken->tokenOffset = offset;
    pToken->tokenSize = size;
    pToken->pBinaryTokenStorage = this;
    return offset;
}

// CReservedWordsStorage
CReservedWordsStorage::CReservedWordsStorage()
    :
        m_lastRWId(0)
{
}
int CReservedWordsStorage::GetReservedWord_Silent(const std::string & word) const
{
    ReservedWordsMap_type::const_iterator it = m_reservedWords.find(word);
    if (it == m_reservedWords.end())
    {
        return 0;
    }
    return it->second;
}
int CReservedWordsStorage::AddReservedWord(const std::string & word)
{
    std::pair<ReservedWordsMap_type::iterator, bool> res = m_reservedWords.insert(std::make_pair(word, m_lastRWId+1));
    if (res.second)
    {
        return ++m_lastRWId;
    }
    return res.first->second;
}


// sign matchers
static bool DotMatcher(int index, 
                       char ch, 
                       char originalChar,
                       bool * pCanContacenate, 
                       bool * pResultWillbeInvalid,
                       bool * pResultWillbeFinal)
{
    switch (index)
    {
    case 1:
        *pCanContacenate = ch == '.';
        *pResultWillbeInvalid = true;
        return true;
    case 2:
        *pCanContacenate = ch == '.';
        *pResultWillbeInvalid = false;
        *pResultWillbeFinal = true;
        return true; 
    }
    return false;
}
static bool MinusMatcher(int index, 
                            char ch, 
                            char originalChar,
                            bool * pCanContacenate, 
                            bool * pResultWillbeInvalid,
                            bool * pResultWillbeFinal)
{
    if (index != 1)
        return false;

    *pResultWillbeFinal = true;
    *pCanContacenate = ch == originalChar || ch == '=' || ch == '>';
    return true;
}
static bool DuplicationOrWithEqualMatcher(int index, 
                            char ch, 
                            char originalChar,
                            bool * pCanContacenate, 
                            bool * pResultWillbeInvalid,
                            bool * pResultWillbeFinal)
{
    if (index != 1)
        return false;

    *pResultWillbeFinal = true;
    *pCanContacenate = ch == originalChar || ch == '=';
    return true;
}
static bool WithEqualMatcher(int index, 
                            char ch, 
                            char originalChar,
                            bool * pCanContacenate, 
                            bool * pResultWillbeInvalid,
                            bool * pResultWillbeFinal)
{
    if (index != 1)
        return false;

    *pResultWillbeFinal = true;
    *pCanContacenate = ch == '=';
    return true;
}

static bool LessMoreMatcher(int index, 
                            char ch, 
                            char originalChar,
                            bool * pCanContacenate, 
                            bool * pResultWillbeInvalid,
                            bool * pResultWillbeFinal)
{
    switch (index)
    {
    case 1:
        if (ch == originalChar)
        {
            *pCanContacenate = true;
            return true;
        }
        if (ch == '=')
        {
            *pResultWillbeFinal = true;
            *pCanContacenate = true;
            return true;
        }
        return true;
    case 2:
        *pCanContacenate = ch == '=';
        *pResultWillbeFinal = true;
        return true; 
    }
    return false;
}

static bool DuplicationMatcher(int index, 
                            char ch, 
                            char originalChar,
                            bool * pCanContacenate, 
                            bool * pResultWillbeInvalid,
                            bool * pResultWillbeFinal)
{
    if (index != 1)
        return false;

    *pResultWillbeFinal = true;
    *pCanContacenate = ch == originalChar;
    return true;
}

// CTokenizer
CTokenizer::CTokenizer(CBinaryTokenStorage * pBinaryTokenStorage, 
                       CReservedWordsStorage * pReservedWordsStorage,
                       ITokenFileSource * pTokenFileSource)
    :
        m_pBinaryTokenStorage(pBinaryTokenStorage),
        m_pTokenFileSource(pTokenFileSource),
        m_pReservedWordsStorage(pReservedWordsStorage),
        m_lineSize(0),
        m_eofReached(false),
        m_lineNumber(-1),
        m_columnPos(0),
        m_inComment(false),
        m_windbgStyle(false)
{
    m_line.resize(1024);
    m_tempStorage.reserve(1024);
}
void CTokenizer::RaiseError(const std::string & type)
{
    if (type.empty())
    {
        throw std::runtime_error("Error");
    }
    throw std::runtime_error(type);
}
void CTokenizer::RaiseWarning(const std::string & type)
{
}
char CTokenizer::ReadOneOrDie(const std::string & error)
{
    if (m_columnPos >= m_lineSize)
    {
        RaiseError(error);
        return 0;
    }
    return m_line[m_columnPos++];
}
char CTokenizer::HasCurrent() const
{
    if ((m_columnPos) >= m_lineSize)
    {
        return 0;
    }
    return m_line[m_columnPos];
}
char CTokenizer::HasOneMore() const
{
    if ((m_columnPos+1) >= m_lineSize)
    {
        return 0;
    }
    return m_line[m_columnPos+1];
}
void CTokenizer::RegisterTempTokenData(Token * pToken)
{
    if (!m_tempStorage.empty())
    {
        RegisterTokenData(pToken, &m_tempStorage[0], m_tempStorage.size());
        m_tempStorage.clear();
    }
}
void CTokenizer::AddToTempStorage(const void * pRawData, size_t size)
{
    m_tempStorage.insert(m_tempStorage.end(),
                        (char*)pRawData,
                        (char*)pRawData + size);
}
void CTokenizer::RegisterTokenData(Token * pToken, const void * pRawData, size_t size)
{
    m_pBinaryTokenStorage->RegisterTokenData(pToken, pRawData, size);
}
Token CTokenizer::BuildNewToken(Token::TokenType_type type)
{
    Token token;
    memset(&token, 0, sizeof(token));

    token.type = type;
    token.line = m_lineNumber;
    token.column = m_columnPos;
    return token;
}
bool CTokenizer::CaptureSign(Token * pToken, SymbolMatcherFnc_type matcher)
{
    if (m_line[m_columnPos] == '/')
    {
        if (HasOneMore() == '/')
        {
            // comment
            m_columnPos += 2;
            return false;
        }
        if (HasOneMore() == '*')
        {
            m_columnPos += 2;
            m_inComment = true;
            return false;
        }
    }
    *pToken = BuildNewToken(Token::ttSpecialSign);
    pToken->signCharCode = (char)m_line[m_columnPos++];

    if (matcher)
    {
        // can match more
        bool resultInvalid = false;
        for(int i = 1; i < TokenMaxOperatorSize; ++i)
        {
            bool canContacenate = false;
            bool resultIsFinal = false;
            bool resultWillBeInvalid = false;
            char ch = HasCurrent();
            if (!matcher(i, ch, pToken->signCharCode, &canContacenate, &resultWillBeInvalid, &resultIsFinal))
            {
                RaiseError("Internal error");
            }
            if (!canContacenate)
            {
                break;
            }
            resultInvalid = resultWillBeInvalid;
            pToken->multiByteChar[i] = ch;
            ++m_columnPos;
            if (resultIsFinal)
            {
                break;
            }
        }
        if (resultInvalid)
        {
            RaiseError("Invalid sign sequence");
        }
    }
    return true;
}
wchar_t CTokenizer::CaptureEscapedChar(bool wide)
{
    ReadOneOrDie();
    wchar_t ch = ReadOneOrDie();
    char * endOfSequence = 0;
    char buffer[7] = {0,0,0,0,0,0,0};
    switch(ch)
    {
    case '\'':
    case '\"':
    case '\?':
    case '\\':
        return ch;
    case 'a':
        return 0x07;
    case 'b':
        return 0x08;
    case 'f':
        return 0x0c;
    case 'n':
        return 0x0a;
    case 'r':
        return 0x0d;
    case 't':
        return 0x09;
    case 'v':
        return 0x0b;
    case 'x':
        // hex sequence
        buffer[0] = ReadOneOrDie();
        buffer[1] = ReadOneOrDie();
        if (wide)
        {
            buffer[2] = ReadOneOrDie();
            buffer[3] = ReadOneOrDie();
        }
        ch = (wchar_t)strtol(buffer, &endOfSequence, 16);
        if (*endOfSequence)
        {
            RaiseError("Invalid hex literal");
        }
        return ch;
    default:
        // octal sequence
        buffer[0] = (char)ch;
        if (isdigit(HasCurrent()))
        {
            buffer[1] = ReadOneOrDie();
            if (isdigit(HasCurrent()))
            {
                buffer[2] = ReadOneOrDie();
            }
        }
        ch = (wchar_t)strtol(buffer, &endOfSequence, 8);
        if (*endOfSequence)
        {
            RaiseError("Invalid dec literal");
        }
        return ch;
    }
}
bool CTokenizer::CaptureStringLiteral(Token::LiteralType_type literalType, 
                                      Token * pToken,
                                      int columnPos)
{
    *pToken = BuildNewToken(Token::ttLiteral);
    if (columnPos != -1)
    {
        pToken->column = columnPos;
    }
    pToken->literalType = literalType;
    ++m_columnPos;

    m_tempStorage.clear();
    bool success = false;
    bool isWide = literalType == Token::ttLiteralWideChar || literalType == Token::ttLiteralWideString;
    bool isChar = literalType == Token::ttLiteralChar || literalType == Token::ttLiteralWideChar;
    while(m_columnPos < m_lineSize)
    {
        wchar_t wideChar = 0;
        char ch = m_line[m_columnPos];
        switch(ch)
        {
        case '\\':
            wideChar = CaptureEscapedChar(isWide);
            if (isWide)
            {
                AddToTempStorage(&wideChar, sizeof(wideChar));
            }
            else
            {
                m_tempStorage.push_back((char)wideChar);
            }
            continue;

        case '\'':
            if (isChar)
            {
                ++m_columnPos;
                success = true;
            }
            break;
        case '\"':
            if (!isChar)
            {
                ++m_columnPos;
                success = true;
            }
            break;
        }
        if (success)
        {
            break;
        }
        m_tempStorage.push_back(ch);
        if (isWide)
        {
            m_tempStorage.push_back(0);
        }
        ++m_columnPos;
    }

    if (success)
    {
        if (isChar)
        {
            if (m_tempStorage.empty())
            {
                RaiseError("Empty character constant");
            }
            if (m_tempStorage.size() > 1)
            {
                RaiseWarning("Char literal of unusual size");
            }
        }
        else
        {
            m_tempStorage.push_back(0);
            if (isWide)
            {
                m_tempStorage.push_back(0);
            }
        }
        RegisterTempTokenData(pToken);
        return true;
    }
    RaiseError("Invalid literal");
    return false;
}

int portable_ui64toa(uint64_t value, char* buffer, size_t size, int radix) {
    if (buffer == NULL || size == 0) {
        return EINVAL;
    }

    int len = -1;

    switch (radix) {
        case 10:
            len = snprintf(buffer, size, "%" PRIu64, value);
            break;
        case 16:
            len = snprintf(buffer, size, "%" PRIx64, value); /* Lowercase hex */
            break;
        case 8:
            len = snprintf(buffer, size, "%" PRIo64, value);
            break;
        case 2:
            /* snprintf does not support binary, so we handle it manually */
            {
                if (value == 0) {
                    if (size < 2) return ERANGE;
                    buffer[0] = '0';
                    buffer[1] = '\0';
                    return 0;
                }

                /* Calculate number of bits required */
                uint64_t temp = value;
                int bits = 0;
                while (temp != 0) {
                    temp >>= 1;
                    bits++;
                }

                if ((size_t)bits + 1 > size) {
                    return ERANGE;
                }

                /* Fill buffer backwards */
                buffer[bits] = '\0';
                for (int i = bits - 1; i >= 0; i--) {
                    buffer[i] = (value & 1) ? '1' : '0';
                    value >>= 1;
                }
                return 0;
            }
        default:
            buffer[0] = '\0';
            return EINVAL; /* Unsupported radix */
    }

    /* Handle snprintf errors */
    if (len < 0) {
        return EINVAL; /* Encoding error */
    }
    
    /* Check for truncation (snprintf returns required length) */
    if ((size_t)len >= size) {
        return ERANGE; /* Buffer too small */
    }

    return 0;
}

bool CTokenizer::CaptureDigitLiteral(Token * pToken)
{
    *pToken = BuildNewToken(Token::ttLiteral);
    pToken->literalType = Token::ttLiteralInt;

    typedef enum {itDec, itOctal, itHex, itBinary} IntType_type;

    // analyze and process first symbol
    IntType_type type = itDec;
    int radix = 10;
    if (m_windbgStyle)
    {
        type = itHex;
        radix = 16;
    }
    m_tempStorage.clear();
    if (m_line[m_columnPos] == '0')
    {
        if (!m_windbgStyle)
        {
            type = itOctal;
            radix = 8;
        }
        // can be octal, binary or hex
        char secondChar = HasOneMore();
        switch(secondChar)
        {
        case 'n':
            m_columnPos += 2;
            type = itDec;
            radix = 10;
            break;
        case 'x':
        case 'X':
            m_columnPos += 2;
            type = itHex;
            radix = 16;
            break;

        case 'b':
            m_columnPos += 2;
            type = itBinary;
            radix = 2;
            break;
        default:
            // add dec zero
            m_tempStorage.push_back(m_line[m_columnPos++]);
        }
    }
    else
    {
        m_tempStorage.push_back(m_line[m_columnPos++]);
    }

    // go through body
    while(m_columnPos < m_lineSize)
    {
        char ch = m_line[m_columnPos];
        bool unknownSymbol = false;
        switch(ch)
        {
            case '0':
            case '1':
                break;

            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (type == itBinary)
                {
                    throw std::runtime_error("Invalid sequence");
                }
                break;

            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
            case 'A':
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
                if (type != itHex)
                {
                    throw std::runtime_error("Invalid sequence");
                }
                break;

            default:
                unknownSymbol = true;
                break;
        }
        if (unknownSymbol)
        {
            break;
        }
        m_tempStorage.push_back(m_line[m_columnPos++]);
    }

    // sequence is ready
    m_tempStorage.push_back(0);
    char * endOfSequence = 0;
    unsigned long long value = strtoull(&m_tempStorage[0], &endOfSequence, radix);
    if (*endOfSequence)
    {
        RaiseError("Can't convert literal");
        return false;
    }
    if (value == ULLONG_MAX)
    {
        // can ve overflow
        m_tempStorage2 = m_tempStorage;
        portable_ui64toa(value, &m_tempStorage2[0], m_tempStorage2.size(), radix);
        size_t size2 = strlen(&m_tempStorage2[0]);
        size_t zeroBytesOffset = m_tempStorage2.size() - size2 - 1;
        if (DIANA_STRICMP(&m_tempStorage[0] + zeroBytesOffset, &m_tempStorage2[0]) != 0)
        {
            RaiseError("Integer overflow");
        }
    }

    // analyse suffixes
    // u
    // l
    // ll
    bool wasU = false;
    bool wasL = false;
    bool wasLL = false;
    for(;;)
    {
        bool found = false;
        char firstSuffixChar = HasCurrent();
        switch (firstSuffixChar)
        {

        case 'h':
            if (m_windbgStyle)
            {
                ++m_columnPos;
                break;
            }
            break;

        case 'u':
        case 'U':
            if (wasU)
            {
                RaiseError("Invalid U");
                return false;
            }
            wasU = true;
            found = true;
            break;
        case 'l':
        case 'L':
            if (wasL)
            {
                if (wasLL)
                {
                    RaiseError("Invalid L");
                    return false;
                }
                wasLL = true;
            }
            wasL = true;
            found = true;
            break;
        }
        if (!found)
            break;

        ++m_columnPos;
    }
    if (wasLL || m_windbgStyle)
    {
        if (wasU || m_windbgStyle)
        {
            RegisterTokenData(pToken, &value, sizeof(value));
        }
        else
        {
            DI_INT64 signedValue = (DI_INT64)value;
            if (signedValue < 0 || (DI_UINT64)signedValue != value)
            {
                RaiseError("Constant too big");
                return false;
            }
            RegisterTokenData(pToken, &signedValue, sizeof(signedValue));
            pToken->signedValue = true;
        }
    }
    else
    {     
        DI_UINT32 value32 = (DI_UINT32)value;
        if ((DI_UINT64)value32 != value)
        {
            RaiseError("Constant too big");
            return false;
        }
        if (wasU)
        {
            RegisterTokenData(pToken, &value32, sizeof(value32));
        }
        else
        {
            DI_INT32 signedValue = (DI_INT32)value32;
            if (signedValue < 0 || (DI_UINT32)signedValue != value32)
            {
                RaiseError("Constant too big");
                return false;
            }
            RegisterTokenData(pToken, &signedValue, sizeof(signedValue));
            pToken->signedValue = true;
        }
    }
    return true;
}

static
bool IsHexChar(char ch)
{
    if ((ch >= '0' && ch <= '9') ||
        (ch >= 'a' && ch <= 'f') ||
        (ch >= 'A' && ch <= 'F'))
    {
        return true;
    }
    return false;
}
bool CTokenizer::IsOtherNameSymbol(char ch)
{
    if (m_windbgStyle)
    {
        return ch == '@' || ch == '$' || ch == '.' || ch == '#' || ch == '!' || ch == '?';
    }
    return false;
}

bool CTokenizer::CaptureName(Token * pToken, int flags)
{
    if (m_line[m_columnPos] == 'L')
    {
        char secondChar = HasOneMore();
        if (secondChar == '\'')
        {   
            ++m_columnPos;
            return CaptureStringLiteral(Token::ttLiteralWideChar, pToken, m_columnPos-1);
        }
        if (secondChar == '\"')
        {
            ++m_columnPos;
            return CaptureStringLiteral(Token::ttLiteralWideString, pToken, m_columnPos-1);
        }
    }

    auto originalColumnPos = m_columnPos;
    // this is name
    *pToken = BuildNewToken(Token::ttName);

    bool isHexInt = IsHexChar(m_line[m_columnPos]);
    bool hFound = false;

    m_tempStorageStr.clear();
    m_tempStorageStr.push_back(m_line[m_columnPos++]);
    while(m_columnPos < m_lineSize)
    {
        char ch = m_line[m_columnPos];
        bool hexChar = false;
        if (isHexInt)
        {
            hexChar = IsHexChar(ch);
        }
        if ((ch >= '0' && ch <= '9') ||
            (ch>='a' && ch<='z') || 
            (ch>='A' && ch<='Z') ||
            (ch == '_') ||
            IsOtherNameSymbol(ch) ||
            IsUTF8SpecialByte((unsigned char)ch))
        {
            if (isHexInt && !hexChar)
            {
                if (ch == 'h')
                {
                    if (hFound)
                    {
                        isHexInt = false;
                    }
                    hFound = true;
                }   
                else
                {
                    isHexInt = false;
                }
            }
            m_tempStorageStr.push_back(m_line[m_columnPos++]);
            continue;
        }
        break;
    }

    if (m_windbgStyle)
    {
        if (!(flags & flags_ForceGetName))
        {
            if (isHexInt)
            {
                m_columnPos = originalColumnPos;
                return CaptureDigitLiteral(pToken);
            }
        }
    }
    pToken->reservedWordId = m_pReservedWordsStorage->GetReservedWord_Silent(m_tempStorageStr);
    if (pToken->reservedWordId)
    {
        pToken->type = Token::ttReservedWord;
    }
    RegisterTokenData(pToken, m_tempStorageStr.c_str(), m_tempStorageStr.size());
    m_tempStorageStr.clear();
    return true;
}

void CTokenizer::TestPopulateAllCaches()
{
    std::string str("TestPopulateAllCaches");
    if (m_tempStorageStr.empty())
    {
        m_tempStorageStr.append(str);
    }
    if (m_tempStorage.empty())
    {
        AddToTempStorage(str.c_str(), str.size());
    }
}

void CTokenizer::ResetSource(ITokenFileSource * pTokenFileSource)
{
    m_pTokenFileSource = pTokenFileSource;
}
void CTokenizer::Clear()
{
    m_lineSize = 0;
    m_eofReached = false;
    m_lineNumber = -1;
    m_columnPos = 0;
    m_tempStorage.clear();
    m_tempStorageStr.clear();
    m_inComment = false;
}
std::string CTokenizer::GetNextRawString()
{
    if (m_eofReached)
    {
        return 0;
    }
    if (!m_pTokenFileSource)
    {
        return 0;
    }
    std::string res;
    for (;;)
    {
        while (!m_lineSize || m_columnPos >= m_lineSize)
        {
            if (!m_pTokenFileSource->GetNextLine(&m_line, &m_lineSize))
            {
                return res;
            }
            m_columnPos = 0;
        }
        res.append(m_line.begin() + m_columnPos, m_line.begin() + m_lineSize);
        m_columnPos = (int)m_lineSize;
    }
}
bool CTokenizer::GetNextToken(Token * pToken, int flags)
{
    if (m_eofReached)
    {
        return false;
    }
    if (!m_pTokenFileSource)
    {
        throw std::runtime_error("No source");
    }

    for(;;)
    {
        while (!m_lineSize || m_columnPos >= m_lineSize)
        {
            if (!m_pTokenFileSource->GetNextLine(&m_line, &m_lineSize))
            {
                if (m_inComment)
                {
                    // c-style comment at the end of file
                    RaiseError("Unexpected end of file found in comment");
                    return false;
                }
                *pToken = BuildNewToken(Token::ttEOF);
                m_eofReached = true;
                return true;
            }
            ++m_lineNumber;
            m_columnPos = 0;
        }
        // at this point we can expect valid non empty line
        char ch = m_line[m_columnPos];
        if (m_inComment)
        {
            if (ch == '*' && HasOneMore() == '/')
            {
                // exit of the comment
                m_columnPos += 2;
                m_inComment = false;
                continue;
            }
            ++m_columnPos;
            continue;
        }

        if (flags & flags_ForceGetName)
        {
            return CaptureName(pToken, flags);
        }
        SymbolMatcherFnc_type symbolMatcher = 0;
        switch(ch)
        {  
            // skip whitespaces
            case 9:
            case 10: 
            case 13: 
            case ' ': ++m_columnPos;
                continue;

            case '.':
                symbolMatcher = DotMatcher;
                break;

            case '>':
            case '<':
                symbolMatcher = LessMoreMatcher;
                break;

            case '+':
                if (m_windbgStyle)
                {
                    break;
                }
            case '&':
            case '|':
                symbolMatcher = DuplicationOrWithEqualMatcher;
                break;

            case '-':
                if (!m_windbgStyle)
                {
                    symbolMatcher = MinusMatcher;
                }
                break;

            case '/':
            case '*':
            case '%':
            case '!':
            case '^':
                symbolMatcher = WithEqualMatcher;
                break;

            case ':':
            case '=':
                symbolMatcher = DuplicationMatcher;
                break;

                // alone group
            case ',':
            case '?':
            case '~':
            case ';':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '@':
            case '$':
                break;

            case '\'':
                return CaptureStringLiteral(Token::ttLiteralChar, pToken);
            case '\"':
                return CaptureStringLiteral(Token::ttLiteralString, pToken);

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                return CaptureDigitLiteral(pToken);

            default:
                if (ch < 20 || ((unsigned char)(ch))>127)
                {
                    RaiseError("Invalid characters");
                }
                return CaptureName(pToken, flags);
        }
        // capture sign
        bool res = CaptureSign(pToken, symbolMatcher);
        if (!res)
        {
            if (!m_inComment)
            {
                // line comment, skip the entire line and exit
                m_lineSize = 0;
            }
            // c-style comment
            continue;
        }
        return res;
    }   
}

// CTokenizerEnv
CTokenizerEnv::CTokenizerEnv()
    :
        m_tokenizer(&m_binaryStorage, &m_reservedWordsStorage)
{
}
bool CTokenizerEnv::GetNextToken(Token * pToken, int flags)
{
    return m_tokenizer.GetNextToken(pToken, flags);
}
void CTokenizerEnv::Clear()
{
    m_tokenizer.Clear();
    m_binaryStorage.Clear();
}
void CTokenizerEnv::ResetSource(ITokenFileSource * pTokenFileSource)
{
    Clear();
    m_tokenizer.ResetSource(pTokenFileSource);
}

}