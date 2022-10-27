#ifndef ORTHIA_INTERFACES_VM_H
#define ORTHIA_INTERFACES_VM_H

#include "orthia_interfaces.h"

namespace orthia
{

struct VmInfo
{
    long long id;
    std::wstring name;
    CCommonDateTime creationTime;
    CCommonDateTime lastWriteTime;

    VmInfo()
        :
            id(0)
    {
    }
};
struct IVmInfoListTarget
{
    virtual ~IVmInfoListTarget(){}
    virtual void OnData(const std::vector<VmInfo> & vmInfo) = 0;
};
struct VmInfoListTargetOverVector:public IVmInfoListTarget
{
    std::vector<VmInfo> m_data;
    typedef std::vector<VmInfo>::const_iterator const_iterator;
    virtual void OnData(const std::vector<VmInfo> & vmInfo)
    {
        m_data.insert(m_data.end(), vmInfo.begin(), vmInfo.end());
    }
};


struct VmModuleInfo
{
    static const long long flags_disabled = 1;

    long long id;
    long long flags;
    std::string descriptionXmlUtf8;
    std::vector<char> data;
    
    VmModuleInfo()
        :
            id(0),
            flags(0)
    {
    }

    bool IsDisabled() const
    {
        return (flags & flags_disabled) != 0;
    }
};

// vmmodule
struct IVmModuleInfoListTarget
{
    virtual ~IVmModuleInfoListTarget(){}
    virtual void OnData(const std::vector<VmModuleInfo> & vmInfo) = 0;
};
struct VmModuleInfoListTargetOverVector:public IVmModuleInfoListTarget
{
    std::vector<VmModuleInfo> m_data;
    typedef std::vector<VmModuleInfo>::const_iterator const_iterator;
    virtual void OnData(const std::vector<VmModuleInfo> & vmInfo)
    {
        m_data.insert(m_data.end(), vmInfo.begin(), vmInfo.end());
    }
};

// guimodule
struct GUIVmModuleInfo
{
    VmModuleInfo rawInfo;
    long long writesCount;
    unsigned long long lowestAddress;
    unsigned long long sizeInBytes;
    std::wstring description;

    typedef std::map<std::wstring, std::wstring> AttributesMap_type;
    AttributesMap_type customAttributes;

    long long guiFlags;
    static const long long guiFlags_has_data = 1;
    static const long long guiFlags_has_custom = 2;
    GUIVmModuleInfo()
        :
            writesCount(0),
            lowestAddress(0),
            sizeInBytes(0),
            guiFlags(0)
    {
    }
};

struct IGUIVmModuleInfoListTarget
{
    virtual ~IGUIVmModuleInfoListTarget(){}
    virtual void OnData(const std::vector<GUIVmModuleInfo> & vmInfo) = 0;
};
struct GUIVmModuleInfoListTargetOverVector:public IGUIVmModuleInfoListTarget
{
    std::vector<GUIVmModuleInfo> m_data;
    typedef std::vector<GUIVmModuleInfo>::const_iterator const_iterator;
    virtual void OnData(const std::vector<GUIVmModuleInfo> & vmInfo)
    {
        m_data.insert(m_data.end(), vmInfo.begin(), vmInfo.end());
    }
};


struct VmMemoryRangeInfo
{
    unsigned long long address;
    unsigned long long size;
    int flags;

    static const int flags_hasData = 1;

    VmMemoryRangeInfo()
        :
            address(0),
            size(0),
            flags(0)
    {
    }
    VmMemoryRangeInfo(unsigned long long address_in,
                      unsigned long long size_in,
                      int flags_in = 0)
        :
            address(address_in),
            size(size_in),
            flags(flags_in)
    {
    }
    bool HasData() const
    {
        return (flags & flags_hasData)!= 0;
    }
};
struct IVmMemoryRangesTarget
{
    virtual ~IVmMemoryRangesTarget(){}
    virtual void OnRange(const VmMemoryRangeInfo & vmRange,
                         const char * pDataStart) = 0;
};

struct VmMemoryRangeInfoWithData:VmMemoryRangeInfo
{
    std::vector<char> data;

