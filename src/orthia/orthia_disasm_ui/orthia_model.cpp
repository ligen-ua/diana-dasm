#include "orthia_model.h"
#include "orthia_files.h"
#include "orthia_pe.h"

namespace orthia
{
    const unsigned long long g_maxSizeBytes = 512 * 1024 * 1024;
    const unsigned long long g_minSizeBytes = 1;

    CProgramModel::CProgramModel()
    {
        m_fileSystem = std::make_shared<oui::CFileSystem>();
    }

    std::shared_ptr<oui::CFileSystem> CProgramModel::GetFileSystem()
    {
        return m_fileSystem;
    }
    bool CProgramModel::QueryActiveWorkspaceItem(WorkplaceItem& item) const
    {
        std::unique_lock<std::mutex> lockGuard(m_lock);

        auto it = m_items.find(m_activeId);
        if (it == m_items.end())
        {
            return false;
        }
        item.uid = m_activeId;
        item.name = it->second->shortName;
        return true;
    }
    int CProgramModel::QueryWorkspaceItems(std::vector<WorkplaceItem>& items) const
    {
        items.clear();
        std::unique_lock<std::mutex> lockGuard(m_lock);

        int activePos = -1;
        for (const auto& item : m_items)
        {
            WorkplaceItem res;
            res.uid = item.first;
            res.name = item.second->shortName;
            if (res.uid == m_activeId)
            {
                activePos = (int)items.size();
            }
            items.push_back(res);
        }
        return activePos;
    }

    void CProgramModel::AddExecutable(std::shared_ptr<oui::IFile> file,
        oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
        bool makeActive)
    {
        // non-ui thread
        auto errorNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
        oui::fsui::OpenResult result;
        result.error = errorNode->QueryValue(ORTHIA_TCSTR("unknown"));
        oui::ScopedGuard handlerGuard([&]() {  
            completeHandler->Reply(completeHandler, file, result);  
        });

        int platformError = 0;
        unsigned long long fileSize = 0;
        std::tie(platformError, fileSize) = file->GetSizeInBytes();
        if (fileSize < g_minSizeBytes)
        {
            result.error = errorNode->QueryValue(ORTHIA_TCSTR("empty"));
            return;
        }
        if (fileSize > g_maxSizeBytes)
        {
            result.error = errorNode->QueryValue(ORTHIA_TCSTR("too-big"));
            return;
        }

        std::vector<char> peFile;
        int error = file->SaveToVector(completeHandler, (size_t)fileSize, peFile);
        if (error)
        {
            result.error.native = oui::GetErrorText(error);
            return;
        }
        
        if (completeHandler->IsCancelled())
        {
            handlerGuard.Release();
            return;
        }
        
        auto mappedPE = std::make_unique<orthia::CSimplePeFile>();
        orthia::MapFileParameters params;
        mappedPE->MapFile(peFile, params);

        auto info = std::make_shared<WorkplaceItemInternal>();
        info->fullName = file->GetFullFileName();
        info->peFile = std::move(mappedPE);
        oui::String shortName;
        orthia::UnparseFileNameFromFullFileName(info->fullName.native, &shortName.native);
        info->shortName = std::move(shortName);

        std::unique_lock<std::mutex> lockGuard(m_lock);
        m_items[++m_lastUid] = info;

        if (makeActive)
        {
            m_activeId = m_lastUid;
        }
        // OK
        result.error.native.clear();
    }

}