#include "orthia_diana_image_format.h"
#include "orthia_module_manager.h"
#include "diana_core_cpp.h"

#include "diana_executable.h"

namespace orthia
{
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
