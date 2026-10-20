#include "orthia_diana_image_format.h"
#include "orthia_module_manager.h"
#include "orthia_log.h"
#include "diana_core_cpp.h"
#include "diana_pe_cpp.h"

namespace orthia
{
    namespace
    {
        // what the parser reports through the PE-style observer: names and values, nothing else
        struct NamedValue
        {
            std::string name;
            OPERAND_SIZE value = 0;
        };
        struct NamedValuesCollector : public diana::CBasePeLinkImportsObserver
        {
            std::vector<NamedValue> values;
            bool skipNull = false;      // exports: a zero value is no function at all

            void QueryFunctionByOrdinal(const char* pDllName,
                DI_UINT32 ordinal,
                OPERAND_SIZE* pAddress) override
            {
                QueryFunctionByName(pDllName, ("$ordinal_" + ToAnsiStringAsHex(ordinal)).c_str(), 0, pAddress);
            }
            void QueryFunctionByName(const char*,
                const char* pFunctionName,
                DI_UINT32,
                OPERAND_SIZE* pAddress) override
            {
                if (skipNull && (!*pAddress || !pFunctionName || !*pFunctionName))
                {
                    return;
                }
                NamedValue value;
                value.name = pFunctionName ? pFunctionName : "";
                value.value = *pAddress;
                values.push_back(std::move(value));
            }
        };
    }

    // CDianaModuleImage
    CDianaModuleImage::CDianaModuleImage(const CDianaImageFormat& format, IMemoryReader& reader, Address_type base, Address_type size)
        :
        m_format(format),
        m_writeCache(&reader, 0x1000),
        m_module(&m_writeCache, base),
        m_stream(0, &m_module, size),
        m_exe(),
        m_base(base),
        m_size(size)
    {
        m_writeCache.SetCacheReads(true);
    }

    CDianaModuleImage::~CDianaModuleImage()
    {
        if (m_initialized)
        {
            DianaExecutable_Free(&m_exe);
        }
    }

    int CDianaModuleImage::Init()
    {
        // the module's pointers are relative to its start
        if (int status = DianaExecutable_Init(&m_exe,
            &m_stream.parent,
            m_size,
            DIANA_EXECUTABLE_FILE_FLAGS_MODULE_MODE))
        {
            return status;
        }
        m_initialized = true;
        return m_exe.type == m_format.GetExecutableType() ? DI_SUCCESS : DI_UNSUPPORTED;
    }

    const IImageFormat& CDianaModuleImage::GetFormat() const
    {
        return m_format;
    }

    WorkAddressRangeInfo CDianaModuleImage::GetRange() const
    {
        WorkAddressRangeInfo range;
        range.address = m_base;
        range.size = m_size;
        range.lastValidAddress = m_base;
        Diana_SafeAdd(&range.lastValidAddress, m_size ? m_size - 1 : 0);
        range.entryPoint = QueryEntryPoint();
        range.dianaMode = m_exe.dianaMode;
        return range;
    }

    void CDianaModuleImage::QueryExports(std::vector<NameInfo>& out)
    {
        NamedValuesCollector collector;
        collector.skipNull = true;
        std::vector<char> page(0x4000);
        int status = DianaExecutable_QueryExports(&m_exe, &m_stream.parent, page.data(), (int)page.size(), collector.GetParent(), 0);
        if (status && status != DI_END)
        {
            ORTHIA_DEV_LOG(orthia::LogSeverity::Debug, "QueryExports: ", CLogParamEx(m_base, 16), " ->", (long)status);
        }
        for (const auto& value : collector.values)
        {
            AddName(out, NameInfo::flags_Export, ToAbsolute(value.value), value.name);
        }
    }

    void CDianaModuleImage::QueryImports(std::vector<NameInfo>& out)
    {
        NamedValuesCollector collector;
        std::vector<char> page(0x1000);
        int status = DianaExecutable_QueryImports(&m_exe,
            m_base,
            &m_stream,
            page.data(),
            (int)page.size(),
            collector.GetParent(),
            DIANA_ANALYZE_RANDOM_READ_ABSOLUTE,
            0);
        if (status && status != DI_END)
        {
            ORTHIA_DEV_LOG(orthia::LogSeverity::Debug, "QueryImports: ", CLogParamEx(m_base, 16), " ->", (long)status);
        }
        for (const auto& value : collector.values)
        {
            // already the target's address: the slot holds what the loader put there
            AddName(out, NameInfo::flags_Import, value.value, value.name);
        }
    }

    // CDianaImageFormat
    static std::string DescribeMapError(int errorCode)
    {
        switch (errorCode)
        {
        case DI_END_OF_STREAM:
            return "file is truncated";
        case DI_INVALID_INPUT:
            return "malformed or truncated headers";
        case DI_UNSUPPORTED:
        case DI_UNSUPPORTED_COMMAND:
            return "unsupported format feature";
        }
        return diana::QueryErrorCode(errorCode);
    }

    CDianaImageFormat::CDianaImageFormat(int executableType)
        : m_executableType(executableType)
    {
    }

    bool CDianaImageFormat::Detect(const char* header, size_t size) const
    {
        return DianaExecutable_DetectType(header, (OPERAND_SIZE)size) == m_executableType;
    }

    std::unique_ptr<IModuleImage> CDianaImageFormat::OpenModuleImage(IMemoryReader& reader,
        Address_type base,
        Address_type size) const
    {
        auto image = CreateModuleImage(reader, base, size);
        if (int status = image->Init())
        {
            ORTHIA_DEV_LOG(orthia::LogSeverity::Debug, "Module: ", CLogParamEx(base, 16), ":", CLogParamEx(size, 16), " ->", (long)status);
            return nullptr;
        }
        return image;
    }

    std::shared_ptr<ISimpleFile> CDianaImageFormat::MapFile(const std::vector<char>& data,
        const MapFileParameters& params) const
    {
        try
        {
            return MapDianaFile(data, params);
        }
        catch (const diana::CException& e)
        {
            // the parser's error code means nothing to the user
            throw std::runtime_error(std::string("Invalid ") + GetDisplayName() +
                " image: " + DescribeMapError(e.GetErrorCode()));
        }
    }

    void CDianaImageFormat::RegisterMainModule(CModuleManager& manager,
        ISimpleFile& file,
        IMemoryReader& reader,
        const PlatformString_type& shortName,
        bool deepAnalysis) const
    {
        if (deepAnalysis)
        {
            manager.ReloadModule(file.GetImageBase(), &reader, false, shortName, 0);
        }
        else
        {
            manager.RegisterModule(file.GetImageBase(), &reader, shortName);
        }
    }
}
