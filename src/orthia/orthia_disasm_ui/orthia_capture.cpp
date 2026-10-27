#include "orthia_capture.h"
#include "orthia_tiny_xml.h"
#include "orthia_log.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <atomic>

namespace orthia
{
    const ORTHIA_TCHAR* const g_captureFolder = ORTHIA_TCSTR("capture");

    namespace
    {
        namespace fs = std::filesystem;

        const int c_captureVersion = 1;
        const char* const c_indexName = "capture.xml";
        const size_t c_readChunk = 0x10000;
        // a read-modify-write of capture.xml at a time: .capture and --capture run on other threads
        std::mutex g_indexLock;

        std::string ToHex(unsigned long long value)
        {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%llx", value);
            return buffer;
        }

        unsigned long long HexAttribute(const tinyxml2::XMLElement* element, const char* name)
        {
            const char* text = element->Attribute(name);
            if (!text || !*text)
            {
                return 0;
            }
            return strtoull(text, nullptr, 16);
        }

        std::string TextAttribute(const tinyxml2::XMLElement* element, const char* name)
        {
            const char* value = element->Attribute(name);
            return value ? value : "";
        }

        // .capture and --capture may write the same region at once (the UI runs commands and the
        // analysis on threads of their own): each write has a temporary file of its own
        std::string UniqueTempSuffix()
        {
            static std::atomic<unsigned long long> counter{ 0 };
            return "." + std::to_string(++counter) + ".tmp";
        }

        fs::path CaptureFolder(const PlatformString_type& folder)
        {
            return fs::path(folder) / g_captureFolder;
        }

        void WriteFileAtomic(const fs::path& target, const char* data, size_t size)
        {
            fs::path temp = target;
            temp += UniqueTempSuffix();
            {
                std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
                stream.write(data, (std::streamsize)size);
                if (!stream)
                {
                    throw std::runtime_error("Can't write " + PlatformStringToUtf8(temp.native()));
                }
            }
            std::error_code ec;
            fs::rename(temp, target, ec);
            if (ec)
            {
                fs::remove(temp, ec);
                throw std::runtime_error("Can't write " + PlatformStringToUtf8(target.native()));
            }
        }

        void WriteCaptureIndex(const PlatformString_type& folder, const CaptureIndex& index)
        {
            const std::string text = FormatCaptureIndex(index);
            WriteFileAtomic(CaptureFolder(folder) / c_indexName, text.data(), text.size());
        }

        bool IsReadable(const oui::MemoryRegionInfo& region)
        {
            return region.committed && (region.protect & (oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_exec));
        }

        CapturedRegion ToCaptured(const oui::MemoryRegionInfo& region)
        {
            CapturedRegion captured;
            captured.base = region.regionBase;
            captured.size = region.regionSize;
            captured.allocationBase = region.allocationBase;
            captured.protect = region.protect;
            captured.committed = region.committed;
            captured.type = region.type;
            captured.name = region.mappedName;
            return captured;
        }

        // the bytes of a region into capture/<base>_<size>.bin
        void SaveRegionBytes(oui::IProcess& process, const PlatformString_type& folder, CapturedRegion& region)
        {
            const std::string fileName = ToHex(region.base) + "_" + ToHex(region.size) + ".bin";
            const fs::path target = CaptureFolder(folder) / Utf8ToPlatformString(fileName);
            fs::path temp = target;
            temp += UniqueTempSuffix();
            Address_type missing = 0;
            {
                std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
                std::vector<char> buffer(c_readChunk);
                for (Address_type offset = 0; offset < region.size; offset += c_readChunk)
                {
                    const size_t chunk = (size_t)std::min<Address_type>(c_readChunk, region.size - offset);
                    if (process.ReadExactEx2(region.base + offset, buffer.data(), chunk))
                    {
                        // a partial read may leave some of it filled: the whole chunk counts as missing
                        memset(buffer.data(), 0, chunk);
                        missing += chunk;
                    }
                    stream.write(buffer.data(), (std::streamsize)chunk);
                }
                if (!stream)
                {
                    throw std::runtime_error("Can't write " + PlatformStringToUtf8(temp.native()));
                }
            }
            std::error_code ec;
            fs::rename(temp, target, ec);
            if (ec)
            {
                fs::remove(temp, ec);
                throw std::runtime_error("Can't write " + PlatformStringToUtf8(target.native()));
            }
            region.file = fileName;
            region.missing = missing;
        }

        // the files of the old index the new one doesn't name: replaced by a newer capture of their
        // range. Only those: another capture may be writing files the index doesn't have yet
        void RemoveReplacedFiles(const PlatformString_type& folder, const CaptureIndex& old, const CaptureIndex& index)
        {
            for (const auto& region : old.regions)
            {
                if (region.file.empty())
                    continue;
                const bool used = std::any_of(index.regions.begin(), index.regions.end(),
                    [&](const CapturedRegion& r) { return r.file == region.file; });
                if (!used)
                {
                    std::error_code ec;
                    fs::remove(CaptureFolder(folder) / Utf8ToPlatformString(region.file), ec);
                }
            }
        }

