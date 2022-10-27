#pragma once

#include "orthia_interfaces_vm.h"

namespace orthia
{
struct ITextPrinter
{
    virtual ~ITextPrinter(){}
    virtual void PrintLine(const std::wstring & line) = 0;
};

typedef void (*ConvertToTextPtr_type)(const char * pBinary, std::wstring * text);
class CVmBinaryMemoryPrinter:public IVmMemoryRangesTarget
{
    ITextPrinter * m_pTextPrinter;
    std::wstring m_currentBlock;
    std::wstring m_currentAscii;
    ConvertToTextPtr_type m_pConvertToTextFnc;
    std::wstring m_noDataPattern;
    int m_varSize;
    int m_dianaMode;
    int m_itemsInRow;

    Address_type m_startAddress;
    int m_currentItemInRow;
    bool m_firstRange;

    void ReportBlock();
    void ReportText(Address_type address, 
                    const std::wstring & text,
                    char charHint);
public:
    CVmBinaryMemoryPrinter(ITextPrinter * pTextPrinter,
                           int varSize,
                           int dianaMode,
                           int itemsInRow);
    virtual void OnRange(const VmMemoryRangeInfo & vmRange,
                         const char * pDataStart);

    void Finish();

};


struct PrintStringWriter:public orthia::ITextPrinter
{
    std::wstring m_result;

    virtual void PrintLine(const std::wstring & line)
    {
        m_result.append(line);
        m_result.append(L"\n");
    }
};

char AsciiEscapeSymbol(char symbol);
std::wstring Address64ToString(Address_type address);
void VmDeserializeMemory(int varSize, 
                         const std::wstring & text,
                         std::vector<char> * pBuffer);

template<class CharType>
class CWindbgTextIterator
{
    CWindbgTextIterator(const CWindbgTextIterator & );
    CWindbgTextIterator&operator = (const CWindbgTextIterator & );

    std::basic_string<CharType> m_arg;
    const CharType * m_pBegin;
    const CharType * m_pEnd;
    
    std::basic_string<CharType> m_buffer;
    std::wstring m_addressBuffer;
    std::vector<char> m_addressBufferUnparsed;

    template<class ParseStragy>
    void ParseTokenToBuffer(ParseStragy strategy)
    {
        m_buffer.clear();
        // skip whitespace
        for(;;++m_pBegin)
        {
            if (m_pBegin == m_pEnd)
            {
                return;
            }
            if (!orthia::IsWhitespace(*m_pBegin))
            {
                break;
            }
        }
        // non-whitespace found
        const CharType * pRangeBegin = m_pBegin;
        for(;m_pBegin != m_pEnd;++m_pBegin)
        {
            if (strategy.IsEnd(*m_pBegin))
            {
                break;
            }
        }
        m_buffer.assign(pRangeBegin, m_pBegin);
    }

    template<class OutCharType>
    void CopyBufferImpl(std::basic_string<OutCharType> * pToken)
    {
        char error[0];
        &error;
    }
    template<>
    void CopyBufferImpl<char>(std::basic_string<char> * pToken)
    {
        *pToken = ToAnsiString_Silent(m_buffer);
    }
    template<>
    void CopyBufferImpl<wchar_t>(std::basic_string<wchar_t> * pToken)
    {
        *pToken = ToWideString(m_buffer);
    }

    template<class OutCharType>
    void CopyBuffer(std::basic_string<OutCharType> * pToken)
    {
        CopyBufferImpl<OutCharType>(pToken);
    }
    template<>
    void CopyBuffer(std::basic_string<CharType> * pToken)
    {
        *pToken = m_buffer;
    }

public:
    CWindbgTextIterator(const std::basic_string<CharType> & arg)
        :
            m_arg(arg),
            m_pBegin(m_arg.c_str()),
            m_pEnd(m_pBegin + m_arg.size())
    {
    }
    CWindbgTextIterator(const CharType * pBegin,
                        const CharType * pEnd)
        :
            m_pBegin(pBegin),
            m_pEnd(pEnd)
    {
    }

    struct WhiteSpaceStrategy 
    {
        template<class CharType>
        bool IsEnd(CharType ch) const
        {
            return orthia::IsWhitespace(ch);
        }
    };
    WhiteSpaceStrategy whitespace()
    {
        return WhiteSpaceStrategy();
    }

    struct EndOfDocumentStrategy
    {
        template<class CharType>
        bool IsEnd(CharType ch) const
        {
            return false;
        }
    };
    EndOfDocumentStrategy end_of_document()
    {
        return EndOfDocumentStrategy();
    }

    template<class OutCharType, class ParseStragy>
    bool ParseToken(std::basic_string<OutCharType> * pToken, ParseStragy strategy)
    {
        pToken->clear();
        ParseTokenToBuffer(strategy);
        if (m_buffer.empty())
            return false;
        CopyBuffer(pToken);
        return true;
    }
    template<class ParseStragy>
    bool ParseToken(DI_UINT64 * pToken, int varSize, ParseStragy strategy)
    {
        if (!ParseToken(&m_addressBuffer, strategy))
        {
            return false;
        }
        VmDeserializeMemory(varSize, 
                            m_addressBuffer,
                            &m_addressBufferUnparsed);
        if (m_addressBufferUnparsed.size() != varSize)
        {
            throw std::runtime_error("Internal error");
        }
        switch(varSize)
        {
        default:
            throw std::runtime_error("Invalid argument");
        case 1:
            *pToken = *(DI_UINT8 *)(&m_addressBufferUnparsed.front());
            break;
        case 2:
            *pToken = *(DI_UINT16 *)(&m_addressBufferUnparsed.front());
            break;
        case 4:
            *pToken = *(DI_UINT32 *)(&m_addressBufferUnparsed.front());
            break;
        case 8:
            *pToken = *(DI_UINT64 *)(&m_addressBufferUnparsed.front());
            break;
        }
        return true;
    }
};

}


