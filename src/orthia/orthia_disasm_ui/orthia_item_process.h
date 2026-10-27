#pragma once
#include "oui_processes.h"
#include "orthia_module_symbols.h"
#include "orthia_module_names.h"
#include "orthia_saved_process.h"

namespace orthia
{
    class CProcessWorkplaceItem:public std::enable_shared_from_this<CProcessWorkplaceItem>, public BaseWorkPlaceItem
    {
        mutable orthia::CCriticalSection m_lock;
        std::shared_ptr<oui::IProcess> m_proc;
        oui::String m_shortName;
        int m_dianaMode = 0;
        int m_dianaArch = DIANA_ARCH_X86;
        PlatformString_type m_procFolder;
        // opened from a saved folder (CSavedProcess): the folder is never removed, modules.xml never written
        bool m_savedView = false;
        // what modules.xml says, from the last ReloadModules; written by the live view
        SavedModuleList m_savedModules;
        std::mutex m_saveLock;
        void SaveModuleList();

        // the OS modules and the floating ones, by address
        std::vector<orthia::ModuleInfo> m_modules;
        std::map<orthia::Address_type, int> m_modulesIndex;
        std::map<orthia::Address_type, int> m_moduleFlags;
        orthia::flat_map<orthia::Address_type, NameInfo> m_exports;
        // what the OS reports, and the floating modules found by the analysis
        std::vector<orthia::ModuleInfo> m_osModules;
        orthia::flat_map<orthia::Address_type, NameInfo> m_osExports;
        std::map<orthia::Address_type, orthia::ModuleInfo> m_floatingModules;
        void RebuildModulesNoLock();

        Address_type m_processModuleAddress = 0;
        std::shared_ptr<CModuleManager> m_moduleManager;
        mutable std::shared_ptr<IPeristentItemStorage> m_persistentStorage;
        std::shared_ptr<ModuleStorage> m_moduleStorage;
        mutable ModuleNamesStorage m_moduleNames;

        void BuildModuleNames(Address_type moduleAddress, ModuleNames& names) const;
        std::shared_ptr<const ModuleNames> QueryModuleNames(Address_type moduleAddress) const;
        NameInfo QueryAddressNameNoLock(Address_type address) const;
        NameInfo QueryAddressNameImpl(Address_type address, orthia::ModuleInfo& moduleInfo) const;
        void SaveModuleFlags(const ModuleInfo& module, int flags);

    public:
        ~CProcessWorkplaceItem();
        CProcessWorkplaceItem(std::shared_ptr<oui::IProcess> proc,
            const oui::String& shortName,
            int dianaMode,
            int dianaArch,
            std::shared_ptr<IPeristentItemStorage> persistentStorage);

        // live view: writes modules.xml there now and after each ReloadModules
        void SetProcFolder(const PlatformString_type& folder);
        // before SetProcFolder: the item shows a saved folder, not a running process
        void SetSavedView() { m_savedView = true; }
        bool IsSavedView() const { return m_savedView; }
        void Init(std::shared_ptr<CModuleManager> moduleManager,
            std::shared_ptr<CFilePersistentItemStorage> persistentItemStorage,
            std::shared_ptr<ModuleStorage> moduleStorage);
        // after Init, on a database reused from an earlier open of the same process: the
        // analysis flags of the modules that are still the same
        void LoadSavedModuleFlags();
        // before LoadSavedModuleFlags, on a reused database: the floating modules found then,
        // stale when their memory changed. TODO: the same check on demand in an open process, and a
        // way to drop a stale module
        void LoadSavedFloatingModules();
        void SetFloatingModule(std::shared_ptr<const FloatingRegionInfo> info) override;
        void RemoveFloatingModule(Address_type address) override;

        void ReloadModules() override;
        WorkAddressData ReadData(Address_type address, Address_type size) override;
        WorkAddressRangeInfo GetRangeInfo(Address_type address) const override;
        const std::shared_ptr<CModuleManager> GetModuleManager() const override;
        oui::String GetShortName() const override;
        void GetModules(std::vector<orthia::ModuleInfo>& modules) const override;
        int GetModulesCount() const override;
        int GetDianaMode() const override { return m_dianaMode;}
        int GetDianaArch() const override { return m_dianaArch; }
        Address_type GetMainModuleAddress() const override;
        std::shared_ptr<IPeristentItemStorage> GetPersistentStorage() override;
        void QueryNames(Address_type moduleAddress, const NameSelectionKey& name, int count, std::vector<NameInfo>& names) const override;
        int QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const override;
        void InvalidateNames(Address_type moduleAddress) override;
        MarkupRangeInfo QueryMarkupRange(Address_type address, IMarkupCache* cache = nullptr) const override;
        void QueryMarkupRange(Address_type address, int index, int count, MarkupRange& range, IMarkupCache* cache = nullptr) const override;
        bool QueryAddressModule(Address_type address, orthia::ModuleInfo& result) const;
        NameInfo QueryAddressName(Address_type address) const;
        std::shared_ptr<::DianaMovableReadStream> CreateDisasmStream(Address_type addressStart);
        Address_type QueryAddressByName(const oui::String& text, Address_type defValue) const override;
        std::shared_ptr<oui::IProcess> GetAssociatedProcess() override;
        void OnPrivateSymbolLoaded(Address_type addr, const oui::String& name) override;
        void OnModuleSymbolsLoaded(Address_type moduleAddress) override;
        std::shared_ptr<IMemoryReader> CreateMemoryReader() override;
        void UpdateModuleFlags(Address_type moduleAddress, int flagsToSet, int flagsToRemove) override;
        ModuleStorage* GetModuleStorage() override;
        void QuerySections(Address_type moduleBase, const FileOpener_type& openFile, ImageSections& sections) override;
        PlatformString_type GetDatabaseFolder() const override { return m_procFolder; }
    };

}