#ifndef DIANA_CORE_CPP_H
#define DIANA_CORE_CPP_H

extern "C"
{
#include "diana_core.h"
#include "diana_pe.h"
#include "diana_text_output_masm.h"
#include "diana_allocators.h"
}
#include "stdexcept"
#include "sstream"
#include "string"
#include "algorithm"
#include "vector"
#include "limits.h"
namespace diana
{

inline std::string QueryErrorCode(int value)
{
    const char * pResult = Diana_QueryErrorText_Silent(value);
    if (pResult)
        return pResult;

    std::stringstream res;
    res<<"Unknown: "<<value;
    return res.str();
}

class CException:public std::runtime_error
{
    int m_errorCode;

    std::string ToText(int errorCode,
                       const std::string & text)
    {
        std::stringstream res;
        res<<text<<", errorCode = "<<errorCode;
        return res.str();
    }
public:
    CException(int errorCode,
               const std::string & text)
        :
            std::runtime_error(ToText(errorCode, text)),
            m_errorCode(errorCode)
    {
    }
};

template<class Strategy>
class Guard
{
    typename Strategy::ObjectType m_object;
    bool m_needToFree;
    Guard(const Guard &);
    Guard & operator =(const Guard &);
public:
    Guard()
        :
            m_needToFree(false)
    {
    }
    Guard(const typename Strategy::ObjectType & object)
        :
            m_object(object),
            m_needToFree(true)
    {
    }
    void reset(const typename Strategy::ObjectType & object)
    {
        Guard guard2(object);
        guard2.swap(*this);
    }
    void swap(Guard & other)
    {
        std::swap(other.m_object, m_object);
        std::swap(other.m_needToFree, m_needToFree);
    }
    ~Guard()
    {
        if (m_needToFree)
        {
            Strategy::Free(m_object);
        }
    }
};

struct PeFile
{
    typedef Diana_PeFile * ObjectType;
    static void Free(Diana_PeFile * pObject)
    {
        DianaPeFile_Free(pObject);
    }
};

struct InstructionsOwner
{
    typedef Diana_InstructionsOwner * ObjectType;
    static void Free(Diana_InstructionsOwner * pObject)
    {
        Diana_InstructionsOwner_Free(pObject);
    }
};
#define DIANA_DEF_ERR_STRING    "DiException"
#define DI_CHECK_CPP2(di____Expression, di____ErrorString) { int di____status = (di____Expression); if (di____status) { throw diana::CException(di____status, di____ErrorString); } }
#define DI_CHECK_CPP(di____Expression) DI_CHECK_CPP2(di____Expression, DIANA_DEF_ERR_STRING)


#define DIANA_CPP_BASE(address, type, field)    ((type*)(((char*)(address) - DIANA_FIELD_OFFSET(type, field))))
#define DI_CPP_BEGIN    try {
#define DI_CPP_END      } catch(const std::exception & ) { return DI_ERROR; }  return DI_SUCCESS;

class CMasmString
{
    std::vector<char> m_buffer;
    DianaStringOutputContext m_context;
    CMasmString(const CMasmString&);
    CMasmString&operator = (const CMasmString&);
public:
    CMasmString(int maxSize = 1024, 
                int spacesCount = 1)
        :
            m_buffer(maxSize)
    {
        DianaStringOutputContext_Init(&m_context, 
                                      DianaTextOutput_String, 
                                      DianaOpOutput_String, 
                                      spacesCount,
                                      &m_buffer.front(), 
                                      m_buffer.size());
    }
    ~CMasmString()
    {
    }
    const char * Assign(DianaParserResult * pResult, 
                        OPERAND_SIZE instructionRIP)
    {
        DI_CHECK_CPP(DianaTextOutputContext_TextOut(&m_context.parent, pResult, instructionRIP));
        return &m_buffer.front();
    }
};


class CDefaultAllocator
{
    CDefaultAllocator(const CDefaultAllocator&);
    CDefaultAllocator & operator = (const CDefaultAllocator&);
protected:
    DianaMAllocator m_allocator;
public:
    CDefaultAllocator()
    {
        Diana_InitMAllocator(&m_allocator);
    }
    virtual ~CDefaultAllocator()
    {
    }
    DianaMAllocator * GetAllocator()
    {
        return &m_allocator;
    }
};
}


#ifdef DI_LINK_C_RUNTIME_CPP 
extern "C"
{
#include "diana_c_runtime.inc"
}
#endif

#endif