        void Accumulate(CaptureStats& stats, const CapturedRegion& region)
        {
            ++stats.regions;
            if (!region.file.empty())
            {
                ++stats.savedRegions;
                stats.bytes += region.size;
                stats.missing += region.missing;
            }
        }

        long long NowUnix()
        {
            return (long long)std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }

        // merges the new regions into capture.xml (and threads, when given)
        void StoreCapture(const PlatformString_type& folder, const std::vector<CapturedRegion>& regions,
            const std::vector<CapturedThread>* threads, bool complete)
        {
            std::lock_guard<std::mutex> guard(g_indexLock);
            CaptureIndex index;
            ReadCaptureIndex(folder, index);
            const CaptureIndex old = index;
            MergeCapturedRegions(index.regions, regions);
            if (threads)
            {
                index.hasThreads = true;
                index.threads = *threads;
            }
            index.createdUnix = NowUnix();
            index.complete = complete;
            WriteCaptureIndex(folder, index);
            RemoveReplacedFiles(folder, old, index);
        }
    }

    std::string FormatCaptureIndex(const CaptureIndex& index)
    {
        tinyxml2::XMLDocument doc;
        auto* root = doc.NewElement("capture");
        doc.InsertEndChild(root);
        root->SetAttribute("format_version", c_captureVersion);
        root->SetAttribute("created", (int64_t)index.createdUnix);
        root->SetAttribute("complete", index.complete ? 1 : 0);
        for (const auto& region : index.regions)
        {
            auto* element = doc.NewElement("region");
            element->SetAttribute("base", ToHex(region.base).c_str());
            element->SetAttribute("size", ToHex(region.size).c_str());
            element->SetAttribute("allocation", ToHex(region.allocationBase).c_str());
            element->SetAttribute("protect", region.protect);
            element->SetAttribute("committed", region.committed ? 1 : 0);
            element->SetAttribute("type", region.type);
            if (!region.name.empty())
                element->SetAttribute("name", region.name.c_str());
            if (!region.file.empty())
            {
                element->SetAttribute("file", region.file.c_str());
                element->SetAttribute("missing", ToHex(region.missing).c_str());
            }
            root->InsertEndChild(element);
        }
        if (index.hasThreads)
        {
            auto* threads = doc.NewElement("threads");
            root->InsertEndChild(threads);
            for (const auto& thread : index.threads)
            {
                auto* element = doc.NewElement("thread");
                element->SetAttribute("tid", ToHex(thread.tid).c_str());
                element->SetAttribute("start", ToHex(thread.startAddress).c_str());
                if (!thread.startTime.empty())
                    element->SetAttribute("created", thread.startTime.c_str());
                std::string stack;
                for (auto address : thread.stack)
                {
                    if (!stack.empty())
                        stack += ' ';
                    stack += ToHex(address);
                }
                if (!stack.empty())
                    element->SetAttribute("stack", stack.c_str());
                threads->InsertEndChild(element);
            }
        }
        tinyxml2::XMLPrinter printer;
        doc.Print(&printer);
        return printer.CStr();
    }

    bool ParseCaptureIndex(const std::string& text, CaptureIndex& index)
    {
        tinyxml2::XMLDocument doc;
        if (doc.Parse(text.c_str(), text.size()) != tinyxml2::XML_SUCCESS)
        {
            return false;
        }
        const auto* root = doc.FirstChildElement("capture");
        if (!root || root->IntAttribute("format_version", 0) <= 0)
        {
            return false;
        }
        CaptureIndex result;
        result.createdUnix = root->Int64Attribute("created", 0);
        result.complete = root->IntAttribute("complete", 1) != 0;
        for (const auto* element = root->FirstChildElement("region"); element; element = element->NextSiblingElement("region"))
        {
            CapturedRegion region;
            region.base = (Address_type)HexAttribute(element, "base");
            region.size = (Address_type)HexAttribute(element, "size");
            if (!region.size)
            {
                return false;
            }
            region.allocationBase = (Address_type)HexAttribute(element, "allocation");
            region.protect = element->IntAttribute("protect", 0);
            region.committed = element->IntAttribute("committed", 0) != 0;
            region.type = element->IntAttribute("type", 0);
            region.name = TextAttribute(element, "name");
            region.file = TextAttribute(element, "file");
            region.missing = (Address_type)HexAttribute(element, "missing");
            result.regions.push_back(std::move(region));
        }
        std::sort(result.regions.begin(), result.regions.end(),
            [](const CapturedRegion& a, const CapturedRegion& b) { return a.base < b.base; });
        if (const auto* threads = root->FirstChildElement("threads"))
        {
            result.hasThreads = true;
            for (const auto* element = threads->FirstChildElement("thread"); element; element = element->NextSiblingElement("thread"))
            {
                CapturedThread thread;
                thread.tid = HexAttribute(element, "tid");
                thread.startAddress = (Address_type)HexAttribute(element, "start");
                thread.startTime = TextAttribute(element, "created");
                const std::string stack = TextAttribute(element, "stack");
                size_t pos = 0;
                while (pos < stack.size())
                {
                    size_t end = stack.find(' ', pos);
                    if (end == std::string::npos)
                        end = stack.size();
                    thread.stack.push_back((Address_type)strtoull(stack.substr(pos, end - pos).c_str(), nullptr, 16));
                    pos = end + 1;
                }
                result.threads.push_back(std::move(thread));
            }
        }
        index = std::move(result);
        return true;
    }

