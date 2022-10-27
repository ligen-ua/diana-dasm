#ifndef DIANA_PE_CPP_H
#define DIANA_PE_CPP_H

#include "diana_core_cpp.h"


namespace diana
{

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