#include "orthia_item_process.h"
#include <filesystem>
#include <set>
#include "orthia_memory_cache.h"
#include "orthia_streams.h"
#include "orthia_process_adapter.h"
#include "orthia_log.h"
#include "orthia_common_print.h"
#include "orthia_common_format.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include "orthia_sections.h"
#include "orthia_databases.h"
#include "formats/orthia_image_format.h"
#include "orthia_floating_code.h"

namespace orthia
{

    static const PlatformString_type& GetSavedModuleName(const ModuleInfo& module)
    {
        return module.fullName.empty() ? module.name : module.fullName;
    }

    // CProcessWorkplaceItem
    CProcessWorkplaceItem::~CProcessWorkplaceItem()
    {
        m_moduleManager = nullptr;
        m_persistentStorage = nullptr;
        m_moduleStorage = nullptr;
        // the folder stays: the next open of the same process reuses it, an exited one is a
        // saved process. CleanupExpiredProcFolders and .database cleanup remove folders
    }
    void CProcessWorkplaceItem::SetProcFolder(const PlatformString_type& folder)
    {
        {
            orthia::CAutoCriticalSection guard(m_lock);
            m_procFolder = folder;
            orthia::EraseLastSlash(m_procFolder);
        }
        SaveModuleList();
    }
    void CProcessWorkplaceItem::SaveModuleList()
    {
        PlatformString_type folder;
        SavedModuleList list;
        {
            orthia::CAutoCriticalSection guard(m_lock);
            if (m_savedView || m_procFolder.empty())
            {
                return;
            }
            folder = m_procFolder;
            list = m_savedModules;
        }
        std::lock_guard<std::mutex> saveGuard(m_saveLock);
        try
        {
            WriteSavedModules(folder, list);
        }
        catch (const std::exception& e)
        {
            ORTHIA_LOG(orthia::LogSeverity::Warning, "Can't save the module list: ", e.what());
        }
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
        InstallExternalTargetFilter(weak_from_this().lock());
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
        // what tells the build of each module's file, for modules.xml
        std::map<Address_type, ImageIdentity> identities;
        int processModuleOffset = 0;
        m_proc->QueryModules(modules, processModuleOffset,
            [&](const orthia::ModuleInfo& moduleInfo, oui::ModuleDisasmContext& context) {

            ImageIdentity identity;
            if (!m_savedView && context.image->QueryIdentity(identity))
            {
                identities[moduleInfo.address] = identity;
            }

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
        SavedModuleList savedModules;
        if (!modules.empty() && processModuleOffset != -1 && processModuleOffset < (int)modules.size())
        {
            processModuleAddress = modules[processModuleOffset].entryPoint;
            savedModules.mainModule = modules[processModuleOffset].address;
        }
        std::sort(modules.begin(), modules.end(), [](auto& m1, auto& m2) { return m1.address < m2.address; });
        for (const auto& mod : modules)
        {
            SavedModuleInfo saved;
            saved.module.address = mod.address;
            saved.module.size = mod.size;
            saved.module.lastValidAddress = mod.lastValidAddress;
            saved.module.name = mod.name;
            saved.module.fullName = mod.fullName;
            saved.module.dianaMode = mod.dianaMode;
            saved.module.dianaArch = mod.dianaArch;
            saved.module.entryPoint = mod.entryPoint;
            saved.module.builtInFlags = mod.builtInFlags;
            auto it = identities.find(mod.address);
            if (it != identities.end())
            {
                saved.identity = it->second;
            }
            savedModules.modules.push_back(std::move(saved));
        }

        exports.sort();
        m_moduleNames.Clear();

        {
            orthia::CAutoCriticalSection guard(m_lock);
            // the flags of a module that unloaded or changed go with it: another module at its
            // address is not analyzed. SyncDatabaseModules gives them back when it returns
            for (const auto& old : m_osModules)
            {
                const bool stays = std::any_of(modules.begin(), modules.end(), [&](const orthia::ModuleInfo& mod) {
                    return mod.address == old.address && mod.size == old.size && GetSavedModuleName(mod) == GetSavedModuleName(old);
                });
                if (!stays && !m_floatingModules.count(old.address))
                {
                    m_moduleFlags.erase(old.address);
                }
            }
            m_osModules = std::move(modules);
            m_osExports = std::move(exports);
            m_savedModules = std::move(savedModules);
            RebuildModulesNoLock();
            if (m_processModuleAddress == 0)
            {
                m_processModuleAddress = processModuleAddress;
            }
        }
        // the module list can change while the process runs: modules.xml and the database follow
        SaveModuleList();
        SyncDatabaseModules();
    }

    // m_modules and m_exports from the OS modules and the floating ones. A floating module where
    // the OS reports a module now (it loaded there since) gives way. TODO: say so instead of hiding it
    void CProcessWorkplaceItem::RebuildModulesNoLock()
    {
        std::vector<orthia::ModuleInfo> modules = m_osModules;
        auto exports = m_osExports;
        for (const auto& [address, floating] : m_floatingModules)
        {
            const bool overlaps = std::any_of(m_osModules.begin(), m_osModules.end(), [&](const orthia::ModuleInfo& mod) {
                return mod.address <= floating.lastValidAddress && floating.address <= mod.lastValidAddress;
            });
            if (overlaps)
            {
                continue;
            }
            modules.push_back(floating);
            std::vector<NameInfo> names;
            QueryFloatingNames(*floating.floating, names);
            for (auto& info : names)
            {
                info.name.native = floating.name + OUI_STR("!") + info.name.native;
                exports.insert(info.address, info);
            }
        }
        std::sort(modules.begin(), modules.end(), [](auto& m1, auto& m2) { return m1.address < m2.address; });
        exports.sort();

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
    }

    void CProcessWorkplaceItem::SetFloatingModule(std::shared_ptr<const FloatingRegionInfo> info)
    {
        if (!info)
        {
            return;
        }
        const Address_type address = info->allocationBase;
        {
            orthia::CAutoCriticalSection guard(m_lock);
            m_floatingModules[address] = MakeFloatingModule(std::move(info), m_dianaMode, m_dianaArch);
            RebuildModulesNoLock();
        }
        m_moduleNames.Invalidate(address);
    }

    void CProcessWorkplaceItem::RemoveFloatingModule(Address_type address)
    {
        {
            orthia::CAutoCriticalSection guard(m_lock);
            if (!m_floatingModules.erase(address))
            {
                return;
            }
            m_moduleFlags.erase(address);
            RebuildModulesNoLock();
        }
        m_moduleNames.Invalidate(address);
    }

    void CProcessWorkplaceItem::LoadSavedFloatingModules()
    {
        auto moduleManager = m_moduleManager;
        if (!moduleManager)
        {
            return;
        }
        std::vector<std::shared_ptr<FloatingRegionInfo>> regions;
        try
        {
            regions = LoadFloatingRegions(*moduleManager);
        }
        catch (const std::exception& e)
        {
            (void)e;
            ORTHIA_DEV_LOG(orthia::LogSeverity::Warning, std::string("Can't read the floating modules: ") + e.what());
            return;
        }
        // read before the lock: hashing reads the process. A saved view checks against its capture,
        // which is the memory later than the analysis; without one the snapshot is what was found
        auto saved = std::dynamic_pointer_cast<CSavedProcess>(m_proc);
        std::vector<bool> unchanged;
        for (const auto& info : regions)
        {
            if (m_savedView && !(saved && saved->IsCaptured(info->allocationBase)))
            {
                unchanged.push_back(true);
                continue;
            }
            unchanged.push_back(IsFloatingRegionUnchanged(*m_proc, *info));
        }
        orthia::CAutoCriticalSection guard(m_lock);
        for (size_t i = 0; i < regions.size(); ++i)
        {
            const Address_type address = regions[i]->allocationBase;
            m_floatingModules[address] = MakeFloatingModule(regions[i], m_dianaMode, m_dianaArch);
            if (!unchanged[i])
            {
                m_moduleFlags[address] |= ModuleInfo::flags_stale;
            }
        }
        RebuildModulesNoLock();
    }
    void CProcessWorkplaceItem::OnPrivateSymbolLoaded(Address_type addr, const oui::String& name)
    {
        orthia::CAutoCriticalSection guard(m_lock);
        auto it = m_exports.find(addr);
        if (it != m_exports.end())
        {
            it->second.privateSymbol = name;
        }
        // m_exports is rebuilt from it when a floating module comes
        auto osIt = m_osExports.find(addr);
        if (osIt != m_osExports.end())
        {
            osIt->second.privateSymbol = name;
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

        // a floating module has no headers: its names are its entry points
        if (module.floating)
        {
            std::vector<NameInfo> found;
            QueryFloatingNames(*module.floating, found);
            for (auto& info : found)
            {
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
    // Saved in the module metainfo for the next open of the same process. Not flags_symbolsLoaded:
    // the symbols of a process are kept in memory only (ModuleStorage, m_exports)
    static const int g_savedProcessModuleFlags = ModuleInfo::flags_analyzeDone | ModuleInfo::flags_analyzePrivateDone;

    // what tells a module at an address from another one loaded there later
    void CProcessWorkplaceItem::UpdateModuleFlags(Address_type moduleAddress, int flagsToSet, int flagsToRemove)
    {
        ModuleInfo module;
        bool moduleKnown = false;
        int flagsToSave = 0;
        {
            orthia::CAutoCriticalSection guard(m_lock);
            auto & flags = m_moduleFlags[moduleAddress];
            flags |= flagsToSet;
            flags &= ~flagsToRemove;
            flagsToSave = flags & g_savedProcessModuleFlags;

            auto it = m_modulesIndex.find(moduleAddress);
            if (it != m_modulesIndex.end())
            {
                m_modules[it->second].flags = flags;
                module = m_modules[it->second];
                moduleKnown = true;
            }
        }
        if (moduleKnown && ((flagsToSet | flagsToRemove) & g_savedProcessModuleFlags))
        {
            SaveModuleFlags(module, flagsToSave);
        }
        // --capture: the module's memory as the analysis saw it
        if (moduleKnown && (flagsToSet & ModuleInfo::flags_analyzeDone) && m_captureModules && !m_savedView &&
            !module.IsFloating() && !m_procFolder.empty())
        {
            try
            {
                const auto stats = CaptureRange(*m_proc, m_procFolder, module.address, module.size);
                ORTHIA_DEV_LOG(orthia::LogSeverity::Info, "Captured ", PlatformStringToUtf8(module.name), ": ",
                    CLogParamEx(stats.bytes, 16), " bytes");
            }
            catch (const std::exception& e)
            {
                ORTHIA_LOG(orthia::LogSeverity::Warning, "Can't capture ", PlatformStringToUtf8(module.name), ": ", e.what());
            }
        }
    }

    void CProcessWorkplaceItem::SaveModuleFlags(const ModuleInfo& module, int flags)
    {
        auto moduleManager = m_moduleManager;
        if (!moduleManager)
        {
            return;
        }
        try
        {
            auto database = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
            // the metainfo belongs to the tbl_modules row
            if (!database->IsModuleExists(module.address))
            {
                return;
            }
            CCommonFormatBuilder builder;
            builder.AddMetadata(ORTHIA_TCSTR("fullname"), GetSavedModuleName(module));
            builder.AddMetadata("size", (unsigned long long)module.size);
            builder.AddMetadata("flags", flags);
            // what tells this build from another at the same place and size
            WriteImageIdentity(builder, QuerySavedIdentity(module.address));
            std::string text;
            builder.Produce(&text);
            database->InsertMetaInfo(module.address, g_database_type_moduleMetaInfo, text, module.address, true);
        }
        catch (const std::exception& e)
        {
            (void)e;
            ORTHIA_DEV_LOG(orthia::LogSeverity::Warning, std::string("Can't save the module flags: ") + e.what());
        }
    }

    ImageIdentity CProcessWorkplaceItem::QuerySavedIdentity(Address_type moduleAddress) const
    {
        orthia::CAutoCriticalSection guard(m_lock);
        for (const auto& saved : m_savedModules.modules)
        {
            if (saved.module.address == moduleAddress)
            {
                return saved.identity;
            }
        }
        return ImageIdentity();
    }

    void CProcessWorkplaceItem::LoadSavedModuleFlags()
    {
        SyncDatabaseModules();
    }

    void CProcessWorkplaceItem::SyncDatabaseModules()
    {
        auto moduleManager = m_moduleManager;
        if (!moduleManager)
        {
            return;
        }
        struct Saved
        {
            PlatformString_type name;
            unsigned long long size = 0;
            int flags = 0;
            ImageIdentity identity;
        };
        std::vector<CommonModuleInfo> rows;
        std::map<Address_type, Saved> saved;
        try
        {
            moduleManager->QueryLoadedModules(&rows);
            moduleManager->QueryDatabaseManager()->GetClassicDatabase()->QueryMetaInfo(g_database_type_moduleMetaInfo,
                [&](Address_type moduleAddress, int, const std::string& text, Address_type)
            {
                CCommonFormatParser parser;
                if (parser.Parse(text))
                {
                    Saved record;
                    parser.QueryMetadata(ORTHIA_TCSTR("fullname"), &record.name);
                    parser.QueryMetadata("size", &record.size);
                    parser.QueryMetadata("flags", &record.flags);
                    ReadImageIdentity(parser, record.identity);
                    saved[moduleAddress] = std::move(record);
                }
                return true;
            });
        }
        catch (const std::exception& e)
        {
            (void)e;
            ORTHIA_DEV_LOG(orthia::LogSeverity::Warning, std::string("Can't read the saved modules: ") + e.what());
            return;
        }

        std::vector<orthia::ModuleInfo> osModules;
        std::map<Address_type, orthia::ModuleInfo> floating;
        {
            orthia::CAutoCriticalSection guard(m_lock);
            osModules = m_osModules;
            floating = m_floatingModules;
        }

        std::vector<Address_type> toDrop;
        std::map<Address_type, int> toRestore;
        for (const auto& row : rows)
        {
            if (!row.size)
            {
                continue;
            }
            // a floating module is checked against the memory (stale), never dropped here
            auto floatingIt = floating.find(row.address);
            if (floatingIt != floating.end())
            {
                auto it = saved.find(row.address);
                if (it != saved.end() && it->second.size == (unsigned long long)floatingIt->second.size &&
                    it->second.name == GetSavedModuleName(floatingIt->second))
                {
                    toRestore[row.address] = it->second.flags & g_savedProcessModuleFlags;
                }
                continue;
            }
            const Address_type rowLast = row.address + row.size - 1;
            std::vector<const orthia::ModuleInfo*> over;
            for (const auto& mod : osModules)
            {
                if (mod.address <= rowLast && row.address <= mod.lastValidAddress)
                {
                    over.push_back(&mod);
                }
            }
            if (over.empty())
            {
                // unloaded: kept
                continue;
            }
            auto it = saved.find(row.address);
            const Saved* record = it == saved.end() ? nullptr : &it->second;
            // the row has the size the analysis mapped, which is not the OS-reported one (a PE may end
            // with its last section, an ELF with its bss): the module at the row's address, and the
            // size, name and build in the metainfo
            auto at = std::find_if(over.begin(), over.end(), [&](const orthia::ModuleInfo* mod) { return mod->address == row.address; });
            bool same = at != over.end();
            if (same && record)
            {
                const auto identity = QuerySavedIdentity(row.address);
                same = record->size == (unsigned long long)(*at)->size &&
                    record->name == GetSavedModuleName(**at) &&
                    (!record->identity.IsValid() || !identity.IsValid() || SameIdentity(record->identity, identity));
            }
            if (same)
            {
                if (record)
                {
                    toRestore[row.address] = record->flags & g_savedProcessModuleFlags;
                }
                continue;
            }
            if (!m_savedView)
            {
                toDrop.push_back(row.address);
            }
        }

        for (auto address : toDrop)
        {
            try
            {
                moduleManager->UnloadModule(address);
                ORTHIA_LOG(orthia::LogSeverity::Info, "Another module is at ", CLogParamEx(address, 16), ": the analysis saved there is dropped");
            }
            catch (const std::exception& e)
            {
                (void)e;
                ORTHIA_DEV_LOG(orthia::LogSeverity::Warning, std::string("Can't drop a replaced module: ") + e.what());
            }
        }
        if (toDrop.empty() && toRestore.empty())
        {
            return;
        }
        {
            orthia::CAutoCriticalSection guard(m_lock);
            for (auto address : toDrop)
            {
                m_moduleFlags.erase(address);
            }
            for (const auto& [address, flags] : toRestore)
            {
                m_moduleFlags[address] |= flags;
            }
            RebuildModulesNoLock();
        }
        for (auto address : toDrop)
        {
            m_moduleNames.Invalidate(address);
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