    bool ReadCaptureIndex(const PlatformString_type& folder, CaptureIndex& index)
    {
        std::ifstream stream(CaptureFolder(folder) / c_indexName, std::ios::binary);
        if (!stream)
        {
            return false;
        }
        const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        return ParseCaptureIndex(text, index);
    }

    bool HasCapture(const PlatformString_type& folder)
    {
        if (folder.empty())
        {
            return false;
        }
        std::error_code ec;
        return fs::is_regular_file(CaptureFolder(folder) / c_indexName, ec);
    }

    void MergeCapturedRegions(std::vector<CapturedRegion>& base, const std::vector<CapturedRegion>& captured)
    {
        std::vector<CapturedRegion> result;
        for (const auto& old : base)
        {
            const bool replaced = std::any_of(captured.begin(), captured.end(), [&](const CapturedRegion& region) {
                return region.base < old.base + old.size && old.base < region.base + region.size;
            });
            if (!replaced)
            {
                result.push_back(old);
            }
        }
        result.insert(result.end(), captured.begin(), captured.end());
        std::sort(result.begin(), result.end(), [](const CapturedRegion& a, const CapturedRegion& b) { return a.base < b.base; });
        base = std::move(result);
    }

    CaptureStats CaptureProcess(oui::IProcess& process, const PlatformString_type& folder, CaptureProgress_type progress)
    {
        if (folder.empty())
        {
            throw std::runtime_error("The process has no database folder");
        }
        fs::create_directories(CaptureFolder(folder));

        std::vector<oui::MemoryRegionInfo> map;
        if (int error = process.EnumMemoryRegions([&](const oui::MemoryRegionInfo& region) {
            map.push_back(region);
            return true;
        }))
        {
            throw std::runtime_error("Can't read the memory map of the process, error " + std::to_string(error));
        }

        CaptureStats stats;
        std::vector<CapturedRegion> regions;
        bool complete = true;
        for (const auto& info : map)
        {
            auto region = ToCaptured(info);
            if (complete && IsReadable(info))
            {
                SaveRegionBytes(process, folder, region);
            }
            Accumulate(stats, region);
            regions.push_back(std::move(region));
            if (complete && !regions.back().file.empty() && progress && !progress(stats))
            {
                // the map is still saved whole; the bytes stop here
                complete = false;
            }
        }

        std::vector<CapturedThread> threads;
        if (complete)
        {
            auto [error, enumerator] = process.CreateThreadEnumerator();
            for (; !error && enumerator;)
            {
                auto [nextError, thread] = enumerator->GetNextThread();
                if (nextError || !thread)
                {
                    break;
                }
                oui::ThreadInfo info;
                if (thread->GetInfo(info))
                {
                    continue;
                }
                CapturedThread captured;
                captured.tid = info.tid;
                captured.startAddress = info.startAddress;
                captured.startTime = info.startTime.ToSQLTime();
                thread->CaptureStack(64, captured.stack);
                threads.push_back(std::move(captured));
            }
            stats.threads = threads.size();
        }
        StoreCapture(folder, regions, complete ? &threads : nullptr, complete);
        return stats;
    }

    CaptureStats CaptureRange(oui::IProcess& process, const PlatformString_type& folder, Address_type base, Address_type size)
    {
        CaptureStats stats;
        if (folder.empty() || !size)
        {
            return stats;
        }
        fs::create_directories(CaptureFolder(folder));
        std::vector<CapturedRegion> regions;
        const Address_type end = base + size;
        for (Address_type address = base; address < end;)
        {
            oui::MemoryRegionInfo info;
            if (process.QueryMemoryRegion(address, info) || !info.regionSize || info.regionBase + info.regionSize <= address)
            {
                break;
            }
            auto region = ToCaptured(info);
            if (IsReadable(info))
            {
                SaveRegionBytes(process, folder, region);
            }
            Accumulate(stats, region);
            regions.push_back(std::move(region));
            address = info.regionBase + info.regionSize;
        }
        if (!regions.empty())
        {
            CaptureIndex index;
            {
                std::lock_guard<std::mutex> guard(g_indexLock);
                ReadCaptureIndex(folder, index);
            }
            StoreCapture(folder, regions, nullptr, index.regions.empty() ? true : index.complete);
        }
        return stats;
    }
}
