#ifndef ORTHIA_INTERFACES_H
#define ORTHIA_INTERFACES_H

#include "orthia_utils.h"
#include "diana_pe_cpp.h"
#include "orthia_common_time.h"

namespace orthia
{

#define ORTHIA_MR_FLAG_READ_THROUGH    1
#define ORTHIA_MR_FLAG_READ_ABSOLUTE   2
#define ORTHIA_MR_FLAG_WRITE_ALLOCATE  4

struct IVmMemoryRangesTarget;
struct IMemoryReader
{
    virtual ~IMemoryReader(){}
    virtual void Read(Address_type offset, 
                      Address_type bytesToRead,
                      void * pBuffer,
                      Address_type * pBytesRead,
                      int flags,
                      Address_type selectorValue,
                      DianaUnifiedRegister selectorHint)=0;

    virtual bool ReportRegions(unsigned long long startAddress,
                               unsigned long long size,
                               IVmMemoryRangesTarget * pTarget,
                               bool includeReaderSpace) const { return false; } 

};
struct IMemoryReaderWriter:IMemoryReader
{
    virtual void Write(Address_type offset, 
                      Address_type bytesToWrite,
                      void * pBuffer,
                      Address_type * pBytesWritten,
                      int flags,
                      Address_type selectorValue,
                      DianaUnifiedRegister selectorHint)=0;
};


template<class Type>
void WriteExact(Type * pContainer,
                Address_type offset, 
                Address_type bytesToWrite,
                void * pBuffer,
                int flags,
                Address_type selectorValue,
                DianaUnifiedRegister selectorHint)
{
    Address_type written = 0;
    pContainer->Write(offset, 
                      bytesToWrite,
                      pBuffer,
                      &written,
                      flags,
                      selectorValue,
                      selectorHint);
    if (bytesToWrite != written)
    {
        throw std::runtime_error("Partial write");
    }
}

class CMemoryReader:public IMemoryReader
{
public:
    CMemoryReader()
    {
    }
    virtual void Read(Address_type offset, 
                      Address_type bytesToRead,
                      void * pBuffer,
                      Address_type * pBytesRead,
                      int flags,
                      Address_type selectorValue,
                      DianaUnifiedRegister selectorHint)
    {
        *pBytesRead = 0;
        if (flags & ORTHIA_MR_FLAG_READ_ABSOLUTE)
        {
            return;
        }
        void * realAddress = (void *)(offset);
        memcpy(pBuffer, realAddress, (size_t)bytesToRead);
        *pBytesRead = (size_t)bytesToRead;
    }
};

struct CommonReferenceInfo
{
    Address_type address;
    bool external;
    CommonReferenceInfo(Address_type address_in,
                       bool external_in)
      :
        address(address_in),
        external(external_in)
    {
    }
};

typedef std::vector<orthia::CommonReferenceInfo> CommonReferenceInfoArray_type;

struct CommonRangeInfo
{
    Address_type address;
    CommonReferenceInfoArray_type references;
    CommonRangeInfo(Address_type address_in = 0)
        : address(address_in)
    {
    }
};
// module info
struct CommonModuleInfo
{
    Address_type address;
    std::wstring name;
    Address_type size;
    CommonModuleInfo()
    {
    }
    CommonModuleInfo(Address_type address_in,
                     Address_type size_in,
                     const std::wstring & name_in)
                     :
        address(address_in),
        name(name_in),
        size(size_in)
    {
    }
};

struct IDebugger
{
    virtual ~IDebugger(){}
    virtual bool IsInterrupted()=0;
};
class CTestDebugger:public IDebugger
{
    CTestDebugger(const CTestDebugger&);
    CTestDebugger & operator = (const CTestDebugger&);
public:
    CTestDebugger()
    {
    }
    virtual bool IsInterrupted()
    {
        return false;
    }
};

class CInterruptException:public std::runtime_error
{
public:
    CInterruptException()
        :
            std::runtime_error("The execution was interrupted")
    {
    }
};

class CProcessor;


struct CommonHandlerParameters;
typedef bool (* CommonHandlerFunction_type)(CommonHandlerParameters & parameters);

struct IAPIHandlerDebugInterface;
struct ICommonAPIHandlerStorage
{
    virtual ~ICommonAPIHandlerStorage(){}
    virtual void RegisterHandler(OPERAND_SIZE rip, CommonHandlerFunction_type pHandler) = 0;
    virtual IAPIHandlerDebugInterface * GetDebugInterface() = 0;
};
struct IAPIHandler:IRefCountedBase, ICommonAPIHandlerStorage
{
    virtual bool HandleAPI(OPERAND_SIZE rip, CProcessor * pProcessor) = 0;
};


struct PoolDescriptor
{
    OPERAND_SIZE begin;
    OPERAND_SIZE size;
};
class CMemoryStorageOfModifiedData;
struct IAPIHandlerDebugInterface
{
    virtual ~IAPIHandlerDebugInterface(){}

    typedef enum {dtNone, dtKernel, dtUser} Debuggee_type;
        
    virtual void Init(CMemoryStorageOfModifiedData * pVirtualEnvironment)=0;
    virtual void Print(const std::wstring & text)=0;
    virtual Debuggee_type GetDebuggeeType()=0;
    virtual OPERAND_SIZE QueryModule(const char * pDllName)=0;
    virtual OPERAND_SIZE QueryFunctionAddress(OPERAND_SIZE module,
                                              const char * pDllName, 
                                              const char * pFunctionName) = 0;
};

class CVirtualSpaceClient
{
    IAPIHandlerDebugInterface * m_pAPIHandlerDebugInterface;
    CMemoryStorageOfModifiedData * m_pVirtualEnvironment;

    CVirtualSpaceClient(const CVirtualSpaceClient &);
    CVirtualSpaceClient&operator=(const CVirtualSpaceClient &);
public:
    CVirtualSpaceClient(IAPIHandlerDebugInterface * pAPIHandlerDebugInterface,
                        CMemoryStorageOfModifiedData * pVirtualEnvironment)
        :
            m_pAPIHandlerDebugInterface(pAPIHandlerDebugInterface),
            m_pVirtualEnvironment(pVirtualEnvironment)
    {
        m_pAPIHandlerDebugInterface->Init(pVirtualEnvironment);
    }
    ~CVirtualSpaceClient()
    {
        m_pAPIHandlerDebugInterface->Init(0);
    }
};
struct IAddressSpace
{
    virtual ~IAddressSpace(){}
    virtual bool IsRegionFree(Address_type offset, 
                              Address_type size,
                              Address_type * startOfNewRegion)=0;
    virtual OPERAND_SIZE GetMaxValidPointer() const = 0;
};


struct IHookAllocator
{
    virtual ~IHookAllocator(){}
    virtual void DianaHook_Alloc(OPERAND_SIZE size, 
                                 OPERAND_SIZE * pAddress,
                                 const OPERAND_SIZE * pHintAddress,
                                 int flags)=0;
    virtual void DianaHook_Free(OPERAND_SIZE pAddress)=0;
};

}

#endif