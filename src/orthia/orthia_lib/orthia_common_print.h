#pragma once

#include "orthia_interfaces_vm.h"

namespace orthia
{
struct ITextPrinter
{
    virtual ~ITextPrinter(){}
    virtual void PrintLine(const PlatformString_type & line) = 0;
};

typedef void (*ConvertToTextPtr_type)(void * context, const char * pBinary, orthia::PlatformString_type * text);
class CVmBinaryMemoryPrinter:public IVmMemoryRangesTarget
{
    ITextPrinter * m_pTextPrinter;
    PlatformString_type m_currentBlock;
    PlatformString_type m_currentAscii;
    ConvertToTextPtr_type m_pConvertToTextFnc;
    PlatformString_type m_noDataPattern;
    int m_varSize;
    int m_dianaMode;
    int m_itemsInRow;

    Address_type m_startAddress;
    int m_currentItemInRow;
    bool m_firstRange;
    Address_type m_extraEatenBytes;
    void* m_pConvertContext;
    void ReportBlock();
    void ReportText(Address_type address, 
                    const PlatformString_type & text,
                    char charHint);
public:
    CVmBinaryMemoryPrinter(ITextPrinter * pTextPrinter,
                           int varSize,
                           int dianaMode,
                           int itemsInRow);
    virtual void OnRange(const VmMemoryRangeInfo & vmRange,
                         const char * pDataStart);

    void SetConvertToText(void * pConvertContext, ConvertToTextPtr_type pConvertToTextFnc);
    Address_type GetExtraEatenBytes() const { return m_extraEatenBytes; }

    void Finish();

};


struct PrintStringWriter:public orthia::ITextPrinter
{
    PlatformString_type m_result;

    virtual void PrintLine(const PlatformString_type & line)
    {
        m_result.append(line);
        m_result.append(ORTHIA_TCSTR("\n"));
    }
};

char AsciiEscapeSymbol(char symbol);
PlatformString_type Address64ToString(Address_type address);
PlatformString_type AddressToString(Address_type address, int dianaMode);
void VmDeserializeMemory(int varSize, 
                         const PlatformString_type & text,
                         std::vector<char> * pBuffer);


namespace WindbgTextIteratorDetail 
{
    template<class InBuffer, class OutCharType>
    void CopyBufferImpl(const InBuffer & buffer, std::basic_string<OutCharType> * pToken)
    {
        *pToken = buffer;
    }
    template<class InBuffer>
    void CopyBufferImpl(const InBuffer & buffer, std::basic_string<char> * pToken)
    {
        *pToken = ToAnsiString_Silent(buffer);
    }
    template<class InBuffer>
    void CopyBufferImpl(const InBuffer & buffer, std::basic_string<wchar_t> * pToken)
    {
        *pToken = ToWideString(buffer);
    }

    template<class InBuffer, class OutCharType>
    void CopyBuffer(const InBuffer & buffer, std::basic_string<OutCharType> * pToken)
    {
        CopyBufferImpl(buffer, pToken);
    }
}
template<class CharType>
class CWindbgTextIterator
{
    CWindbgTextIterator(const CWindbgTextIterator & );
    CWindbgTextIterator&operator = (const CWindbgTextIterator & );

    std::basic_string<CharType> m_arg;
    const CharType * m_pBegin;
    const CharType * m_pEnd;
    
    std::basic_string<CharType> m_buffer;
    PlatformString_type m_addressBuffer;
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
        template<class CharTypeEx>
        bool IsEnd(CharTypeEx ch) const
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
        template<class CharTypeEx>
        bool IsEnd(CharTypeEx ch) const
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
        WindbgTextIteratorDetail::CopyBuffer(m_buffer, pToken);
        return true;
    }
    template<class ParseStragy>
    bool ParseToken(DI_UINT64 * pToken, int varSize, ParseStragy strategy)
    {
        if (!ParseToken(&m_addressBuffer, strategy))
        {
            return false;
        }
        try
        {
            VmDeserializeMemory(varSize,
                m_addressBuffer,
                &m_addressBufferUnparsed);
        }
        catch (std::exception&e)
        {
            // it's io, just reaction on some weird windbg error
            // don't pass an error, do our best
            &e;
            return false;
        }
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


