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
    void CProgramModel::QueryWorkspace(std::vector<std::string>& allNames, std::string& active)
    {
    }

    void CProgramModel::AddExecutable(std::shared_ptr<oui::IFile> file,
        oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler)
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
        
        orthia::CSimplePeFile mappedPE;
        orthia::MapFileParameters params;
        mappedPE.MapFile(peFile, params);

        // OK
        result.error.native.clear();
    }

}