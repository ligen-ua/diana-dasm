#ifndef ORTHIA_COMMANDS_H
#define ORTHIA_COMMANDS_H

#include "orthia_utils.h"
#include "orthia_interfaces.h"

extern "C"
{
#include "diana_pe_analyzer.h"
}
namespace orthia
{



void Orthia_CustomMemoryRead(orthia::Address_type offset,
                                    void * pBuffer,
                                    orthia::Address_type bytesToRead,
                                    orthia::Address_type * pBytesRead,
                                    DianaUnifiedRegister selectorHint);
void Orthia_CustomMemoryReadEx(void * pBuffer,
                               orthia::Address_type bytesToRead,
                               orthia::Address_type * pBytesRead,
                               const std::string & addressText);
class CWindbgMemoryReader:public orthia::IMemoryReader
{
    int m_defaultDianaMode;
public:
    CWindbgMemoryReader(int defaultDianaMode = 0);
    void Init(int defaultDianaMode);
    virtual void Read(orthia::Address_type offset, 
                      orthia::Address_type bytesToRead,
                      void * pBuffer,
                      orthia::Address_type * pBytesRead,
                      int flags,
                      orthia::Address_type selectorValue,
                      DianaUnifiedRegister selectorHint);
};

class COrthiaDebugger:public IDebugger
{
    COrthiaDebugger(const COrthiaDebugger&);
    COrthiaDebugger & operator = (const COrthiaDebugger&);
public:
    COrthiaDebugger();
    virtual bool IsInterrupted();
};

class COrthiaWindbgAPIHandlerDebugInterface;
class COrthiaPeLinkImportsObserver:public diana::CBasePeLinkImportsObserver
{
    COrthiaWindbgAPIHandlerDebugInterface * m_pDebugInterface;
public:
    COrthiaPeLinkImportsObserver(COrthiaWindbgAPIHandlerDebugInterface * pDebugInterface);
    virtual void QueryFunctionByOrdinal(const char * pDllName,
                                        DI_UINT32 ordinal,
                                        OPERAND_SIZE * pAddress);

    virtual void QueryFunctionByName(const char * pDllName,
                                     const char * pFunctionName,
                                     DI_UINT32 hint,
                                     OPERAND_SIZE * pAddress);

};

class CMemoryStorageOfModifiedData;

class COrthiaWindbgAPIHandlerDebugInterface:public IAPIHandlerDebugInterface
{
    int m_dianaMode;
    typedef std::map<std::string, OPERAND_SIZE> ModulesMapByName_type;
    ModulesMapByName_type m_modulesMapByName;

    struct DianaSharedContext:orthia::RefCountedBase
    {
        DianaMovableReadStreamOverMemory peFileStream;
        Diana_PeFile peFile;
        diana::Guard<diana::PeFile> peFileGuard;
    };
    struct ModuleInfo
    {
        std::string windbgName;
        std::string fullName;
        OPERAND_SIZE moduleStart;
        OPERAND_SIZE moduleEnd;
        std::vector<char> preloadedModule;
        orthia::Ptr<DianaSharedContext> pDianaContext;
        ModuleInfo(const std::string & windbgName_in,
                   const std::string & fullName_in, 
                   OPERAND_SIZE moduleStart_in, 
                   OPERAND_SIZE moduleEnd_in)
            :
                windbgName(windbgName_in),
                fullName(fullName_in),
                moduleStart(moduleStart_in),
                moduleEnd(moduleEnd_in)
        {
        }
    };
    typedef std::map<OPERAND_SIZE, ModuleInfo> ModulesMapByOffset_type;
    ModulesMapByOffset_type m_modulesMapByOffset;

    void RegisterModule(const std::string & windbgName,
                        const std::string & fullName, 
                        OPERAND_SIZE moduleStart, 
                        OPERAND_SIZE moduleEnd);
    std::string NormalizeModuleName(const std::string & fullName);

    bool m_printHappened;
    CMemoryStorageOfModifiedData * m_pVirtualEnvironment;

    OPERAND_SIZE QueryFunctionNameEx(ModuleInfo & info,
                                     const char * pDllName,
                                     const char * pFunctionName,
                                     DI_UINT32 ordinal);

    OPERAND_SIZE QueryFunctionNameEx(ModuleInfo & info,
                                     const char * pDllName,
                                     const char * pFunctionName,
                                     DI_UINT32 ordinal,
                                     std::string * pTargetDll,
                                     std::string * pTargetFunction);
public:
    COrthiaWindbgAPIHandlerDebugInterface();
    void Reload(int dianaMode);
    void Init(CMemoryStorageOfModifiedData * pVirtualEnvironment);
    virtual OPERAND_SIZE QueryModule(const char * pDllName);
    virtual OPERAND_SIZE QueryFunctionAddress(OPERAND_SIZE module,
                                              const char * pDllName, 
                                              const char * pFunctionName);

    OPERAND_SIZE QueryFunctionAddressByOrdinal(const char * pDllName,
                                               DI_UINT32 ordinal);
    Debuggee_type GetDebuggeeType();
    virtual void Print(const std::wstring & text);
    bool HaveSomePrintsHappened() const;
};

class CWindbgAddressSpace:public IAddressSpace
{
    OPERAND_SIZE m_maxValid;
    OPERAND_SIZE m_maxValidKernel;
public:
    CWindbgAddressSpace();
    virtual bool IsRegionFree(Address_type offset, 
                              Address_type size,
                              Address_type * startOfNewRegion);
    virtual OPERAND_SIZE GetMaxValidPointer() const;
};


}

#endif