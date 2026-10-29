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

    size_t pos = fwString.find('.');
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
int DianaPeFile_LinkImports_Observer_ReportInfo_fnc_proxy(void* pThis,  
                                                          OPERAND_SIZE address);

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
    friend int DianaPeFile_LinkImports_Observer_ReportInfo_fnc_proxy(void* pThis, OPERAND_SIZE address);

    DianaPeFile_LinkImports_Observer m_parent;
    OPERAND_SIZE m_lastAddress;
public:
	CBasePeLinkImportsObserver()
		:
			m_lastAddress(0)
    {
        DianaPeFile_LinkImports_Observer_Init2(&m_parent, 
                                              DianaPeFile_LinkImports_Observer_QueryFunctionByOrdinal_proxy,
                                              DianaPeFile_LinkImports_Observer_QueryFunctionByName_proxy, 
                                              DianaPeFile_LinkImports_Observer_ReportInfo_fnc_proxy);

    }
    virtual ~CBasePeLinkImportsObserver()
    {
    }
    // Valid in QueryFunctionByName/ByOrdinal: where the function is reported (ReportInfo, just before
    // the call):
    //  - a PE import: its IAT slot; an ELF import: its GOT slot (the stream's address + the slot)
    //  - a PE export: its AddressOfFunctions entry (an RVA), not the export's value
    //  - an ELF export: the address of its name in the string table
    // The value itself is *pAddress (a PE forwarder: 0, and hint is the forwarder string's RVA).
    // Code relies on this (import names keyed by slot, floating code slot hooks): see the
    // PointerSlots tests in ui_tests.cpp
    OPERAND_SIZE GetLastAddress() const
    {
        return m_lastAddress;
    }
    virtual void QueryFunctionByOrdinal(const char * pDllName,
                                        DI_UINT32 ordinal,
                                        OPERAND_SIZE * pAddress) = 0;

    virtual void QueryFunctionByName(const char * pDllName,
                                     const char * pFunctionName,
                                     DI_UINT32 hint,
                                     OPERAND_SIZE * pAddress) = 0;

    // reportInfo_fnc, before each function; see GetLastAddress for what address is
    virtual void ReportInfo(OPERAND_SIZE address) { m_lastAddress = address; }

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

inline int DianaPeFile_LinkImports_Observer_ReportInfo_fnc_proxy(void* pThis, OPERAND_SIZE address)
{
    DI_CPP_BEGIN
        DIANA_CPP_BASE(pThis, CBasePeLinkImportsObserver, m_parent)->ReportInfo(address);
    DI_CPP_END
}

}

#endif