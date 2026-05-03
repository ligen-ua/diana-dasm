#pragma once

#include "orthia_model_interfaces.h"
#include <memory>
#include <vector>

namespace orthia
{
    class CClassicDatabase;

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

    class IExternalSymbolsLoader
    {
    public:
        virtual ~IExternalSymbolsLoader() = default;
        virtual bool CanLoad(const ModuleInfo& mod) const = 0;
        virtual void Load(const ModuleInfo& mod,
                          intrusive_ptr<CClassicDatabase> db) = 0;
    };

    std::unique_ptr<IExternalSymbolsLoader> CreateExternalSymbolsLoader(
        const std::vector<PlatformString_type>& symbolFolders,
        std::shared_ptr<CLoaderUILogger> logger = nullptr);
}
