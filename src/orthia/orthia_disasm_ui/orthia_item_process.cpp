#include "orthia_item_process.h"
#include <filesystem>
#include "orthia_memory_cache.h"
#include "orthia_streams.h"
#include "orthia_process_adapter.h"
#include "orthia_log.h"
#include "orthia_common_print.h"
#include "orthia_common_format.h"
#include "orthia_module_manager.h"
#include "orthia_sections.h"
#include "formats/orthia_image_format.h"

namespace orthia
{

    // CProcessWorkplaceItem
    CProcessWorkplaceItem::~CProcessWorkplaceItem()
    {
        m_moduleManager = nullptr;
        m_persistentStorage = nullptr;
        m_moduleStorage = nullptr;

        if (!m_procFolder.empty())
        {
            std::error_code ec;
            std::filesystem::remove_all(std::filesystem::path(m_procFolder), ec);
        }
    }
    void CProcessWorkplaceItem::SetProcFolder(const PlatformString_type& folder)
    {
        m_procFolder = folder;
        orthia::EraseLastSlash(m_procFolder);
    }
    CProcessWorkplaceItem::CProcessWorkplaceItem(std::shared_ptr<oui::IProcess> proc,
        const oui::String& shortName,
        int dianaMode,
        int dianaArch,
        std::shared_ptr<IPeristentItemStorage> persistentStorage)
        :
        m_proc(proc),
        m_shortName(shortName),
        m_dianaMode(dianaMode),
        m_dianaArch(dianaArch),
        m_persistentStorage(persistentStorage)
    {
    }
    void CProcessWorkplaceItem::Init(std::shared_ptr<CModuleManager> moduleManager,
        std::shared_ptr<CFilePersistentItemStorage> persistentItemStorage,
        std::shared_ptr<ModuleStorage> moduleStorage)
    {
        m_moduleManager = moduleManager;
        m_persistentStorage = persistentItemStorage;
        m_moduleStorage = moduleStorage;
    }
    ModuleStorage* CProcessWorkplaceItem::GetModuleStorage()
    {
        return m_moduleStorage.get();
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
        info.dianaArch = m_dianaArch;
        return info;
    }

    const std::shared_ptr<CModuleManager> CProcessWorkplaceItem::GetModuleManager() const
    {
        return m_moduleManager;
    }
    oui::String CProcessWorkplaceItem::GetShortName() const
    {
        return m_shortName;
    }
    Address_type CProcessWorkplaceItem::GetMainModuleAddress() const
    {
        return m_processModuleAddress;
    }

