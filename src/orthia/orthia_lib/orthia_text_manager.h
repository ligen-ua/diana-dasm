#pragma once
#include "orthia_utils.h"

namespace orthia
{

orthia::PlatformString_type PassParameter1(const orthia::PlatformString_type & text, 
                            const orthia::PlatformString_type & param1);
orthia::PlatformString_type PassParameter2(const orthia::PlatformString_type & text, 
                            const orthia::PlatformString_type & param1,
                            const orthia::PlatformString_type & param2);
orthia::PlatformString_type PassParameter3(const orthia::PlatformString_type & text, 
                            const orthia::PlatformString_type & param1,
                            const orthia::PlatformString_type & param2,
                            const orthia::PlatformString_type & param3);

orthia::PlatformString_type EscapeListEntry(const orthia::PlatformString_type & entry);
orthia::PlatformString_type UnescapeListEntry(const orthia::PlatformString_type & value);
class CTextNode:public orthia::RefCountedBase
{
public:
    typedef std::map<orthia::PlatformString_type, orthia::PlatformString_type> Values_type;
    typedef std::vector<std::pair<orthia::PlatformString_type, orthia::PlatformString_type> > ValuesSequence_type;
private:
    Values_type m_values;
    ValuesSequence_type m_valueSequence;
    Values_type m_keys;
    orthia::PlatformString_type m_name;
public:
    CTextNode(const orthia::PlatformString_type & name);
    void RegisterValue(const orthia::PlatformString_type & name, const orthia::PlatformString_type & value);
    void QueryValues(std::vector<std::pair<orthia::PlatformString_type, orthia::PlatformString_type> > * pValues) const;
    void QueryValues(std::vector<orthia::PlatformString_type> * pValues) const;
    void QueryValuesFiltered(const orthia::PlatformString_type & keyFilter, std::vector<orthia::PlatformString_type> * pValues) const;
    void QueryValues(std::map<orthia::PlatformString_type, orthia::PlatformString_type> * pValues) const;
    void QueryKeysByValue(std::map<orthia::PlatformString_type, orthia::PlatformString_type> * pValues) const;
    orthia::PlatformString_type QueryEscapedValue(const orthia::PlatformString_type & name) const;
    orthia::PlatformString_type QueryKeyByEscapedValue(const orthia::PlatformString_type & value) const;
    orthia::PlatformString_type QueryValue(const orthia::PlatformString_type & name) const;
    orthia::PlatformString_type QueryKeyByValue(const orthia::PlatformString_type & name) const;

    orthia::PlatformString_type QueryValueListByList(const orthia::PlatformString_type & name) const;
    orthia::PlatformString_type QueryKeyListByValueList(const orthia::PlatformString_type & name) const;
    orthia::PlatformString_type QueryValueDef(const orthia::PlatformString_type & name, const orthia::PlatformString_type & def) const;
    void QueryValueListByList2(const orthia::PlatformString_type & name, std::vector<orthia::PlatformString_type> * pValues) const;

    std::string QueryValueUtf8(const orthia::PlatformString_type& name) const;
};
void ParseValueList(const orthia::PlatformString_type & valueList,
                    std::vector<orthia::PlatformString_type>  * pValueList);
template <class ControlType>
void InitTextControl(CTextNode * pNode, 
                     ControlType * pControl,
                     CTextNode * pPrefixNode = 0)
{
    std::vector<std::pair<orthia::PlatformString_type, orthia::PlatformString_type> > values, prefix;
    pNode->QueryValues(&values);
    if (pPrefixNode)
    {
        pPrefixNode->QueryValues(&prefix);
        values.insert(values.end(), prefix.begin(), prefix.end());
    }
    pControl->Init(values);
}
template <class OptionComboType>
void InitOptionCombo(CTextNode * pNode, 
                     OptionComboType * pControl)
{
    std::vector<orthia::PlatformString_type> values;
    pNode->QueryValues(&values);
    int pos = 1;
    for(std::vector<orthia::PlatformString_type>::const_iterator it = values.begin(), it_end = values.end();
        it != it_end;
        ++it, ++pos)
    {
        pControl->Add(pos, *it);
    }
}
class CTextManager:public orthia::RefCountedBase
{
    CTextManager(const CTextManager &);
    CTextManager & operator =(const CTextManager &);
public:
    struct ValueRef;
    struct NodeRef
    {
        intrusive_ptr<CTextNode> pNode;
        NodeRef(intrusive_ptr<CTextNode> pNode_in)
            :
                pNode(pNode_in)
        {
        }
        NodeRef & operator << (const ValueRef & value);
    };
    struct ValueRef
    {
        orthia::PlatformString_type name;
        orthia::PlatformString_type value;
        ValueRef(const orthia::PlatformString_type & name_in, const orthia::PlatformString_type & value_in)
            :
                name(name_in),
                value(value_in)
        {
        }
    };

    typedef std::map<orthia::PlatformString_type, intrusive_ptr<CTextNode> >  Nodes_type;

protected:
    Nodes_type m_nodes;

public:
    CTextManager();
    ~CTextManager();

    NodeRef RegisterNode(const orthia::PlatformString_type& name);
    ValueRef RegisterValue(const orthia::PlatformString_type& name, const orthia::PlatformString_type& value);

    intrusive_ptr<CTextNode> QueryNode(const orthia::PlatformString_type & name);
    intrusive_ptr<CTextNode> QueryNode_Silent(const orthia::PlatformString_type & name);
    intrusive_ptr<CTextNode> QueryNodeDef(const orthia::PlatformString_type& name);
};



}