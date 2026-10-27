#pragma once

// Floating code: executable memory of a process that no loaded module owns (hooks, injected
// code, JIT stubs). The analysis of a module records the jumps, calls and taken addresses that
// leave it (tbl_references); the ones that land in such memory make a floating module per OS
// allocation, "fl_<allocation base>". A floating module is raw code analyzed from its entry
// points, and is saved in the process database (metainfo g_database_type_floatingRegion).

#include "orthia_model_interfaces.h"
#include "oui_processes.h"

namespace orthia
{
    class CModuleManager;

    // metainfo of a floating module at its address: FloatingRegionInfo as text
    const static int g_database_type_floatingRegion = 6;

    struct FloatingEntryInfo
    {
        Address_type address = 0;
        std::vector<Address_type> referrers;    // the instructions that lead here, sorted
    };

    struct FloatingRegionInfo
    {
        Address_type allocationBase = 0;
        Address_type allocationSize = 0;
        int protect = 0;                        // oui::MemoryRegionInfo::protect_* where the entries are
        int type = 0;                           // oui::MemoryRegionInfo::type_*
        std::string mappedName;                 // utf8, may be empty
        // "MZ", "ELF" or empty: a module mapped by hand. TODO: open it with its format, not as raw code
        std::string header;
        // what tells the same memory on a later open
        unsigned long long bytesHash = 0;
        Address_type hashedSize = 0;
        // the lowest and the highest instruction start the analysis found; 0 before it
        Address_type codeStart = 0;
        Address_type codeLast = 0;
        std::vector<FloatingEntryInfo> entries; // by address
        // the bytes as they were found: a file in the floating/ folder of the process database
        std::string snapshotFile;               // utf8 file name; empty when none was saved
        Address_type snapshotSize = 0;          // from the allocation base on
        Address_type snapshotMissing = 0;       // bytes that couldn't be read, zero filled
        std::string sha256;                     // of the snapshot file, hex

        Address_type GetLastAddress() const { return allocationBase + allocationSize - 1; }
        bool HasEntry(Address_type address) const;
        // adds the entry and the referrer; true when either is new, *pNewEntry when the entry is
        bool AddReference(Address_type entry, Address_type referrer, bool* pNewEntry = nullptr);
    };

    // "fl_1f0000"
    PlatformString_type MakeFloatingModuleName(Address_type allocationBase);
    // "RWX", "R-X", "---"
    PlatformString_type FormatRegionProtect(int protect);
    // "private", "mapped", "image"
    PlatformString_type FormatRegionType(int type);

    // the module of a floating region: raw code in the mode and architecture of the process
    ModuleInfo MakeFloatingModule(std::shared_ptr<const FloatingRegionInfo> info, int dianaMode, int dianaArch);

    // the metainfo text and back; false when the text doesn't parse
    std::string FormatFloatingRegion(const FloatingRegionInfo& info);
    bool ParseFloatingRegion(const std::string& text, FloatingRegionInfo& info);

    // header and bytesHash from the memory as it is now
    void FingerprintFloatingRegion(oui::IProcess& process, FloatingRegionInfo& info);

    // the folder of the snapshots in a process database folder
    extern const ORTHIA_TCHAR* const g_floatingSnapshotFolder;
    // whether a process database folder holds snapshots of floating code: such a folder is evidence,
    // kept when the process exits, removed only by .database delete
    bool HasFloatingSnapshots(const PlatformString_type& databaseFolder);
    // writes the bytes of the allocation (up to 16 MB, unreadable pages zero filled) to
    // <databaseFolder>/floating/<base>_<sha256 prefix>.bin and records it in info. An earlier
    // snapshot with other bytes stays. False (and info unchanged) when it can't be written
    bool SaveFloatingSnapshot(oui::IProcess& process, const PlatformString_type& databaseFolder, FloatingRegionInfo& info);
    // whether the allocation is still there with the same executable bytes
    bool IsFloatingRegionUnchanged(oui::IProcess& process, const FloatingRegionInfo& info);
    // the region as it is now: allocation, protection, fingerprint, the entries still in executable
    // memory; no analysis yet. nullptr when the allocation went away, a module the OS reports took
    // its place, or none of its entries is executable any more
    std::shared_ptr<FloatingRegionInfo> RefreshFloatingRegion(oui::IProcess& process,
        const std::vector<ModuleInfo>& modules,
        const FloatingRegionInfo& info);
    // drops a floating module: from the database and from the item
    void RemoveFloatingModule(IWorkPlaceItem& item, Address_type address);
    // saves a refreshed region in place of the old one, not stale and waiting for its analysis
    void ReplaceFloatingRegion(IWorkPlaceItem& item, std::shared_ptr<FloatingRegionInfo> info);

    enum class FloatingRefresh
    {
        refreshed,      // module is the region as it is now, waiting for its analysis
        removed,        // the region is gone, so is the module
        unavailable     // no process to look at: nothing changed
    };
    // .analyze /f on a stale floating module: checks it against the memory again
    FloatingRefresh RefreshStaleFloatingModule(IWorkPlaceItem& item, ModuleInfo& module);

    // names for x: $entry_0001, ... by address
    void QueryFloatingNames(const FloatingRegionInfo& info, std::vector<NameInfo>& names);

    struct FloatingDiscovery
    {
        // new regions and known ones with new entries, ready to save
        std::vector<std::shared_ptr<FloatingRegionInfo>> changed;
        // of them, the ones whose analysis is not up to date any more
        std::vector<Address_type> toAnalyze;
    };
    // the floating code the references from source lead to. modules: every module of the
    // process, floating ones too
    FloatingDiscovery CollectFloatingRegions(CModuleManager& moduleManager,
        oui::IProcess& process,
        const std::vector<ModuleInfo>& modules,
        const ModuleInfo& source);

    // the module row and the metainfo
    void SaveFloatingRegion(CModuleManager& moduleManager, const FloatingRegionInfo& info);
    // every floating region saved in the database
    std::vector<std::shared_ptr<FloatingRegionInfo>> LoadFloatingRegions(CModuleManager& moduleManager);

    // `modinfo` lines of a floating module; referrers named through the item
    void DescribeFloatingModule(const IWorkPlaceItem& item, const ModuleInfo& module, std::vector<PlatformString_type>& lines);

    // Discovery from source: saves what it found and gives it to the item. True when a floating
    // module waits for its analysis. Process items only; false for the others
    bool DiscoverFloatingCode(IWorkPlaceItem& item, const ModuleInfo& source);
    // analyzes a floating module from its entry points and records the code it found
    void AnalyzeFloatingModule(IWorkPlaceItem& item, const ModuleInfo& module);
    // the floating modules of the item that wait for their analysis (not stale)
    std::vector<ModuleInfo> QueryPendingFloatingModules(const IWorkPlaceItem& item);
}
