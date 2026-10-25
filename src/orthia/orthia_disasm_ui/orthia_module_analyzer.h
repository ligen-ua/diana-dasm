#pragma once
#include "oui_threadpool.h"
#include "oui_window_thread.h"
#include "orthia_model_interfaces.h"
#include "orthia_config.h"
#include <atomic>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <mutex>

namespace orthia
{
    class CModuleAnalyzer : oui::Noncopyable
    {
        mutable std::mutex m_opsLock;
        std::map<int, std::shared_ptr<oui::BaseOperation>> m_ops;
        std::map<int, std::vector<std::weak_ptr<oui::BaseOperation>>> m_workspaceOps;
        std::atomic<int> m_nextItemId{0};
        oui::CThreadPool m_pool{1};

        std::shared_ptr<oui::CWindowThread> m_uiThread;
        std::weak_ptr<IUILogInterface> m_uiLog;
        std::shared_ptr<CConfigOptionsStorage> m_config;
        std::shared_ptr<oui::IFileSystem> m_fileSystem;

        // the items with an "analyze floating modules" task in the queue
        std::set<const IWorkPlaceItem*> m_floatingQueued;

        void Cleanup(int workspaceId);
        int RegisterOp(std::shared_ptr<oui::BaseOperation> op, int workspaceId);
        // a floating module, then the floating code it leads to
        void AnalyzeFloatingNow(std::shared_ptr<IWorkPlaceItem> item, const ModuleInfo& module, bool force);

    public:
        void WriteLog(const oui::String& line);
        ~CModuleAnalyzer();
        void Stop();

        void Init(std::weak_ptr<IUILogInterface> uiLog,
                  std::shared_ptr<CConfigOptionsStorage> config,
                  std::shared_ptr<oui::IFileSystem> fileSystem);

        // force: the full analysis again, even of an analyzed module. Then the floating code the
        // module leads to (process items): found here, analyzed by EnqueueAnalyzeFloating
        void EnqueueAnalyze(std::shared_ptr<IWorkPlaceItem> item,
                     std::shared_ptr<oui::BaseOperation> op,
                     Address_type mainModuleAddr,
                     int workspaceId,
                     std::function<void()> onComplete,
                     bool force = false);

        // one task per item at the end of the queue: every floating module that waits for its
        // analysis, and again while that finds more
        void EnqueueAnalyzeFloating(std::shared_ptr<IWorkPlaceItem> item,
                     std::shared_ptr<oui::BaseOperation> op,
                     int workspaceId);

        void EnqueueLoadSymbols(std::shared_ptr<IWorkPlaceItem> item,
                                std::shared_ptr<oui::BaseOperation> op,
                                int workspaceId,
                                std::function<void()> onComplete,
                                std::function<void()> onProgress,
                                Address_type moduleAddressHint = 0);

        void EnqueueAnalyzePrivateSymbols(std::shared_ptr<IWorkPlaceItem> item,
                                          std::shared_ptr<oui::BaseOperation> op,
                                          Address_type mainModuleAddr,
                                          int workspaceId,
                                          std::function<void()> onComplete);

        void Cancel(int workspaceId);
        void CancelAll();

        // Queued or running analysis tasks. Chained work is re-enqueued from inside the
        // active task, so the count stays non-zero across such a hand-off.
        bool IsBusy() const { return m_pool.GetTasksCount() != 0; }
    };
}
