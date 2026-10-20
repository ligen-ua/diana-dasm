#pragma once

#include "../orthia_model_interfaces.h"
#include "orthia_simple_file.h"
#include "orthia_image_identity.h"

namespace orthia
{
    class CModuleManager;
    struct IImageFormat;

    // What a format may read about one module, in file mode and in process mode. The mapped
    // image is not the whole file (ELF section headers, .gnu_debuglink), so the module's own
    // file is reachable too: on the target, through openFile. It may be missing, or another
    // build than the one mapped; a format checks it against memory and falls back to memory
    struct ModuleAccess
    {
        IMemoryReader* reader = nullptr;            // the image mapped at mod->address
        const ModuleInfo* mod = nullptr;
        PlatformString_type imageFile;              // the module's file on the target, may be empty
        FileOpener_type openFile;                   // may be empty
    };

    // One parsed module, the same in file mode and process mode. Opened per use, not cached
    struct IModuleImage
    {
        virtual ~IModuleImage() = default;
        virtual const IImageFormat& GetFormat() const = 0;
        // address and size as opened, entryPoint absolute (0 = none), dianaMode
        virtual WorkAddressRangeInfo GetRange() const = 0;
        // Every name the image gives an address, in this order: $entrypoint, the format's own
        // "$" names, exports. flags_Export, an absolute address, and the name without "module!":
        // "memcpy", "$tls_0001", "$ifunc_memcpy", "$ordinal_0005". Several names can share an
        // address; whether a caller keeps all of them or the first one is up to the caller
        virtual void QueryNames(std::vector<NameInfo>& out) = 0;
        // flags_Import, address = the value in the import slot, name = the function name,
        // "$ordinal_XXXX" or empty; the caller resolves the target's own name
        virtual void QueryImports(std::vector<NameInfo>& out) = 0;
    };

    // Gives every format the same order and the same $entrypoint rule
    class CModuleImageBase : public IModuleImage
    {
    public:
        void QueryNames(std::vector<NameInfo>& out) final;
        void QueryImports(std::vector<NameInfo>& out) override {}
    protected:
        virtual void QuerySpecialNames(std::vector<NameInfo>& out) {}
        virtual void QueryExports(std::vector<NameInfo>& out) {}
        static void AddName(std::vector<NameInfo>& out, int flags, Address_type address, const std::string& utf8Name);
    };

    // File mode: finds the opened file's dependencies, maps them into free address ranges,
    // links the imports and records the modules and their names in the database
    struct IDependencyLoader
    {
        virtual ~IDependencyLoader() = default;
        // before the main module is registered
        virtual void LoadModules(const oui::String& fileName,
            std::shared_ptr<ISimpleFile> file,
            std::shared_ptr<oui::IFileSystem> fs) = 0;
        // after it
        virtual void ReportModules(std::shared_ptr<CModuleManager> moduleManager) = 0;
        // true when the main module's import rows are keyed by their slot address (the PE IAT),
        // so FileWorkplaceItem::ApplyLinkedImports can write the same values back on reopen
        // without loading the dependencies again
        virtual bool ImportsKeyedBySlot() const { return false; }
    };

    // How to read an image (PE, ELF, ...): one stateless object per format, found through the
    // registry below. Not to be confused with IImageSource (orthia_image_source.h), which says
    // where a module's bytes come from (the opened file, a linked dependency, ...).
    struct IImageFormat
    {
        virtual ~IImageFormat() = default;
        // stable, lowercase: "pe", "elf". For logs, --format and saved open parameters
        virtual const char* GetId() const = 0;
        // shown to the user: "PE", "ELF"
        virtual const char* GetDisplayName() const = 0;
        // ModuleInfo::builtInFlags_moduleType*, stored in the database
        virtual int GetBuiltInFlag() const = 0;

        // whether the first bytes of a file are this format; false for formats only the user picks
        virtual bool Detect(const char* header, size_t size) const = 0;
        // the module mapped at base: a process module, or a file mapped by MapFile (read through
        // CMemoryReaderOnLoadedData). nullptr when it isn't this format or its headers are broken
        virtual std::unique_ptr<IModuleImage> OpenModuleImage(IMemoryReader& reader,
            Address_type base,
            Address_type size) const = 0;
        // whether MapFile needs MapFileParameters from the user (raw code: base and mode).
        // They are saved next to the database, so a reopen doesn't ask again
        virtual bool NeedsUserParameters() const { return false; }
        // throws std::exception with a message for the user when the image is malformed
        virtual std::shared_ptr<ISimpleFile> MapFile(const std::vector<char>& data,
            const MapFileParameters& params) const = 0;
        // nullptr: the format has no dependencies to load
        virtual std::unique_ptr<IDependencyLoader> CreateDependencyLoader(
            std::shared_ptr<oui::BaseOperation> operation) const { return nullptr; }
        // adds the opened file's own module to a new database; reader reads the mapped file.
        // Returns whether the module's code was analyzed (ModuleInfo::flags_analyzeDone)
        virtual bool RegisterMainModule(CModuleManager& manager,
            ISimpleFile& file,
            IMemoryReader& reader,
            const PlatformString_type& shortName,
            bool deepAnalysis) const = 0;
        // where the UI starts showing a newly opened file; 0: its own choice
        virtual Address_type GetInitialAddress(ISimpleFile& file) const { return 0; }

        // what tells this build of a file mapped by MapFile from another one; false: nothing
        virtual bool QueryIdentity(ISimpleFile& file, ImageIdentity& identity) const { return false; }
        // a cheaper check from the file's first bytes, without mapping it: how many bytes it
        // needs (0: the format has no such check), and the identity they give (maybe partial)
        virtual size_t GetHeaderIdentitySize() const { return 0; }
        virtual bool QueryHeaderIdentity(const char* header, size_t size, ImageIdentity& identity) const { return false; }

        // `modinfo` lines after the module's name and file: the debug record and the like
        virtual void DescribeModule(const ModuleAccess& access, std::vector<PlatformString_type>& lines) const {}
        // false when the format has no sections or the headers don't parse
        virtual bool QuerySections(const ModuleAccess& access, ImageSections& sections) const { return false; }
    };

    // how much of a file's start FindFormatByData needs to tell every format apart
    const size_t c_formatDetectSize = 0x1000;

    // every format, in detection order
    const std::vector<const IImageFormat*>& GetImageFormats();
    // nullptr when no format recognizes the bytes
    const IImageFormat* FindFormatByData(const char* header, size_t size);
    const IImageFormat* FindFormatById(const std::string& id);
    // by the type bit in mod.builtInFlags; a module recorded before the bits existed is PE when
    // its file is a .dll, .exe or .sys. nullptr when nothing tells
    const IImageFormat* FindFormatByModule(const ModuleInfo& mod);
    // the module's sections through its format; for a module of unknown format, through
    // whatever its headers in memory look like
    void QueryModuleSections(const ModuleAccess& access, ImageSections& sections);
    // IModuleImage::QueryNames of a file mapped by format.MapFile; false when its headers don't parse
    bool QueryMappedFileNames(const IImageFormat& format, const ISimpleFile& file, std::vector<NameInfo>& out);

}
