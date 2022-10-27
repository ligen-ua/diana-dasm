#include "orthia_memory_cache.h"
#include "diana_pe.h"
#include "orthia_streams.h"
#undef min
#undef max
namespace orthia
{

CMemoryCache::CMemoryCache(IMemoryReader * pReader,
                           Address_type regionAddress)
    :
        m_pReader(pReader),
        m_regionAddress(regionAddress),
        m_regionSize(0)
{
}
    

void CMemoryCache::Init(Address_type regionAddress,
                          Address_type size)
{
   if (!size)
        return;

    if (size >= SIZE_MAX)
        throw std::runtime_error("Internal error: size too big");
    m_regionData.resize((size_t)size);
    
    m_regionAddress = regionAddress;
    m_regionSize = m_regionData.size();
    
    Address_type bytesRead = 0;
    m_pReader->Read(m_regionAddress, 
                    m_regionSize, 
                    &m_regionData.front(), 
                    &bytesRead,
                    ORTHIA_MR_FLAG_READ_THROUGH,
                    0,
                    reg_none);
}
void CMemoryCache::Init(Address_type regionAddress,
                          Address_type size,
                          DIANA_IMAGE_SECTION_HEADER * pCapturedSections,
                          int capturedSectionCount)
{
    if (!size)
        return;

    if (size >= SIZE_MAX)
        throw std::runtime_error("Internal error: size too big");
    m_regionData.resize((size_t)size);
    
    m_regionAddress = regionAddress;
    m_regionSize = m_regionData.size();
    for(int i = 0; i < capturedSectionCount; ++i)
    {
        DIANA_IMAGE_SECTION_HEADER & section = pCapturedSections[i];
        if (section.Misc.VirtualSize > m_regionData.size())
            continue;
        if (section.VirtualAddress > m_regionData.size() - section.Misc.VirtualSize)
            continue;
        Address_type bytesRead = 0;
        m_pReader->Read(m_regionAddress + section.VirtualAddress, 
                        section.Misc.VirtualSize, 
                        section.VirtualAddress + &m_regionData.front(), 
                        &bytesRead,
                        ORTHIA_MR_FLAG_READ_THROUGH,
                        0,
                        reg_none);
    }
}
    
void CMemoryCache::Read(Address_type offset, 
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
        m_pReader->Read(offset, 
                        bytesToRead, 
                        pBuffer, 
                        pBytesRead, 
                        flags,
                        selectorValue,
                        selectorHint);
        return;
    }
    // it gets relative addresses!!!!
    offset += m_regionAddress;
    if (!m_regionSize)
    {
        m_pReader->Read(offset, 
                        bytesToRead, 
                        pBuffer, 
                        pBytesRead, 
                        flags,
                        selectorValue,
                        selectorHint);
        return;
    }

    Address_type end = offset + bytesToRead;

    // 1
    //A: [-]
    //B:    [--------]
    // 2
    //A: [----]
    //B:    [--------]
    // 3
    //A: [----------------]
    //B:    [--------]
    // 4
    //A:      [---]
    //B:    [--------]
    // 5
    //A:        [-------]
    //B:    [--------]
    // 6
    //A:               [---]
    //B:    [--------]

    if (offset < m_regionAddress)
    {
        // 1,2,3
        return;
    }
    // 4, 5, 6
    Address_type regionEnd = m_regionAddress + m_regionSize;
    if (offset >= regionEnd)
    {
        // 6
        return;
    }
    if (end < regionEnd)
    {
        // 4
        memcpy(pBuffer, 
               &m_regionData.front() + offset - m_regionAddress,
               (size_t)bytesToRead);
        *pBytesRead = bytesToRead;
        return;
    }
    // 5
    Address_type skipSize = offset - m_regionAddress;
    Address_type sizeToRead = m_regionSize - skipSize;
    memcpy(pBuffer, 
           &m_regionData.front() + skipSize,
           (size_t)sizeToRead);    
    
    *pBytesRead += sizeToRead;
}

// CMemoryStorageOfModifiedData
CMemoryStorageOfModifiedData::CMemoryStorageOfModifiedData(IMemoryReader * pReader,
                                                           int pageSize,
                                                           bool hasAssociatedSelector)
    :
        m_lastSuccessEnd(0),
        m_pReader(pReader),
        m_pageSize(pageSize),
        m_hasAssociatedSelector(hasAssociatedSelector)
{
    if (m_pageSize < 0x10 || (m_pageSize % 0x10))
    {
        throw std::runtime_error("Invalid parameter");
    }
}

