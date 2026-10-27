#pragma once

// Fakes shared by the UI tests (ui_tests*.cpp)

#include "gtest/gtest.h"
#include "orthia_utils.h"
#include "formats/orthia_image_format.h"
#include "orthia_process_adapter.h"
#include "oui_processes.h"
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
