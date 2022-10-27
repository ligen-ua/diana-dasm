#ifndef ORTHIA_VMLIB_UTILS_H
#define ORTHIA_VMLIB_UTILS_H

#include "orthia_vmlib.h"
#include "orthia_common_format.h"
#include "orthia_memory_cache.h"

namespace orthia
{

// desctipion
#define ORTHIA_DESCRIPTION_FIELD_WRITES_COUNT     "writes_count"
#define ORTHIA_DESCRIPTION_FIELD_LOWEST_ADDRESS   "lowest_address"
#define ORTHIA_DESCRIPTION_FIELD_SIZE_BYTES       "size_bytes"
#define ORTHIA_DESCRIPTION_FIELD_DESCRIPTION      "description"

// description
#define ORTHIA_WRITE_FIELD_ADDRESS                "address"

// custom
#define ORTHIA_DESCRIPTION_FIELD_CUSTOM_PREFIX      "ca_"
#define ORTHIA_DESCRIPTION_FIELD_CUSTOM_PREFIX_LEN  3

class CDescriptionManager
{
    CCommonFormatParser m_parser;

    void UnparseCustomAttributes(GUIVmModuleInfo * pInfo);
public:
    void UnparseDescriptionGUIData(const std::string & utf8,
                                   bool collectCustomAttributes,
                                   GUIVmModuleInfo * pInfo);
    void Convert(const VmModuleInfo & source,
                 bool collectCustomAttributes,
                 GUIVmModuleInfo * pTarget);
    CCommonFormatParser * GetParser();
};



struct VmModuleInfoListTargetProxy:public IVmModuleInfoListTarget
{
    IGUIVmModuleInfoListTarget * m_pTarget;
    bool m_queryData, m_queryCustom;

    std::vector<GUIVmModuleInfo> m_guiData;
    CDescriptionManager m_descrptionManager;

    virtual void OnData(const std::vector<VmModuleInfo> & vmInfo)
    {
        for(std::vector<VmModuleInfo>::const_iterator it = vmInfo.begin(), it_end = vmInfo.end();
            it != it_end;
            ++it)
        {
            if (m_guiData.empty())
            {
                m_guiData.push_back(GUIVmModuleInfo());
            }
            m_descrptionManager.Convert(*it, m_queryCustom, &m_guiData.front());
            m_pTarget->OnData(m_guiData);
        }
    }
    VmModuleInfoListTargetProxy(IGUIVmModuleInfoListTarget * pTarget,
                                bool queryData,
                                bool queryCustom)
        :   
            m_pTarget(pTarget),
            m_queryData(queryData),
            m_queryCustom(queryCustom)
    {
    }
};

// query mem
class CVMVirtualSpace:IGUIVmModuleInfoListTarget
{
    long long m_vmId;
    bool m_onlyEnabled;
    unsigned long long m_startAddress;
    unsigned long long m_size;
    unsigned long long m_lastAddress;
    IMemoryReader * m_pReader;

    CMemoryStorageOfModifiedData m_cache;

    CCommonFormatParser m_parser;
    std::vector<char> m_value;

    void OnData(const GUIVmModuleInfo & vmInfo);
    virtual void OnData(const std::vector<GUIVmModuleInfo> & vmInfo);

public:
    CVMVirtualSpace(long long vmId,
                    bool onlyEnabled,
                    unsigned long long startAddress,
                    unsigned long long size,
                    IMemoryReader * pReader);

    void Init(orthia::intrusive_ptr<CVMDatabase> pVmDatabase);
    const CMemoryStorageOfModifiedData & GetCache() const;
    CMemoryStorageOfModifiedData & GetCache();
    unsigned long long GetLastAddress() const;
};

}

#endif