// COutputWriteHelper
class COutputWriteHelper
{
    char * m_pBuffer;
    Address_type m_bytesToWrite;
    Address_type * m_pBytesWritten;
    Address_type m_currentOffset;
    IVmMemoryRangesTarget * m_pTarget;
    
public:
    COutputWriteHelper(Address_type offset,
                       void * pBuffer, 
                       Address_type bytesToWrite,
                       Address_type * pBytesWritten,
                       IVmMemoryRangesTarget * pTarget)
        :
            m_currentOffset(offset),
            m_pBuffer((char * )pBuffer),
            m_bytesToWrite(bytesToWrite),
            m_pBytesWritten(pBytesWritten),
            m_pTarget(pTarget)
    {
        *m_pBytesWritten = 0;
    }

    bool Shift(Address_type delta)
    {
        if (delta > m_bytesToWrite)
        {
            throw std::runtime_error("Internal error");
        }
        m_pBuffer += delta;
        m_bytesToWrite -= delta;
        m_currentOffset += delta;
        return m_bytesToWrite == 0;
    }
    bool WriteInvalidRegion(Address_type size)
    {
        Address_type delta = std::min(size, m_bytesToWrite);
        m_pTarget->OnRange(VmMemoryRangeInfo(m_currentOffset, delta, 0),
                              0);
        m_bytesToWrite -= delta;
        m_currentOffset += delta;
        *m_pBytesWritten += delta;
        return m_bytesToWrite == 0;
    }
    bool Write(const void * pData, 
               Address_type size)
    {
        Address_type delta = std::min(size, m_bytesToWrite);
        if (m_pTarget)
        {
            m_pTarget->OnRange(VmMemoryRangeInfo(m_currentOffset, delta, VmMemoryRangeInfo::flags_hasData),
                              (const char *)pData);
        }
        else
        {
            memcpy(m_pBuffer, pData, (size_t)delta);
            m_pBuffer += delta;
        }
        m_bytesToWrite -= delta;
        m_currentOffset += delta;
        *m_pBytesWritten += delta;
        return m_bytesToWrite == 0;
    }

    Address_type GetCurrentOffset() const
    {
        return m_currentOffset;
    }
    Address_type GetRequiredBytesCount() const 
    {
        return m_bytesToWrite;
    }
};
Address_type CMemoryStorageOfModifiedData::QueryPageStartAddress(Address_type offset) const
{
    Address_type offsetInPage = (offset % m_pageSize);
    return offset - offsetInPage;
}

