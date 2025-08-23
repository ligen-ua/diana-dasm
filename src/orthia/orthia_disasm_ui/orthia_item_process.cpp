#include "orthia_item_process.h"
#include "orthia_pe.h"


namespace orthia
{

    // CProcessWorkplaceItem
    CProcessWorkplaceItem::CProcessWorkplaceItem(std::shared_ptr<oui::IProcess> proc,
        const oui::String& shortName,
        int dianaMode)
        :
            m_proc(proc),
            m_shortName(shortName),
            m_dianaMode(dianaMode)
    {
    }

    WorkAddressData CProcessWorkplaceItem::ReadData(Address_type address, Address_type size)
    {
        return m_proc->ReadExactEx(address, (size_t)size);
    }

    WorkAddressRangeInfo CProcessWorkplaceItem::GetRangeInfo(Address_type address) const
    {
        orthia::CAutoCriticalSection guard(m_lock);

        for (const auto& module : m_modules)
        {
            if (module.IsInRange(address))
            {
                return module;
            }
        }
        // no modules found, return address space as the big module
        int platformError = 0;
        unsigned long long fileSize = 0;
        std::tie(platformError, fileSize) = m_proc->GetSizeInBytes();
        WorkAddressRangeInfo info;
        info.address = 0;
        info.lastValidAddress = fileSize;
        info.entryPoint = 0;
        info.size = fileSize;
        info.dianaMode = m_dianaMode;
        return info;
    }

    const std::shared_ptr<CModuleManager> CProcessWorkplaceItem::GetModuleManager() const
    {
        return nullptr;
    }
    oui::String CProcessWorkplaceItem::GetShortName() const
    {
        return m_shortName;
    }
    Address_type CProcessWorkplaceItem::GerProcessModuleAddress()
    {
        return m_processModuleAddress;
    }
    void CProcessWorkplaceItem::ReloadModules()
    {
        std::vector<orthia::ModuleInfo> modules;
        int processModuleOffset = 0;
        m_proc->QueryModules(modules, processModuleOffset);

        Address_type processModuleAddress = 0;
        if (!modules.empty() && processModuleOffset != -1 && processModuleOffset < (int)modules.size())
        {
            processModuleAddress = modules[processModuleOffset].entryPoint;
        }
        std::sort(modules.begin(), modules.begin(), [](auto& m1, auto& m2) { return m1.address < m2.address; });

        orthia::CAutoCriticalSection guard(m_lock);
        m_modules = std::move(modules);
        if (m_processModuleAddress == 0)
        {
            m_processModuleAddress = processModuleAddress;
        }
    }
    void CProcessWorkplaceItem::GetModules(std::vector<orthia::ModuleInfo>& modules) const
    {
        orthia::CAutoCriticalSection guard(m_lock);
        modules = m_modules;
    }
}