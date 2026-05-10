#pragma once

#include "orthia_utils.h"
#include <sstream>

namespace orthia
{

class CBinaryTokenStorage;
class CSymbolStorage;
const int TokenMaxOperatorSize = 4;
struct Token
{
    typedef enum {ttEOF, 
        ttReservedWord, 
        ttLiteral, 
        ttName, 
        ttSpecialSign,
        ttNone} TokenType_type;

    typedef enum {ttLiteralNone, 
        ttLiteralInt, 
        ttLiteralChar, 
        ttLiteralString,
        ttLiteralWideChar, 
        ttLiteralWideString} LiteralType_type;

    TokenType_type type;
    LiteralType_type literalType;
    union
    {
        char signCharCode;
        char multiByteChar[TokenMaxOperatorSize];
        int operatorValue;
    };
    bool signedValue;

    // offset
    int line;
    int column;

    int reservedWordId;

    // storage
    size_t tokenOffset;
    size_t tokenSize;

    CBinaryTokenStorage * pBinaryTokenStorage;

    // raw symbol storage (source text of the token)
    size_t symbolOffset;
    size_t symbolSize;

    CSymbolStorage * pSymbolStorage;
};

bool operator == (const Token & token1, const Token & token2);

orthia::PlatformString_type ReadString(const Token& token);
orthia::PlatformString_type ReadRawString(const Token& token);

struct ITokenFileSource
{
    virtual ~ITokenFileSource(){}
    virtual bool GetNextLine(std::vector<char> * pLine, size_t * pSize)=0;
};

class CBinaryTokenStorage
{
    std::vector<char> m_storage;
    size_t m_lastPos;
public:
    CBinaryTokenStorage(size_t hintToReserve = 4096);
    size_t RegisterTokenData(Token * pToken, const void * pRawData, size_t size);
    void Clear();
    const void * QueryData(size_t offset, size_t size);
};

class CSymbolStorage
{
    std::vector<char> m_storage;
public:
    CSymbolStorage(size_t hintToReserve = 4096);
    size_t RegisterSymbolData(Token * pToken, const void * pRawData, size_t size);
    void Clear();
    const void * QueryData(size_t offset, size_t size) const;
};

class CReservedWordsStorage
{
    typedef std::map<std::string, int> ReservedWordsMap_type;
    ReservedWordsMap_type m_reservedWords;
    int m_lastRWId;
public:
    CReservedWordsStorage();
    int AddReservedWord(const std::string & word);
    int GetReservedWord_Silent(const std::string & word) const;
};

typedef bool (*SymbolMatcherFnc_type)(int index, 
                                      char currentChar,
                                      char originalChar,
                                      bool * pCanContacenate, 
                                      bool * pResultWillbeInvalid,
                                      bool * pResultWillbeFinal);

class CTokenizer
{
public:
    static const int flags_ForceGetName = 1;
protected:
    ITokenFileSource * m_pTokenFileSource;
    CBinaryTokenStorage * m_pBinaryTokenStorage;
    CSymbolStorage * m_pSymbolStorage;
    CReservedWordsStorage * m_pReservedWordsStorage;

    std::vector<char> m_line;
    size_t m_lineSize;
    bool m_eofReached;

    int m_lineNumber;
    int m_columnPos;
    int m_tokenStartPos;

    Token m_tokensCache;
    std::vector<char> m_tempStorage;
    std::vector<char> m_tempStorage2;
    std::string m_tempStorageStr;

    bool m_inComment;
    bool m_windbgStyle;
    Token BuildNewToken(Token::TokenType_type type);
    char ReadOneOrDie(const std::string & error = std::string());
    char HasOneMore() const;
    char HasCurrent() const;
    void RegisterTokenData(Token * pToken, const void * pRawData, size_t size);
    void RegisterTempTokenData(Token * pToken);
    void RegisterSymbolData(Token * pToken);
    void AddToTempStorage(const void * pRawData, size_t size);

    // all capture functions must change the position
    bool CaptureSign(Token * pToken, SymbolMatcherFnc_type matcher);
    bool CaptureStringLiteral(Token::LiteralType_type literalType, 
                              Token * pToken, 
                              int columnPos = -1);
    bool CaptureDigitLiteral(Token * pToken);
    bool CaptureName(Token * pToken, int flags);
    wchar_t CaptureEscapedChar(bool wide);

    void RaiseError(const std::string & type);
    void RaiseWarning(const std::string & type);
    bool IsOtherNameSymbol(char ch);

public:
    CTokenizer(CBinaryTokenStorage * pBinaryTokenStorage, 
               CReservedWordsStorage * pReservedWordsStorage,
               ITokenFileSource * pTokenFileSource = 0);
    bool GetNextToken(Token * pToken, int flags = 0);
    std::string GetNextRawString();

    void Clear();
    void ResetSource(ITokenFileSource * pTokenFileSource);
    void TestPopulateAllCaches();
    void SetWindbgStyle(bool val) { m_windbgStyle = val; }
    void SetSymbolStorage(CSymbolStorage * pSymbolStorage) { m_pSymbolStorage = pSymbolStorage; }
};


class CStreamTokenFileSource:public ITokenFileSource
{
    const int m_maxLineChars;
    std::stringstream m_stream;
public:
    CStreamTokenFileSource(const char * str)
            :
            m_maxLineChars(1024)
    {
        m_stream<<str;
    }
    CStreamTokenFileSource(const std::string & str = std::string(), int maxLineChars = 1024)
        :
            m_maxLineChars(maxLineChars)
    {
        if (!str.empty())
        {
            m_stream<<str;
        }
    }
    std::stringstream & GetStream() { return m_stream; }
    const std::stringstream & GetStream() const { return m_stream; }
    virtual bool GetNextLine(std::vector<char> * pLine, size_t * pSize)
    {
        pLine->resize(m_maxLineChars);
        
        (*pLine)[0] = 0;
        m_stream.getline(&pLine->front(), pLine->size());
        if (m_stream.fail())
        {
            return false;
        }
        *pSize = m_stream.gcount();
        if (*pSize && !(*pLine)[*pSize-1])
        {
            (*pSize)--;
        }
        return true;
    }
};


class CTokenizerEnv
{
    CReservedWordsStorage m_reservedWordsStorage;
    CBinaryTokenStorage m_binaryStorage;
    CSymbolStorage m_symbolStorage;
    CTokenizer m_tokenizer;
public:
    CTokenizerEnv();
    bool GetNextToken(Token * pToken, int flags = 0);
    void Clear();
    void ResetSource(ITokenFileSource * pTokenFileSource);

        
    CTokenizer & GetTokenizer() { return m_tokenizer; }
    CBinaryTokenStorage & GetBinaryStorage() { return m_binaryStorage; }
    const CBinaryTokenStorage & GetBinaryStorage() const { return m_binaryStorage; }
    CSymbolStorage & GetSymbolStorage() { return m_symbolStorage; }
    const CSymbolStorage & GetSymbolStorage() const { return m_symbolStorage; }
    CReservedWordsStorage & GetReservedWordsStorage() { return m_reservedWordsStorage; }
    const CReservedWordsStorage & GetReservedWordsStorage() const { return m_reservedWordsStorage; }
};


int portable_ui64toa(uint64_t value, char* buffer, size_t size, int radix);

}