static bool TestAndReportReaderSpace(COutputWriteHelper & writeHelper,
                                     unsigned long long size,
                                     IMemoryReader * pReader,
                                     std::vector<char> & tmpBuffer,
                                     bool includeReaderSpace)
{
    if (!includeReaderSpace)
    {
        return writeHelper.WriteInvalidRegion(size);
    }
    const int optimisticPageSize = 64*1024;
    const int pessimisticPageSize = 0x1000;
    
    Address_type sizeToReport = std::min(size, writeHelper.GetRequiredBytesCount());
    Address_type startOffset = writeHelper.GetCurrentOffset();

    tmpBuffer.resize(optimisticPageSize);

    bool dataTransferMode = true;
    for(;sizeToReport;)
    {

        Address_type pageSize = optimisticPageSize;
        if (!dataTransferMode)
        {
            pageSize = pessimisticPageSize;
        }
        Address_type currentOffset = writeHelper.GetCurrentOffset();
        Address_type currentBytesToRead = std::min(sizeToReport, 
                                                   pageSize);

        Address_type currentBytesRead = 0;
        pReader->Read(currentOffset,
                      currentBytesToRead,
                      &tmpBuffer.front(),
                      &currentBytesRead,
                      0,
                      0,
                      reg_none);

        if (currentBytesRead)
        {
            if (writeHelper.Write(&tmpBuffer.front(), currentBytesRead))
            {
                return true;
            }
            sizeToReport -= currentBytesRead;
            if (!sizeToReport)
            {
                return false;
            }
            if (currentBytesRead == currentBytesToRead)
            {
                dataTransferMode = true;
                continue;
            }
        }
        // report bad data
        Address_type badDataSize = currentBytesToRead - currentBytesRead;

        Address_type pessimisticSizeToReport = pessimisticPageSize;
        if (currentOffset % pessimisticPageSize)
        {
            pessimisticSizeToReport = pessimisticPageSize - currentOffset % pessimisticPageSize;
        }
        pessimisticSizeToReport = std::min(badDataSize, pessimisticSizeToReport);
        if (writeHelper.WriteInvalidRegion(pessimisticSizeToReport))
        {
            return true;
        }
        sizeToReport -= pessimisticSizeToReport;
        dataTransferMode = false;
    }
    return writeHelper.GetRequiredBytesCount() == 0;
}
bool CMemoryStorageOfModifiedData::ReportRegions(unsigned long long startAddress,
                                                 unsigned long long size,
                                                 IVmMemoryRangesTarget * pTarget,
                                                 bool includeReaderSpace) const
{
    Address_type bytesToRead = size;
    Address_type bytesRead = 0;
    if (!m_pReader)
    {
        return false;
    }
    COutputWriteHelper writeHelper(startAddress, 
                                   0, 
                                   bytesToRead, 
                                   &bytesRead,
                                   pTarget);

    if (bytesToRead == 0)
    {
        return true;
    }

    Address_type firstPage = QueryPageStartAddress(startAddress);
    PagesMap_type::const_iterator it = m_pageMap.lower_bound(firstPage);
    PagesMap_type::const_iterator it_end = m_pageMap.end();
    if (it == it_end)
    {
        TestAndReportReaderSpace(writeHelper,
                                 bytesToRead,
                                 m_pReader,
                                 m_tmpBuffer,
                                 includeReaderSpace);
        return true;
    }
    if (startAddress < it->first)
    {
        Address_type delta = it->first - startAddress;
        Address_type toRead = std::min(delta, bytesToRead);

        if (TestAndReportReaderSpace(writeHelper,
                                     toRead,
                                     m_pReader,
                                     m_tmpBuffer,
                                     includeReaderSpace))
        { 
            return true;
        }
    }

    for(;;)
    {
        Address_type delta = writeHelper.GetCurrentOffset() - it->first;
        if (delta >= (Address_type)m_pageSize)
        {
            throw std::runtime_error("Internal error");
        }
        if (writeHelper.Write(&it->second.data.front() + delta, m_pageSize - delta))
        {
            return true;
        }

        PagesMap_type::const_iterator next_page = it;
        ++next_page;
        
        Address_type rangeBetweenPages = 0;
        if (next_page == it_end)
        {
            rangeBetweenPages = writeHelper.GetRequiredBytesCount();
        }
        else
        {
            rangeBetweenPages = next_page->first - (Address_type)m_pageSize - it->first;
        }
        if (rangeBetweenPages)
        {
            if (TestAndReportReaderSpace(writeHelper,
                                         rangeBetweenPages,
                                         m_pReader,
                                         m_tmpBuffer,
                                         includeReaderSpace))
            { 
                return true;
            }
        }
        ++it;
        if (it == it_end)
        {
            break;
        }
    }
    return true;
}
CMemoryStorageOfModifiedData * CMemoryStorageOfModifiedData::QuerySelector(Address_type selectorValue)
{
    SelectorSpacesMap_type::iterator it = m_selectorsMap.find(selectorValue);
    if (it != m_selectorsMap.end())
    {
        return &it->second;
    }
    std::pair<SelectorSpacesMap_type::iterator, bool> res = 
        m_selectorsMap.insert(std::make_pair(selectorValue, 
                                             CMemoryStorageOfModifiedData(m_pReader, m_pageSize, true)));
    return &res.first->second;
}
void CMemoryStorageOfModifiedData::Read(Address_type offset, 
                                        Address_type bytesToRead,
                                        void * pBuffer,
                                        Address_type * pBytesRead,
                                        int flags,
                                        Address_type selectorValue,
                                        DianaUnifiedRegister selectorHint)
{
    if (m_breakpoints.find(offset) != m_breakpoints.end())
    {
        __debugbreak();
    }
    *pBytesRead = 0;
    if (!m_pReader)
    {
        return;
    }
    if (selectorValue)
    {
        if (!m_hasAssociatedSelector)
        {
            QuerySelector(selectorValue)->Read(offset, 
                                               bytesToRead,
                                               pBuffer,
                                               pBytesRead,
                                               flags,
                                               selectorValue,
                                               selectorHint);
            return;
        }
    }
    COutputWriteHelper writeHelper(offset, pBuffer, bytesToRead, pBytesRead, 0);

    if (bytesToRead == 0)
    {
        return;
    }

    Address_type firstPage = QueryPageStartAddress(offset);
    PagesMap_type::iterator it = m_pageMap.lower_bound(firstPage);
    PagesMap_type::iterator it_end = m_pageMap.end();
    for(;it != it_end; ++it)
    {
        if (offset < it->first ||
            (offset - it->first) < m_pageSize) 
        {
            break;
        }
    }

    if (it == it_end)
    {
        m_pReader->Read(offset, 
                        bytesToRead,
                        pBuffer,
                        pBytesRead,
                        flags,
                        selectorValue,
                        selectorHint);
        return;
    }

    if (offset < it->first)
    {
        Address_type delta = it->first - offset;
        Address_type toRead = std::min(delta, bytesToRead);
        m_pReader->Read(offset, 
                        toRead,
                        pBuffer,
                        pBytesRead,
                        flags,
                        selectorValue,
                        selectorHint);
        if (writeHelper.Shift(*pBytesRead))
        {
            return;
        }
        if (*pBytesRead < toRead)
        {
            return;
        }
    }

    for(;;)
    {
        if (writeHelper.GetCurrentOffset() < it->first)
        {
            throw std::runtime_error("Internal error");
        }
        Address_type delta = writeHelper.GetCurrentOffset() - it->first;
        if (delta >= (Address_type)m_pageSize)
        {
            throw std::runtime_error("Internal error");
        }

        if (writeHelper.Write(&it->second.data.front() + delta, m_pageSize - delta))
        {
            return;
        }

        PagesMap_type::iterator next_page = it;
        ++next_page;
        
        Address_type rangeBetweenPages = 0;
        if (next_page == it_end)
        {
            rangeBetweenPages = writeHelper.GetRequiredBytesCount();
        }
        else
        {
            rangeBetweenPages = next_page->first - (Address_type)m_pageSize - it->first;
        }
        if (rangeBetweenPages)
        {
            m_tmpBuffer.resize((size_t)rangeBetweenPages);

            Address_type readBytes = 0;
            Address_type requestedBytes(std::min(rangeBetweenPages, writeHelper.GetRequiredBytesCount()));
            m_pReader->Read(writeHelper.GetCurrentOffset(), 
                            requestedBytes,
                            &m_tmpBuffer.front(),
                            &readBytes,
                            flags,
                            selectorValue,
                            selectorHint);

            if (readBytes == 0)
            {
                return;
            }
            if (writeHelper.Write(&m_tmpBuffer.front(), readBytes))
            {
                return;
            }
            if (readBytes < requestedBytes)
            {
                return;
            }
        }
        ++it;
        if (it == it_end)
        {
            break;
        }
    }
}
void CMemoryStorageOfModifiedData::Swap(CMemoryStorageOfModifiedData & other)
{
    m_allocations.swap(other.m_allocations);
    m_tmpBuffer.swap(other.m_tmpBuffer);
    std::swap(m_pageSize, other.m_pageSize);
    m_pageMap.swap(other.m_pageMap);
    std::swap(m_pReader, other.m_pReader);
    m_selectorsMap.swap(other.m_selectorsMap);
    std::swap(m_hasAssociatedSelector, other.m_hasAssociatedSelector);
    m_allocHintRandom.swap(other.m_allocHintRandom);

    std::swap(m_lastSuccessEnd, other.m_lastSuccessEnd);
}

