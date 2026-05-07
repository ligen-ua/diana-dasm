#include "orthia_module_analyzer.h"
#include "orthia_item_process.h"
#include "orthia_process_adapter.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include "orthia_external_symbols.h"

namespace orthia
{
    CModuleAnalyzer::~CModuleAnalyzer()
    {
        CancelAll();
        m_pool.Stop();
    }

    void CModuleAnalyzer::Init(std::weak_ptr<IUILogInterface> uiLog,
                               std::shared_ptr<CConfigOptionsStorage> config)
    {
        m_uiLog = std::move(uiLog);
        m_config = std::move(config);
    }

    void CModuleAnalyzer::WriteLog(const oui::String& line)
    {
        if (!m_uiThread)
            return;
        m_uiThread->AddTask([uiLog = m_uiLog, line]() {
            if (auto log = uiLog.lock())
                log->WriteLog(line);
        });
    }

    void CModuleAnalyzer::CancelAll()
    {
        std::unique_lock<std::mutex> lock(m_opsLock);
        for (auto& [id, op] : m_ops)
            op->Cancel();
    }

    void CModuleAnalyzer::Cancel(int workspaceId)
    {
        std::unique_lock<std::mutex> lock(m_opsLock);
        auto it = m_ops.find(workspaceId);
        if (it != m_ops.end())
            it->second->Cancel();
    }

    void CModuleAnalyzer::Cleanup(int workspaceId)
    {
        std::unique_lock<std::mutex> lock(m_opsLock);
        m_ops.erase(workspaceId);
    }

    void CModuleAnalyzer::Enqueue(int workspaceId,
        std::shared_ptr<CProcessWorkplaceItem> item,
        std::shared_ptr<oui::BaseOperation> op,
        Address_type mainModuleAddr,
        std::function<void()> onComplete)
    {
        {
            std::unique_lock<std::mutex> lock(m_opsLock);
            m_ops[workspaceId] = op;
            if (!m_uiThread)
                m_uiThread = op->GetThread();
        }

        m_pool.AddTask([this,
            weakItem = std::weak_ptr<CProcessWorkplaceItem>(item),
            op,
            workspaceId,
            mainModuleAddr,
            onComplete = std::move(onComplete)]() mutable
        {
            auto item = weakItem.lock();
            if (!item || op->IsCancelled())
            {
                Cleanup(workspaceId);
                return;
            }

            auto proc = item->GetAssociatedProcess();
            auto moduleManager = item->GetModuleManager();
            if (!proc || !moduleManager)
            {
                Cleanup(workspaceId);
                return;
            }

            std::vector<orthia::ModuleInfo> modules;
            item->GetModules(modules);

            auto mainIt = std::find_if(modules.begin(), modules.end(),
                [mainModuleAddr](const auto& m) { return m.IsInRange(mainModuleAddr); });
            if (mainIt == modules.end())
            {
                return;
            }
            ProcessReaderAdapter reader(proc.get());
            auto db = moduleManager->QueryDatabaseManager()->GetClassicDatabase();

            if (op->IsCancelled())
                return;
            try
            {
                if (!db->IsModuleExists(mainIt->address))
                {
                    auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                    WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("reloading-module")), mainIt->name));
                    moduleManager->ReloadModule(mainIt->address, &reader, false, mainIt->name, 0);
                    item->UpdateModuleFlags(mainIt->address, ModuleInfo::flags_analyzeDone, 0);
                }
            }
            catch (const std::exception& e)
            {
                oui::LogOutput(oui::LogFlags::Error, e.what());
            }

