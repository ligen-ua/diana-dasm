#include "orthia_common_format.h"
#include "orthia_tiny_xml.h"

namespace orthia
{

// CCommonFormatBuilder
#define CFB_CONTENT_SIZE_FIELD "sys_bin_size"
#define CFB_ROOT_NODE_NAME "root"

CCommonFormatBuilder::CCommonFormatBuilder(bool compact)
    :
        m_pRoot(0),
        m_compact(compact),
        m_wasMetadata(false),
        m_pXML(0)
{
    InitNew();
}
CCommonFormatBuilder::CCommonFormatBuilder(CCommonFormatParser * pParser, bool compact)
    :
        m_pRoot(0),
        m_compact(compact),
        m_wasMetadata(false),
        m_pXML(0)
{
    if (tinyxml2::XMLElement * pRoot = pParser->QueryRoot())
    {
        m_pRoot = pRoot;
        m_wasMetadata = true;
        m_pXML = pParser->QueryXML();
    }
    else
    {
        InitNew();
    }
}
CCommonFormatBuilder::~CCommonFormatBuilder()
{
}
void CCommonFormatBuilder::InitNew()
{
    m_xml.reset(new tinyxml2::XMLDocument());
    m_pRoot = m_xml->NewElement(CFB_ROOT_NODE_NAME);
    m_xml->InsertEndChild(m_pRoot);
    m_pXML = m_xml.get();
}
void CCommonFormatBuilder::AddMetadata(const std::wstring & name, const std::wstring & value)
{
    std::string utf8name(orthia::Utf16ToUtf8(name));
    std::string utf8value(orthia::Utf16ToUtf8(value));
    AddMetadata(utf8name, utf8value);
}
void CCommonFormatBuilder::AddMetadata(const std::string & utf8name, const std::string & utf8value)
{
    m_wasMetadata = true;
    m_pRoot->SetAttribute(utf8name.c_str(), utf8value.c_str());
}
void CCommonFormatBuilder::DeleteMetadata(const std::wstring & name)
{
    DeleteMetadata(orthia::Utf16ToUtf8(name));
}
void CCommonFormatBuilder::DeleteMetadata(const std::string & utf8name)
{
    m_pRoot->DeleteAttribute(utf8name.c_str());
}
void CCommonFormatBuilder::AddMetadata(const std::string & name, long long value)
{
    std::string text;
    orthia::ToStringAsHex_Short(value, &text);
    AddMetadata(name, text);
}

void CCommonFormatBuilder::Produce(const std::vector<char> & value, std::vector<char> * pResult)
{
    if (value.empty())
    {
        return Produce(0, 0, pResult);
    }
    return Produce(&value.front(), &value.front() + value.size(), pResult);
}
void CCommonFormatBuilder::Produce(const char * pValueBegin, 
                                   const char * pValueEnd,
                                   std::vector<char> * pResult)
{
    if (pValueEnd < pValueBegin)
    {
        throw std::runtime_error("Invalid range");
    }
    long long valueSize((long long)(pValueEnd - pValueBegin));
    pResult->clear();
    if (m_compact && valueSize ==0 && !m_wasMetadata)
    {
        return;
    }

    std::string resultSizeStrHex;
    orthia::ToStringAsHex_Short(valueSize, &resultSizeStrHex);

    AddMetadata(CFB_CONTENT_SIZE_FIELD, resultSizeStrHex);
    std::string header;
    XmlDocToString(*m_pXML, header, m_compact);
    pResult->assign(header.c_str(), header.c_str()+header.size()+1);
    pResult->insert(pResult->end(), pValueBegin, pValueEnd);
}
void CCommonFormatBuilder::Produce(std::string * pResult)
{
    pResult->clear();
    if (m_compact && !m_wasMetadata)
    {
        return;
    }
    XmlDocToString(*m_pXML, *pResult, m_compact);
}
void CCommonFormatBuilder::Produce(std::vector<char> * pResult)
{
    Produce(std::vector<char>(), pResult);
}


// CCommonFormatParser
CCommonFormatParser::CCommonFormatParser()
        :
            m_pRoot(0), 
            m_empty(false),
            m_pDataStart(0),
            m_pDataEnd(0),
            m_fixedLength(0)
{
    m_xml.reset(new tinyxml2::XMLDocument());
}
CCommonFormatParser::~CCommonFormatParser()
{
}

bool CCommonFormatParser::Parse(const std::string & data)
{
    return Parse(data.c_str(), data.c_str() + data.size(), true);
}
bool CCommonFormatParser::Parse(const std::wstring & strValue)
{
    std::string data = orthia::Utf16ToUtf8(strValue);
    return Parse(data.c_str(), data.c_str() + data.size(), true);
}

bool CCommonFormatParser::Parse(const std::vector<char> & source, bool bMakeCopy)
{
    const char * pStart = orthia::GetFrontPointer(source);
    const char * pEnd = pStart + source.size();
    return Parse(pStart, pEnd, bMakeCopy);
}
bool CCommonFormatParser::Parse(const char * pStart, bool bMakeCopy)
{
    const char * pEnd = pStart + strlen(pStart);
    return Parse(pStart, pEnd, bMakeCopy);
}
bool CCommonFormatParser::Parse(const char * pStart, const char * pEnd, bool bMakeCopy)
{
    if (pStart == pEnd)
    {
        m_empty = true;
        m_fixedLength = 0;
        return true;
    }
    const char * p = pStart;
    size_t len = 0;
    bool zeroFound = false;
    for(; ; ++p, ++len)
    {
        if (p == pEnd)
        {
            break;
        }
        if (!*p)
        {
            zeroFound = true;
            break;
        }
    }
    m_xml->Parse(pStart, len);
    if (m_xml->Error())
        return false;
    m_fixedLength = len;
    if (zeroFound)
    {
        ++m_fixedLength;
    }
    tinyxml2::XMLElement * pRoot = m_xml->RootElement();
    if (!pRoot)
        return false;
    if (orthia::QueryNodeName_Utf8(pRoot) != CFB_ROOT_NODE_NAME)
        return false;
    m_pRoot = pRoot;
    if (p+1 < pEnd)
    {
        if (bMakeCopy)
        {
            m_data.assign(p+1, pEnd);
            m_pDataStart = orthia::GetFrontPointer(m_data);
            m_pDataEnd = m_pDataStart + m_data.size();
        }
        else
        {
            m_pDataStart = p + 1;
            m_pDataEnd = pEnd;
        }
    }
    return true;
}

bool CCommonFormatParser::QueryMetadata(const std::string & name, std::string * pValue) const
{
    pValue->clear();
    if (!m_pRoot)
    {
        return false;
    }
    return orthia::QueryAttribute_Silent_Utf8(m_pRoot, name, pValue);
}
bool CCommonFormatParser::QueryMetadata(const std::string & name, unsigned long long * pValue) const
{
    std::string value;
    bool res = QueryMetadata(name, &value);
    if (!res)
    {
        return false;
    }
    return HexStringToObject_Silent(value, pValue);
}
bool CCommonFormatParser::QueryMetadata(const std::string & name, long long * pValue) const
{
    std::string value;
    bool res = QueryMetadata(name, &value);
    if (!res)
    {
        return false;
    }
    return HexStringToObject_Silent(value, pValue);
}
bool CCommonFormatParser::QueryMetadata(const std::string & name, std::wstring * pValue) const
{
    pValue->clear();
    std::string utf8;
    bool res = QueryMetadata(name, &utf8);
    if (!res)
    {
        return false;
    }
    *pValue = orthia::Utf8ToUtf16(utf8);
    return res;
}
bool CCommonFormatParser::QueryMetadata(const std::wstring & name, std::wstring * pValue) const
{
    pValue->clear();
    std::string utf8;
    bool res = QueryMetadata(orthia::Utf16ToUtf8(name), &utf8);
    if (!res)
    {
        return false;
    }
    *pValue = orthia::Utf8ToUtf16(utf8);
    return res;
}
bool CCommonFormatParser::QueryValue(std::vector<char> * pValue) const
{
    pValue->clear();
    if (m_empty)
    {
        return true;
    }
    if (!m_pRoot)
    {
        return false;
    }
    std::string sizeStr;
    if (!QueryMetadata(CFB_CONTENT_SIZE_FIELD, &sizeStr))
    {
        return false;
    }
    long long size = 0;
    if (!orthia::HexStringToObject_Silent(sizeStr, &size))
    {
        return false;
    }
    if (size < 0 || size > m_pDataEnd - m_pDataStart)
    {
        return false;
    }
    pValue->assign(m_pDataStart, m_pDataStart + size);
    return true;
}

tinyxml2::XMLElement * CCommonFormatParser::QueryRoot()
{
    return m_pRoot;
}
tinyxml2::XMLDocument * CCommonFormatParser::QueryXML()
{
    return m_xml.get();
}
const tinyxml2::XMLAttribute * CCommonFormatParser::QueryFirstAttribute() const
{
    if (!m_pRoot)
        return 0;
    return m_pRoot->FirstAttribute();
}
unsigned long long CCommonFormatParser::QuerySizeOfBinary() const
{
    unsigned long long result = 0;
    if (!QueryMetadata(CFB_CONTENT_SIZE_FIELD, &result))
    {
        throw std::runtime_error("Invalid format");
    }
    return result;
}
long long CCommonFormatParser::QuerySize() const
{
    long long length = m_fixedLength;
    if (!length)
    {
        return 0;
    }
    if (m_empty)
    {
        return length;
    }
    if (!m_pRoot)
    {
        return length;
    }
    std::string sizeStr;
    if (!QueryMetadata(CFB_CONTENT_SIZE_FIELD, &sizeStr))
    {
        return length;
    }
    long long size = 0;
    if (!orthia::HexStringToObject_Silent(sizeStr, &size))
    {
        return length;
    }
    if (size < 0 || size > m_pDataEnd - m_pDataStart)
    {
        return length;
    }
    return length + size;
}

// multiparser
CCommonFormatMultiParser::CCommonFormatMultiParser(const std::vector<char> & data, bool bMakeCopy)
{
    Init(orthia::GetFrontPointer(data), orthia::GetFrontPointer(data) + data.size(), bMakeCopy);
}
CCommonFormatMultiParser::CCommonFormatMultiParser(const char * pStart, const char * pEnd, bool bMakeCopy)
{
    Init(pStart, pEnd, bMakeCopy);
}
void CCommonFormatMultiParser::Init(const char * pStart, const char * pEnd, bool bMakeCopy)
{
    if (bMakeCopy)
    {
        m_data.assign(pStart, pEnd);
        m_pDataStart = orthia::GetFrontPointer(m_data);
        m_pDataEnd = m_pDataStart + m_data.size();
    }
    else
    {
        m_pDataStart = pStart;
        m_pDataEnd = pEnd;
    }
    m_pCurrentStart = m_pDataStart;
}
bool CCommonFormatMultiParser::QueryNextItem(CCommonFormatParser * pParser)
{
    if (m_pCurrentStart == m_pDataEnd)
    {
        return false;
    }
    if (!pParser->Parse(m_pCurrentStart, m_pDataEnd, false))
    {
        return false;
    }
    long long size = pParser->QuerySize();
    if (!size)
    {
        return false;
    }
    m_pCurrentStart += size;
    return true;
}


CCommonFormatMultiBuilder::CCommonFormatMultiBuilder()
{
}
void CCommonFormatMultiBuilder::Init(const std::vector<char> & data)
{
    m_data = data;
}
void CCommonFormatMultiBuilder::AddItem(const std::string & otherXml)
{
    m_data.insert(m_data.end(), otherXml.c_str(), otherXml.c_str() + otherXml.size() + 1);
}
void CCommonFormatMultiBuilder::AddItem(const std::vector<char> & data)
{
    m_data.insert(m_data.end(), data.begin(), data.end());
}
void CCommonFormatMultiBuilder::Produce(std::vector<char> * pValue) const
{
    *pValue = m_data;
}

} // orthia