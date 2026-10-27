#include "orthia_module_analyzer.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include "orthia_external_symbols.h"
#include "orthia_module_symbols.h"
#include "orthia_helpers.h"
#include "orthia_floating_code.h"
#include <algorithm>

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
                               std::shared_ptr<CConfigOptionsStorage> config,
                               std::shared_ptr<oui::IFileSystem> fileSystem)
    {
        m_uiLog = std::move(uiLog);
        m_config = std::move(config);
        m_fileSystem = std::move(fileSystem);
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

    int CModuleAnalyzer::RegisterOp(std::shared_ptr<oui::BaseOperation> op, int workspaceId)
    {
        int itemId = ++m_nextItemId;
        std::unique_lock<std::mutex> lock(m_opsLock);
        m_ops[itemId] = op;
        m_workspaceOps[workspaceId].push_back(op);
        if (!m_uiThread)
            m_uiThread = op->GetThread();
        return itemId;
    }

    // the most floating modules one process gets analyzed: a chain of them can't run forever
    static const size_t c_maxFloatingModules = 256;

    void CModuleAnalyzer::AnalyzeFloatingNow(std::shared_ptr<IWorkPlaceItem> item, const ModuleInfo& module, bool force)
    {
        auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
        ModuleInfo current = module;
        if (module.flags & ModuleInfo::flags_stale)
        {
            // /f: the memory as it is now
            const auto refresh = force ? RefreshStaleFloatingModule(*item, current) : FloatingRefresh::unavailable;
            if (refresh == FloatingRefresh::unavailable)
            {
                WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("floating-stale")), module.name));
                return;
            }
            if (refresh == FloatingRefresh::removed)
            {
                WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("floating-gone")), module.name));
                return;
            }
            WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("floating-refreshed")), module.name));
            // the row is new: nothing to clear
            force = false;
        }
        if (force || !(current.flags & ModuleInfo::flags_analyzeDone))
        {
            WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("analyzing-floating")), current.name));
            if (force)
            {
                item->GetModuleManager()->ClearModuleReferences(current.address);
            }
            AnalyzeFloatingModule(*item, current);
        }
        DiscoverFloatingCode(*item, current);
    }

    void CModuleAnalyzer::EnqueueAnalyzeFloating(std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<oui::BaseOperation> op,
        int workspaceId)
    {
        if (item->IsDeletePending())
            return;
        {
            std::unique_lock<std::mutex> lock(m_opsLock);
            if (!m_floatingQueued.insert(item.get()).second)
                return;
        }
        int itemId = RegisterOp(op, workspaceId);
        m_pool.AddTask([this,
            weakItem = std::weak_ptr<IWorkPlaceItem>(item),
            rawItem = item.get(),
            op,
            itemId,
            workspaceId]() mutable
        {
            {
                // a module found from here queues the next pass
                std::unique_lock<std::mutex> lock(m_opsLock);
                m_floatingQueued.erase(rawItem);
            }
            auto item = weakItem.lock();
            if (!item || op->IsCancelled() || !item->GetModuleManager())
            {
                Cleanup(itemId);
                return;
            }
            bool more = false;
            try
            {
                std::vector<orthia::ModuleInfo> modules;
                item->GetModules(modules);
                const size_t floatingCount = std::count_if(modules.begin(), modules.end(),
                    [](const ModuleInfo& m) { return m.IsFloating(); });
                if (floatingCount > c_maxFloatingModules)
                {
                    auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                    WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("floating-limit")),
                        oui::String(Utf8ToPlatformString(std::to_string(c_maxFloatingModules)))));
                }
                else
                {
                    for (const auto& module : QueryPendingFloatingModules(*item))
                    {
                        if (op->IsCancelled())
                            break;
                        try
                        {
                            AnalyzeFloatingNow(item, module, false);
                        }
                        catch (const std::exception& e)
                        {
                            WriteLog(oui::String(Utf8ToPlatformString(e.what())));
                            // tried: not again on every pass
                            item->UpdateModuleFlags(module.address, ModuleInfo::flags_analyzeDone, 0);
                        }
                    }
                    more = !QueryPendingFloatingModules(*item).empty();
                }
            }
            catch (const std::exception& e)
            {
                WriteLog(oui::String(Utf8ToPlatformString(e.what())));
            }
            // queued before Cleanup: IsBusy stays true across the hand-off
            if (more && !op->IsCancelled())
            {
                EnqueueAnalyzeFloating(item, op, workspaceId);
            }
            Cleanup(itemId);
        });
    }

    void CModuleAnalyzer::EnqueueAnalyze(std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<oui::BaseOperation> op,
        Address_type mainModuleAddr,
        int workspaceId,
        std::function<void()> onComplete,
        bool force)
    {
        if (item->IsDeletePending())
            return;
        int itemId = RegisterOp(op, workspaceId);

        m_pool.AddTask([this,
            weakItem = std::weak_ptr<IWorkPlaceItem>(item),
            op,
            itemId,
            workspaceId,
            force,
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
            auto reader = item->CreateMemoryReader();
            auto db = moduleManager->QueryDatabaseManager()->GetClassicDatabase();

            if (op->IsCancelled())
            {
                Cleanup(itemId);
                return;
            }
            bool floatingPending = false;
            try
            {
                if (mainIt->IsFloating())
                {
                    AnalyzeFloatingNow(item, *mainIt, force);
                    floatingPending = !QueryPendingFloatingModules(*item).empty();
                }
                else
                {
                    if (force && (mainIt->flags & ModuleInfo::flags_analyzeDone))
                    {
                        // the references made from the module go, its exports and symbols stay
                        moduleManager->ClearModuleReferences(mainIt->address);
                        item->UpdateModuleFlags(mainIt->address, 0, ModuleInfo::flags_analyzeDone);
                        mainIt->flags &= ~ModuleInfo::flags_analyzeDone;
                    }
                    // not IsModuleExists: a quick open (--cmd without --analyze) registers the module unanalyzed
                    if (!(mainIt->flags & ModuleInfo::flags_analyzeDone))
                    {
                        auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                        WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("reloading-module")), mainIt->name));
                        if (reader)
                        {
                            if (db->IsModuleExists(mainIt->address))
                            {
                                moduleManager->AnalyzeRegisteredModule(mainIt->address, reader.get(), mainIt->name, 0);
                            }
                            else
                            {
                                moduleManager->ReloadModule(mainIt->address, reader.get(), false, mainIt->name, 0);
                            }
                            item->UpdateModuleFlags(mainIt->address, ModuleInfo::flags_analyzeDone, 0);
                            // the analysis rewrites the names of the module and of its dependencies
                            for (const auto& mod : modules)
                            {
                                item->InvalidateNames(mod.address);
                            }
                        }
                    }
                    // from the references saved now or by an earlier analysis
                    floatingPending = DiscoverFloatingCode(*item, *mainIt);
                }
            }
            catch (const std::exception& e)
            {
                WriteLog(oui::String(Utf8ToPlatformString(e.what())));
            }
            // the last task in the queue, so the modules queued after this one go first
            if (floatingPending && !op->IsCancelled())
            {
                EnqueueAnalyzeFloating(item, op, workspaceId);
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
                Cleanup(itemId);
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
                        key.offset += (int)page.size();
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
                        item->InvalidateNames(mainIt->address);
                        item->UpdateModuleFlags(mainIt->address, ModuleInfo::flags_analyzePrivateDone, 0);
                        WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("analyzing-private-symbols-done")), mainIt->name));
                    }
                }
            }
            catch (const std::exception& e)
            {
                WriteLog(oui::String(Utf8ToPlatformString(e.what())));
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
            auto loader = CreateExternalSymbolsLoader(m_config->GetSymbolsFolders(), logger,
                                                      CreateFileOpener(m_fileSystem));
            auto memoryReader = item->CreateMemoryReader();

            for (auto& mod : modules)
            {
                if (op->IsCancelled())
                    break;

                if (moduleAddressHint && !mod.IsInRange(moduleAddressHint))
                    continue;

                // no file, no symbols
                if ((mod.flags & mod.flags_symbolsLoaded) || mod.IsFloating())
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
                    loader->Load(mod, memoryReader.get(), syms, onSymbol);
                    if (!syms.IsEmpty())
                    {
                        if (auto* storage = item->GetModuleStorage())
                            storage->Store(mod.address, std::move(syms));
                        else
                            syms.FlushToDB(mod.address, db);
                        // OnModuleSymbolsLoaded came with the first symbol, before they were stored
                        item->InvalidateNames(mod.address);
                    }
                    if (onProgress)
                    {
                        onProgress();
                    }
                }
                catch (const std::exception& e)
                {
                    WriteLog(oui::String(Utf8ToPlatformString(e.what())));
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
