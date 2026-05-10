#pragma once
#include "oui_threadpool.h"
#include "oui_window_thread.h"
#include "orthia_model_interfaces.h"
#include "orthia_config.h"
#include <atomic>
#include <map>
#include <memory>
#include <functional>
#include <mutex>

namespace orthia
{
    class CModuleAnalyzer : oui::Noncopyable
    {
        mutable std::mutex m_opsLock;
        std::map<int, std::shared_ptr<oui::BaseOperation>> m_ops;
        std::atomic<int> m_nextItemId{0};
        oui::CThreadPool m_pool{1};

        std::shared_ptr<oui::CWindowThread> m_uiThread;
        std::weak_ptr<IUILogInterface> m_uiLog;
        std::shared_ptr<CConfigOptionsStorage> m_config;

        void Cleanup(int workspaceId);
        void WriteLog(const oui::String& line);

    public:
        ~CModuleAnalyzer();

        void Init(std::weak_ptr<IUILogInterface> uiLog,
                  std::shared_ptr<CConfigOptionsStorage> config);

        void EnqueueAnalyze(std::shared_ptr<IWorkPlaceItem> item,
                     std::shared_ptr<oui::BaseOperation> op,
                     Address_type mainModuleAddr,
                     std::function<void()> onComplete);

        void EnqueueLoadSymbols(std::shared_ptr<IWorkPlaceItem> item,
                                std::shared_ptr<oui::BaseOperation> op,
                                std::function<void()> onComplete,
                                std::function<void()> onProgress,
                                Address_type moduleAddressHint = 0);

        void EnqueueAnalyzePrivateSymbols(std::shared_ptr<IWorkPlaceItem> item,
                                          std::shared_ptr<oui::BaseOperation> op,
                                          Address_type mainModuleAddr,
                                          std::function<void()> onComplete);

        void Cancel(int workspaceId);
        void CancelAll();
    };
}
