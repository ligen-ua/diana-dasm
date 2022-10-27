#ifndef ORTHIA_MEMORY_CACHE_H
#define ORTHIA_MEMORY_CACHE_H

#include "orthia_interfaces_vm.h"

extern "C"
{
#include "diana_win32.h"
#include "diana_win32_sys_memcpy.h"
}

struct _DIANA_IMAGE_SECTION_HEADER;
namespace orthia
{

class CMemoryCache:public IMemoryReader
{
    typedef std::vector<char> RegionData_type;
    RegionData_type m_regionData;
    Address_type m_regionAddress;
    Address_type m_regionSize;

    IMemoryReader * m_pReader;
public:
    CMemoryCache(IMemoryReader * pReader,
                 Address_type regionAddress);

    void Init(Address_type regionAddress,
              Address_type size,
              _DIANA_IMAGE_SECTION_HEADER * pCapturedSections,
              int capturedSectionCount);
    void Init(Address_type regionAddress,
              Address_type size);
    virtual void Read(Address_type offset, 
                      Address_type bytesToRead,
                      void * pBuffer,
                      Address_type * pBytesRead,
                      int flags,
                      Address_type selectorValue,
                      DianaUnifiedRegister selectorHint);

};

class CMemoryStorageOfModifiedData:public IMemoryReaderWriter
{
public:
    struct PageInfo
    {
        std::vector<char> data;
    };
    typedef std::map<Address_type, PageInfo> PagesMap_type;

private:
    // swapped fields:
    orthia::orthia_pcg32_random m_allocHintRandom;
    OPERAND_SIZE m_lastSuccessEnd;
    mutable std::vector<char> m_tmpBuffer;
    int m_pageSize;
    PagesMap_type m_pageMap;
    mutable IMemoryReader * m_pReader;

    typedef std::map<Address_type, CMemoryStorageOfModifiedData> SelectorSpacesMap_type;
    SelectorSpacesMap_type m_selectorsMap;
    bool m_hasAssociatedSelector;

    typedef std::map<Address_type, int> BreakpointMap_type;
    BreakpointMap_type  m_breakpoints;
    BreakpointMap_type  m_pageBreakpoints;
    
    struct AllocationInfo
    {
        static const int flags_released = 1;
        Address_type size;
        int flags;
        AllocationInfo(Address_type size_in)
            :
                size(size_in),
                flags(0)
        {
        }
    };
    typedef std::map<Address_type, AllocationInfo> AllocationsMap_type;
    AllocationsMap_type m_allocations;
        
    Address_type QueryPageStartAddress(Address_type offset) const;

    bool ReportRegion(IAddressSpace * pExternalSpace, 
                      Address_type startOffset, 
                      Address_type endOffset, 
                      Address_type size,
                      Address_type * pOffset);
    CMemoryStorageOfModifiedData * QuerySelector(Address_type selectorValue);
public:
    CMemoryStorageOfModifiedData(IMemoryReader * pReader, 
                                 int pageSize = 0x100,
                                 bool hasAssociatedSelector = false);
    void AddMemoryWriteBreakPoint(Address_type address);
    virtual bool ReportRegions(unsigned long long startAddress,
                               unsigned long long size,
                               IVmMemoryRangesTarget * pTarget,
                               bool includeReaderSpace) const;
    virtual void Read(Address_type offset, 
                      Address_type bytesToRead,
                      void * pBuffer,
                      Address_type * pBytesRead,
                      int flags,
                      Address_type selectorValue,
                      DianaUnifiedRegister selectorHint);
    virtual void Write(Address_type offset, 
                       Address_type bytesToWrite,
                       void * pBuffer,
                       Address_type * pBytesWritten,
                       int flags,
                       Address_type selectorValue,
                       DianaUnifiedRegister selectorHint);

    bool ChooseTheRegion(IAddressSpace * pExternalSpace, 
                         Address_type * pOffset,
                         Address_type size);
    bool VmAlloc(IAddressSpace * pExternalSpace,
                 Address_type offsetHint,
                 Address_type size,
                 Address_type * pResult,
                 Address_type * pSize,
                 bool bCommit);
    bool VmFree(IAddressSpace * pExternalSpace,
                Address_type offsetHint,
                Address_type * pResult,
                Address_type * pSize,
                bool bRelease);
    void Swap(CMemoryStorageOfModifiedData & other);

    int GetPageSize() const { return m_pageSize; }
    void Clear() { m_pageMap.clear(); }
    typedef PagesMap_type::const_iterator const_iterator;
    const_iterator begin() const { return m_pageMap.begin(); }
    const_iterator end() const { return m_pageMap.end(); }
    const_iterator find(Address_type offset) const { return m_pageMap.find(offset); }
};


class CReaderOverVector:public IMemoryReader
{
public:
    typedef std::vector<char> RegionData_type;
private:
    RegionData_type m_regionData;
    Address_type m_regionAddress;
public:
    CReaderOverVector(Address_type regionAddress, 
                    const std::vector<char> & data);
    CReaderOverVector(Address_type regionAddress, 
                    int size);
    
    virtual void Read(Address_type offset, 
                      Address_type bytesToRead,
                      void * pBuffer,
                      Address_type * pBytesRead,
                      int flags,
                      Address_type selectorValue,
                      DianaUnifiedRegister selectorHint);

    RegionData_type & GetData() { return m_regionData; }
    const RegionData_type & GetData() const { return m_regionData; }
};


class CReaderOverRealWorld:public IMemoryReader
{
    Address_type m_regionAddress;
    const void * m_pAddress;
    DI_UINT64 m_size;
    sys_memcpy_ptr_type m_memcpy_sys;

    CReaderOverRealWorld(const CReaderOverRealWorld&);
    CReaderOverRealWorld& operator = (const CReaderOverRealWorld&);
public:
    CReaderOverRealWorld();
    ~CReaderOverRealWorld();
    virtual void Read(Address_type offset, 
                      Address_type bytesToRead,
                      void * pBuffer,
                      Address_type * pBytesRead,
                      int flags,
                      Address_type selectorValue,
                      DianaUnifiedRegister selectorHint);
};


}

#endif