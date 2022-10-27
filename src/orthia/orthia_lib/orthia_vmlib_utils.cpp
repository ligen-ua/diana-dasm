#include "orthia_vmlib_utils.h"
#include "orthia_tiny_xml.h"
namespace orthia
{


void CDescriptionManager::UnparseDescriptionGUIData(const std::string & utf8,
                                                    bool collectCustomAttributes,
                                                    GUIVmModuleInfo * pInfo)
{
    pInfo->writesCount = 0;
    pInfo->lowestAddress = 0;
    pInfo->sizeInBytes = 0;
    pInfo->customAttributes.clear();
    pInfo->description.clear();

    if (!m_parser.Parse(utf8))
    {
        return;
    }

    m_parser.QueryMetadata(ORTHIA_DESCRIPTION_FIELD_WRITES_COUNT, &pInfo->writesCount);
    m_parser.QueryMetadata(ORTHIA_DESCRIPTION_FIELD_LOWEST_ADDRESS, &pInfo->lowestAddress);
    m_parser.QueryMetadata(ORTHIA_DESCRIPTION_FIELD_SIZE_BYTES, &pInfo->sizeInBytes);
    m_parser.QueryMetadata(ORTHIA_DESCRIPTION_FIELD_DESCRIPTION, &pInfo->description);

    if (collectCustomAttributes)
    {
        UnparseCustomAttributes(pInfo);
        pInfo->guiFlags |= pInfo->guiFlags_has_custom;
    }
}

void CDescriptionManager::UnparseCustomAttributes(GUIVmModuleInfo * pInfo)
{
    pInfo->customAttributes.clear();
    for(const tinyxml2::XMLAttribute * pAttribute = m_parser.QueryFirstAttribute(); 
        pAttribute;
        pAttribute = pAttribute->Next())
    {
        if (!pAttribute->Name() || !pAttribute->Value())
        {
            continue;
        }
        if (strncmp(ORTHIA_DESCRIPTION_FIELD_CUSTOM_PREFIX, 
                pAttribute->Name(), 
                ORTHIA_DESCRIPTION_FIELD_CUSTOM_PREFIX_LEN))
        {
            continue;
        }
        pInfo->customAttributes[orthia::Utf8ToUtf16(pAttribute->Name())] = orthia::Utf8ToUtf16(pAttribute->Value());
    }
}
void CDescriptionManager::Convert(const VmModuleInfo & source,
                                  bool collectCustomAttributes,
                                  GUIVmModuleInfo * pTarget)
{
    if (&pTarget->rawInfo != &source)
    {
        pTarget->rawInfo = source;
    }

    UnparseDescriptionGUIData(pTarget->rawInfo.descriptionXmlUtf8,
                              collectCustomAttributes,
                              pTarget);
                

    if (!source.data.empty())
    {
        pTarget->guiFlags |= pTarget->guiFlags_has_data;
    }
}
CCommonFormatParser * CDescriptionManager::GetParser()
{
    return &m_parser; 
}



// query mem
CVMVirtualSpace::CVMVirtualSpace(long long vmId,
                               bool onlyEnabled,
                               unsigned long long startAddress,
                               unsigned long long size,
                               IMemoryReader * pReader)
    :
        m_vmId(vmId),
        m_onlyEnabled(onlyEnabled),
        m_startAddress(startAddress),
        m_size(size),
        m_pReader(pReader),
        m_cache(pReader)
{
    m_lastAddress = m_startAddress + m_size - 1;
    if (m_lastAddress < m_startAddress)
    {
        throw std::runtime_error("Overflow");
    }
}
void CVMVirtualSpace::OnData(const GUIVmModuleInfo & vmInfo)
{
    // enumerate writes
    CCommonFormatMultiParser multiParser(vmInfo.rawInfo.data, false);
    while(multiParser.QueryNextItem(&m_parser))
    {
        unsigned long long currentStart = 0;
        if (!m_parser.QueryMetadata(ORTHIA_WRITE_FIELD_ADDRESS, &currentStart))
        {
            throw std::runtime_error("Invalid metadata of item, no address");
        }
        unsigned long long currentSize = m_parser.QuerySizeOfBinary();
        if (currentSize == 0)
        {
            continue;
        }
        unsigned long long currentLast = currentStart + (currentSize-1);
        if (currentLast < currentStart)
        {
            throw std::runtime_error("Overflow");
        }
        if (currentLast < m_startAddress || m_lastAddress < currentStart)
        {
            continue;
        }
        if (!m_parser.QueryValue(&m_value))
        {
            continue;
        }
        if (m_value.empty())
        {
            continue;
        }
        const char * pBufferToWrite = (const char * )&m_value.front();
        Address_type sizeToWrite = m_value.size();
        Address_type written = 0, currentStartToUse = currentStart;

        if (currentStart < m_startAddress)
        {
            unsigned long long unusedPartSize = m_startAddress - currentStart;
            if (unusedPartSize >= sizeToWrite)
            {
                throw std::runtime_error("Internal error");
            }
            currentStartToUse = m_startAddress;
            sizeToWrite -= unusedPartSize;
            pBufferToWrite += unusedPartSize;
        }

        if (currentLast > m_lastAddress)
        {
            unsigned long long unusedPartSize = currentLast - m_lastAddress;
            if (unusedPartSize >= sizeToWrite)
            {
                throw std::runtime_error("Internal error");
            }
            sizeToWrite -= unusedPartSize;
        }

        m_cache.Write(currentStart,
                      m_value.size(),
                      &m_value.front(),
                      &written,
                      ORTHIA_MR_FLAG_WRITE_ALLOCATE,
                      0,
                      reg_none);
        if (written != m_value.size())
        {
            throw std::runtime_error("Internal error");
        }
    }
}
void CVMVirtualSpace::OnData(const std::vector<GUIVmModuleInfo> & vmInfo)
{
    for(std::vector<GUIVmModuleInfo>::const_iterator it = vmInfo.begin(), it_end = vmInfo.end();
        it != it_end;
        ++it)
    {
        if (m_onlyEnabled && it->rawInfo.IsDisabled())
        {
            continue;
        }
        if (m_lastAddress < it->lowestAddress)
        {
            continue;
        }
        if (!it->sizeInBytes)
        {
            continue;
        }
        unsigned long long moduleLastAddress = it->lowestAddress + (it->sizeInBytes - 1);
        if (moduleLastAddress < it->lowestAddress)
        {
            throw std::runtime_error("Overflow");
        }
        if (moduleLastAddress < m_startAddress)
        {
            continue;
        }
        OnData(*it);
    }
}
void CVMVirtualSpace::Init(orthia::intrusive_ptr<CVMDatabase> pVmDatabase)
{
    VmModuleInfoListTargetProxy proxy(this, true, false);
    pVmDatabase->QueryModules(m_vmId, true, &proxy);
}
const CMemoryStorageOfModifiedData & CVMVirtualSpace::GetCache() const
{ 
    return m_cache; 
}
CMemoryStorageOfModifiedData & CVMVirtualSpace::GetCache() 
{ 
    return m_cache; 
}
unsigned long long CVMVirtualSpace::GetLastAddress() const
{
    return m_lastAddress;
}

}

