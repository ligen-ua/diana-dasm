#include "orthia_floating_code.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include "orthia_common_format.h"
#include "orthia_common_print.h"
#include "orthia_log.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>

namespace orthia
{
    namespace
    {
        // the most a pass registers: the references of one module can't flood the process
        const size_t c_maxNewRegions = 1024;
        // the bytes hashed of an allocation, and the most of it one analysis looks at: the analyzer
        // keeps a few pointers per byte, a JIT heap can reserve gigabytes
        const Address_type c_maxHashedSize = 16 * 1024 * 1024;
        const Address_type c_maxAnalyzedSize = 16 * 1024 * 1024;
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

    bool FloatingRegionInfo::AddReference(Address_type entry, Address_type referrer, bool* pNewEntry)
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
        auto refIt = std::lower_bound(referrers.begin(), referrers.end(), referrer);
        if (refIt != referrers.end() && *refIt == referrer)
        {
            // a new entry has no referrers yet: nothing changed
            return false;
        }
        referrers.insert(refIt, referrer);
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
        // entries: "<entry>:<referrer>,<referrer>;<entry>:..." in hex
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
            if (colon != std::string::npos)
            {
                for (const auto& ref : Split(item.substr(colon + 1), ','))
                {
                    unsigned long long referrer = 0;
                    if (!ParseHex(ref, referrer))
                    {
                        return false;
                    }
                    entry.referrers.push_back((Address_type)referrer);
                }
            }
            std::sort(entry.referrers.begin(), entry.referrers.end());
            info.entries.push_back(entry);
        }
        std::sort(info.entries.begin(), info.entries.end(),
            [](const FloatingEntryInfo& a, const FloatingEntryInfo& b) { return a.address < b.address; });
        return true;
    }

    static unsigned long long HashRegion(oui::IProcess& process, Address_type base, Address_type size, Address_type* pHashed)
    {
        unsigned long long hash = 0xcbf29ce484222325ULL;
        const Address_type hashed = std::min(size, c_maxHashedSize);
        std::vector<unsigned char> buffer(c_readChunk);
        for (Address_type offset = 0; offset < hashed; offset += c_readChunk)
        {
            const size_t chunk = (size_t)std::min<Address_type>(c_readChunk, hashed - offset);
            if (process.ReadExactEx2(base + offset, buffer.data(), chunk))
            {
                // a page that can't be read counts as one
                const unsigned char marker[] = { 0xFF, 0x00, 0xFF, 0x00 };
                HashBytes(hash, marker, sizeof(marker));
                continue;
            }
            HashBytes(hash, buffer.data(), chunk);
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
        return HashRegion(process, info.allocationBase, info.hashedSize, &hashed) == info.bytesHash;
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
        const ModuleInfo& source)
    {
        FloatingDiscovery result;
        if (!source.size)
        {
            return result;
        }
        std::vector<std::pair<Address_type, Address_type>> refs;
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
        auto addReference = [&](const std::shared_ptr<FloatingRegionInfo>& info, Address_type to, Address_type from) {
            bool newEntry = false;
            if (info->AddReference(to, from, &newEntry))
            {
                changed.insert(info->allocationBase);
            }
            if (newEntry)
            {
                toAnalyze.insert(info->allocationBase);
            }
        };

        for (const auto& ref : refs)
        {
            const Address_type from = ref.first;
            const Address_type to = ref.second;
            // the range query includes the first byte after the module
            if (!to || !source.IsInRange(from) || source.IsInRange(to))
            {
                continue;
            }
            const ModuleInfo* owner = FindModule(sorted, to);
            if (owner && !(owner->IsFloating() && owner->floating))
            {
                continue;
            }
            if (IsZeroFilled(process, to))
            {
                continue;
            }
            if (owner)
            {
                auto& info = regions[owner->address];
                if (!info)
                {
                    info = std::make_shared<FloatingRegionInfo>(*owner->floating);
                }
                addReference(info, to, from);
                continue;
            }
            if (auto info = findNew(to))
            {
                addReference(info, to, from);
                continue;
            }
            if (std::any_of(rejected.begin(), rejected.end(),
                [to](const std::pair<Address_type, Address_type>& r) { return to - r.first < r.second; }))
            {
                continue;
            }
            oui::MemoryRegionInfo region;
            if (process.QueryMemoryRegion(to, region) || !region.allocationSize)
            {
                rejected.push_back({ to & ~(Address_type)0xFFF, 0x1000 });
                continue;
            }
            if (!region.committed || !(region.protect & oui::MemoryRegionInfo::protect_exec) ||
                OverlapsModule(sorted, region.allocationBase, region.allocationSize))
            {
                rejected.push_back({ region.regionBase, region.regionSize });
                continue;
            }
            if (created.size() >= c_maxNewRegions)
            {
                if (!capReported)
                {
                    capReported = true;
                    ORTHIA_DEV_LOG(orthia::LogSeverity::Warning, std::string("Too many floating regions, the rest is skipped"));
                }
                continue;
            }
            auto info = std::make_shared<FloatingRegionInfo>();
            info->allocationBase = region.allocationBase;
            info->allocationSize = region.allocationSize;
            info->protect = region.protect;
            info->type = region.type;
            info->mappedName = region.mappedName;
            regions[info->allocationBase] = info;
            created.insert(info->allocationBase);
            addReference(info, to, from);
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
        if (module.flags & ModuleInfo::flags_stale)
        {
            lines.push_back(ORTHIA_TCSTR("State: stale, the memory changed since it was found"));
        }
        lines.push_back(ORTHIA_TCSTR("Entries: ") + Utf8ToPlatformString(std::to_string(info.entries.size())));
        std::vector<NameInfo> names;
        QueryFloatingNames(info, names);
        for (size_t i = 0; i < info.entries.size(); ++i)
        {
            const auto& entry = info.entries[i];
            line = ORTHIA_TCSTR("  ") + AddressToString(entry.address, mode) + ORTHIA_TCSTR("  ") + names[i].name.native;
            for (size_t r = 0; r < entry.referrers.size(); ++r)
            {
                line += r ? ORTHIA_TCSTR(", ") : ORTHIA_TCSTR("  from ");
                line += QueryAddressNameDef(&item, entry.referrers[r], mode).native;
            }
            lines.push_back(line);
        }
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
        auto found = CollectFloatingRegions(*moduleManager, *process, modules, source);
        for (const auto& info : found.changed)
        {
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
        // a big allocation: the part from the first entry on
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
