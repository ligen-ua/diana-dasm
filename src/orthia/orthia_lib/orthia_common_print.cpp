#include "orthia_common_print.h"


namespace orthia
{
static const wchar_t * g_no_data_patterns[] = 
{
    L"??",
    L"????",
    L"????????",
    L"????????`????????"
};
std::wstring Address64ToString(Address_type address)
{
    ULARGE_INTEGER largeInt;
    largeInt.QuadPart = address;

    std::wstring result = orthia::ToWideStringAsHex((unsigned int)largeInt.HighPart);
    result.append(L"`");
    result.append(orthia::ToWideStringAsHex((unsigned int)largeInt.LowPart));
    return result;
}
static void ConvertToText1(const char * pBinary, std::wstring * pText)
{
    *pText = orthia::ToHexString(pBinary, 1);
}
static void ConvertToText2(const char * pBinary, std::wstring * pText)
{
    *pText = orthia::ToWideStringAsHex(*(unsigned short*)pBinary);
}
static void ConvertToText4(const char * pBinary, std::wstring * pText)
{
    *pText = orthia::ToWideStringAsHex(*(unsigned int*)pBinary);
}
static void ConvertToText8(const char * pBinary, std::wstring * pText)
{
    *pText = Address64ToString(*(unsigned long long*)pBinary);
}
CVmBinaryMemoryPrinter::CVmBinaryMemoryPrinter(ITextPrinter * pTextPrinter,
                                               int varSize,
                                               int dianaMode,
                                               int itemsInRow)
    :
        m_pTextPrinter(pTextPrinter),
        m_varSize(varSize),
        m_dianaMode(dianaMode),
        m_itemsInRow(itemsInRow),
        m_currentItemInRow(0),
        m_startAddress(0),
        m_firstRange(true)
{
    int defItemsInRow = 0;
    switch(varSize)
    {
    default:
        throw std::runtime_error("Invalid argument");
    case 1:
        m_pConvertToTextFnc = ConvertToText1;
        defItemsInRow = 16;
        m_noDataPattern = g_no_data_patterns[0];
        break;
    case 2:
        m_pConvertToTextFnc = ConvertToText2;
        defItemsInRow = 8;
        m_noDataPattern = g_no_data_patterns[1];
        break;
    case 4:
        m_pConvertToTextFnc = ConvertToText4;
        defItemsInRow = 4;
        m_noDataPattern = g_no_data_patterns[2];
        break;
    case 8:
        m_pConvertToTextFnc = ConvertToText8;
        defItemsInRow = 2;
        m_noDataPattern = g_no_data_patterns[3];
        break;
    }
    if (m_itemsInRow == -1)
    {
        m_itemsInRow = defItemsInRow;
    }
}
void CVmBinaryMemoryPrinter::OnRange(const VmMemoryRangeInfo & vmRange,
                                     const char * pDataStart)
{
    if (m_firstRange)
    {
        m_startAddress = vmRange.address;
        m_firstRange = false;
    }
    // reports from aligned start to unaligned end
    Address_type startOffsetInRange = 0;
    int rangeModulus = (int)(vmRange.address % m_varSize);
    int startModulus = (int)(m_startAddress % m_varSize);
    if (rangeModulus > startModulus)
    {
        startOffsetInRange = m_varSize - rangeModulus + startModulus;
    }
    else
    if (rangeModulus < startModulus)
    {
        startOffsetInRange = startModulus - rangeModulus;
    }

    Address_type lastOfRange = startOffsetInRange + vmRange.size - 1;
    if (lastOfRange < startOffsetInRange)
    {
        throw std::runtime_error("Overflow");
    }
    std::wstring currentText;
    if (startOffsetInRange <= lastOfRange)
    {
        for(Address_type offsetInRange = startOffsetInRange; ;offsetInRange += m_varSize)
        {
            // report
            bool dataFits = (offsetInRange + m_varSize - 1) <= lastOfRange;
            char charHint = '?';
            if  (vmRange.HasData() && dataFits)
            {
                (*m_pConvertToTextFnc)(pDataStart + offsetInRange, &currentText);
                charHint = pDataStart[offsetInRange];
            }
            else
            {
                currentText = m_noDataPattern;
            }

            ReportText(offsetInRange + vmRange.address, 
                       currentText,
                       charHint);

            if (offsetInRange + m_varSize - 1 >= lastOfRange)
            {
                break;
            }
        }
    }
}

void CVmBinaryMemoryPrinter::ReportBlock()
{
    if (!m_currentBlock.empty())
    {
        if (!m_currentAscii.empty())
        {
            int count = 59;
            if (m_dianaMode == 8)
            {
                count = 68;
            }
            m_currentBlock.resize(count, L' ');
            m_currentBlock.append(m_currentAscii);
        }
        m_pTextPrinter->PrintLine(m_currentBlock);
    }
    m_currentItemInRow = 0;
    m_currentBlock.clear();
    m_currentAscii.clear();
}
void CVmBinaryMemoryPrinter::ReportText(Address_type address, 
                                        const std::wstring & text,
                                        char charHint)
{
    if (m_currentItemInRow >= m_itemsInRow)
    {
        ReportBlock();
    };
    
    if (m_currentItemInRow == 0)
    {
        if (m_dianaMode < 8)
        {
            m_currentBlock.append(orthia::ToWideStringAsHex((unsigned int)address));
        }
        else
        {   
            m_currentBlock.append(Address64ToString(address));
        }
        m_currentBlock.append(L" ");
    }
    if (m_varSize == 1 && m_currentItemInRow == 8)
    {
        m_currentBlock.append(L"-");
    }
    else
    {
        m_currentBlock.append(L" ");
    }
    if (m_varSize == 1)
    {
        m_currentAscii += ((wchar_t)AsciiEscapeSymbol(charHint));
    }
    m_currentBlock.append(text);
    ++m_currentItemInRow;
}
void CVmBinaryMemoryPrinter::Finish()
{
    ReportBlock();
}

char AsciiEscapeSymbol(char symbol)
{
    if ((int)symbol < (int)0x20 || (int)symbol > (int)0x7E)
        return '.';

    return symbol;
}


static bool ReadNext(const wchar_t *& pData,
                     const wchar_t * pEnd,
                     Address_type & result)
{
    result = 0;
    for(;;++pData)
    {
        if (pData == pEnd)
        {
            return false;
        }
        if (*pData != ' ')
        {
            break;
        }
    };
    wchar_t * pParam = (wchar_t * )pEnd; 
    result = _wcstoui64(pData, &pParam, 16);
    if (pData == pParam)
    {
        throw std::runtime_error("Can't deserialize");
    }
    pData = pParam;
    return true;
}
class CVmDeserializeIterator
{
    const wchar_t * m_pCurrentData;
    const wchar_t * m_pEnd;

public:
    CVmDeserializeIterator(const std::wstring & text)
        :
            m_pCurrentData(text.c_str()), 
            m_pEnd(text.c_str() + text.size())
    {
    }