static Address_type GrowAlignPage(Address_type p, const Address_type pageSize)
{
    Address_type modulo = p%pageSize;
    if (!modulo)
    {
        return p;
    }
    Address_type newP = p + (pageSize - modulo);
    if (newP < p)
    {
        throw std::runtime_error("Overflow");
    }
    return newP;

}
bool CMemoryStorageOfModifiedData::ReportRegion(IAddressSpace * pExternalSpace, 
                                                Address_type startOffset, 
                                                Address_type endOffset, 
                                                Address_type size,
                                                Address_type * pOffset)
{
    const Address_type pageSize = 0x1000;
            
    
    VmMemoryRangesTargetOverVector checker;
    for(Address_type p = GrowAlignPage(startOffset, pageSize); p < endOffset - size; )
    {
        Address_type startOfNewRegion = 0;
        if (pExternalSpace->IsRegionFree(p, size, &startOfNewRegion))
        {
            checker.Clear();
            bool bSuccess = true;
            VmMemoryRangesTargetGrouppedProxy proxy(&checker, 0x100000);
            if (m_pReader->ReportRegions(p,size, &proxy, true))
            {
                proxy.ReportOnce();
                bSuccess = checker.m_data.size() == 1 && !checker.m_data.front().HasData();
            }
            if (bSuccess)
            {
                *pOffset = p;
                m_lastSuccessEnd = p;
                return true;
            }
        }
        if (startOfNewRegion)
        {
            Address_type newP = GrowAlignPage(startOfNewRegion, pageSize);
            if (newP <= p)
            {
                newP = p + pageSize;
            }
            p = newP;
            continue;
        }
        p += pageSize;
        p = GrowAlignPage(p, pageSize);
    }
    return false;
}
void CMemoryStorageOfModifiedData::AddMemoryWriteBreakPoint(Address_type address)
{
    m_breakpoints.insert(std::make_pair(address, 0));
    m_pageBreakpoints.insert(std::make_pair(QueryPageStartAddress(address), 0));
}
bool CMemoryStorageOfModifiedData::ChooseTheRegion(IAddressSpace * pExternalSpace, 
                                                   Address_type * pOffset,
                                                   Address_type size)
{
    *pOffset = 0;
    const Address_type pageSize = 0x1000;
    OPERAND_SIZE startOffset = m_lastSuccessEnd;
    // prepare end offset
    OPERAND_SIZE maxEndOffset = pExternalSpace->GetMaxValidPointer();
    if (maxEndOffset < pageSize)
    {
        throw std::runtime_error("Internal error");
    }
    maxEndOffset -= pageSize;

    if (size > maxEndOffset)
    {
        return false;
    }
    // go through data
    for(int i = 0; i < 2; ++i)
    {
        if (startOffset > maxEndOffset || startOffset == 0)
        {
            startOffset = 0x3260000;
        }

        PagesMap_type::const_iterator it = m_pageMap.upper_bound(startOffset);
        PagesMap_type::const_iterator it_end = m_pageMap.end();
        if (it == it_end)
        {
            return ReportRegion(pExternalSpace, startOffset, maxEndOffset, size, pOffset);
        }
        if (it->first > startOffset && it->first - startOffset >= size)
        {
            if (ReportRegion(pExternalSpace, startOffset, it->first, size, pOffset))
            {
                return true;
            }
        }

        for(;;)
        {
            PagesMap_type::const_iterator it2 = it;
            ++it2;
            if (it2 == it_end)
            {
                // try after it
                if (ReportRegion(pExternalSpace, it->first + m_pageSize, maxEndOffset, size, pOffset))
                {
                    return true;
                }
                break;
            }
            // it2 is ok, analyze the range
            if (it2->first - it->first + m_pageSize > size)
            {
                if (ReportRegion(pExternalSpace, it->first + m_pageSize, it2->first, size, pOffset))
                {
                    return true;
                }
            }
            it = it2;
        }
    }
    return false;
}
bool CMemoryStorageOfModifiedData::VmFree(IAddressSpace * pExternalSpace,
                                          Address_type offsetHint,
                                          Address_type * pResult,
                                          Address_type * pSize,
                                          bool bRelease)
{
    *pResult = 0;
    *pSize = 0;
    offsetHint = offsetHint & ~0xFFFULL;
    AllocationsMap_type::iterator it = m_allocations.find(offsetHint);
    if (it == m_allocations.end())
    {
        return false;
    }
    *pSize = it->second.size;

    if (bRelease)
    {
        PagesMap_type::iterator it2 = m_pageMap.find(offsetHint);
        PagesMap_type::iterator it2_end = m_pageMap.end();
        for(;  it2 != it2_end; )
        {
            if (it2->first - offsetHint >= *pSize)
            {
                break;
            }
            m_pageMap.erase(it2++);
        }
        m_allocations.erase(it);
    }
    else
    {
        it->second.flags |= AllocationInfo::flags_released;
    }
    
    *pResult = offsetHint;
    return true;
}
bool CMemoryStorageOfModifiedData::VmAlloc(IAddressSpace * pExternalSpace,
                                         Address_type offsetHint,
                                         Address_type size,
                                         Address_type * pResult,
                                         Address_type * pSize,
                                         bool bCommit)
{
    const Address_type pageSize = 0x1000;
    // scan VAD from the start and find the data
    if (size == 0)
    {
        size = pageSize;
    }
    offsetHint = offsetHint & ~0xFFFULL;
    if (size % pageSize)
    {
        size -= size % pageSize;
        size += pageSize;
    }

    if (offsetHint)
    {
        if (!pExternalSpace->IsRegionFree(offsetHint, size, 0))
        {
            return false;
        }
    }
    else
    {
        if (!ChooseTheRegion(pExternalSpace, &offsetHint, size))
        {
            return false;
        }
    }

    //PagesMap_type::const_iterator it = m_pageMap.lower_bound(offsetHint);
    //for(;it != m_pageMap.end();++it)
    //{
    //    if (it->first >= offsetHint && (it->first - offsetHint) < size)
    //    {
    //        return false;
    //    }
    //    if (it->first - offsetHint >= size)
    //    {
    //        break;
    //    }
    //}
    // everything is fine, allocate the page
    if (bCommit)
    {
        for(Address_type i = 0;  i < size; i += m_pageSize)
        {
            OPERAND_SIZE addressToAdd = i;
            DI_CHECK_CPP(Diana_SafeAdd(&addressToAdd, offsetHint));
            m_pageMap.insert(std::make_pair(addressToAdd, PageInfo())).first->second.data.resize(m_pageSize);
        }
    }
    std::pair<AllocationsMap_type::iterator, bool> res = 
        m_allocations.insert(std::make_pair(offsetHint, AllocationInfo(size)));
    if (!bCommit)
    {
        res.first->second.flags |= AllocationInfo::flags_released;
    }
    
    *pSize = size;
    *pResult = offsetHint;
    return true;
}

