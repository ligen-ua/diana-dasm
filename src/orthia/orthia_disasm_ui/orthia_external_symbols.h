#pragma once

#include "orthia_model_interfaces.h"
#include "orthia_module_symbols.h"
#include "orthia_file_reader.h"
#include <memory>
#include <vector>

namespace orthia
{
    class CClassicDatabase;

    // Whether the module's image can be read at mod.address: an unresolved dependency owns
    // an address range with nothing mapped there, and so does, in file mode, any dependency
    bool IsModuleImageReadable(IMemoryReader* memoryReader, const ModuleInfo& mod);

    // Reads a module's own memory image (already mapped by memoryReader at
    // mod.address, e.g. from a live process or a loaded module database) and
    // extracts the GUID+Age+PDB name recorded in its CodeView (RSDS) debug
    // directory entry. This is the same adapter the "pe_info" command uses,
    // so it works uniformly whether the module's original file is still
    // reachable on disk or not. Returns false if no CodeView entry could be
    // found/parsed; pdbName is left empty when the module carries no PDB
    // name (or none could be read), even on success.
    bool QueryModulePeDebugInfo(IMemoryReader* memoryReader,
                                const ModuleInfo& mod,
                                DIANA_UUID& guid,
                                DI_UINT32& age,
                                PlatformString_type& pdbName);

    // ELF analogue of QueryModulePeDebugInfo: reads a module's own memory
    // image and extracts the GNU build-id (NT_GNU_BUILD_ID note, if any)
    // as a lowercase hex string. Returns false if the module carries no
    // such note (e.g. built without --build-id).
    bool QueryModuleElfDebugInfo(IMemoryReader* memoryReader,
                                 const ModuleInfo& mod,
                                 PlatformString_type& buildIdHex);

    // Host-side lookup of an ELF module's separate debug file, where GDB looks for it:
    // <root>/.build-id/ab/cdef.debug for each symbol folder and /usr/lib/debug, then the
    // .gnu_debuglink name next to the module, in its .debug/ subfolder, under /usr/lib/debug
    // and in the symbol folders. Returns the path found, empty when there is none.
    PlatformString_type FindElfDebugFile(const PlatformString_type& moduleFile,
                                         const PlatformString_type& buildIdHex,
                                         const PlatformString_type& debugLink,
                                         const std::vector<PlatformString_type>& symbolFolders);

    class CLoaderUILogger
    {
        std::shared_ptr<oui::CWindowThread> m_uiThread;
        std::weak_ptr<IUILogInterface> m_uiLog;
    public:
        CLoaderUILogger(std::shared_ptr<oui::CWindowThread> uiThread,
                        std::weak_ptr<IUILogInterface> uiLog)
            : m_uiThread(std::move(uiThread))
            , m_uiLog(std::move(uiLog))
        {
        }

        void WriteLog(const oui::String& line)
        {
            if (!m_uiThread)
                return;
            m_uiThread->AddTask([uiLog = m_uiLog, line]() {
                if (auto log = uiLog.lock())
                    log->WriteLog(line);
            });
        }
    };

    using OnPrivateSymbolLoaded = std::function<void(Address_type addr, const oui::String& name)>;

    class IExternalSymbolsLoader
    {
    public:
        virtual ~IExternalSymbolsLoader() = default;
        virtual bool CanLoad(const ModuleInfo& mod) const = 0;
        virtual void Load(const ModuleInfo& mod,
                          IMemoryReader* memoryReader,
                          ModuleSymbols& out,
                          OnPrivateSymbolLoaded onSymbol = nullptr) = 0;
    };

    struct SymbolsLoaderParams
    {
        std::vector<PlatformString_type> symbolFolders;     // host-side
        std::shared_ptr<CLoaderUILogger> logger;
        FileOpener_type openFile;                           // the modules' own (target) files
    };
    // the loaders behind the formats (IImageFormat::CreateSymbolsLoader):
    // PDB files for PE modules, separate debug files for ELF modules
    std::unique_ptr<IExternalSymbolsLoader> CreatePdbSymbolsLoader(const SymbolsLoaderParams& params);
    std::unique_ptr<IExternalSymbolsLoader> CreateElfDebugFileLoader(const SymbolsLoaderParams& params);

    // every format's loader; a module is loaded by the first one that can
    std::unique_ptr<IExternalSymbolsLoader> CreateExternalSymbolsLoader(
        const std::vector<PlatformString_type>& symbolFolders,
        std::shared_ptr<CLoaderUILogger> logger = nullptr,
        // opens the modules' own (target) files; PDBs come from host-side symbol folders
        FileOpener_type openFile = FileOpener_type());
}
