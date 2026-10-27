#pragma once

// Fakes shared by the UI tests (ui_tests*.cpp)

#include "gtest/gtest.h"
#include "orthia_utils.h"
#include "formats/orthia_image_format.h"
#include "orthia_process_adapter.h"
#include "oui_processes.h"
#include "orthia_item_process.h"
#include "orthia_module_manager.h"
#include "orthia_floating_code.h"
#include "orthia_databases.h"
#include "orthia_saved_process.h"
extern "C"
{
#include "diana_core.h"
}
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace orthia_tests
{
    // A process made of images that the formats mapped: what the OS layer of a live process reports,
    // for machines this host can't run. The modules are opened as both OS layers open them
    class CFakeProcess : public oui::IProcess
    {
    public:
        struct Image
        {
            const orthia::IImageFormat* format = nullptr;
            orthia::Address_type base = 0;
            std::vector<char> data;
            orthia::PlatformString_type name = ORTHIA_TCSTR("module");
            orthia::PlatformString_type fullName;
        };
        // memory no module owns: heap, JIT, injected code
        struct Region
        {
            orthia::Address_type base = 0;
            std::vector<char> data;
            int protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_write |
                          oui::MemoryRegionInfo::protect_exec;
            // a part of the allocation that starts there (0: its own allocation)
            orthia::Address_type allocationBase = 0;
        };
        int dianaMode = DIANA_MODE64;
        int dianaArch = DIANA_ARCH_X86;
        std::vector<Image> images;
        std::vector<Region> regions;

        int QueryMemoryRegion(unsigned long long address, oui::MemoryRegionInfo& info) override
        {
            info = oui::MemoryRegionInfo();
            auto fill = [&](orthia::Address_type base, size_t size, int protect, int type) {
                info.allocationBase = info.regionBase = base;
                info.allocationSize = info.regionSize = size;
                info.protect = protect;
                info.committed = true;
                info.type = type;
            };
            for (const auto& region : regions)
            {
                if (address >= region.base && address - region.base < region.data.size())
                {
                    fill(region.base, region.data.size(), region.protect, oui::MemoryRegionInfo::type_private);
                    if (region.allocationBase)
                    {
                        // the allocation: every part with the same allocation base
                        info.allocationBase = region.allocationBase;
                        orthia::Address_type end = region.allocationBase;
                        for (const auto& part : regions)
                        {
                            if (part.allocationBase == region.allocationBase)
                            {
                                end = std::max<orthia::Address_type>(end, part.base + part.data.size());
                            }
                        }
                        info.allocationSize = end - region.allocationBase;
                    }
                    return 0;
                }
            }
            for (const auto& image : images)
            {
                if (address >= image.base && address - image.base < image.data.size())
                {
                    fill(image.base, image.data.size(), oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_exec,
                         oui::MemoryRegionInfo::type_image);
                    return 0;
                }
            }
            return 1;
        }

        int EnumMemoryRegions(std::function<bool(const oui::MemoryRegionInfo& info)> callback) override
        {
            std::vector<oui::MemoryRegionInfo> all;
            for (const auto& region : regions)
            {
                oui::MemoryRegionInfo info;
                info.allocationBase = region.allocationBase ? region.allocationBase : region.base;
                info.regionBase = region.base;
                info.regionSize = region.data.size();
                info.protect = region.protect;
                info.committed = true;
                info.type = oui::MemoryRegionInfo::type_private;
                all.push_back(info);
            }
            for (const auto& image : images)
            {
                oui::MemoryRegionInfo info;
                info.allocationBase = info.regionBase = image.base;
                info.regionSize = image.data.size();
                info.protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_exec;
                info.committed = true;
                info.type = oui::MemoryRegionInfo::type_image;
                all.push_back(info);
            }
            std::sort(all.begin(), all.end(), [](const oui::MemoryRegionInfo& a, const oui::MemoryRegionInfo& b) { return a.regionBase < b.regionBase; });
            for (const auto& info : all)
            {
                if (!callback(info))
                    break;
            }
            return 0;
        }

        std::tuple<int, unsigned long long> GetSizeInBytes() const override
        {
            return std::make_tuple(0, dianaMode == DIANA_MODE32 ? 0xFFFFFFFFull : ~0ull);
        }
        int ReadExact(std::shared_ptr<oui::BaseOperation>, unsigned long long offset, size_t size, std::vector<char>& data) override
        {
            data.resize(size);
            return ReadExactEx2(offset, data.data(), size);
        }
        oui::String GetFullFileName() const override
        {
            oui::String name;
            name.native = OUI_TCSTR("[1] fake");
            return name;
        }
        oui::String GetFullFileNameForUI() const override
        {
            return GetFullFileName();
        }
        int ReadExactEx2(unsigned long long offset, void* pBuffer, size_t size) override
        {
            memset(pBuffer, 0, size);
            bool found = false;
            auto copy = [&](orthia::Address_type base, const std::vector<char>& data) {
                const unsigned long long dataEnd = base + data.size();
                const unsigned long long start = std::max<unsigned long long>(offset, base);
                const unsigned long long end = std::min<unsigned long long>(offset + size, dataEnd);
                if (start < end)
                {
                    memcpy((char*)pBuffer + (start - offset), data.data() + (start - base), (size_t)(end - start));
                    found = true;
                }
            };
            for (const auto& image : images)
            {
                copy(image.base, image.data);
            }
            for (const auto& region : regions)
            {
                copy(region.base, region.data);
            }
            return found ? 0 : 1;
        }
        orthia::WorkAddressData ReadExactEx(unsigned long long offset, size_t size) override
        {
            std::vector<char> buffer(size);
            if (ReadExactEx2(offset, buffer.data(), size))
            {
                return orthia::WorkAddressData();
            }
            auto* pBufferStart = buffer.data();
            return orthia::WorkAddressData(pBufferStart, size, nullptr, orthia::WorkAddressData::flags_FullValid,
                [buffer = std::move(buffer)](orthia::WorkAddressData*) mutable {});
        }
        int QueryModules(std::vector<orthia::ModuleInfo>& modules, int& processModuleOffset,
            std::function<void(const orthia::ModuleInfo& info, oui::ModuleDisasmContext&)> contextCallback) override
        {
            processModuleOffset = 0;
            modules.clear();
            orthia::ProcessReaderAdapter reader(this);
            for (const auto& image : images)
            {
                orthia::ModuleInfo info;
                info.name = image.name;
                info.fullName = image.fullName;
                info.address = image.base;
                info.size = image.data.size();
                info.lastValidAddress = image.base + image.data.size() - 1;
                if (auto moduleImage = image.format->OpenModuleImage(reader, image.base, image.data.size()))
                {
                    const auto range = moduleImage->GetRange();
                    info.dianaMode = range.dianaMode;
                    info.dianaArch = range.dianaArch;
                    info.entryPoint = range.entryPoint ? range.entryPoint : info.address;
                    info.builtInFlags |= image.format->GetBuiltInFlag() | moduleImage->GetExtraBuiltInFlags();
                    oui::ModuleDisasmContext context;
                    context.image = moduleImage.get();
                    contextCallback(info, context);
                }
                modules.push_back(info);
            }
            return 0;
        }
        std::tuple<int, std::shared_ptr<oui::IThreadEnumerator>> CreateThreadEnumerator() override
        {
            return std::make_tuple(1, std::shared_ptr<oui::IThreadEnumerator>());
        }
        int GetDianaMode() const override
        {
            return dianaMode;
        }
        int GetDianaArch() const override
        {
            return dianaArch;
        }
        oui::ProcessStartInfo GetStartInfo() const override
        {
            return oui::ProcessStartInfo();
        }
        bool IsRunning() const override
        {
            return false;
        }
    };

    // e8/e9 rel32 at offset of code placed at base
    inline void PutRel32(std::vector<char>& code, size_t offset, unsigned char opcode, orthia::Address_type base, orthia::Address_type target)
    {
        code[offset] = (char)opcode;
        const int rel = (int)(long long)(target - (base + offset + 5));
        memcpy(&code[offset + 1], &rel, sizeof(rel));
    }

    // A process with one module (raw code) that calls into memory no module owns
    struct FloatingFixture
    {
        static constexpr orthia::Address_type moduleBase = 0x10000000;
        static constexpr orthia::Address_type regionA = 0x10100000;     // RWX, called twice
        static constexpr orthia::Address_type otherModule = 0x10200000;
        static constexpr orthia::Address_type regionData = 0x10300000;  // RW-
        static constexpr orthia::Address_type regionD = 0x10400000;     // RWX, A jumps there

        std::filesystem::path root;
        // a process folder (meta.xml, data.db, modules.xml) that live items write and a saved view opens
        bool processFolder = false;
        std::shared_ptr<CFakeProcess> process = std::make_shared<CFakeProcess>();
        std::shared_ptr<orthia::CModuleManager> moduleManager = std::make_shared<orthia::CModuleManager>();

        explicit FloatingFixture(const ORTHIA_TCHAR* folderName = ORTHIA_TCSTR("orthia_test_floating"), bool processFolder_in = false)
            :
            processFolder(processFolder_in)
        {
            namespace fs = std::filesystem;
            root = fs::temp_directory_path() / folderName;
            std::error_code ec;
            fs::remove_all(root, ec);
            fs::create_directories(root);
            if (processFolder)
            {
                orthia::FolderMeta meta;
                meta.kind = orthia::g_folderKindProcess;
                meta.dianaMode = DIANA_MODE64;
                meta.dianaArch = DIANA_ARCH_X86;
                meta.pid = 1;
                meta.name = ORTHIA_TCSTR("[1] fake");
                // a live item that closes removes the folder unless a capture or a snapshot is in it
                // (the fake has no start time, so it doesn't count as running)
                orthia::WriteFolderMeta((root / ORTHIA_TCSTR("meta.xml")).native(), meta);
            }
            moduleManager->Reinit((root / (processFolder ? ORTHIA_TCSTR("data.db") : ORTHIA_TCSTR("db.sqlite"))).native(), true);

            const auto* raw = orthia::FindFormatById("raw");
            CFakeProcess::Image module;
            module.format = raw;
            module.base = moduleBase;
            module.data.assign(0x1000, (char)0xCC);
            PutRel32(module.data, 0, 0xE8, moduleBase, regionA);
            PutRel32(module.data, 5, 0xE8, moduleBase, regionA + 0x20);
            PutRel32(module.data, 10, 0xE8, moduleBase, otherModule);
            PutRel32(module.data, 15, 0xE8, moduleBase, regionData);
            module.data[20] = (char)0xC3;
            process->images.push_back(module);

            CFakeProcess::Image other;
            other.format = raw;
            other.base = otherModule;
            other.data.assign(0x1000, (char)0xC3);
            process->images.push_back(other);

            CFakeProcess::Region a;
            a.base = regionA;
            a.data.assign(0x1000, (char)0xCC);
            PutRel32(a.data, 0, 0xE9, regionA, regionD);
            a.data[0x20] = (char)0xC3;
            process->regions.push_back(a);

            CFakeProcess::Region data;
            data.base = regionData;
            data.data.assign(0x1000, 0);
            data.protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_write;
            process->regions.push_back(data);

            CFakeProcess::Region d;
            d.base = regionD;
            d.data.assign(0x1000, (char)0xC3);
            process->regions.push_back(d);
        }
        ~FloatingFixture()
        {
            moduleManager.reset();
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }
        std::shared_ptr<orthia::CProcessWorkplaceItem> OpenItem()
        {
            auto item = std::make_shared<orthia::CProcessWorkplaceItem>(process, process->GetFullFileNameForUI(),
                process->GetDianaMode(), process->GetDianaArch(), nullptr);
            item->ReloadModules();
            if (processFolder)
            {
                item->SetProcFolder(root.native());
            }
            item->Init(moduleManager, nullptr, nullptr);
            return item;
        }
        // the same process opened again, as the database reuse does (CreateProcItemFS)
        std::shared_ptr<orthia::CProcessWorkplaceItem> Reopen()
        {
            auto item = OpenItem();
            item->LoadSavedFloatingModules();
            item->LoadSavedModuleFlags();
            return item;
        }
        // the folder opened as --proc-folder does (AddSavedProcess); needs processFolder
        std::shared_ptr<orthia::CProcessWorkplaceItem> OpenSaved()
        {
            auto saved = orthia::OpenSavedProcess(root.native());
            for (const auto& region : orthia::LoadFloatingRegions(*moduleManager))
            {
                saved->AddSnapshot(*region);
            }
            auto item = std::make_shared<orthia::CProcessWorkplaceItem>(saved, saved->GetFullFileNameForUI(),
                saved->GetDianaMode(), saved->GetDianaArch(), nullptr);
            item->SetSavedView();
            item->ReloadModules();
            item->SetProcFolder(root.native());
            item->Init(moduleManager, nullptr, nullptr);
            item->LoadSavedFloatingModules();
            item->LoadSavedModuleFlags();
            return item;
        }
        // the module analyzed, as .analyze leaves it
        void AnalyzeModule(orthia::CProcessWorkplaceItem& item)
        {
            orthia::ProcessReaderAdapter reader(process.get());
            moduleManager->ReloadRange(moduleBase, 0x1000, &reader, DIANA_MODE64, DIANA_ARCH_X86, 0);
            item.UpdateModuleFlags(moduleBase, orthia::ModuleInfo::flags_analyzeDone, 0);
        }
        static orthia::ModuleInfo FindModule(const orthia::IWorkPlaceItem& item, orthia::Address_type address)
        {
            std::vector<orthia::ModuleInfo> modules;
            item.GetModules(modules);
            for (const auto& mod : modules)
            {
                if (mod.address == address)
                    return mod;
            }
            return orthia::ModuleInfo();
        }
        // what the floating task does until nothing waits
        static void AnalyzePending(orthia::IWorkPlaceItem& item)
        {
            for (int pass = 0; pass < 10; ++pass)
            {
                auto pending = orthia::QueryPendingFloatingModules(item);
                if (pending.empty())
                    return;
                for (const auto& mod : pending)
                {
                    orthia::AnalyzeFloatingModule(item, mod);
                    orthia::DiscoverFloatingCode(item, FindModule(item, mod.address));
                }
            }
            FAIL() << "the floating modules don't stop coming";
        }
    };

    // data/ of the source tree, above the folder of the binary (bin/Release/amd64, cmake-release/src/...)
    inline std::filesystem::path FindTestData(const std::filesystem::path& relative)
    {
        namespace fs = std::filesystem;
        fs::path dir = fs::path(orthia::GetCurrentProcessDir()).parent_path();
        for (int i = 0; i < 8; ++i)
        {
            std::error_code error;
            const fs::path candidate = dir / ORTHIA_TCSTR("data") / relative;
            if (fs::exists(candidate, error))
            {
                return candidate;
            }
            if (dir == dir.parent_path())
            {
                break;
            }
            dir = dir.parent_path();
        }
        return fs::path();
    }
}
