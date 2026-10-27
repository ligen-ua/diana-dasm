#pragma once

// A process folder (proc/[pid] name <start>) opened after the process exited: the module list
// saved while it ran (modules.xml), the bytes from what is still there. The analysis, comments
// and floating modules are in its data.db as usual.

#include "oui_processes.h"
#include "orthia_image_identity.h"
#include "orthia_databases.h"
#include <memory>
#include <mutex>

namespace orthia
{
    struct IImageSource;
    struct FloatingRegionInfo;

    // modules.xml of a process folder: what the OS reported last
    extern const ORTHIA_TCHAR* const g_savedModulesFileName;

    struct SavedModuleInfo
    {
        ModuleInfo module;          // address, size, name, fullName, mode, arch, entry, builtInFlags
        ImageIdentity identity;     // from the memory headers; invalid when they don't tell
    };

    struct SavedModuleList
    {
        // the module of the process image (its entry point is the main address)
        Address_type mainModule = 0;
        std::vector<SavedModuleInfo> modules;   // by address
    };

    // the text of modules.xml and back; false when the text doesn't parse
    std::string FormatSavedModules(const SavedModuleList& list);
    bool ParseSavedModules(const std::string& text, SavedModuleList& list);
    // writes <folder>/modules.xml when it says something else than now. Throws when it can't
    void WriteSavedModules(const PlatformString_type& folder, const SavedModuleList& list);
    // false when the folder has no modules.xml or it doesn't parse
    bool ReadSavedModules(const PlatformString_type& folder, SavedModuleList& list);

    // The process as the folder keeps it. Module bytes come from the module files that are still
    // the build the process ran (file on disk: no in-memory patches), floating code from its
    // snapshots; anything else reads as missing
    class CSavedProcess : public oui::IProcess
    {
        struct Snapshot
        {
            Address_type base = 0;
            Address_type allocationSize = 0;
            int protect = 0;
            int type = 0;
            std::string mappedName;
            std::vector<char> data;
        };
        PlatformString_type m_folder;
        FolderMeta m_meta;
        SavedModuleList m_list;
        // by base, one per module
        std::vector<std::shared_ptr<IImageSource>> m_sources;
        mutable std::mutex m_lock;      // m_snapshots
        std::vector<Snapshot> m_snapshots;

        IImageSource* FindSource(Address_type address) const;
    public:
        CSavedProcess(const PlatformString_type& folder, const FolderMeta& meta, const SavedModuleList& list);

        const PlatformString_type& GetFolder() const { return m_folder; }
        const FolderMeta& GetMeta() const { return m_meta; }
        bool HasModuleList() const { return !m_list.modules.empty(); }
        // the bytes of a floating module, from its snapshot file in the floating/ folder; false
        // when it has none or the file can't be read
        bool AddSnapshot(const FloatingRegionInfo& info);

        // oui::IFile
        std::tuple<int, unsigned long long> GetSizeInBytes() const override;
        int ReadExact(std::shared_ptr<oui::BaseOperation> operation, unsigned long long offset, size_t size, std::vector<char>& data) override;
        oui::String GetFullFileName() const override;
        oui::String GetFullFileNameForUI() const override;
        // oui::IProcess
        WorkAddressData ReadExactEx(unsigned long long offset, size_t size) override;
        int ReadExactEx2(unsigned long long offset, void* pBuffer, size_t size) override;
        int QueryModules(std::vector<ModuleInfo>& modules, int& processModuleOffset,
            std::function<void(const ModuleInfo& info, oui::ModuleDisasmContext&)> contextCallback) override;
        std::tuple<int, std::shared_ptr<oui::IThreadEnumerator>> CreateThreadEnumerator() override;
        int GetDianaMode() const override { return m_meta.dianaMode; }
        int GetDianaArch() const override { return m_meta.dianaArch; }
        oui::ProcessStartInfo GetStartInfo() const override;
        bool IsRunning() const override { return false; }
        int QueryMemoryRegion(unsigned long long address, oui::MemoryRegionInfo& info) override;
    };

    // the folder a --proc-folder argument means: a proc/ folder or its data.db, or a selector of
    // .database delete (pid, folder name). Throws with a message for the user
    PlatformString_type ResolveSavedProcessFolder(const std::vector<DatabaseInfo>& all, const PlatformString_type& argument);
    // the folder with its meta.xml and modules.xml. Throws with a message for the user
    std::shared_ptr<CSavedProcess> OpenSavedProcess(const PlatformString_type& folder);
}
