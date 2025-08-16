#pragma once

#include "orthia_utils.h"
namespace tinyxml2
{
class XMLDocument;
class XMLElement;
class XMLAttribute;
}
namespace orthia
{

class CCommonFormatParser;
class CCommonFormatBuilder
{
    CCommonFormatBuilder(const CCommonFormatBuilder&);
    CCommonFormatBuilder&operator = (const CCommonFormatBuilder&);

    DIANA_AUTO_PTR<tinyxml2::XMLDocument> m_xml;
    tinyxml2::XMLDocument * m_pXML;
    tinyxml2::XMLElement * m_pRoot;
    bool m_compact;
    bool m_wasMetadata;
    void ProduceProp(const std::string & value, std::vector<char> & result);
    void InitNew();
public:
    CCommonFormatBuilder(bool compact = true);
    CCommonFormatBuilder(CCommonFormatParser * pParser, bool compact = true);
    ~CCommonFormatBuilder();

    void AddMetadata(const std::string & name, const std::string & value);
    void AddMetadata(const std::wstring & name, const std::wstring & value);
    void AddMetadata(const std::string & name, long long value);
    void DeleteMetadata(const std::string & utf8name);
    void DeleteMetadata(const std::wstring & name);

    void Produce(const std::vector<char> & value, std::vector<char> * pResult);
    void Produce(std::vector<char> * pResult);
    void Produce(std::string * pResult);
    void Produce(const char * pValueBegin, 
                 const char * pValueEnd, 
                 std::vector<char> * pResult);
                                   

};

class CCommonFormatParser
{
    CCommonFormatParser(const CCommonFormatParser&);
    CCommonFormatParser&operator = (const CCommonFormatParser&);

    mutable DIANA_AUTO_PTR<tinyxml2::XMLDocument> m_xml;
    mutable tinyxml2::XMLElement * m_pRoot;

    std::vector<char> m_data;
    const char * m_pDataStart;
    const char * m_pDataEnd;
    bool m_empty;
    long long m_fixedLength;

    bool QueryValueProp(std::string * pValue) const;
public:
    CCommonFormatParser();
    ~CCommonFormatParser();

    bool Parse(const std::wstring & strValue);
    bool Parse(const std::string & strValue);
    bool Parse(const std::vector<char> & source, bool bMakeCopy);
    bool Parse(const char * pStart, const char * pEnd, bool bMakeCopy);
    bool Parse(const char * pStart, bool bMakeCopy);

    bool QueryMetadata(const std::string & name, std::string * pValue) const;
    bool QueryMetadata(const std::string & name, std::wstring * pValue) const;
    bool QueryMetadata(const std::wstring & name, std::wstring * pValue) const;
    bool QueryMetadata(const std::string & name, long long * pValue) const;
    bool QueryMetadata(const std::string & name, unsigned long long * pValue) const;

    bool QueryValue(std::vector<char> * pValue) const;
    long long QuerySize() const;
    unsigned long long QuerySizeOfBinary() const;

    const tinyxml2::XMLAttribute * QueryFirstAttribute() const;
    tinyxml2::XMLDocument * QueryXML();
    tinyxml2::XMLElement * QueryRoot();
};


class CCommonFormatMultiParser
{
    CCommonFormatMultiParser(const CCommonFormatMultiParser&);
    CCommonFormatMultiParser&operator = (const CCommonFormatMultiParser&);

    std::vector<char> m_data;
    const char * m_pDataStart;
    const char * m_pDataEnd;
    const char * m_pCurrentStart;

    void Init(const char * pStart, const char * pEnd, bool bMakeCopy);
public:
    CCommonFormatMultiParser(const std::vector<char> & data, bool bMakeCopy);
    CCommonFormatMultiParser(const char * pStart, const char * pEnd, bool bMakeCopy);
    bool QueryNextItem(CCommonFormatParser * pParser);
};


class CCommonFormatMultiBuilder
{
    CCommonFormatMultiBuilder(const CCommonFormatMultiBuilder&);
    CCommonFormatMultiBuilder&operator = (const CCommonFormatMultiBuilder&);

    std::vector<char> m_data;
public:
    CCommonFormatMultiBuilder();
    void Init(const std::vector<char> & data);
    void AddItem(const std::vector<char> & data);
    void AddItem(const std::string & otherXml);
    void Produce(std::vector<char> * pValue) const;

    const std::vector<char> & GetData() const { return m_data; }
};

inline std::string SafeQueryText(const char * pData)
{
    if (!pData)
        return std::string();
    return pData;
}
}