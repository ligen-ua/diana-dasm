#ifndef DIANA_PE_CPP_H
#define DIANA_PE_CPP_H

#include "diana_core_cpp.h"


namespace diana
{

inline int ParseForwarderString(const std::string& fwString, std::string& dllName, std::string& functionName, OPERAND_SIZE& ordinal)
{
    dllName.clear();
    functionName.clear();
    ordinal = DI_MAX_OPERAND_SIZE;

    auto pos = fwString.find('.');
    if (pos == fwString.npos)
    {
        return DI_ERROR;
    }
    dllName.assign(fwString.begin(), fwString.begin() + pos);
    if (pos + 1 >= fwString.size())
    {
        return DI_ERROR;
    }
    if (fwString[pos + 1] == '#')
    {
        char* endPtr = 0;
        long value = strtol(fwString.data() + pos + 2, &endPtr, 10);
        if (endPtr != fwString.c_str() + fwString.size())
        {
            return DI_ERROR;
        }
        ordinal = value;
        return DI_SUCCESS;
    }
    functionName.assign(fwString.begin() + pos + 1, fwString.begin() + fwString.size());
    return DI_SUCCESS;
}

int DianaPeFile_LinkImports_Observer_QueryFunctionByOrdinal_proxy(void * pThis, 
                                                                         const char * pDllName,
                                                                         DI_UINT32 ordinal,
                                                                         OPERAND_SIZE * pAddress);

int DianaPeFile_LinkImports_Observer_QueryFunctionByName_proxy(void * pThis, 
                                                                         const char * pDllName,
                                                                         const char * pFunctionName,
                                                                         DI_UINT32 hint,
                                                                         OPERAND_SIZE * pAddress);

class CBasePeLinkImportsObserver
{
    
    friend int DianaPeFile_LinkImports_Observer_QueryFunctionByOrdinal_proxy(void * pThis, 
                                                                         const char * pDllName,
                                                                         DI_UINT32 ordinal,
                                                                         OPERAND_SIZE * pAddress);

    friend int DianaPeFile_LinkImports_Observer_QueryFunctionByName_proxy(void * pThis, 
                                                                         const char * pDllName,
                                                                         const char * pFunctionName,
                                                                         DI_UINT32 hint,
                                                                         OPERAND_SIZE * pAddress);

    DianaPeFile_LinkImports_Observer m_parent;
public:
    CBasePeLinkImportsObserver()
    {
        DianaPeFile_LinkImports_Observer_Init(&m_parent, 
                                              DianaPeFile_LinkImports_Observer_QueryFunctionByOrdinal_proxy,
                                              DianaPeFile_LinkImports_Observer_QueryFunctionByName_proxy);

    }
    virtual ~CBasePeLinkImportsObserver()
    {
    }

    virtual void QueryFunctionByOrdinal(const char * pDllName,
                                        DI_UINT32 ordinal,
                                        OPERAND_SIZE * pAddress) = 0;

    virtual void QueryFunctionByName(const char * pDllName,
                                     const char * pFunctionName,
                                     DI_UINT32 hint,
                                     OPERAND_SIZE * pAddress) = 0;

    DianaPeFile_LinkImports_Observer * GetParent() 
    { 
        return &m_parent; 
    }
};

inline int DianaPeFile_LinkImports_Observer_QueryFunctionByOrdinal_proxy(void * pThis, 
                                                                         const char * pDllName,
                                                                         DI_UINT32 ordinal,
                                                                         OPERAND_SIZE * pAddress)
{
    DI_CPP_BEGIN
        DIANA_CPP_BASE(pThis, CBasePeLinkImportsObserver, m_parent)->QueryFunctionByOrdinal(pDllName,
                                                                   ordinal,
                                                                   pAddress);
    DI_CPP_END
}

inline int DianaPeFile_LinkImports_Observer_QueryFunctionByName_proxy(void * pThis, 
                                                                         const char * pDllName,
                                                                         const char * pFunctionName,
                                                                         DI_UINT32 hint,
                                                                         OPERAND_SIZE * pAddress)
{
    DI_CPP_BEGIN
        DIANA_CPP_BASE(pThis, CBasePeLinkImportsObserver, m_parent)->QueryFunctionByName(pDllName,
                                                                pFunctionName,
                                                                hint,
                                                                pAddress);
    DI_CPP_END
}


}

#endif