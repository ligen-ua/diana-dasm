#include "orthia_diana_image_format.h"
#include "orthia_elf.h"
#include "orthia_log.h"
#include "../orthia_model_modules_elf.h"
#include "../orthia_external_symbols.h"

namespace orthia
{
    namespace
    {
        class CElfModuleImage : public CDianaModuleImage
        {
            bool IsExecutable() const
            {
                return GetExecutable().u.elfFile.pImpl->elfHeader.e_type == DIANA_ET_EXEC;
            }
        public:
            using CDianaModuleImage::CDianaModuleImage;

        protected:
            // ET_EXEC values are link-time addresses and the module stays at them;
            // everything else is relative to where the module was loaded
            Address_type ToAbsolute(OPERAND_SIZE value) const override
            {
                return IsExecutable() ? value : CDianaModuleImage::ToAbsolute(value);
            }
            Address_type QueryEntryPoint() const override
            {
                const OPERAND_SIZE entry = GetExecutable().u.elfFile.pImpl->elfHeader.e_entry;
                return entry ? ToAbsolute(entry) : 0;
            }
            // IFUNC resolvers and initializers run before the entry point
            void QuerySpecialNames(std::vector<NameInfo>& out) override
            {
                std::vector<ElfStartupRoutine> routines;
                if (int status = QueryElfStartupRoutines(&GetExecutable().u.elfFile, &GetStream().parent, m_base, routines))
                {
                    ORTHIA_DEV_LOG(orthia::LogSeverity::Debug, "QueryElfStartupRoutines: ", CLogParamEx(m_base, 16), " ->", (long)status);
                }
                for (const auto& routine : routines)
                {
                    if (routine.address)
                    {
                        AddName(out, NameInfo::flags_Export, routine.address, GetElfStartupRoutineName(routine));
                    }
                }
            }
        };

        class CElfImageFormat : public CDianaImageFormat
        {
        public:
            CElfImageFormat()
                : CDianaImageFormat(DIANA_EXECUTABLE_TYPE_ELF)
            {
            }
            const char* GetId() const override { return "elf"; }
            const char* GetDisplayName() const override { return "ELF"; }
            int GetBuiltInFlag() const override { return ModuleInfo::builtInFlags_moduleTypeElf; }
            void DescribeModule(const ModuleAccess& access, std::vector<PlatformString_type>& lines) const override
            {
                PlatformString_type buildIdHex;
                if (!QueryModuleElfDebugInfo(access.reader, *access.mod, buildIdHex))
                {
                    buildIdHex = ORTHIA_TCSTR("<none>");
                }
                lines.push_back(ORTHIA_TCSTR("Build ID: ") + buildIdHex);
            }
            std::unique_ptr<IExternalSymbolsLoader> CreateSymbolsLoader(const SymbolsLoaderParams& params) const override
            {
                return CreateElfDebugFileLoader(params);
            }
            // the section headers are not mapped: read from the module's file when it is the
            // build in memory, the program headers from memory otherwise
            bool QuerySections(const ModuleAccess& access, ImageSections& sections) const override
            {
                return QueryElfImageSections(access.reader, access.mod->address, access.imageFile, access.openFile, sections);
            }
            std::unique_ptr<IDependencyLoader> CreateDependencyLoader(
                std::shared_ptr<oui::BaseOperation> operation) const override
            {
                return std::make_unique<CElfImportsLoader>(operation);
            }

        protected:
            std::unique_ptr<CDianaModuleImage> CreateModuleImage(IMemoryReader& reader,
                Address_type base,
                Address_type size) const override
            {
                return std::make_unique<CElfModuleImage>(*this, reader, base, size);
            }
            std::shared_ptr<ISimpleFile> MapDianaFile(const std::vector<char>& data,
                const MapFileParameters& params) const override
            {
                auto file = std::make_shared<CSimpleElfFile>();
                file->MapFile(data, params);
                return file;
            }
        };
    }

    const IImageFormat& GetElfImageFormat()
    {
        static const CElfImageFormat format;
        return format;
    }
}