    bool ReadNext(Address_type & result)
    {
        return orthia::ReadNext(m_pCurrentData, 
                                m_pEnd,
                                result);
    }
    void MoveOnNextChar()
    {
        ++m_pCurrentData;
    }
    wchar_t GetNextChar() const
    {
        return *m_pCurrentData;
    }
};
template<class ObjectType>
void Append(std::vector<char> * pBuffer, 
            ObjectType value)
{
    pBuffer->insert(pBuffer->end(), 
                    (const char*)&value,
                    (const char*)&value + sizeof(value));
}

template<class TargetType, unsigned long long MaxValue>
void VmDeserializeMemory_t(const std::wstring & text_in,
                           std::vector<char> * pBuffer)
{
    Address_type result = 0;
    CVmDeserializeIterator iterator(text_in);
    while(iterator.ReadNext(result))
    {
        if (result > MaxValue)
        {
            throw std::runtime_error("Overflow error");
        }
        Append(pBuffer, (TargetType)result);
    }
}
static void VmDeserializeMemory_8(const std::wstring & text_in,
                                  std::vector<char> * pBuffer)
{
    Address_type result = 0;
    CVmDeserializeIterator iterator(text_in);
    while(iterator.ReadNext(result))
    {
        if (iterator.GetNextChar() != L'`')
        {
            Append(pBuffer, (unsigned long long)result);
            continue;
        }
        iterator.MoveOnNextChar();
        if (result > 0xFFFFFFFFULL)
        {
            throw std::runtime_error("Overflow");
        }
        Address_type high = result;
        if (!iterator.ReadNext(result))
        {
            throw std::runtime_error("Invalid format");
        }
        if (result > 0xFFFFFFFFULL)
        {
            throw std::runtime_error("Overflow");
        }
        Append(pBuffer, (unsigned int)result);
        Append(pBuffer, (unsigned int)high);
    }
}
void VmDeserializeMemory(int varSize, 
                         const std::wstring & text_in,
                         std::vector<char> * pBuffer)
{
    pBuffer->clear();
    const std::wstring & text = text_in;
    switch(varSize)
    {
    default:
        throw std::runtime_error("Invalid argument");
    case 1:
        return VmDeserializeMemory_t<unsigned char, 0xFFULL>(text, pBuffer);
    case 2:
        return VmDeserializeMemory_t<unsigned short, 0xFFFFULL>(text, pBuffer);
    case 4:
        return VmDeserializeMemory_t<unsigned int, 0xFFFFFFFFULL>(text, pBuffer);
    case 8:
        return VmDeserializeMemory_8(text, pBuffer);
    }
}

}