#pragma once

#include "orthia_utils.h"
#include <sstream>

namespace orthia
{

class CBinaryTokenStorage;
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
};

bool operator == (const Token & token1, const Token & token2);

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
    ITokenFileSource * m_pTokenFileSource;
    CBinaryTokenStorage * m_pBinaryTokenStorage;
    CReservedWordsStorage * m_pReservedWordsStorage;

    std::vector<char> m_line;
    size_t m_lineSize;
    bool m_eofReached;

    int m_lineNumber;
    int m_columnPos;

    bool m_tokensCacheReady;
    Token m_tokensCache;
    std::vector<char> m_tempStorage;
    std::vector<char> m_tempStorage2;
    std::string m_tempStorageStr;

    bool m_inComment;
    Token BuildNewToken(Token::TokenType_type type);
    char ReadOneOrDie(const std::string & error = std::string());
    char HasOneMore() const;
    char HasCurrent() const;
    void RegisterTokenData(Token * pToken, const void * pRawData, size_t size);
    void RegisterTempTokenData(Token * pToken);
    void AddToTempStorage(const void * pRawData, size_t size);

    // all capture functions must change the position
    bool CaptureSign(Token * pToken, SymbolMatcherFnc_type matcher);
    bool CaptureStringLiteral(Token::LiteralType_type literalType, 
                              Token * pToken, 
                              int columnPos = -1);
    bool CaptureDigitLiteral(Token * pToken);
    bool CaptureName(Token * pToken);
    wchar_t CaptureEscapedChar(bool wide);

    void RaiseError(const std::string & type);
    void RaiseWarning(const std::string & type);
public:
    CTokenizer(CBinaryTokenStorage * pBinaryTokenStorage, 
               CReservedWordsStorage * pReservedWordsStorage,
               ITokenFileSource * pTokenFileSource = 0);
    bool GetNextToken(Token * pToken);
    void Clear();
    void ResetSource(ITokenFileSource * pTokenFileSource);
    void TestPopulateAllCaches();
};


class CStreamTokenFileSource:public ITokenFileSource
{
    const int m_maxLineChars;
    std::stringstream m_stream;
public:
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
    CTokenizer m_tokenizer;
public:
    CTokenizerEnv();
    bool GetNextToken(Token * pToken);
    void Clear();
    void ResetSource(ITokenFileSource * pTokenFileSource);

        
    CTokenizer & GetTokenizer() { return m_tokenizer; }
    CBinaryTokenStorage & GetBinaryStorage() { return m_binaryStorage; }
    const CBinaryTokenStorage & GetBinaryStorage() const { return m_binaryStorage; }
    CReservedWordsStorage & GetReservedWordsStorage() { return m_reservedWordsStorage; }
    const CReservedWordsStorage & GetReservedWordsStorage() const { return m_reservedWordsStorage; }
};

}