    void CProcessWorkplaceItem::ReloadModules()
    {
        orthia::flat_map<orthia::Address_type, NameInfo> exports;
        std::vector<orthia::ModuleInfo> modules;
        int processModuleOffset = 0;
        m_proc->QueryModules(modules, processModuleOffset,
            [&](const orthia::ModuleInfo& moduleInfo, oui::ModuleDisasmContext& context) {

            std::vector<NameInfo> names;
            context.image->QueryNames(names);
            for (auto& info : names)
            {
                // every name is kept: $ifunc_memcpy and memcpy share an address
                info.name.native = moduleInfo.name + OUI_STR("!") + info.name.native;
                exports.insert(info.address, info);
            }
        });

        Address_type processModuleAddress = 0;
        if (!modules.empty() && processModuleOffset != -1 && processModuleOffset < (int)modules.size())
        {
            processModuleAddress = modules[processModuleOffset].entryPoint;
        }
        std::sort(modules.begin(), modules.end(), [](auto& m1, auto& m2) { return m1.address < m2.address; });

        exports.sort();
        m_moduleNames.Clear();

        orthia::CAutoCriticalSection guard(m_lock);
        m_modules = std::move(modules);
        m_exports = std::move(exports);
        m_modulesIndex.clear();
        for (int i = 0, size = (int)m_modules.size(); i < size; ++i)
        {
            m_modulesIndex[m_modules[i].address] = i;
        }
        for (auto& mod : m_modules)
        {
            auto it = m_moduleFlags.find(mod.address);
            if (it != m_moduleFlags.end())
                mod.flags |= it->second;
        }
        if (m_processModuleAddress == 0)
        {
            m_processModuleAddress = processModuleAddress;
        }
    }
    void CProcessWorkplaceItem::OnPrivateSymbolLoaded(Address_type addr, const oui::String& name)
    {
        orthia::CAutoCriticalSection guard(m_lock);
        auto it = m_exports.find(addr);
        if (it != m_exports.end())
        {
            it->second.privateSymbol = name;
        }
    }
    void CProcessWorkplaceItem::OnModuleSymbolsLoaded(Address_type moduleAddress)
    {
        // before the flag: the private symbols analysis waits for it and then reads the names
        m_moduleNames.Invalidate(moduleAddress);
        UpdateModuleFlags(moduleAddress, ModuleInfo::flags_symbolsLoaded, 0);
    }
    void CProcessWorkplaceItem::GetModules(std::vector<orthia::ModuleInfo>& modules) const
    {
        orthia::CAutoCriticalSection guard(m_lock);
        modules = m_modules;
    }
    int CProcessWorkplaceItem::GetModulesCount() const
    {
        return (int)m_modules.size();
    }
    std::shared_ptr<IPeristentItemStorage> CProcessWorkplaceItem::GetPersistentStorage()
    {
        return m_persistentStorage;
    }


    void CProcessWorkplaceItem::BuildModuleNames(Address_type moduleAddress, ModuleNames& names) const
    {
        orthia::ModuleInfo module;
        {
            orthia::CAutoCriticalSection guard(m_lock);

            auto it = m_modulesIndex.find(moduleAddress);
            if (it == m_modulesIndex.end())
            {
                return;
            }
            module = m_modules.at(it->second);
        }

        auto* format = FindFormatByModule(module);
        orthia::ProcessReaderAdapter memReader(m_proc.get());
        if (auto image = format ? format->OpenModuleImage(memReader, moduleAddress, module.size) : nullptr)
        {
            std::vector<NameInfo> found;
            image->QueryNames(found);
            for (auto& info : found)
            {
                names.Add(std::move(info));
            }

            found.clear();
            image->QueryImports(found);
            for (auto& import : found)
            {
                // the target's own name when it is known, the imported one otherwise
                NameInfo info;
                info.flags = NameInfo::flags_Import;
                info.address = import.address;
                info.name = QueryAddressName(import.address).name;
                if (info.name.native.empty())
                {
                    info.name = import.name.native.empty() ? oui::String(OUI_TCSTR("<unknown>")) : import.name;
                }
                names.Add(std::move(info));
            }
        }

        // in-memory if available, DB fallback otherwise
        if (m_moduleStorage)
        {
            m_moduleStorage->CollectModulePrivateSymbols(moduleAddress, [&](NameInfo info) {
                names.Add(std::move(info));
            });
        }
    }
    std::shared_ptr<const ModuleNames> CProcessWorkplaceItem::QueryModuleNames(Address_type moduleAddress) const
    {
        return m_moduleNames.Query(moduleAddress, [&](ModuleNames& names) {
            BuildModuleNames(moduleAddress, names);
        });
    }
    void CProcessWorkplaceItem::QueryNames(Address_type moduleAddress, const NameSelectionKey& name, int count, std::vector<NameInfo>& names)const
    {
        names.clear();
        if (count <= 0)
        {
            return;
        }
        QueryModuleNames(moduleAddress)->QueryPage(name, count, names);
    }

    int CProcessWorkplaceItem::QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const
    {
        return QueryModuleNames(moduleAddress)->QueryCount(name);
    }
    void CProcessWorkplaceItem::InvalidateNames(Address_type moduleAddress)
    {
        m_moduleNames.Invalidate(moduleAddress);
    }

