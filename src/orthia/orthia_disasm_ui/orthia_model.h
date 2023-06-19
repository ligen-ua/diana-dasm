#pragma once

#include "orthia_utils.h"
#include "oui_filesystem.h"
#include "orthia_text_manager.h"
#include "orthia_config.h"
#include "orthia_module_manager.h"
#include "oui_processes.h"

extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

namespace orthia
{
    class CSimplePeFile;
    class CModuleManager;

    struct WorkAddressRangeInfo
    {
        Address_type address = 0;
        Address_type lastValidAddress = 0;
        Address_type entryPoint = 0;
        Address_type size = 0;
        int dianaMode = 0;
    };
    struct WorkAddressData:oui::Noncopyable
    {
        const static int flags_FullValid = 1;
        const static int flags_FullInvalid = 2;

        const static int dataFlags_Invalid = 1;

        const char* pDataStart = 0;
        Address_type dataSize = 0;
        const char* pDataFlags = 0;
        int rangeFlags = 0;
        std::function<void (WorkAddressData*)> completeHandler;

        WorkAddressData()
        {
        }
        ~WorkAddressData()
        {
            if (completeHandler)
            {
                completeHandler(this);
                completeHandler = nullptr;
            }
        }
        WorkAddressData(const char* pDataStart_in,
            Address_type dataSize_in,
            const char* pDataFlags_in,
            int rangeFlags_in,
            std::function<void(WorkAddressData*)> && completeHandler_in)
            :
                pDataStart(pDataStart_in),
                dataSize(dataSize_in),
                pDataFlags(pDataFlags_in),
                rangeFlags(rangeFlags_in),
                completeHandler(std::move(completeHandler_in))
        {
        }
    };
    struct IWorkPlaceItem
    {
        virtual ~IWorkPlaceItem() {}
        virtual WorkAddressRangeInfo GetRangeInfo(Address_type address) const = 0;
        virtual const std::shared_ptr<CModuleManager> GetModuleManager() const = 0;
        virtual WorkAddressData ReadData(Address_type address, Address_type size) = 0;
    };
    class CSimplePeFile;
    struct WorkplaceItemInternal:std::enable_shared_from_this<WorkplaceItemInternal>, IWorkPlaceItem
    {
        std::unique_ptr<orthia::CSimplePeFile> peFile;
        oui::String fullName, shortName;
        std::shared_ptr<CModuleManager> moduleManager;
        Address_type moduleLastValidAddress = 0;

        // public interface
        WorkAddressData ReadData(Address_type address, Address_type size) override;
        WorkAddressRangeInfo GetRangeInfo(Address_type address) const override;
        const std::shared_ptr<CModuleManager> GetModuleManager() const override;
    };

    struct WorkplaceItem
    {
        int uid = 0;
        oui::String name;
    };
    struct IUILogInterface
    {
        virtual ~IUILogInterface() {}
        virtual void WriteLog(const oui::String& line) = 0;
    };
    class CProgramModel
    {
        std::shared_ptr<oui::CFileSystem> m_fileSystem;
        std::shared_ptr<oui::CProcessSystem> m_processSystem;

        mutable std::mutex m_lock;
        std::map<int, std::shared_ptr<WorkplaceItemInternal>> m_items;
        int m_lastUid = 0;

        int m_activeId = 0;
        std::shared_ptr<orthia::CConfigOptionsStorage> m_config;
        std::weak_ptr<IUILogInterface> m_uiLog;

        void WriteLog(std::shared_ptr<oui::CWindowThread> thread, const oui::String& line);
    public:
        CProgramModel(std::shared_ptr<orthia::CConfigOptionsStorage> config);

        std::shared_ptr<oui::CFileSystem> GetFileSystem();
        std::shared_ptr<oui::CProcessSystem> GetProcessSystem();

        int QueryWorkspaceItems(std::vector<WorkplaceItem>& items) const;
        bool QueryActiveWorkspaceItem(WorkplaceItem& item) const;
        void SetUILog(std::shared_ptr<IUILogInterface> uiLog);
        std::shared_ptr<IWorkPlaceItem> GetItem(int uid);

        // other thread
        void AddExecutable(std::shared_ptr<oui::IFile> file,
            oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
            bool makeActive);

    };
}