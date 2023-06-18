#include "orthia_model.h"
#include "orthia_files.h"
#include "orthia_pe.h"
#include "orthia_helpers.h"
#include "orthia_database_module.h"

namespace orthia
{
    const unsigned long long g_maxSizeBytes = 512 * 1024 * 1024;
    const unsigned long long g_minSizeBytes = 1;

    // WorkplaceItemInternal
    WorkAddressData WorkplaceItemInternal::ReadData(Address_type address, Address_type size)
    {
        if (!size)
        {
            return WorkAddressData();
        }
        Address_type lastValid = address;
        if (Diana_SafeAdd(&lastValid, size - 1))
        {
            return WorkAddressData();
        }
        // check fast cases
        if (lastValid < peFile->GetImageBase() ||
            address > moduleLastValidAddress)
        {
            // the entire range is inaccessible
            std::vector<char> buffer(size);
            auto* pBufferStart = buffer.data();
            return WorkAddressData(
                pBufferStart,
                size,
                nullptr,
                WorkAddressData::flags_FullInvalid,
                [buffer = std::move(buffer)](WorkAddressData*) {
                }
            );
        }

        if (address >= peFile->GetImageBase() &&
            lastValid <= moduleLastValidAddress)
        {
            // the entire range is good
            auto offset = address - peFile->GetImageBase();
            auto pBufferStart = peFile->GetMappedPeFile().data() + offset;
            auto sharedThis = shared_from_this();
            return WorkAddressData(
                pBufferStart,
                size,
                nullptr,
                WorkAddressData::flags_FullValid,
                [sharedThis = std::move(sharedThis)](WorkAddressData*) mutable {
                    sharedThis.reset();
                }
            );
        }
        // damn, the range is partially inaccessible, it will be slow
        // [startInvalidBytes][module itself][endInvalidBytes]
        Address_type startInvalidBytes = 0;
        Address_type positiveAddress = 0;
        if (address < peFile->GetImageBase())
        {
            startInvalidBytes = peFile->GetImageBase() - address;
        }
        else
        {
            positiveAddress = address - peFile->GetImageBase();
        }
        Address_type startValidBytes = (size - startInvalidBytes) - positiveAddress;
        Address_type endInvalidBytes = 0;
        if (startValidBytes > peFile->GetMappedPeFile().size())
        {
            endInvalidBytes = startValidBytes - peFile->GetMappedPeFile().size();
            startValidBytes = peFile->GetMappedPeFile().size();
        }
        if (startInvalidBytes + startValidBytes + endInvalidBytes != size)
        {
            // something is just plain wrong, the main assumption is broken
            return WorkAddressData();
        }

        // ok here we go, prepare the final data
        std::vector<char> buffer(size);
        auto* pBufferStart = buffer.data();
        memcpy(pBufferStart + startInvalidBytes, peFile->GetMappedPeFile().data() + positiveAddress, startValidBytes);

        std::vector<char> flags(size);
        auto* pFlagsStart = flags.data();
        memset(pFlagsStart + 0, WorkAddressData::dataFlags_Invalid, startInvalidBytes);
        memset(pBufferStart + startInvalidBytes + startValidBytes, WorkAddressData::dataFlags_Invalid, endInvalidBytes);

        return WorkAddressData(
            pBufferStart,
            size,
            pFlagsStart,
            0,
            [buffer = std::move(buffer), 
              flags = std::move(flags)](WorkAddressData*) {
            }
        );
    }

    WorkAddressRangeInfo WorkplaceItemInternal::GetRangeInfo(Address_type address) const
    {
        Address_type entryPoint = peFile->GetImageBase();
        Diana_SafeAdd(&entryPoint, peFile->GetImpl()->mappedPE.pImpl->addressOfEntryPoint);
        return {
            peFile->GetImageBase(),
            moduleLastValidAddress,
            entryPoint,
            peFile->GetMappedPeFile().size(),
            peFile->GetImpl()->mappedPE.pImpl->dianaMode
        };
    }

    const std::shared_ptr<CModuleManager> WorkplaceItemInternal::GetModuleManager() const
    {
        return moduleManager;
    }

    // CProgramModel
    CProgramModel::CProgramModel(std::shared_ptr<orthia::CConfigOptionsStorage> config)
        :
            m_config(config)
    {
        m_fileSystem = std::make_shared<oui::CFileSystem>();
    }
    std::shared_ptr<IWorkPlaceItem> CProgramModel::GetItem(int uid)
    {
        std::unique_lock<std::mutex> lockGuard(m_lock);

        auto it = m_items.find(uid);
        if (it == m_items.end())
        {
            return nullptr;
        }
        return it->second;
    }
    void CProgramModel::SetUILog(std::shared_ptr<IUILogInterface> uiLog)
    {
        m_uiLog = uiLog;
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

    void CProgramModel::WriteLog(std::shared_ptr<oui::CWindowThread> thread, const oui::String& line)
    {
        thread->AddTask([=]() {
            if (auto log = m_uiLog.lock())
            {
                log->WriteLog(line);
            }
        });
    }
    void CProgramModel::AddExecutable(std::shared_ptr<oui::IFile> file,
        oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
        bool makeActive)
    {
        // non-ui thread
        
        // prepare the message on unknown error
        auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
        auto errorNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
        oui::fsui::OpenResult result;
        result.error = errorNode->QueryValue(ORTHIA_TCSTR("unknown"));
        oui::ScopedGuard handlerGuard([&]() {  
            completeHandler->Reply(completeHandler, file, result);  
        });

        // opening
        WriteLog(completeHandler->GetThread(), oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("opening")),
            file->GetFullFileNameForUI()));

