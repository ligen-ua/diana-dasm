#include "orthia_module_analyzer.h"
#include "orthia_item_process.h"
#include "orthia_process_adapter.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"

namespace orthia
{
    CModuleAnalyzer::~CModuleAnalyzer()
    {
        CancelAll();
        m_pool.Stop();
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
            if (mainIt != modules.end() && mainIt != modules.begin())
                std::iter_swap(modules.begin(), mainIt);

            ProcessReaderAdapter reader(proc.get());
            auto db = moduleManager->QueryDatabaseManager()->GetClassicDatabase();

            for (auto& module : modules)
            {
                if (op->IsCancelled())
                    break;
                try
                {
                    if (!db->IsModuleExists(module.address))
                        moduleManager->ReloadModule(module.address, &reader, false, module.name, 0);
                }
                catch (const std::exception&) {}
            }

            Cleanup(workspaceId);
            if (!op->IsCancelled())
                onComplete();
        });
    }
}
