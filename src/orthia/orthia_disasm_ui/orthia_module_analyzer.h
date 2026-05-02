#pragma once
#include "oui_threadpool.h"
#include "oui_window_thread.h"
#include "orthia_model_interfaces.h"
#include <map>
#include <memory>
#include <functional>
#include <mutex>

namespace orthia
{
    class CProcessWorkplaceItem;

    class CModuleAnalyzer : oui::Noncopyable
    {
        mutable std::mutex m_opsLock;
        std::map<int, std::shared_ptr<oui::BaseOperation>> m_ops;
        oui::CThreadPool m_pool{1};

        std::shared_ptr<oui::CWindowThread> m_uiThread;
        std::weak_ptr<IUILogInterface> m_uiLog;

        void Cleanup(int workspaceId);
        void WriteLog(const oui::String& line);

    public:
        ~CModuleAnalyzer();

        void Init(std::weak_ptr<IUILogInterface> uiLog);

        void Enqueue(int workspaceId,
                     std::shared_ptr<CProcessWorkplaceItem> item,
                     std::shared_ptr<oui::BaseOperation> op,
                     Address_type mainModuleAddr,
                     std::function<void()> onComplete);

        void Cancel(int workspaceId);
        void CancelAll();
    };
}