            Cleanup(workspaceId);
            if (!op->IsCancelled())
            {
                onComplete();
            }
        });
    }

    void CModuleAnalyzer::EnqueueAnalyzePrivateSymbols(int workspaceId,
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<oui::BaseOperation> op,
        Address_type mainModuleAddr,
        std::function<void()> onComplete)
    {
        {
            std::unique_lock<std::mutex> lock(m_opsLock);
            m_ops[workspaceId] = op;
            if (!m_uiThread)
                m_uiThread = op->GetThread();
        }

        m_pool.AddTask([this,
            weakItem = std::weak_ptr<IWorkPlaceItem>(item),
            op,
            workspaceId,
            mainModuleAddr,
            onComplete = std::move(onComplete)]() mutable
        {
            auto item = weakItem.lock();
            if (!item || op->IsCancelled())
            {
                Cleanup(workspaceId);
                return;
            }

            auto moduleManager = item->GetModuleManager();
            if (!moduleManager)
            {
                Cleanup(workspaceId);
                return;
            }

            std::vector<orthia::ModuleInfo> modules;
            item->GetModules(modules);

            auto mainIt = std::find_if(modules.begin(), modules.end(),
                [mainModuleAddr](const auto& m) { return m.IsInRange(mainModuleAddr); });
            if (mainIt == modules.end())
            {
                Cleanup(workspaceId);
                return;
            }

            try
            {
                auto db = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
                std::vector<orthia::Address_type> hints;
                db->QueryMetaInfoModule2(mainIt->address,
                    g_database_type_fnc_PrivateSymbol, -1,
                    [&hints](orthia::Address_type, int, const std::string&, orthia::Address_type metaAddr) -> bool
                    {
                        hints.push_back(metaAddr);
                        return true;
                    });

                if (!hints.empty() && !op->IsCancelled())
                {
                    auto reader = item->CreateMemoryReader();
                    if (reader)
                    {
                        auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                        WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("analyzing-private-symbols")), mainIt->name));

                        moduleManager->ReloadModuleWithHints(mainIt->address, reader.get(), mainIt->name, 0, hints);
                        item->UpdateModuleFlags(mainIt->address, ModuleInfo::flags_analyzeDone, 0);
                        WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("analyzing-private-symbols-done")), mainIt->name));
                    }
                }
            }
            catch (const std::exception& e)
            {
                oui::LogOutput(oui::LogFlags::Error, e.what());
            }

            Cleanup(workspaceId);
            if (!op->IsCancelled())
            {
                onComplete();
            }
        });
    }

    void CModuleAnalyzer::EnqueueLoadSymbols(int workspaceId,
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<oui::BaseOperation> op,
        std::function<void()> onComplete,
        const PlatformString_type& singleModuleName)
    {
        {
            std::unique_lock<std::mutex> lock(m_opsLock);
            m_ops[workspaceId] = op;
            if (!m_uiThread)
                m_uiThread = op->GetThread();
        }

        m_pool.AddTask([this,
            weakItem = std::weak_ptr<IWorkPlaceItem>(item),
            op,
            workspaceId,
            singleModuleName,
            onComplete = std::move(onComplete)]() mutable
        {
            auto item = weakItem.lock();
            if (!item || op->IsCancelled())
            {
                Cleanup(workspaceId);
                return;
            }

            auto moduleManager = item->GetModuleManager();
            if (!moduleManager)
            {
                Cleanup(workspaceId);
                return;
            }

            std::vector<orthia::ModuleInfo> modules;
            item->GetModules(modules);

            auto db = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
            auto logger = std::make_shared<CLoaderUILogger>(m_uiThread, m_uiLog);
            auto loader = CreateExternalSymbolsLoader(m_config->GetSymbolsFolders(), logger);

            for (auto& mod : modules)
            {
                if (op->IsCancelled())
                    break;

                if (!singleModuleName.empty() && mod.name != singleModuleName)
                    continue;

                try
                {
                    bool anyLoaded = false;
                    auto onSymbol = 
                        [&mod, item, &anyLoaded](Address_type addr, const oui::String& symName)
                    {
                        item->OnPrivateSymbolLoaded(addr, symName);
                        if (!anyLoaded)
                        {
                            anyLoaded = true;
                            item->OnModuleSymbolsLoaded(mod.address);
                        }
                    };
                    loader->Load(mod, db, onSymbol);
                }
                catch (const std::exception& e)
                {
                    oui::LogOutput(oui::LogFlags::Error, e.what());
                }
            }

            Cleanup(workspaceId);
            if (!op->IsCancelled())
            {
                onComplete();
            }
        });
    }
}