    MarkupRangeInfo CProcessWorkplaceItem::QueryMarkupRange(Address_type address, IMarkupCache* cache) const
    {
        MarkupRangeInfo res;
        auto it = m_exports.find(address);
        if (it != m_exports.end())
        {
            res.sizeInLines = 1;
        }

        if (m_moduleManager)
        {
            const CommonReferenceInfoArray_type* refsPtr = nullptr;
            std::vector<CommonReferenceInfo> refsStorage;
            if (!cache)
            {
                m_moduleManager->QueryReferencesToInstruction(address, &refsStorage);
                refsPtr = &refsStorage;
            }
            else
            {
                cache->QueryReferences(address, &refsPtr);
            }
            if (refsPtr && !refsPtr->empty())
            {
                ++res.sizeInLines;
            }
        }
        return res;
    }
    void CProcessWorkplaceItem::QueryMarkupRange(Address_type address, int index, int count, MarkupRange& range, IMarkupCache* cache) const
    {
        range.lines.clear();
        if (count == 0)
        {
            return;
        }

        std::vector<MarkupLine> allLines;
        auto nameInfo = QueryAddressName(address);
        if ((nameInfo.flags & NameInfo::flags_Export) ||
            (nameInfo.flags & NameInfo::flags_PrivateSymbol))
        {
            allLines.push_back(MarkupLine(GetPreferredName(nameInfo)));
        }
        
        AppendXrefLine(address, cache, m_moduleManager.get(), GetDianaMode(), allLines);

        for (int i = index; i < (int)allLines.size() && (int)range.lines.size() < count; ++i)
        {
            range.lines.push_back(allLines[i]);
        }
    }
    bool CProcessWorkplaceItem::QueryAddressModule(Address_type address, orthia::ModuleInfo & result) const
    {
        orthia::CAutoCriticalSection guard(m_lock);

        auto it = m_modulesIndex.lower_bound(address), it_end = m_modulesIndex.end();
        if (it != m_modulesIndex.begin())
        {
            --it;
        }
        for (; it != it_end; ++it)
        {
            auto& module = m_modules[it->second];
            if (module.address > address)
            {
                break;
            }
            if (module.IsInRange(address))
            {
                result = module;
                return true;
            }
        }
        return false;
    }
    std::shared_ptr<oui::IProcess> CProcessWorkplaceItem::GetAssociatedProcess()
    {
        return m_proc;
    }
    std::shared_ptr<IMemoryReader> CProcessWorkplaceItem::CreateMemoryReader()
    {
        return std::make_shared<ProcessReaderAdapter>(m_proc.get());
    }
    void CProcessWorkplaceItem::UpdateModuleFlags(Address_type moduleAddress, int flagsToSet, int flagsToRemove)
    {
        orthia::CAutoCriticalSection guard(m_lock);
        auto & flags = m_moduleFlags[moduleAddress];
        flags |= flagsToSet;
        flags &= ~flagsToRemove;

        auto it = m_modulesIndex.find(moduleAddress);
        if (it != m_modulesIndex.end())
        {
            m_modules[it->second].flags = flags;
        }
    }

    Address_type CProcessWorkplaceItem::QueryAddressByName(const oui::String& text, Address_type defValue) const
    {
        auto downcased = orthia::Downcase(text.native);
        orthia::CAutoCriticalSection guard(m_lock);
        {
            auto it = std::find_if(m_exports.begin(), m_exports.end(), [&](const auto& pair) { return orthia::Downcase(pair.second.name.native) == downcased;  });
            if (it != m_exports.end())
            {
                return it->first;
            }
        }

        {
            auto it = std::find_if(m_modules.begin(), m_modules.end(), [&](const auto& mod) { return orthia::Downcase(mod.name) == downcased;  });
            if (it != m_modules.end())
            {
                return it->address;
            }
        }
        return defValue;
    }

