#pragma once

#include "orthia_model_interfaces.h"
#include "orthia_module_symbols.h"
#include <memory>
#include <vector>

namespace orthia
{
    class CClassicDatabase;

    bool IsPeModule(const ModuleInfo& mod);

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

    std::unique_ptr<IExternalSymbolsLoader> CreateExternalSymbolsLoader(
        const std::vector<PlatformString_type>& symbolFolders,
        std::shared_ptr<CLoaderUILogger> logger = nullptr);
}