    VmMemoryRangeInfoWithData()
    {
    }
    VmMemoryRangeInfoWithData(const VmMemoryRangeInfo & vmRange)
    {
        VmMemoryRangeInfo & this_ = *this;
        this_ = vmRange;
    }
};
struct VmMemoryRangesTargetOverVector:public IVmMemoryRangesTarget
{
    std::vector<VmMemoryRangeInfoWithData> m_data;
    typedef std::vector<VmMemoryRangeInfoWithData>::const_iterator const_iterator;

    void Clear()
    {
        m_data.clear();
    }
    virtual void OnRange(const VmMemoryRangeInfo & vmRange,
                         const char * pDataStart)
    {
        m_data.push_back(VmMemoryRangeInfoWithData(vmRange));
        if (pDataStart)
        {
            m_data.back().data.assign(pDataStart, pDataStart + vmRange.size);
        }
    }
};

struct VmMemoryRangesTargetOverVectorPlain:public IVmMemoryRangesTarget
{
    std::vector<char> m_data;
    unsigned long long m_last;
    unsigned char m_charToFill;
    
    VmMemoryRangesTargetOverVectorPlain(unsigned char charToFill = 0)
        :
            m_last(0),
            m_charToFill(charToFill)
    {
    }
    virtual void OnRange(const VmMemoryRangeInfo & vmRange,
                         const char * pDataStart)
    {
        if (!m_data.empty())
        {
            if (vmRange.address != m_last)
            {
                throw std::runtime_error("Internal error");
            }
        }

        if (pDataStart)
        {
            m_data.insert(m_data.end(), pDataStart, pDataStart + vmRange.size);
        }
        else
        {
            m_data.resize(m_data.size() + (size_t)vmRange.size, (char)m_charToFill);
        }
        m_last = vmRange.address + vmRange.size;
    }
};

struct VmMemoryRangesTargetGrouppedProxy:public IVmMemoryRangesTarget
{
    IVmMemoryRangesTarget * m_pVmMemoryRangesTarget;

    VmMemoryRangeInfoWithData m_prevRange;
    bool m_prevRangeValid;
    unsigned long long m_limit;

    VmMemoryRangesTargetGrouppedProxy(IVmMemoryRangesTarget * pVmMemoryRangesTarget,
                                      unsigned long long limit = 0)
        :
            m_pVmMemoryRangesTarget(pVmMemoryRangesTarget),
            m_prevRangeValid(false),
            m_limit(limit)
    {
    }
    virtual void OnRange(const VmMemoryRangeInfo & vmRange,
                         const char * pDataStart)
    {
        if (m_prevRangeValid)
        {
            if (m_prevRange.HasData() == vmRange.HasData())
            {
                if (m_prevRange.address + m_prevRange.size != vmRange.address)
                {
                    throw std::runtime_error("Internal error");
                }
                if (vmRange.HasData())
                {
                    m_prevRange.data.insert(m_prevRange.data.end(), 
                                            pDataStart, 
                                            pDataStart + vmRange.size);
                }
                m_prevRange.size += vmRange.size;
                return;
            }
            ReportOnce();
        }

        m_prevRange.data.clear();
        m_prevRange = vmRange;
        if (vmRange.HasData())
        {
            m_prevRange.data.insert(m_prevRange.data.end(), 
                                    pDataStart, 
                                    pDataStart + vmRange.size);
        }
        m_prevRangeValid = true;

        if (m_limit && 
            (unsigned long long)m_prevRange.data.size() > m_limit)
        {
            ReportOnce();
        }
    }
    void ReportOnce()
    {
        if (m_prevRangeValid)
        {
            m_pVmMemoryRangesTarget->OnRange(m_prevRange, orthia::GetFrontPointer(m_prevRange.data));
            m_prevRange.data.clear();
            m_prevRangeValid = false;
        }
    }
};



}

#endif
