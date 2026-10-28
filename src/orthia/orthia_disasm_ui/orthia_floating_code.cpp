#include "orthia_floating_code.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include "orthia_common_format.h"
#include "orthia_common_print.h"
#include "orthia_log.h"
#include "orthia_sha256.h"
#include "orthia_process_adapter.h"
#include "formats/orthia_image_format.h"
#include "orthia_capture.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace orthia
{
    namespace
    {
        // the most a pass registers: the references of one module can't flood the process.
        // TODO: an option, or a note in lmf of what was left out (docs/orthia.md, Floating code)
        const size_t c_maxNewRegions = 1024;
        // the bytes hashed of an allocation, and the most of it one analysis looks at: the analyzer
        // keeps a few pointers per byte, a JIT heap can reserve gigabytes.
        // TODO: entry points past the window are not analyzed, bytes past it don't make a module stale
        const Address_type c_maxHashedSize = 16 * 1024 * 1024;
        const Address_type c_maxAnalyzedSize = 16 * 1024 * 1024;
        // TODO: a JIT heap gives many big allocations; a budget per process, or an option
        const Address_type c_maxSnapshotSize = 16 * 1024 * 1024;
        const size_t c_readChunk = 0x10000;

        std::string ToHex(unsigned long long value)
        {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%llx", value);
            return buffer;
        }

        bool ParseHex(const std::string& text, unsigned long long& value)
        {
            if (text.empty())
            {
                return false;
            }
            char* end = nullptr;
            value = strtoull(text.c_str(), &end, 16);
            return end && *end == 0;
        }

        std::vector<std::string> Split(const std::string& text, char separator)
        {
            std::vector<std::string> parts;
            if (text.empty())
            {
                return parts;
            }
            std::string::size_type start = 0;
            for (;;)
            {
                auto pos = text.find(separator, start);
                parts.push_back(text.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
                if (pos == std::string::npos)
                {
                    break;
                }
                start = pos + 1;
            }
            return parts;
        }

        // a slot name in the slots text: the separators and % as %XX
        std::string EscapeName(const std::string& name)
        {
            std::string text;
            for (unsigned char c : name)
            {
                if (c < 0x20 || c == '%' || c == ',' || c == ';' || c == ':' || c == '=')
                {
                    char buffer[4];
                    snprintf(buffer, sizeof(buffer), "%%%02X", c);
                    text += buffer;
                }
                else
                {
                    text += (char)c;
                }
            }
            return text;
        }

        std::string UnescapeName(const std::string& text)
        {
            std::string name;
            for (size_t i = 0; i < text.size(); ++i)
            {
                unsigned long long value = 0;
                if (text[i] == '%' && i + 2 < text.size() && ParseHex(text.substr(i + 1, 2), value))
                {
                    name += (char)value;
                    i += 2;
                }
                else
                {
                    name += text[i];
                }
            }
            return name;
        }

        // FNV-1a
        void HashBytes(unsigned long long& hash, const unsigned char* data, size_t size)
        {
            for (size_t i = 0; i < size; ++i)
            {
                hash ^= data[i];
                hash *= 0x100000001b3ULL;
            }
        }

        const ModuleInfo* FindModule(const std::vector<ModuleInfo>& sorted, Address_type address)
        {
            auto it = std::upper_bound(sorted.begin(), sorted.end(), address,
                [](Address_type a, const ModuleInfo& m) { return a < m.address; });
            if (it == sorted.begin())
            {
                return nullptr;
            }
            --it;
            return it->IsInRange(address) ? &*it : nullptr;
        }

        // 00 00 is no entry point: zero filled memory, where a 32-bit constant that looks like an
        // address (the analysis takes it as one) often lands
        bool IsZeroFilled(oui::IProcess& process, Address_type address)
        {
            unsigned char bytes[2] = { 0xFF, 0xFF };
            return !process.ReadExactEx2(address, bytes, sizeof(bytes)) && !bytes[0] && !bytes[1];
        }

        // "code", "address taken"; empty when unknown (saved before the kinds were)
        PlatformString_type DescribeReferenceKind(const FloatingEntryInfo& entry, size_t index)
        {
            const int kind = index < entry.referrerKinds.size() ? entry.referrerKinds[index] : reference_unknown;
            switch (kind)
            {
            case reference_code: return ORTHIA_TCSTR("code");
            case reference_address: return ORTHIA_TCSTR("address taken");
            case reference_data: return ORTHIA_TCSTR("data");
            default: return PlatformString_type();
            }
        }

        bool OverlapsModule(const std::vector<ModuleInfo>& sorted, Address_type base, Address_type size)
        {
            const Address_type last = base + size - 1;
            for (const auto& mod : sorted)
            {
                if (mod.address <= last && base <= mod.lastValidAddress)
                {
                    return true;
                }
            }
            return false;
        }
    }

    bool FloatingRegionInfo::HasEntry(Address_type address) const
    {
        auto it = std::lower_bound(entries.begin(), entries.end(), address,
            [](const FloatingEntryInfo& e, Address_type a) { return e.address < a; });
        return it != entries.end() && it->address == address;
    }

    bool FloatingRegionInfo::AddReference(Address_type entry, Address_type referrer, bool* pNewEntry, int kind)
    {
        if (pNewEntry)
        {
            *pNewEntry = false;
        }
        auto it = std::lower_bound(entries.begin(), entries.end(), entry,
            [](const FloatingEntryInfo& e, Address_type a) { return e.address < a; });
        if (it == entries.end() || it->address != entry)
        {
            FloatingEntryInfo info;
            info.address = entry;
            it = entries.insert(it, info);
            if (pNewEntry)
            {
                *pNewEntry = true;
            }
        }
        auto& referrers = it->referrers;
        auto& kinds = it->referrerKinds;
        kinds.resize(referrers.size(), reference_unknown);
        auto refIt = std::lower_bound(referrers.begin(), referrers.end(), referrer);
        const auto index = refIt - referrers.begin();
        if (refIt != referrers.end() && *refIt == referrer)
        {
            // a new entry has no referrers yet: nothing changed, unless the kind got known
            if (kinds[index] == reference_unknown && kind != reference_unknown)
            {
                kinds[index] = kind;
                return true;
            }
            return false;
        }
        referrers.insert(refIt, referrer);
        kinds.insert(kinds.begin() + index, kind);
        return true;
    }

    bool FloatingRegionInfo::AddSlot(Address_type entry, const PointerSlotInfo& slot, bool* pNewEntry)
    {
        if (pNewEntry)
        {
            *pNewEntry = false;
        }
        auto it = std::lower_bound(entries.begin(), entries.end(), entry,
            [](const FloatingEntryInfo& e, Address_type a) { return e.address < a; });
        if (it == entries.end() || it->address != entry)
        {
            FloatingEntryInfo info;
            info.address = entry;
            it = entries.insert(it, info);
            if (pNewEntry)
            {
                *pNewEntry = true;
            }
        }
        auto& slots = it->slots;
        auto slotIt = std::lower_bound(slots.begin(), slots.end(), slot.slot,
            [](const PointerSlotInfo& s, Address_type a) { return s.slot < a; });
        if (slotIt != slots.end() && slotIt->slot == slot.slot)
        {
            if (slotIt->kind == slot.kind && slotIt->name == slot.name)
            {
                return false;
            }
            slotIt->kind = slot.kind;
            slotIt->name = slot.name;
            return true;
        }
        PointerSlotInfo stored = slot;
        stored.value = entry;
        slots.insert(slotIt, stored);
        return true;
    }

    PlatformString_type MakeFloatingModuleName(Address_type allocationBase)
    {
        return Utf8ToPlatformString("fl_" + ToHex(allocationBase));
    }

    PlatformString_type FormatRegionProtect(int protect)
    {
        std::string text;
        text += (protect & oui::MemoryRegionInfo::protect_read) ? 'R' : '-';
        text += (protect & oui::MemoryRegionInfo::protect_write) ? 'W' : '-';
        text += (protect & oui::MemoryRegionInfo::protect_exec) ? 'X' : '-';
        return Utf8ToPlatformString(text);
    }

    PlatformString_type FormatRegionType(int type)
    {
        switch (type)
        {
        case oui::MemoryRegionInfo::type_mapped: return ORTHIA_TCSTR("mapped");
        case oui::MemoryRegionInfo::type_image:  return ORTHIA_TCSTR("image");
        default:                                 return ORTHIA_TCSTR("private");
        }
    }

    ModuleInfo MakeFloatingModule(std::shared_ptr<const FloatingRegionInfo> info, int dianaMode, int dianaArch)
    {
        ModuleInfo module;
        module.address = info->allocationBase;
        module.size = info->allocationSize;
        module.lastValidAddress = info->GetLastAddress();
        module.entryPoint = info->entries.empty() ? info->allocationBase : info->entries.front().address;
        module.dianaMode = dianaMode;
        module.dianaArch = dianaArch;
        module.builtInFlags = ModuleInfo::builtInFlags_moduleTypeRaw | ModuleInfo::builtInFlags_floating;
        module.name = MakeFloatingModuleName(info->allocationBase);
        module.floating = std::move(info);
        return module;
    }

    std::string FormatFloatingRegion(const FloatingRegionInfo& info)
    {
        // entries: "<entry>:<referrer>,<referrer>/<kind>;<entry>:..." in hex; the kind (c code,
        // a address taken, d data) is left out when unknown
        std::string entries;
        for (const auto& entry : info.entries)
        {
            if (!entries.empty())
            {
                entries += ';';
            }
            entries += ToHex(entry.address);
            entries += ':';
            for (size_t i = 0; i < entry.referrers.size(); ++i)
            {
                if (i)
                {
                    entries += ',';
                }
                entries += ToHex(entry.referrers[i]);
                const int kind = i < entry.referrerKinds.size() ? entry.referrerKinds[i] : reference_unknown;
                switch (kind)
                {
                case reference_code: entries += "/c"; break;
                case reference_address: entries += "/a"; break;
                case reference_data: entries += "/d"; break;
                default: break;
                }
            }
        }
        // slots: "<entry>:<i|e><slot>=<name>,...;<entry>:...", the entries that have any
        std::string slots;
        for (const auto& entry : info.entries)
        {
            if (entry.slots.empty())
            {
                continue;
            }
            if (!slots.empty())
            {
                slots += ';';
            }
            slots += ToHex(entry.address);
            slots += ':';
            for (size_t i = 0; i < entry.slots.size(); ++i)
            {
                const auto& slot = entry.slots[i];
                if (i)
                {
                    slots += ',';
                }
                slots += slot.kind == PointerSlotInfo::kind_export ? 'e' : 'i';
                slots += ToHex(slot.slot);
                slots += '=';
                slots += EscapeName(slot.name);
            }
        }
        CCommonFormatBuilder builder;
        builder.AddMetadata("base", ToHex(info.allocationBase));
        builder.AddMetadata("size", ToHex(info.allocationSize));
        builder.AddMetadata("protect", info.protect);
        builder.AddMetadata("type", info.type);
        builder.AddMetadata("mapped", info.mappedName);
        builder.AddMetadata("header", info.header);
        builder.AddMetadata("hash", ToHex(info.bytesHash));
        builder.AddMetadata("hashed", ToHex(info.hashedSize));
        builder.AddMetadata("code_start", ToHex(info.codeStart));
        builder.AddMetadata("code_last", ToHex(info.codeLast));
        builder.AddMetadata("entries", entries);
        if (!slots.empty())
        {
            builder.AddMetadata("slots", slots);
        }
        if (!info.snapshotFile.empty())
        {
            builder.AddMetadata("snapshot", info.snapshotFile);
            builder.AddMetadata("snapshot_size", ToHex(info.snapshotSize));
            builder.AddMetadata("snapshot_missing", ToHex(info.snapshotMissing));
            builder.AddMetadata("sha256", info.sha256);
        }
        std::string text;
        builder.Produce(&text);
        return text;
    }

    bool ParseFloatingRegion(const std::string& text, FloatingRegionInfo& info)
    {
        CCommonFormatParser parser;
        if (!parser.Parse(text))
        {
            return false;
        }
        info = FloatingRegionInfo();
        auto queryHex = [&](const char* name, Address_type& value) {
            std::string str;
            unsigned long long parsed = 0;
            if (!parser.QueryMetadata(std::string(name), &str) || !ParseHex(str, parsed))
            {
                return false;
            }
            value = (Address_type)parsed;
            return true;
        };
        unsigned long long hash = 0;
        Address_type hashValue = 0;
        if (!queryHex("base", info.allocationBase) || !queryHex("size", info.allocationSize) || !info.allocationSize)
        {
            return false;
        }
        parser.QueryMetadata(std::string("protect"), &info.protect);
        parser.QueryMetadata(std::string("type"), &info.type);
        parser.QueryMetadata(std::string("mapped"), &info.mappedName);
        parser.QueryMetadata(std::string("header"), &info.header);
        if (queryHex("hash", hashValue))
        {
            hash = hashValue;
        }
        info.bytesHash = hash;
        queryHex("hashed", info.hashedSize);
        queryHex("code_start", info.codeStart);
        queryHex("code_last", info.codeLast);
        parser.QueryMetadata(std::string("snapshot"), &info.snapshotFile);
        queryHex("snapshot_size", info.snapshotSize);
        queryHex("snapshot_missing", info.snapshotMissing);
        parser.QueryMetadata(std::string("sha256"), &info.sha256);

        std::string entries;
        parser.QueryMetadata(std::string("entries"), &entries);
        for (const auto& item : Split(entries, ';'))
        {
            auto colon = item.find(':');
            unsigned long long address = 0;
            if (!ParseHex(item.substr(0, colon), address))
            {
                return false;
            }
            FloatingEntryInfo entry;
            entry.address = (Address_type)address;
            std::vector<std::pair<Address_type, int>> referrers;
            if (colon != std::string::npos)
            {
                for (const auto& ref : Split(item.substr(colon + 1), ','))
                {
                    // "<referrer>" from before the kinds, or "<referrer>/<kind>"
                    const auto slash = ref.find('/');
                    unsigned long long referrer = 0;
                    if (!ParseHex(ref.substr(0, slash), referrer))
                    {
                        return false;
                    }
                    int kind = reference_unknown;
                    if (slash != std::string::npos)
                    {
                        const auto letter = ref.substr(slash + 1);
                        kind = letter == "c" ? reference_code : letter == "a" ? reference_address :
                            letter == "d" ? reference_data : reference_unknown;
                    }
                    referrers.push_back({ (Address_type)referrer, kind });
                }
            }
            std::sort(referrers.begin(), referrers.end());
            for (const auto& ref : referrers)
            {
                entry.referrers.push_back(ref.first);
                entry.referrerKinds.push_back(ref.second);
            }
            info.entries.push_back(entry);
        }
        std::sort(info.entries.begin(), info.entries.end(),
            [](const FloatingEntryInfo& a, const FloatingEntryInfo& b) { return a.address < b.address; });

        std::string slots;
        parser.QueryMetadata(std::string("slots"), &slots);
        for (const auto& item : Split(slots, ';'))
        {
            auto colon = item.find(':');
            unsigned long long address = 0;
            if (colon == std::string::npos || !ParseHex(item.substr(0, colon), address))
            {
                return false;
            }
            for (const auto& text : Split(item.substr(colon + 1), ','))
            {
                auto equals = text.find('=');
                unsigned long long slotAddress = 0;
                if (text.size() < 2 || (text[0] != 'i' && text[0] != 'e') || equals == std::string::npos ||
                    !ParseHex(text.substr(1, equals - 1), slotAddress))
                {
                    return false;
                }
                PointerSlotInfo slot;
                slot.slot = (Address_type)slotAddress;
                slot.kind = text[0] == 'e' ? PointerSlotInfo::kind_export : PointerSlotInfo::kind_import;
                slot.name = UnescapeName(text.substr(equals + 1));
                info.AddSlot((Address_type)address, slot);
            }
        }
        return true;
    }

    // the executable pages of the allocation only, with their offsets: data written next to the code
    // (an RW part of the allocation) doesn't make the module stale, a protection change does
    static unsigned long long HashRegion(oui::IProcess& process, Address_type base, Address_type size, Address_type* pHashed)
    {
        unsigned long long hash = 0xcbf29ce484222325ULL;
        Address_type hashed = 0;
        std::vector<unsigned char> buffer(c_readChunk);
        const Address_type end = base + size;
        for (Address_type address = base; address < end && hashed < c_maxHashedSize;)
        {
            oui::MemoryRegionInfo region;
            if (process.QueryMemoryRegion(address, region) || region.allocationBase != base ||
                !region.regionSize || region.regionBase + region.regionSize <= address)
            {
                break;
            }
            const Address_type regionEnd = std::min<Address_type>(end, region.regionBase + region.regionSize);
            if (region.committed && (region.protect & oui::MemoryRegionInfo::protect_exec))
            {
                const unsigned long long offset = address - base;
                HashBytes(hash, (const unsigned char*)&offset, sizeof(offset));
                for (; address < regionEnd && hashed < c_maxHashedSize;)
                {
                    const size_t chunk = (size_t)std::min<Address_type>({ (Address_type)c_readChunk,
                        regionEnd - address, c_maxHashedSize - hashed });
                    if (process.ReadExactEx2(address, buffer.data(), chunk))
                    {
                        // a page that can't be read counts as one
                        const unsigned char marker[] = { 0xFF, 0x00, 0xFF, 0x00 };
                        HashBytes(hash, marker, sizeof(marker));
                    }
                    else
                    {
                        HashBytes(hash, buffer.data(), chunk);
                    }
                    address += chunk;
                    hashed += chunk;
                }
            }
            address = regionEnd;
        }
        if (pHashed)
        {
            *pHashed = hashed;
        }
        return hash;
    }

    void FingerprintFloatingRegion(oui::IProcess& process, FloatingRegionInfo& info)
    {
        info.header.clear();
        unsigned char header[4] = { 0, };
        if (info.allocationSize >= sizeof(header) && !process.ReadExactEx2(info.allocationBase, header, sizeof(header)))
        {
            if (header[0] == 'M' && header[1] == 'Z')
            {
                info.header = "MZ";
            }
            else if (header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F')
            {
                info.header = "ELF";
            }
        }
        info.bytesHash = HashRegion(process, info.allocationBase, info.allocationSize, &info.hashedSize);
    }

    const ORTHIA_TCHAR* const g_floatingSnapshotFolder = ORTHIA_TCSTR("floating");

    bool HasFloatingSnapshots(const PlatformString_type& databaseFolder)
    {
        namespace fs = std::filesystem;
        if (databaseFolder.empty())
        {
            return false;
        }
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(fs::path(databaseFolder) / g_floatingSnapshotFolder, ec))
        {
            std::error_code ec2;
            if (entry.is_regular_file(ec2) && entry.path().extension() == ORTHIA_TCSTR(".bin"))
            {
                return true;
            }
        }
        return false;
    }

    bool SaveFloatingSnapshot(oui::IProcess& process, const PlatformString_type& databaseFolder, FloatingRegionInfo& info)
    {
        namespace fs = std::filesystem;
        if (databaseFolder.empty() || !info.allocationSize)
        {
            return false;
        }
        const size_t size = (size_t)std::min(info.allocationSize, c_maxSnapshotSize);
        std::vector<char> bytes(size);
        Address_type missing = 0;
        for (size_t offset = 0; offset < size; offset += c_readChunk)
        {
            const size_t chunk = std::min(c_readChunk, size - offset);
            if (process.ReadExactEx2(info.allocationBase + offset, bytes.data() + offset, chunk))
            {
                // a partial read may leave some of it filled: the whole chunk counts as missing
                memset(bytes.data() + offset, 0, chunk);
                missing += chunk;
            }
        }
        const std::string sha256 = Sha256Hex(bytes.data(), bytes.size());
        const std::string fileName = ToHex(info.allocationBase) + "_" + sha256.substr(0, 16) + ".bin";
        try
        {
            const fs::path folder = fs::path(databaseFolder) / g_floatingSnapshotFolder;
            fs::create_directories(folder);
            const fs::path path = folder / Utf8ToPlatformString(fileName);
            std::error_code ec;
            // the same bytes are saved already
            if (!fs::exists(path, ec))
            {
                const fs::path temp = folder / Utf8ToPlatformString(fileName + ".tmp");
                {
                    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
                    file.write(bytes.data(), (std::streamsize)bytes.size());
                    file.close();
                    if (!file)
                    {
                        throw std::runtime_error("can't write " + PlatformStringToUtf8(temp.native()));
                    }
                }
                fs::rename(temp, path);
            }
        }
        catch (const std::exception& e)
        {
            ORTHIA_DEV_LOG(orthia::LogSeverity::Warning, std::string("Can't save the floating code snapshot: ") + e.what());
            (void)e;
            return false;
        }
        info.snapshotFile = fileName;
        info.snapshotSize = size;
        info.snapshotMissing = missing;
        info.sha256 = sha256;
        return true;
    }

    bool IsFloatingRegionUnchanged(oui::IProcess& process, const FloatingRegionInfo& info)
    {
        oui::MemoryRegionInfo region;
        if (process.QueryMemoryRegion(info.allocationBase, region) ||
            region.allocationBase != info.allocationBase ||
            region.allocationSize != info.allocationSize)
        {
            return false;
        }
        Address_type hashed = 0;
        return HashRegion(process, info.allocationBase, info.allocationSize, &hashed) == info.bytesHash &&
            hashed == info.hashedSize;
    }

    std::shared_ptr<FloatingRegionInfo> RefreshFloatingRegion(oui::IProcess& process,
        const std::vector<ModuleInfo>& modules,
        const FloatingRegionInfo& info)
    {
        oui::MemoryRegionInfo region;
        if (process.QueryMemoryRegion(info.allocationBase, region) || region.allocationBase != info.allocationBase ||
            !region.allocationSize)
        {
            return nullptr;
        }
        for (const auto& mod : modules)
        {
            if (!mod.IsFloating() && mod.address <= info.allocationBase + region.allocationSize - 1 &&
                info.allocationBase <= mod.lastValidAddress)
            {
                // a module the OS reports is there now
                return nullptr;
            }
        }
        auto refreshed = std::make_shared<FloatingRegionInfo>();
        refreshed->allocationBase = region.allocationBase;
        refreshed->allocationSize = region.allocationSize;
        refreshed->type = region.type;
        refreshed->mappedName = region.mappedName;
        // the entries that still are in executable memory of the allocation
        for (const auto& entry : info.entries)
        {
            oui::MemoryRegionInfo entryRegion;
            if (entry.address - refreshed->allocationBase >= refreshed->allocationSize ||
                process.QueryMemoryRegion(entry.address, entryRegion) ||
                !entryRegion.committed || !(entryRegion.protect & oui::MemoryRegionInfo::protect_exec))
            {
                continue;
            }
            if (refreshed->entries.empty())
            {
                refreshed->protect = entryRegion.protect;
            }
            refreshed->entries.push_back(entry);
        }
        if (refreshed->entries.empty())
        {
            return nullptr;
        }
        FingerprintFloatingRegion(process, *refreshed);
        return refreshed;
    }

    // the tbl_modules row, the references made from it and its metainfo (cascade)
    static void UnloadFloatingRow(CModuleManager& moduleManager, Address_type address)
    {
        if (moduleManager.QueryDatabaseManager()->GetClassicDatabase()->IsModuleExists(address))
        {
            moduleManager.UnloadModule(address);
        }
    }

    void RemoveFloatingModule(IWorkPlaceItem& item, Address_type address)
    {
        if (auto moduleManager = item.GetModuleManager())
        {
            UnloadFloatingRow(*moduleManager, address);
        }
        item.RemoveFloatingModule(address);
    }

    void ReplaceFloatingRegion(IWorkPlaceItem& item, std::shared_ptr<FloatingRegionInfo> info)
    {
        auto moduleManager = item.GetModuleManager();
        if (!moduleManager || !info)
        {
            return;
        }
        // the bytes as they are now; the snapshot of the old ones stays
        if (auto process = item.GetAssociatedProcess())
        {
            SaveFloatingSnapshot(*process, item.GetDatabaseFolder(), *info);
        }
        // the allocation may have another size now: a new row, not the old one
        UnloadFloatingRow(*moduleManager, info->allocationBase);
        SaveFloatingRegion(*moduleManager, *info);
        item.SetFloatingModule(info);
        item.UpdateModuleFlags(info->allocationBase, 0, ModuleInfo::flags_stale | ModuleInfo::flags_analyzeDone);
    }

    FloatingRefresh RefreshStaleFloatingModule(IWorkPlaceItem& item, ModuleInfo& module)
    {
        auto process = item.GetAssociatedProcess();
        if (!process || !module.floating || !item.GetModuleManager())
        {
            return FloatingRefresh::unavailable;
        }
        std::vector<ModuleInfo> modules;
        item.GetModules(modules);
        auto refreshed = RefreshFloatingRegion(*process, modules, *module.floating);
        if (!refreshed)
        {
            RemoveFloatingModule(item, module.address);
            return FloatingRefresh::removed;
        }
        ReplaceFloatingRegion(item, refreshed);
        module = MakeFloatingModule(refreshed, module.dianaMode, module.dianaArch);
        return FloatingRefresh::refreshed;
    }

    void QueryFloatingNames(const FloatingRegionInfo& info, std::vector<NameInfo>& names)
    {
        int index = 0;
        for (const auto& entry : info.entries)
        {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "$entry_%04d", ++index);
            NameInfo name;
            name.flags = NameInfo::flags_Export;
            name.address = entry.address;
            name.name = oui::String(Utf8ToPlatformString(buffer));
            names.push_back(std::move(name));
        }
    }

    FloatingDiscovery CollectFloatingRegions(CModuleManager& moduleManager,
        oui::IProcess& process,
        const std::vector<ModuleInfo>& modules,
        const ModuleInfo& source,
        const std::vector<PointerSlotInfo>& slots)
    {
        FloatingDiscovery result;
        if (!source.size)
        {
            return result;
        }
        // calls, jumps and taken addresses; data read or written there is no entry point.
        // TODO: an immediate the analysis takes for an address is a taken address too, so a 32-bit
        // constant that falls into an allocation still adds an entry. Other entry points (threads,
        // stacks, pointers in data) are not looked at (docs/orthia.md, Floating code)
        std::vector<ModuleReference> refs;
        moduleManager.QueryDatabaseManager()->GetClassicDatabase()->QueryAllModuleReferences(source.address, source.size, refs);

        std::vector<ModuleInfo> sorted(modules);
        std::sort(sorted.begin(), sorted.end(), [](const ModuleInfo& a, const ModuleInfo& b) { return a.address < b.address; });

        // by allocation base: copies of the known regions that got references, and the new ones
        std::map<Address_type, std::shared_ptr<FloatingRegionInfo>> regions;
        std::set<Address_type> changed;
        std::set<Address_type> toAnalyze;
        std::set<Address_type> created;
        // what the OS said no to: [base, base + size)
        std::vector<std::pair<Address_type, Address_type>> rejected;
        bool capReported = false;

        auto findNew = [&](Address_type address) -> std::shared_ptr<FloatingRegionInfo> {
            auto it = regions.upper_bound(address);
            if (it == regions.begin())
            {
                return nullptr;
            }
            --it;
            const auto& info = it->second;
            return address - info->allocationBase < info->allocationSize ? info : nullptr;
        };
        // from: an instruction of source, or a slot of it (slot != nullptr)
        auto addReference = [&](const std::shared_ptr<FloatingRegionInfo>& info, Address_type to, Address_type from,
            const PointerSlotInfo* slot, int kind) {
            bool newEntry = false;
            if (slot ? info->AddSlot(to, *slot, &newEntry) : info->AddReference(to, from, &newEntry, kind))
            {
                changed.insert(info->allocationBase);
            }
            if (newEntry)
            {
                toAnalyze.insert(info->allocationBase);
            }
        };

        auto consider = [&](Address_type from, Address_type to, const PointerSlotInfo* slot, int kind) {
            // only what leaves the module
            if (!to || kind == reference_data || !source.IsInRange(from) || source.IsInRange(to))
            {
                return;
            }
            const ModuleInfo* owner = FindModule(sorted, to);
            if (owner && !(owner->IsFloating() && owner->floating))
            {
                return;
            }
            if (IsZeroFilled(process, to))
            {
                return;
            }
            if (owner)
            {
                auto& info = regions[owner->address];
                if (!info)
                {
                    info = std::make_shared<FloatingRegionInfo>(*owner->floating);
                }
                addReference(info, to, from, slot, kind);
                return;
            }
            if (auto info = findNew(to))
            {
                addReference(info, to, from, slot, kind);
                return;
            }
            if (std::any_of(rejected.begin(), rejected.end(),
                [to](const std::pair<Address_type, Address_type>& r) { return to - r.first < r.second; }))
            {
                return;
            }
            oui::MemoryRegionInfo region;
            if (process.QueryMemoryRegion(to, region) || !region.allocationSize)
            {
                rejected.push_back({ to & ~(Address_type)0xFFF, 0x1000 });
                return;
            }
            if (!region.committed || !(region.protect & oui::MemoryRegionInfo::protect_exec) ||
                OverlapsModule(sorted, region.allocationBase, region.allocationSize))
            {
                rejected.push_back({ region.regionBase, region.regionSize });
                return;
            }
            if (created.size() >= c_maxNewRegions)
            {
                if (!capReported)
                {
                    capReported = true;
                    ORTHIA_DEV_LOG(orthia::LogSeverity::Warning, std::string("Too many floating regions, the rest is skipped"));
                }
                return;
            }
            auto info = std::make_shared<FloatingRegionInfo>();
            info->allocationBase = region.allocationBase;
            info->allocationSize = region.allocationSize;
            info->protect = region.protect;
            info->type = region.type;
            info->mappedName = region.mappedName;
            regions[info->allocationBase] = info;
            created.insert(info->allocationBase);
            addReference(info, to, from, slot, kind);
        };

        for (const auto& ref : refs)
        {
            consider(ref.from, ref.to, nullptr, ref.kind);
        }
        // a slot that leads out of the module into memory no module owns: an import or export hook
        for (const auto& slot : slots)
        {
            consider(slot.slot, slot.value, &slot, reference_unknown);
        }

        for (auto base : created)
        {
            FingerprintFloatingRegion(process, *regions[base]);
        }
        for (auto base : changed)
        {
            result.changed.push_back(regions[base]);
        }
        result.toAnalyze.assign(toAnalyze.begin(), toAnalyze.end());
        return result;
    }

    std::vector<PointerSlotInfo> QueryModulePointerSlots(oui::IProcess& process, const ModuleInfo& module)
    {
        std::vector<PointerSlotInfo> slots;
        if (module.IsFloating() || !module.size)
        {
            return slots;
        }
        try
        {
            const auto* format = FindFormatByModule(module);
            ProcessReaderAdapter reader(&process);
            if (auto image = format ? format->OpenModuleImage(reader, module.address, module.size) : nullptr)
            {
                image->QueryPointerSlots(slots);
            }
        }
        catch (const std::exception& e)
        {
            ORTHIA_DEV_LOG(orthia::LogSeverity::Debug, std::string("QueryModulePointerSlots: ") + e.what());
            (void)e;
        }
        return slots;
    }

    void SaveFloatingRegion(CModuleManager& moduleManager, const FloatingRegionInfo& info)
    {
        moduleManager.RegisterRange(info.allocationBase, info.allocationSize, MakeFloatingModuleName(info.allocationBase));
        moduleManager.QueryDatabaseManager()->GetClassicDatabase()->InsertMetaInfo(info.allocationBase,
            g_database_type_floatingRegion, FormatFloatingRegion(info), info.allocationBase, true);
    }

    std::vector<std::shared_ptr<FloatingRegionInfo>> LoadFloatingRegions(CModuleManager& moduleManager)
    {
        std::vector<std::shared_ptr<FloatingRegionInfo>> result;
        auto database = moduleManager.QueryDatabaseManager()->GetClassicDatabase();
        database->QueryMetaInfo(g_database_type_floatingRegion,
            [&](Address_type moduleAddress, int, const std::string& text, Address_type)
        {
            auto info = std::make_shared<FloatingRegionInfo>();
            if (ParseFloatingRegion(text, *info) && info->allocationBase == moduleAddress)
            {
                result.push_back(info);
            }
            return true;
        });
        return result;
    }

    // "import slot: KERNEL32.dll!CreateFileW", "export: NtClose"
    static PlatformString_type DescribeSlot(const PointerSlotInfo& slot)
    {
        return (slot.kind == PointerSlotInfo::kind_export ? PlatformString_type(ORTHIA_TCSTR("export: ")) :
            PlatformString_type(ORTHIA_TCSTR("import slot: "))) + Utf8ToPlatformString(slot.name);
    }

    // a slot is data: "module+2f020h", not the nearest symbol
    static PlatformString_type NameSlot(const std::vector<ModuleInfo>& modules, Address_type slot, int mode)
    {
        for (const auto& mod : modules)
        {
            if (!mod.IsFloating() && mod.IsInRange(slot))
            {
                return ComposeName(oui::String(mod.name), mod.address, slot).native;
            }
        }
        return AddressToString(slot, mode);
    }

    void DescribeFloatingModule(const IWorkPlaceItem& item, const ModuleInfo& module, std::vector<PlatformString_type>& lines)
    {
        if (!module.floating)
        {
            return;
        }
        const auto& info = *module.floating;
        const int mode = module.dianaMode;
        PlatformString_type line = ORTHIA_TCSTR("Floating code: ") + FormatRegionProtect(info.protect) +
            ORTHIA_TCSTR(", ") + FormatRegionType(info.type);
        if (!info.mappedName.empty())
        {
            line += ORTHIA_TCSTR(", ") + Utf8ToPlatformString(info.mappedName);
        }
        lines.push_back(line);
        lines.push_back(ORTHIA_TCSTR("Allocation: ") + AddressToString(info.allocationBase, mode) + ORTHIA_TCSTR(" - ") +
            AddressToString(info.allocationBase + info.allocationSize, mode) +
            ORTHIA_TCSTR(" (") + Utf8ToPlatformString(ToHex(info.allocationSize)) + ORTHIA_TCSTR(")"));
        if (!info.header.empty())
        {
            lines.push_back(ORTHIA_TCSTR("Header: ") + Utf8ToPlatformString(info.header));
        }
        if (module.flags & ModuleInfo::flags_analyzeDone)
        {
            lines.push_back(info.codeStart || info.codeLast ?
                ORTHIA_TCSTR("Code: ") + AddressToString(info.codeStart, mode) + ORTHIA_TCSTR(" - ") + AddressToString(info.codeLast, mode) :
                PlatformString_type(ORTHIA_TCSTR("Code: none found")));
        }
        else
        {
            lines.push_back(ORTHIA_TCSTR("Code: not analyzed"));
        }
        lines.push_back(ORTHIA_TCSTR("Bytes hash: ") + Utf8ToPlatformString(ToHex(info.bytesHash)) +
            ORTHIA_TCSTR(" (") + Utf8ToPlatformString(ToHex(info.hashedSize)) + ORTHIA_TCSTR(" bytes)"));
        if (!info.snapshotFile.empty())
        {
            const auto folder = item.GetDatabaseFolder();
            const auto path = folder.empty() ? std::filesystem::path(Utf8ToPlatformString(info.snapshotFile)) :
                std::filesystem::path(folder) / g_floatingSnapshotFolder / Utf8ToPlatformString(info.snapshotFile);
            PlatformString_type snapshot = ORTHIA_TCSTR("Snapshot: ") + path.native() +
                ORTHIA_TCSTR(" (") + Utf8ToPlatformString(ToHex(info.snapshotSize)) + ORTHIA_TCSTR(" bytes");
            if (info.snapshotMissing)
            {
                snapshot += ORTHIA_TCSTR(", ") + Utf8ToPlatformString(ToHex(info.snapshotMissing)) +
                    ORTHIA_TCSTR(" unreadable, zero filled");
            }
            lines.push_back(snapshot + ORTHIA_TCSTR(")"));
            lines.push_back(ORTHIA_TCSTR("SHA-256: ") + Utf8ToPlatformString(info.sha256));
        }
        else
        {
            lines.push_back(ORTHIA_TCSTR("Snapshot: none"));
        }
        // the region in a capture of the process (.capture, --capture): the memory later than the snapshot
        bool captured = false;
        CaptureIndex capture;
        const auto folder = item.GetDatabaseFolder();
        if (!folder.empty() && ReadCaptureIndex(folder, capture))
        {
            for (const auto& region : capture.regions)
            {
                if (region.file.empty() || region.allocationBase != info.allocationBase)
                {
                    continue;
                }
                const auto path = std::filesystem::path(folder) / g_captureFolder / Utf8ToPlatformString(region.file);
                lines.push_back(ORTHIA_TCSTR("Capture: ") + path.native() + ORTHIA_TCSTR(" (") +
                    AddressToString(region.base, mode) + ORTHIA_TCSTR(", ") +
                    Utf8ToPlatformString(ToHex(region.size)) + ORTHIA_TCSTR(" bytes)"));
                captured = true;
            }
        }
        if (module.flags & ModuleInfo::flags_stale)
        {
            auto process = const_cast<IWorkPlaceItem&>(item).GetAssociatedProcess();
            const bool savedView = process && !process->IsRunning() && captured;
            lines.push_back(savedView ?
                PlatformString_type(ORTHIA_TCSTR("State: stale, the captured memory differs from what was found and analyzed (the snapshot has that)")) :
                PlatformString_type(ORTHIA_TCSTR("State: stale, the memory changed since it was found")));
        }
        lines.push_back(ORTHIA_TCSTR("Entries: ") + Utf8ToPlatformString(std::to_string(info.entries.size())));
        std::vector<ModuleInfo> modules;
        item.GetModules(modules);
        std::vector<NameInfo> names;
        QueryFloatingNames(info, names);
        for (size_t i = 0; i < info.entries.size(); ++i)
        {
            const auto& entry = info.entries[i];
            line = ORTHIA_TCSTR("  ") + AddressToString(entry.address, mode) + ORTHIA_TCSTR("  ") + names[i].name.native;
            bool first = true;
            for (size_t r = 0; r < entry.referrers.size(); ++r)
            {
                line += first ? ORTHIA_TCSTR("  from ") : ORTHIA_TCSTR(", ");
                first = false;
                line += QueryAddressNameDef(&item, entry.referrers[r], mode).native;
                const auto kind = DescribeReferenceKind(entry, r);
                if (!kind.empty())
                {
                    line += ORTHIA_TCSTR(" (") + kind + ORTHIA_TCSTR(")");
                }
            }
            for (const auto& slot : entry.slots)
            {
                line += first ? ORTHIA_TCSTR("  from ") : ORTHIA_TCSTR(", ");
                first = false;
                line += NameSlot(modules, slot.slot, mode) + ORTHIA_TCSTR(" (") + DescribeSlot(slot) + ORTHIA_TCSTR(")");
            }
            lines.push_back(line);
        }
    }

    void DescribeFloatingReferrers(const IWorkPlaceItem& item, std::vector<PlatformString_type>& lines)
    {
        std::vector<ModuleInfo> modules;
        item.GetModules(modules);
        struct Row
        {
            Address_type referrer = 0;
            PlatformString_type from;
            PlatformString_type to;
            PlatformString_type notes;
        };
        std::vector<Row> rows;
        const int mode = item.GetDianaMode();
        for (const auto& module : modules)
        {
            if (!module.IsFloating() || !module.floating)
            {
                continue;
            }
            const auto& info = *module.floating;
            // what the region is: the same for each of its rows
            PlatformString_type regionNotes = FormatRegionProtect(info.protect) + ORTHIA_TCSTR(" ") + FormatRegionType(info.type);
            if (!info.header.empty())
            {
                regionNotes += ORTHIA_TCSTR(" ") + Utf8ToPlatformString(info.header) + ORTHIA_TCSTR("-header");
            }
            if (!info.mappedName.empty())
            {
                regionNotes += ORTHIA_TCSTR(" ") + Utf8ToPlatformString(info.mappedName);
            }
            if (module.flags & ModuleInfo::flags_stale)
            {
                regionNotes += ORTHIA_TCSTR(" stale");
            }
            std::vector<NameInfo> names;
            QueryFloatingNames(info, names);
            for (size_t i = 0; i < info.entries.size(); ++i)
            {
                for (size_t r = 0; r < info.entries[i].referrers.size(); ++r)
                {
                    const auto referrer = info.entries[i].referrers[r];
                    Row row;
                    row.referrer = referrer;
                    const auto name = GetPreferredName(item.QueryAddressName(referrer)).native;
                    row.from = name.empty() ? AddressToString(referrer, mode) : name;
                    row.to = module.name + ORTHIA_TCSTR("!") + names[i].name.native;
                    row.notes = regionNotes;
                    if (!name.empty() && name.find(ORTHIA_TCHAR('+')) == PlatformString_type::npos)
                    {
                        row.notes += ORTHIA_TCSTR(" [function start]");
                    }
                    const auto kind = DescribeReferenceKind(info.entries[i], r);
                    if (!kind.empty())
                    {
                        row.notes += ORTHIA_TCSTR(" [") + kind + ORTHIA_TCSTR("]");
                    }
                    rows.push_back(std::move(row));
                }
                for (const auto& slot : info.entries[i].slots)
                {
                    Row row;
                    row.referrer = slot.slot;
                    row.from = NameSlot(modules, slot.slot, mode);
                    row.to = module.name + ORTHIA_TCSTR("!") + names[i].name.native;
                    row.notes = regionNotes + ORTHIA_TCSTR(" [") + DescribeSlot(slot) + ORTHIA_TCSTR("]");
                    rows.push_back(std::move(row));
                }
            }
        }
        std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.referrer < b.referrer; });
        size_t fromWidth = 0;
        size_t toWidth = 0;
        for (const auto& row : rows)
        {
            fromWidth = std::max(fromWidth, row.from.size());
            toWidth = std::max(toWidth, row.to.size());
        }
        lines.push_back(rows.empty() ? PlatformString_type(ORTHIA_TCSTR("Referrers: none")) :
            PlatformString_type(ORTHIA_TCSTR("Referrers:")));
        for (auto& row : rows)
        {
            row.from.resize(fromWidth, ORTHIA_TCHAR(' '));
            row.to.resize(toWidth, ORTHIA_TCHAR(' '));
            lines.push_back(ORTHIA_TCSTR("  ") + row.from + ORTHIA_TCSTR("  -> ") + row.to + ORTHIA_TCSTR("  ") + row.notes);
        }
    }

    PlatformString_type FormatFloatingSummary(const IWorkPlaceItem& item)
    {
        std::vector<ModuleInfo> modules;
        item.GetModules(modules);
        size_t count = 0;
        size_t referrers = 0;
        size_t stale = 0;
        for (const auto& module : modules)
        {
            if (!module.IsFloating() || !module.floating)
            {
                continue;
            }
            ++count;
            if (module.flags & ModuleInfo::flags_stale)
            {
                ++stale;
            }
            for (const auto& entry : module.floating->entries)
            {
                referrers += entry.referrers.size() + entry.slots.size();
            }
        }
        if (!count)
        {
            return PlatformString_type();
        }
        auto plural = [](size_t n, const char* one, const char* many) {
            return std::to_string(n) + " " + (n == 1 ? one : many);
        };
        std::string text = "Floating code: " + plural(count, "module", "modules") + ", " +
            plural(referrers, "referrer", "referrers");
        if (stale)
        {
            text += ", " + std::to_string(stale) + " stale";
        }
        text += " (lmf /v lists them)";
        return Utf8ToPlatformString(text);
    }

    bool DiscoverFloatingCode(IWorkPlaceItem& item, const ModuleInfo& source)
    {
        auto process = item.GetAssociatedProcess();
        auto moduleManager = item.GetModuleManager();
        if (!process || !moduleManager)
        {
            return false;
        }
        std::vector<ModuleInfo> modules;
        item.GetModules(modules);
        auto found = CollectFloatingRegions(*moduleManager, *process, modules, source,
            QueryModulePointerSlots(*process, source));
        const auto databaseFolder = item.GetDatabaseFolder();
        for (const auto& info : found.changed)
        {
            // the bytes as they are when found: a new region, or one saved before snapshots were
            if (info->snapshotFile.empty())
            {
                SaveFloatingSnapshot(*process, databaseFolder, *info);
            }
            SaveFloatingRegion(*moduleManager, *info);
            item.SetFloatingModule(info);
        }
        for (auto address : found.toAnalyze)
        {
            item.UpdateModuleFlags(address, 0, ModuleInfo::flags_analyzeDone);
        }
        return !QueryPendingFloatingModules(item).empty();
    }

    void AnalyzeFloatingModule(IWorkPlaceItem& item, const ModuleInfo& module)
    {
        auto moduleManager = item.GetModuleManager();
        auto reader = item.CreateMemoryReader();
        if (!moduleManager || !reader || !module.floating)
        {
            return;
        }
        const auto& info = *module.floating;
        std::vector<Address_type> entries;
        for (const auto& entry : info.entries)
        {
            entries.push_back(entry.address);
        }
        // a big allocation: the part from the first entry on.
        // TODO: ARM32 saves its code map (metainfo type 5) under start, which has no tbl_modules row
        // when start is not the allocation base; the foreign key refuses it
        Address_type start = info.allocationBase;
        Address_type size = info.allocationSize;
        if (size > c_maxAnalyzedSize && !entries.empty())
        {
            start = std::max(info.allocationBase, entries.front() & ~(Address_type)0xFFFF);
            size = std::min(c_maxAnalyzedSize, info.allocationBase + info.allocationSize - start);
        }
        else if (size > c_maxAnalyzedSize)
        {
            size = c_maxAnalyzedSize;
        }
        const auto extent = moduleManager->AnalyzeRange(start, size, reader.get(), module.dianaMode, module.dianaArch,
            0, entries, module.name);

        auto updated = std::make_shared<FloatingRegionInfo>(info);
        updated->codeStart = extent.first;
        updated->codeLast = extent.second;
        SaveFloatingRegion(*moduleManager, *updated);
        item.SetFloatingModule(updated);
        item.UpdateModuleFlags(module.address, ModuleInfo::flags_analyzeDone, 0);
        item.InvalidateNames(module.address);
    }

    std::vector<ModuleInfo> QueryPendingFloatingModules(const IWorkPlaceItem& item)
    {
        std::vector<ModuleInfo> modules;
        item.GetModules(modules);
        std::vector<ModuleInfo> pending;
        for (auto& mod : modules)
        {
            if (mod.IsFloating() && !(mod.flags & (ModuleInfo::flags_analyzeDone | ModuleInfo::flags_stale)))
            {
                pending.push_back(std::move(mod));
            }
        }
        return pending;
    }
}
