#ifndef ORTHIA_TINY_XML_H
#define ORTHIA_TINY_XML_H

#include "orthia_utils.h"
#include "tinyxml2.h"

#ifndef TIXML_USE_STL
#define TIXML_USE_STL
#endif


#define TIXML_SUCCESS 0
#define TIXML_ERROR   1

namespace orthia
{

typedef tinyxml2::XMLNode TiXmlNode;
typedef tinyxml2::XMLElement TiXmlElement;
typedef tinyxml2::XMLDocument TiXmlDocument;
typedef tinyxml2::XMLText TiXmlText;

inline int QueryValueAttribute(TiXmlNode * pNode, 
                               const std::string & attrName, 
                               std::string * pValue8)
{
    pValue8->clear();
    const char * pData = pNode->ToElement()->Attribute(attrName.c_str());
    if (pData)
    {
        pValue8->assign(pData);
        return TIXML_SUCCESS;
    }
    return TIXML_ERROR;
}
inline int QueryValueAttribute(TiXmlElement* pElement, 
                               const std::string & attrName, 
                               std::string * pValue8)
{
    pValue8->clear();
    const char * pData = pElement->Attribute(attrName.c_str());
    if (pData)
    {
        pValue8->assign(pData);
        return TIXML_SUCCESS;
    }
    return TIXML_ERROR;
}
inline void XmlDocToString(tinyxml2::XMLDocument & doc, std::string & data, bool compact = true)
{
    data.clear();
    tinyxml2::XMLPrinter stream( 0, compact);
    doc.Print( &stream );
    const char* pData = stream.CStr();
    data.assign(pData);
}
 
inline std::string SafeQueryText(TiXmlNode * pNode)
{
    const char * pData = pNode->ToElement()->GetText();
    if (!pData)
        return std::string();
    return pData;
}
inline void SafeQueryText(TiXmlNode * pNode, std::string * pText)
{
    pText->clear();
    const char * pData = pNode->ToElement()->GetText();
    if (!pData)
        return;
    pText->assign(pData);
}

inline TiXmlNode * ScanOnce_Silent(TiXmlNode * pRoot, const std::string & nodeName)
{
    TiXmlNode * node =0;
    for (    node = pRoot->FirstChild();
            node;
            node = node->NextSibling() )
    {
        if ( !node->ToElement() )
            continue;
        if (node->ToElement()->Value() == nodeName)
        {
            return node;
        }
    }
    return 0;
}

inline TiXmlNode * ScanOnce(TiXmlNode * pRoot, const std::string & nodeName)
{
    TiXmlNode * node = ScanOnce_Silent(pRoot, nodeName);
    if (node)
        return node;
    std::stringstream res;
    res<<"["<<pRoot->ToElement()->Value()<<"] Subnode not found: "<<nodeName;
    throw std::runtime_error(res.str());
}

inline TiXmlNode * Scan(TiXmlNode * pRoot, TiXmlNode * node, const std::string & nodeName)
{
    if (!node)
    {
        node = pRoot->FirstChild();
    }
    for (    ;
            node;
            node = node->NextSibling() )
    {
        if ( !node->ToElement() )
            continue;
        if (node->ToElement()->Value() == nodeName)
        {
            return node;
        }
    }
    return 0;
}

inline void QueryAttribute(TiXmlNode * pNode, const std::string & attrName, std::wstring * pResult)
{
    std::string value8;
    if (QueryValueAttribute(pNode, attrName, &value8) == TIXML_SUCCESS)
    {
        *pResult = orthia::Utf8ToUtf16(value8);
        return;
    }
    std::stringstream res;
    res<<"["<<pNode->ToElement()->Value()<<"] Attribute not found: "<<attrName;
    throw std::runtime_error(res.str());
}
inline bool QueryAttribute_Silent_Utf8(TiXmlNode * pNode, const std::string & attrName, std::string * pResult)
{
    pResult->clear();
    TiXmlElement* pElement = pNode->ToElement();
    if (!pElement)
    {
        return false;
    }
    if (QueryValueAttribute(pElement, attrName, pResult) != TIXML_SUCCESS)
    {
        return false;
    }
    return true;
}

inline void QueryNodeName(TiXmlNode * pNode, 
                          std::string * pResult)
{
    pResult->clear();
    TiXmlElement* pElement = pNode->ToElement();
    if (!pElement)
    {
        return;
    }
    const char* pValue = pElement->Value();
    if (!pValue)
        return;
    *pResult = pValue;
}
inline std::string QueryNodeName_Utf8(TiXmlNode * pNode)
{
    std::string result;
    QueryNodeName(pNode, &result);
    return result;
}

}
#endif