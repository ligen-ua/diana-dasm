#pragma once

// A capture: the memory of a process saved into its folder, for the deep case where the module
// files on disk are not enough (in-memory patches, unloaded or deleted modules, data, stacks).
//   capture/capture.xml       the memory map, which regions have bytes, the threads
//   capture/<base>_<size>.bin the bytes of a region, unreadable pages zero filled
// .capture saves everything; --capture saves each module's memory when it is analyzed. A capture
// of a range replaces the regions saved before over it, the others stay (an unloaded module)

#include "oui_processes.h"
#include <functional>

namespace orthia
{
    extern const ORTHIA_TCHAR* const g_captureFolder;

    struct CapturedRegion
    {
        Address_type base = 0;
        Address_type size = 0;
        Address_type allocationBase = 0;
        int protect = 0;                // oui::MemoryRegionInfo::protect_*
        bool committed = false;
        int type = 0;                   // oui::MemoryRegionInfo::type_*
        std::string name;               // the mapped file or pseudo name, utf8
        std::string file;               // the bytes, in capture/; empty: not saved (reserved, no access)
        Address_type missing = 0;       // bytes of the file that couldn't be read, zero filled
    };

    struct CapturedThread
    {
        unsigned long long tid = 0;
        Address_type startAddress = 0;
        std::string startTime;          // CCommonDateTime::ToSQLTime, may be empty
        std::vector<Address_type> stack;
    };

    struct CaptureIndex
    {
        long long createdUnix = 0;
        bool complete = true;           // false: cancelled half way
        std::vector<CapturedRegion> regions;    // by base, not overlapping
        bool hasThreads = false;
        std::vector<CapturedThread> threads;
    };

    std::string FormatCaptureIndex(const CaptureIndex& index);
    bool ParseCaptureIndex(const std::string& text, CaptureIndex& index);
    // false when the folder has no capture or it doesn't parse
    bool ReadCaptureIndex(const PlatformString_type& folder, CaptureIndex& index);
    bool HasCapture(const PlatformString_type& folder);
    // the regions of captured, in place of what base had over their range; the rest of base stays
    void MergeCapturedRegions(std::vector<CapturedRegion>& base, const std::vector<CapturedRegion>& captured);

    struct CaptureStats
    {
        size_t regions = 0;             // in the map
        size_t savedRegions = 0;        // with bytes
        unsigned long long bytes = 0;
        unsigned long long missing = 0;
        size_t threads = 0;
    };
    // after each region with bytes; false cancels: what is saved so far stays, marked incomplete
    using CaptureProgress_type = std::function<bool(const CaptureStats&)>;

    // .capture: the whole memory map, the bytes of every committed readable region, the threads.
    // Reads only: the process is not stopped, so the bytes of a running thread may change while
    // they are read. Throws when the map can't be read or the folder can't be written
    CaptureStats CaptureProcess(oui::IProcess& process, const PlatformString_type& folder, CaptureProgress_type progress);
    // --capture: the regions of [base, base + size), a module
    CaptureStats CaptureRange(oui::IProcess& process, const PlatformString_type& folder, Address_type base, Address_type size);
}