void CMemoryStorageOfModifiedData::Write(Address_type offset, 
                                         Address_type bytesToWrite,
                                         void * pBuffer,
                                         Address_type * pBytesWritten,
                                         int flags,
                                         Address_type selectorValue,
                                         DianaUnifiedRegister selectorHint)
{
    if (m_breakpoints.find(offset) != m_breakpoints.end())
    {
        __debugbreak();
    }
    *pBytesWritten = 0;
    if (!m_pReader)
    {
        return;
    }
    if (selectorValue)
    {
        if (!m_hasAssociatedSelector)
        {
            QuerySelector(selectorValue)->Write(offset, 
                                                bytesToWrite,
                                                pBuffer,
                                                pBytesWritten,
                                                flags,
                                                selectorValue,
                                                selectorHint);
            return;
        }
    }
    Address_type endByte = offset + bytesToWrite;
    Address_type firstPage = QueryPageStartAddress(offset);
    Address_type lastPage = QueryPageStartAddress(endByte);

    PagesMap_type::iterator oldit;
    bool oldIteratorValid = false;
    Address_type offsetInPage = offset % m_pageSize;

    const char * pCurrentInputIterator = (const char *)pBuffer;
    Address_type currentBytesToWrite = bytesToWrite;
    for(Address_type currentPage = firstPage; currentBytesToWrite; currentPage += m_pageSize, offsetInPage = 0)
    {
        if (oldIteratorValid)
        {
            // try to increment it
            ++oldit;
            oldIteratorValid = oldit != m_pageMap.end() && oldit->first == currentPage;
        }
        if (!oldIteratorValid)
        {
            if (m_pageBreakpoints.find(currentPage) != m_pageBreakpoints.end())
            {
                __debugbreak();
            }
            std::pair<PagesMap_type::iterator, bool> res = m_pageMap.insert(std::make_pair(currentPage, PageInfo()));
            oldit = res.first;
            if (res.second)
            {
                oldit->second.data.resize(m_pageSize);
                // read the entire page
                Address_type readBytes = 0;
                m_pReader->Read(currentPage, 
                                m_pageSize,
                                &oldit->second.data.front(),
                                &readBytes,
                                flags,
                                selectorValue,
                                selectorHint);
                if (readBytes != m_pageSize)
                {
                    if (!(flags & ORTHIA_MR_FLAG_WRITE_ALLOCATE))
                    {
                        m_pageMap.erase(oldit);
                        return;
                    }
                }
            }
        }
        PagesMap_type::iterator it = oldit;
        Address_type pageSize = m_pageSize - offsetInPage;
        Address_type pageBytesToWrite = std::min(currentBytesToWrite, pageSize);
        memcpy(&it->second.data.front() + offsetInPage,  pCurrentInputIterator, (size_t)pageBytesToWrite);
        pCurrentInputIterator += pageBytesToWrite;
        currentBytesToWrite -= pageBytesToWrite;
        *pBytesWritten += pageBytesToWrite;
   }
}