    std::shared_ptr<::DianaMovableReadStream> CProcessWorkplaceItem::CreateDisasmStream(Address_type addressStart)
    {
        struct DianaReadStreamAdapter
        {
            std::shared_ptr<oui::IProcess> proc;
            orthia::ProcessReaderAdapter memReader;
            orthia::CMemoryStorageOfModifiedData cache;
            orthia::DianaMemoryStream stream;

            DianaReadStreamAdapter(std::shared_ptr<oui::IProcess> proc_in, Address_type addressStart)
                :
                proc(proc_in),
                memReader(proc_in.get()),
                cache(&memReader, 4096),
                stream(addressStart, &cache, 0)
            {
                cache.SetCacheReads(true);
            }
        };

        auto streamAdapter = std::make_shared<DianaReadStreamAdapter>(m_proc, addressStart);
        return std::shared_ptr<::DianaMovableReadStream>(streamAdapter, &streamAdapter->stream.parent);
    }
    NameInfo CProcessWorkplaceItem::QueryAddressName(Address_type address) const
    {
        orthia::CAutoCriticalSection guard(m_lock);
        return QueryAddressNameNoLock(address);
    }
    NameInfo CProcessWorkplaceItem::QueryAddressNameNoLock(Address_type address) const
    {
        orthia::ModuleInfo moduleInfo;
        auto nameInfo = QueryAddressNameImpl(address, moduleInfo);
        if (m_persistentStorage)
        {
            auto comment = m_persistentStorage->SyncReadComment(address);
            if (!comment.native.empty())
            {
                nameInfo.comment = comment;
            }
        }
        if (nameInfo.privateSymbol.native.empty() && m_moduleStorage)
        {
            m_moduleStorage->QueryNearestPrivateSymbol(address, nameInfo);
        }
        if (!moduleInfo.name.empty() && !nameInfo.privateSymbol.native.empty())
        {
            nameInfo.privateSymbol = moduleInfo.name + OUI_TCSTR("!") + nameInfo.privateSymbol.native;
        }
        return nameInfo;
    }
    NameInfo CProcessWorkplaceItem::QueryAddressNameImpl(Address_type address, orthia::ModuleInfo & moduleInfo) const
    {
        orthia::Address_type capturedAddress = 0;
        NameInfo capturedInfo;
        {
            auto it = m_exports.upper_bound(address);
            auto it_end = m_exports.end();
            if (it != it_end && it != m_exports.begin())
            {
                --it;
            }
            for(;it != it_end; ++it)
            {
                if (address >= it->first)
                {
                    capturedAddress = it->first;
                    capturedInfo = it->second;
                    break;
                }
            }
        }
        if (address == capturedAddress)
        {
            QueryAddressModule(address, moduleInfo);
            capturedInfo.flags |= NameInfo::flags_Export;
            return capturedInfo;
        }
        if (QueryAddressModule(address, moduleInfo))
        {
            if (moduleInfo.address > capturedAddress)
            {
                // export function is not found or there is module with a closest address
                NameInfo r;
                r.name = ComposeName(moduleInfo.name, moduleInfo.address, address);
                return r;
            }
        }
        // use found function
        if (!capturedInfo.name.native.empty())
        {
            NameInfo r;
            r.name = ComposeName(capturedInfo.name, capturedAddress, address);
            return r;
        }
        return NameInfo();
    }

    void CProcessWorkplaceItem::QuerySections(Address_type moduleBase, const FileOpener_type& openFile, ImageSections& sections)
    {
        orthia::ModuleInfo mod;
        {
            orthia::CAutoCriticalSection guard(m_lock);
            auto it = m_modulesIndex.find(moduleBase);
            if (it != m_modulesIndex.end())
            {
                mod = m_modules[it->second];
            }
            mod.address = moduleBase;
        }
        auto reader = CreateMemoryReader();
        ModuleAccess access;
        access.reader = reader.get();
        access.mod = &mod;
        // the file the module was mapped from: ELF section headers are not in memory
        access.imageFile = mod.fullName;
        access.openFile = openFile;
        QueryModuleSections(access, sections);
    }

}