        // read entire file in memory
        int error = 0;
        unsigned long long fileSize = 0;
        std::tie(error, fileSize) = file->GetSizeInBytes();
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

        std::vector<char> binPeFile;
        error = file->ReadExact(completeHandler, 0, (size_t)fileSize, binPeFile);
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

        // try map it first
        auto mappedPE = std::make_unique<orthia::CSimplePeFile>();
        orthia::MapFileParameters params;
        mappedPE->MapFile(binPeFile, params);

        // check folder
        auto fileHash = CalcSha1(binPeFile);
        auto fileHashStr = orthia::ToHexString(fileHash.data(), fileHash.size());
        auto dbFolder = m_config->GetDBFolder() + AddSlash2(fileHashStr);
        auto dbFileName = dbFolder + m_config->GetDBFileName();
        auto binFileName = dbFolder + m_config->GetBinFileName();
        auto readmeFileName = dbFolder + m_config->GetReadmeFileName();
        CreateAllDirectoriesForFile(dbFileName);

        // check binary file
        bool hashIsValid = false;
        try
        {
            orthia::CFile existingFile;
            error = existingFile.Open_Silent(binFileName, g_desired_read, g_share_read, g_open_existing);
            if (!error)
            {
                auto savedFileHash = CalcSha1(existingFile, completeHandler);
                hashIsValid = savedFileHash == fileHash;
            }
        }
        catch (std::exception&)
        {
            hashIsValid = false;
        }
        
        WriteLog(completeHandler->GetThread(), oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("module-sha1")),
            fileHashStr));

        if (!hashIsValid)
        {
            // save file to local dir
            orthia::CFile localFile;
            error = localFile.Open_Silent(binFileName, g_desired_write, g_share_read, g_create_always);
            if (error)
            {
                result.error = errorNode->QueryValue(ORTHIA_TCSTR("too-big"));
                return;
            }
            localFile.WriteToFile(binPeFile.data(), binPeFile.size());

            // save readme file 
            orthia::CFile readmeFile;
            readmeFile.Open_Silent(readmeFileName, g_desired_write, g_share_read, g_create_always);
            if (!error)
            {
                auto readmeHeader = mainNode->QueryValue(ORTHIA_TCSTR("readme-header"));
                auto originalName = oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("original-name")),
                    file->GetFullFileNameForUI());

                std::stringstream textInfo;                
                textInfo << Utf16ToUtf8(readmeHeader) << "\n";
                textInfo << Utf16ToUtf8(originalName.native) << "\n";
                auto str = textInfo.str();
                readmeFile.WriteToFile_Silent(str.c_str(), str.size());
            }
        }

        // fill the model data
        auto info = std::make_shared<WorkplaceItemInternal>();
        info->fullName = file->GetFullFileName();
        info->peFile = std::move(mappedPE);
        {
            oui::String shortName;
            orthia::UnparseFileNameFromFullFileName(info->fullName.native, &shortName.native);
            info->shortName = std::move(shortName);
        }
        info->moduleManager = std::make_shared<CModuleManager>();
        info->moduleManager->Reinit(dbFileName, false);

        if (info->shortName.native == m_config->GetBinFileName())
        {
            // opening own database, give more info
            std::vector<char> readmeBuffer;
            if (!orthia::LoadFileToVector_Silent(readmeFileName, readmeBuffer))
            {
                readmeBuffer.push_back(0);
                WriteLog(completeHandler->GetThread(), Utf8ToUtf16(readmeBuffer.data()));
            }
        }

        const auto& mappedFile = info->peFile->GetMappedPeFile();
        if (mappedFile.empty())
        {
            result.error = errorNode->QueryValue(ORTHIA_TCSTR("empty"));
            return;
        }
        info->moduleLastValidAddress = mappedFile.size() - 1;
        if (Diana_SafeAdd(&info->moduleLastValidAddress, info->peFile->GetImageBase()))
        {
            result.error = errorNode->QueryValue(ORTHIA_TCSTR("invalid-image-base"));
            return;
        }

        CMemoryReaderOnLoadedData reader(info->peFile->GetImageBase(), mappedFile.data(), mappedFile.size());
        if (!info->moduleManager->QueryDatabaseManager()->GetClassicDatabase()->IsModuleExists(info->peFile->GetImageBase()))
        {
            // first open, warn user it may take quite a time
            WriteLog(completeHandler->GetThread(), mainNode->QueryValue(ORTHIA_TCSTR("analyzing-file")));
        }

        info->moduleManager->ReloadModule(info->peFile->GetImageBase(),
            &reader,
            false,
            info->shortName.native,
            0);

        {
            std::unique_lock<std::mutex> lockGuard(m_lock);
            m_items[++m_lastUid] = info;

            if (makeActive)
            {
                m_activeId = m_lastUid;
            }
        }
        // OK
        result.error.native.clear();
    }

}