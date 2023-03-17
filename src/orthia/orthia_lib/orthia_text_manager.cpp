#include "orthia_text_manager.h"

#undef min
namespace orthia
{
orthia::PlatformString_type PassParameter1(const orthia::PlatformString_type & text, const orthia::PlatformString_type & param1)
{
    orthia::PlatformStringStream_type res;
    for(int i = 0; i < (int)text.size(); ++i)
    {
        if (text[i] == ORTHIA_TCSTR('%') && i +1 < (int)text.size() && text[i+1] == ORTHIA_TCSTR('1'))
        {
            res<<param1;
            ++i;
            continue;
        }
        res<<text[i];
    }
    return res.str();
}
orthia::PlatformString_type PassParameter2(const orthia::PlatformString_type & text, 
                            const orthia::PlatformString_type & param1,
                            const orthia::PlatformString_type & param2)
{
    orthia::PlatformStringStream_type res;
    for(int i = 0; i < (int)text.size(); ++i)
    {
        if (text[i] == ORTHIA_TCSTR('%') && i +1 < (int)text.size())
        {
            switch(text[i+1])
            {
            case ORTHIA_TCSTR('1'):
                res<<param1;
                break;
            case ORTHIA_TCSTR('2'):
                res<<param2;
                break;
            default:
                continue;
            }
            ++i;
            continue;
        }
        res<<text[i];
    }
    return res.str();
}

orthia::PlatformString_type PassParameter3(const orthia::PlatformString_type & text, 
                            const orthia::PlatformString_type & param1,
                            const orthia::PlatformString_type & param2,
                            const orthia::PlatformString_type & param3)
{
    orthia::PlatformStringStream_type res;
    for(int i = 0; i < (int)text.size(); ++i)
    {
        if (text[i] == ORTHIA_TCSTR('%') && i +1 < (int)text.size())
        {
            switch(text[i+1])
            {
            case ORTHIA_TCSTR('1'):
                res<<param1;
                break;
            case ORTHIA_TCSTR('2'):
                res<<param2;
                break;
            case ORTHIA_TCSTR('3'):
                res<<param3;
                break;
            default:
                continue;
            }
            ++i;
            continue;
        }
        res<<text[i];
    }
    return res.str();
}

//CTextNode
CTextNode::CTextNode(const orthia::PlatformString_type & name)
    :
        m_name(name)
{
}
void CTextNode::RegisterValue(const orthia::PlatformString_type & name, const orthia::PlatformString_type & value)
{
    std::pair<Values_type::iterator, bool> res = m_values.insert(std::make_pair(name, value));
    if (!res.second)
        throw std::runtime_error("Value already registered: " + orthia::PlatformStringToAcp(name));

    if (!name.empty())
    {
        m_valueSequence.push_back(std::make_pair(name, value));
        m_keys[value] = name;
    }
}
void CTextNode::QueryValuesFiltered(const orthia::PlatformString_type & keyFilter, 
    std::vector<orthia::PlatformString_type> * pValues) const
{
    pValues->clear();
    for(ValuesSequence_type::const_iterator it = m_valueSequence.begin(), it_end = m_valueSequence.end();
        it != it_end;
        ++it)
    {
        if (ORTHIA_TSTRNCMP(it->first.c_str(), keyFilter.c_str(), std::min(keyFilter.size(), it->first.size())) == 0)
        {
            pValues->push_back(it->second);
        }
    }
}
void CTextNode::QueryValues(std::vector<orthia::PlatformString_type> * pValues) const
{
    pValues->clear();
    for(ValuesSequence_type::const_iterator it = m_valueSequence.begin(), it_end = m_valueSequence.end();
        it != it_end;
        ++it)
    {
        pValues->push_back(it->second);
    }
}
void CTextNode::QueryValues(std::vector<std::pair<orthia::PlatformString_type, orthia::PlatformString_type> > * pValues) const
{
    *pValues = m_valueSequence;
}
void CTextNode::QueryValues(std::map<orthia::PlatformString_type, orthia::PlatformString_type> * pValues) const
{
    *pValues = m_values;
}
void CTextNode::QueryKeysByValue(std::map<orthia::PlatformString_type, orthia::PlatformString_type> * pValues) const
{
    pValues->clear();
    for(Values_type::const_iterator it = m_values.begin(), it_end = m_values.end();
        it != it_end;
        ++it)
    {
        pValues->insert(std::make_pair(it->second, it->first));
    }
}
orthia::PlatformString_type CTextNode::QueryValue(const orthia::PlatformString_type & name) const
{
    Values_type::const_iterator it = m_values.find(name);
    if (it == m_values.end())
        return name;
    return it->second;
}
orthia::PlatformString_type CTextNode::QueryValueDef(const orthia::PlatformString_type & name, 
    const orthia::PlatformString_type & def) const
{
    Values_type::const_iterator it = m_values.find(name);
    if (it == m_values.end())
        return def;
    return it->second;
}

orthia::PlatformString_type EscapeListEntry(const orthia::PlatformString_type & entry)
{
    if (entry.find_first_of(ORTHIA_TCSTR(", ;()")) != entry.npos)
    {
       return ORTHIA_TCSTR("\"") + entry + ORTHIA_TCSTR("\"");       
    }
    return entry;
}
orthia::PlatformString_type UnescapeListEntry(const orthia::PlatformString_type & value)
{
    if (value.size() > 2 && value[0] == ORTHIA_TCSTR('\"') && value[value.size()-1] == ORTHIA_TCSTR('\"'))
    {
        return orthia::PlatformString_type(value.c_str()+1, value.c_str()+value.size()-2);
    }
    return value;
}
orthia::PlatformString_type CTextNode::QueryKeyByValue(const orthia::PlatformString_type & value) const
{
    Values_type::const_iterator it = m_keys.find(value);
    if (it != m_keys.end())
    {
        return it->second;
    }
    return value;
}
orthia::PlatformString_type CTextNode::QueryKeyByEscapedValue(const orthia::PlatformString_type & value) const
{
    return QueryKeyByValue(UnescapeListEntry(value));
}
orthia::PlatformString_type CTextNode::QueryEscapedValue(const orthia::PlatformString_type & name) const
{
    orthia::PlatformString_type res = QueryValue(name);
    return EscapeListEntry(res);
}
orthia::PlatformString_type CTextNode::QueryValueListByList(const orthia::PlatformString_type & name) const
{
    std::vector<orthia::PlatformString_type> values;
    orthia::SplitStringWithoutWhitespace(name, ORTHIA_TCSTR(","), &values); 
    
    orthia::PlatformString_type result;
    for(std::vector<orthia::PlatformString_type>::const_iterator it = values.begin(), it_end = values.end();
        it != it_end;
        ++it)
    {
        if (!result.empty())
        {
            result.append(ORTHIA_TCSTR(", "));
        }
        result.append(EscapeListEntry(QueryValue(*it)));
    }
    return result;
}
std::string CTextNode::QueryValueUtf8(const orthia::PlatformString_type& name) const
{
    return PlatformStringToUtf8(QueryValue(name));
}
void CTextNode::QueryValueListByList2(const orthia::PlatformString_type & name, 
    std::vector<orthia::PlatformString_type> * pValues) const
{
    std::vector<orthia::PlatformString_type> values;
    orthia::SplitStringWithoutWhitespace(name, ORTHIA_TCSTR(","), &values);

    pValues->reserve(values.size());
    pValues->clear();
    for(std::vector<orthia::PlatformString_type>::const_iterator it = values.begin(), it_end = values.end();
        it != it_end;
        ++it)
    {
        pValues->push_back(QueryValue(*it));
    }
}

orthia::PlatformString_type CTextNode::QueryKeyListByValueList(const orthia::PlatformString_type & name) const
{
    std::map<orthia::PlatformString_type, int> valueList;
    orthia::ParseValueList(name, &valueList);

    std::map<int, orthia::PlatformString_type> convertedMap;
    for(Values_type::const_iterator it = m_values.begin(), it_end = m_values.end();
        it != it_end;
        ++it)
    {
        std::map<orthia::PlatformString_type, int>::const_iterator it2 = valueList.find(it->second);
        if (it2 == valueList.end())
        {
            continue;
        }
        convertedMap.insert(std::make_pair(it2->second, it->first));
    }

    orthia::PlatformString_type result;
    for(std::map<int, orthia::PlatformString_type>::iterator it = convertedMap.begin(), it_end = convertedMap.end();
        it != it_end;
        ++it)
    {
        if (!result.empty())
        {
            result.append(ORTHIA_TCSTR(", "));
        }
        result.append(it->second);
    }
    return result;
}

// CTextManager
CTextManager::CTextManager()
{
}
CTextManager::~CTextManager()
{
}

CTextManager::NodeRef & CTextManager::NodeRef::operator << (const ValueRef & value)
{
    pNode->RegisterValue(value.name, value.value);
    return *this;
}
CTextManager::NodeRef CTextManager::RegisterNode(const orthia::PlatformString_type & name)
{
    intrusive_ptr<CTextNode> pNode(new CTextNode(name));
    std::pair<Nodes_type::iterator, bool> res = m_nodes.insert(std::make_pair(name, pNode));
    if (!res.second)
        throw std::runtime_error("Node already registered: " + orthia::PlatformStringToAcp(name));
    return CTextManager::NodeRef(pNode);
}
CTextManager::ValueRef CTextManager::RegisterValue(const orthia::PlatformString_type & name, 
                                                   const orthia::PlatformString_type & value)
{
    return ValueRef(name, value);
}
intrusive_ptr<CTextNode> CTextManager::QueryNode(const orthia::PlatformString_type & name)
{
    Nodes_type::iterator it = m_nodes.find(name);
    if (it == m_nodes.end())
        throw std::runtime_error("Node not found: " + orthia::PlatformStringToAcp(name));
    return it->second;
}
intrusive_ptr<CTextNode> CTextManager::QueryNodeDef(const orthia::PlatformString_type& name)
{
    Nodes_type::iterator it = m_nodes.find(name);
    if (it == m_nodes.end())
        return new CTextNode(name);
    return it->second;
}
intrusive_ptr<CTextNode> CTextManager::QueryNode_Silent(const orthia::PlatformString_type & name)
{
    Nodes_type::iterator it = m_nodes.find(name);
    if (it == m_nodes.end())
        return 0;
    return it->second;
}

}