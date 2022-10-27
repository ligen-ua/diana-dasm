#include "map"
#include "orthia_shuttle_interface.h"

#pragma pack(push,_CRT_PACKING)
#if (_MSC_VER > 1600)
//-----------------------------
// DinkumWare New
// VS 11 - 14
// struct _Tree_node
//{
//  _Voidptr _Left;    // left subtree, or smallest element if head
//  _Voidptr _Parent;    // parent, or root of tree if head
//  _Voidptr _Right;    // right subtree, or largest element if head
//  char _Color;    // _Red or _Black, _Black if head
//  char _Isnil;    // true only if head (also nil) node
//  _Value_type _Myval;    // the stored value, unused if head
class IterableMap:public std::map<int, int>
{
    typedef std::map<int, int> Parent_type;
public:
    class Iterator
    {
        bool m_bFirstRun;
        Parent_type::const_iterator m_it;
        Parent_type::const_iterator m_it_end;
        int m_plusDelta;
        
        void * InternalGet() const
        {
            return (char *)&m_it->first + m_plusDelta;
        }
    public:
        Iterator(Parent_type::const_iterator it,
                 Parent_type::const_iterator it_end,
                 int plusDelta)
            :
                m_it(it),
                m_it_end(it_end),
                m_bFirstRun(true),
                m_plusDelta(plusDelta)
        {
        }
       
        void * GetValue()
        {
            return InternalGet();
        }
        const void * GetValue() const
        {
            return InternalGet();
        }
        bool Inc()
        {    
            if (m_it == m_it_end)
            {
                return false;
            }
            if (m_bFirstRun)
            {
                m_bFirstRun = false;
                return true;
            }
            ++m_it;
            return m_it != m_it_end;
        }
    };
    Iterator CreateIterator(int plusDelta)
    {
        return Iterator(begin(), end(), plusDelta);
    }
};

ORTHIA_SHUTTLE_EXPORT(dinkumware_map_for_each)
{
    orthia_shuttle::CPrintStream hyperOutput(arguments.pHypervisorInterface);
    int plusDelta = 0;
    if (arguments.argsCount != 1 && arguments.argsCount != 2)
    {
        hyperOutput<<"Expected: address [plus]";
        return;
    }
    IterableMap * p = (IterableMap * )arguments.GetArgument(0);

    if (arguments.argsCount == 2)
    {
        plusDelta = (int)arguments.GetArgument_Long(1);
    }

    IterableMap::Iterator it = p->CreateIterator(plusDelta);
        
    while(it.Inc())
    {
        hyperOutput<<it.GetValue()<<"\n";
    }
}


#else
// == > (_MSC_VER <= 1600)
//-----------------------------
// DinkumWare Old
//  7 - 10 studios
struct OldNode
{
    OldNode * _Left; // left subtree, or smallest element if head
    OldNode * _Parent;       // parent, or root of tree if head
    OldNode * _Right;        // right subtree, or largest element if head
};
//  value_type _Myval;      // the stored value, unused if head
struct OldNodePart2
{
    char _Color;    // _Red or _Black, _Black if head
    char _Isnil;    // true only if head (also nil) node
};

#pragma pack(pop)


static
char & GetIsNil(OldNode * pNode, 
                size_t valueTypeSize)
{
    if (valueTypeSize % _CRT_PACKING)
    {
        valueTypeSize += _CRT_PACKING - (valueTypeSize % _CRT_PACKING);
    }
    OldNodePart2 * pNode2 = (OldNodePart2*)((char*)(pNode + 1) + valueTypeSize);
    return pNode2->_Isnil;
}
class IterableMap:private std::map<int, int>
{
    typedef std::map<int, int> Parent_type;
public:
    class Iterator
    {
        bool m_bFirstRun;
        OldNode * m_pNode;
        size_t m_valueTypeSize;
    public:
        Iterator(OldNode * pNode,
                 size_t valueTypeSize)
            :
                m_pNode(pNode),
                m_valueTypeSize(valueTypeSize),
                m_bFirstRun(true)
        {
        }
        OldNode * ScanMin(OldNode * pNode)
        {    
            while (!GetIsNil(pNode->_Left, m_valueTypeSize))
                pNode = pNode->_Left;
            return pNode;
        }
        void * GetValue()
        {
            return m_pNode+1;
        }
        const void * GetValue() const
        {
            return m_pNode+1;
        }
        bool Inc()
        {    
            if (m_pNode == 0
                || GetIsNil(m_pNode, m_valueTypeSize))
            {
                return false;
            }
            if (m_bFirstRun)
            {
                m_bFirstRun = false;
                return true;
            }

            if (!GetIsNil(m_pNode->_Right, m_valueTypeSize))
            {
                m_pNode = ScanMin(m_pNode->_Right);
                return true;
            }

            // climb looking for right subtree
            OldNode * pNode = 0;
            for(;;)
            {
                pNode = m_pNode->_Parent;
                if (GetIsNil(pNode, m_valueTypeSize))
                {
                    break;
                }
                if (m_pNode != pNode->_Right)
                {
                    break;
                }
                m_pNode = pNode; 
            }
            m_pNode = pNode;
            if (m_pNode == 0
                || GetIsNil(m_pNode, m_valueTypeSize))
            {
                return false;
            }
            return true;
        }
    };
    Iterator CreateIterator(size_t valueTypeSize)
    {
        return Iterator((OldNode * )_Myhead->_Left, valueTypeSize);
    }
};

ORTHIA_SHUTTLE_EXPORT(dinkumware_map_for_each)
{
    orthia_shuttle::CPrintStream hyperOutput(arguments.pHypervisorInterface);
    if (arguments.argsCount != 2)
    {
        hyperOutput<<"Expected: address sizeof_value_type";
        return;
    }
    IterableMap * p = (IterableMap * )arguments.GetArgument(0);
    IterableMap::Iterator it = p->CreateIterator((size_t)arguments.GetArgument_Long(1));

    while(it.Inc())
    {
        hyperOutput<<it.GetValue()<<"\n";
    }
}
#endif
