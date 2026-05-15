#include "orthia_module_analyzer.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include "orthia_external_symbols.h"
#include "orthia_module_symbols.h"

namespace orthia
{
    CModuleAnalyzer::~CModuleAnalyzer()
    {
        Stop();
    }
    void CModuleAnalyzer::Stop()
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
        auto it = m_workspaceOps.find(workspaceId);
        if (it != m_workspaceOps.end())
        {
            for (auto& weakOp : it->second)
            {
                if (auto op = weakOp.lock())
                    op->Cancel();
            }
            m_workspaceOps.erase(it);
        }
    }

    void CModuleAnalyzer::Cleanup(int workspaceId)
    {
        std::unique_lock<std::mutex> lock(m_opsLock);
        m_ops.erase(workspaceId);
    }

    void CModuleAnalyzer::EnqueueAnalyze(std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<oui::BaseOperation> op,
        Address_type mainModuleAddr,
        int workspaceId,
        std::function<void()> onComplete)
    {
        if (item->IsDeletePending())
            return;
        int itemId = ++m_nextItemId;
        {
            std::unique_lock<std::mutex> lock(m_opsLock);
            m_ops[itemId] = op;
            m_workspaceOps[workspaceId].push_back(op);
            if (!m_uiThread)
                m_uiThread = op->GetThread();
        }

        m_pool.AddTask([this,
            weakItem = std::weak_ptr<IWorkPlaceItem>(item),
            op,
            itemId,
            mainModuleAddr,
            onComplete = std::move(onComplete)]() mutable
        {
            auto item = weakItem.lock();
            if (!item || op->IsCancelled())
            {
                Cleanup(itemId);
                return;
            }

            auto moduleManager = item->GetModuleManager();
            if (!moduleManager)
            {
                Cleanup(itemId);
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
            auto reader = item->CreateMemoryReader();
            auto db = moduleManager->QueryDatabaseManager()->GetClassicDatabase();

            if (op->IsCancelled())
                return;
            try
            {
                if (!db->IsModuleExists(mainIt->address))
                {
                    auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                    WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("reloading-module")), mainIt->name));
                    if (reader)
                    {
                        moduleManager->ReloadModule(mainIt->address, reader.get(), false, mainIt->name, 0);
                        item->UpdateModuleFlags(mainIt->address, ModuleInfo::flags_analyzeDone, 0);
                    }
                }
            }
            catch (const std::exception& e)
            {
                oui::LogOutput(oui::LogFlags::Error, e.what());
            }

            Cleanup(itemId);
            if (!op->IsCancelled())
            {
                onComplete();
            }
        });
    }

    void CModuleAnalyzer::EnqueueAnalyzePrivateSymbols(std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<oui::BaseOperation> op,
        Address_type mainModuleAddr,
        int workspaceId,
        std::function<void()> onComplete)
    {
        if (item->IsDeletePending())
            return;
        int itemId = ++m_nextItemId;
        {
            std::unique_lock<std::mutex> lock(m_opsLock);
            m_ops[itemId] = op;
            m_workspaceOps[workspaceId].push_back(op);
            if (!m_uiThread)
                m_uiThread = op->GetThread();
        }

        m_pool.AddTask([this,
            weakItem = std::weak_ptr<IWorkPlaceItem>(item),
            op,
            itemId,
            mainModuleAddr,
            onComplete = std::move(onComplete)]() mutable
        {
            auto item = weakItem.lock();
            if (!item || op->IsCancelled())
            {
                Cleanup(itemId);
                return;
            }

            auto moduleManager = item->GetModuleManager();
            if (!moduleManager)
            {
                Cleanup(itemId);
                return;
            }

            std::vector<orthia::ModuleInfo> modules;
            item->GetModules(modules);

            auto mainIt = std::find_if(modules.begin(), modules.end(),
                [mainModuleAddr](const auto& m) { return m.IsInRange(mainModuleAddr); });
            if (mainIt == modules.end())
            {
                Cleanup(itemId);
                return;
            }
            if (mainIt->flags & ModuleInfo::flags_analyzePrivateDone)
            {
                return;
            }
            if (!(mainIt->flags & ModuleInfo::flags_symbolsLoaded))
            {
                return;
            }
            try
            {
                std::vector<orthia::Address_type> hints;
                {
                    const int c_pageSize = 5000;
                    NameSelectionKey key;
                    key.privateSymbolsOnly = true;
                    std::vector<NameInfo> page;
                    for (;;)
                    {
                        item->QueryNames(mainIt->address, key, c_pageSize, page);
                        if (page.empty())
                            break;
                        for (const auto& info : page)
                            hints.push_back(info.address);
                        key.flags |= NameSelectionKey::flags_ContinueFrom;
                        key.address = page.back().address;
                        key.continueMarkNameFlag = page.back().flags;
                    }
                }

                if (!hints.empty() && !op->IsCancelled())
                {
                    auto reader = item->CreateMemoryReader();
                    if (reader)
                    {
                        auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                        WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("analyzing-private-symbols")), mainIt->name));

                        moduleManager->ReloadModuleWithHints(mainIt->address, reader.get(), mainIt->name, 0, hints);
                        item->UpdateModuleFlags(mainIt->address, ModuleInfo::flags_analyzePrivateDone, 0);
                        WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("analyzing-private-symbols-done")), mainIt->name));
                    }
                }
            }
            catch (const std::exception& e)
            {
                oui::LogOutput(oui::LogFlags::Error, e.what());
            }

            Cleanup(itemId);
            if (!op->IsCancelled())
            {
                onComplete();
            }
        });
    }

    void CModuleAnalyzer::EnqueueLoadSymbols(
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<oui::BaseOperation> op,
        int workspaceId,
        std::function<void()> onComplete,
        std::function<void()> onProgress,
        Address_type moduleAddressHint)
    {
        if (item->IsDeletePending())
            return;
        int itemId = ++m_nextItemId;
        {
            std::unique_lock<std::mutex> lock(m_opsLock);
            m_ops[itemId] = op;
            m_workspaceOps[workspaceId].push_back(op);
            if (!m_uiThread)
                m_uiThread = op->GetThread();
        }

        m_pool.AddTask([this,
            weakItem = std::weak_ptr<IWorkPlaceItem>(item),
            op,
            itemId,
            moduleAddressHint,
            onComplete = std::move(onComplete),
            onProgress = std::move(onProgress)]() mutable
        {
            auto item = weakItem.lock();
            if (!item || op->IsCancelled())
            {
                Cleanup(itemId);
                return;
            }

            auto moduleManager = item->GetModuleManager();
            if (!moduleManager)
            {
                Cleanup(itemId);
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

                if (moduleAddressHint && !mod.IsInRange(moduleAddressHint))
                    continue;

                if (mod.flags & mod.flags_symbolsLoaded)
                {
                    continue;
                }
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
                    ModuleSymbols syms;
                    loader->Load(mod, syms, onSymbol);
                    if (!syms.IsEmpty())
                    {
                        if (auto* storage = item->GetModuleStorage())
                            storage->Store(mod.address, std::move(syms));
                        else
                            syms.FlushToDB(mod.address, db);
                    }
                    if (onProgress)
                    {
                        onProgress();
                    }
                }
                catch (const std::exception& e)
                {
                    oui::LogOutput(oui::LogFlags::Error, e.what());
                }
            }

            Cleanup(itemId);
            if (!op->IsCancelled())
            {
                if (onComplete)
                {
                    onComplete();
                }
            }
        });
    }
}