// CReaderOverVector
CReaderOverVector::CReaderOverVector(Address_type regionAddress, 
                                 const std::vector<char> & data)
    :
        m_regionAddress(regionAddress),
        m_regionData(data)
{
}
CReaderOverVector::CReaderOverVector(Address_type regionAddress, 
                                 int size)
    :
        m_regionAddress(regionAddress)
{
    m_regionData.resize(size);
}
void CReaderOverVector::Read(Address_type offset, 
                           Address_type bytesToRead,
                           void * pBuffer,
                           Address_type * pBytesRead,
                           int flags,
                           Address_type selectorValue,
                           DianaUnifiedRegister selectorHint)
{
    *pBytesRead = 0;
    Address_type end = offset + bytesToRead;
    Address_type regionEndAddress = m_regionAddress + m_regionData.size();
    if (offset < m_regionAddress || offset >= regionEndAddress || m_regionData.empty())
    {
        return;
    }

    if (end > regionEndAddress)
    {
        bytesToRead -= (end - regionEndAddress);
    }

    memcpy(pBuffer, &m_regionData.front() + offset - m_regionAddress, (size_t)bytesToRead);
    *pBytesRead = bytesToRead;
}


// CReaderOverRealWorld
CReaderOverRealWorld::CReaderOverRealWorld()
    :
        m_regionAddress(0),
        m_pAddress(0),
        m_size((size_t)-1),
        m_memcpy_sys(0)
{
    m_memcpy_sys = DianaWin32_AllocSysMemcpy();
}
CReaderOverRealWorld::~CReaderOverRealWorld()
{
    if (m_memcpy_sys)
    {
        DianaWin32_FreeSysMemcpy(m_memcpy_sys);
    }
}
static Address_type ReadSafe(void * dst, const void * src, Address_type bytesToRead)
{
     __try
     {
        memcpy(dst, src, (size_t)bytesToRead);
     } 
     __except(EXCEPTION_EXECUTE_HANDLER) 
     {
          return 0;
     }
     return bytesToRead;
}
static Address_type ReadSafe(sys_memcpy_ptr_type memcpy_sys,
                             void * dst,
                             const void * src, 
                             Address_type bytesToRead,
                             DI_UINT16 selector)
{
     __try
     {
        memcpy_sys(dst, src, (size_t)bytesToRead, selector);
     } 
     __except(EXCEPTION_EXECUTE_HANDLER) 
     {
          return 0;
     }
     return bytesToRead;
}
void CReaderOverRealWorld::Read(Address_type offset, 
                           Address_type bytesToRead,
                           void * pBuffer,
                           Address_type * pBytesRead,
                           int flags,
                           Address_type selectorValue,
                           DianaUnifiedRegister selectorHint)
{
    *pBytesRead = 0;
    Address_type end = offset;
    DI_CHECK_CPP(Diana_SafeAdd(&end, bytesToRead));

    Address_type regionEndAddress = m_regionAddress;
    DI_CHECK_CPP(Diana_SafeAdd(&regionEndAddress, m_size));
    if (offset < m_regionAddress || offset >= regionEndAddress || !m_size)
    {
        return;
    }

    if (end > regionEndAddress)
    {
        bytesToRead -= (end - regionEndAddress);
    }

    if (!IsDefaultSelector(selectorHint))
    {
        if (!m_memcpy_sys)
        {
            return;
        }
        *pBytesRead = ReadSafe(m_memcpy_sys, 
                               pBuffer, 
                               (char*)m_pAddress + offset - m_regionAddress, 
                               bytesToRead, 
                               (DI_UINT16)selectorValue);
    }
    else
    {
        *pBytesRead = ReadSafe(pBuffer, (char*)m_pAddress + offset - m_regionAddress, bytesToRead);
    }
}

}