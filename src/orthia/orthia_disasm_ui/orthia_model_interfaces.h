#pragma once

#include "orthia_utils.h"
#include "oui_string.h"
#include "oui_window_thread.h"
#include "oui_filesystem.h"
#include "orthia_common_time.h"

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

        bool IsInRange(Address_type offset) const
        {
            return offset >= address && offset <= lastValidAddress;
        }
    };

    struct ModuleInfo:WorkAddressRangeInfo
    {
        static const int flags_analyzeDone   = 1;
        static const int flags_symbolsLoaded = 2;
        
        PlatformString_type fullName;
        int flags = 0;
    };

    struct WorkAddressData :oui::Noncopyable
    {
        const static int flags_FullValid = 1;
        const static int flags_FullInvalid = 2;

        const static int dataFlags_Invalid = 1;

        const char* pDataStart = 0;
        Address_type dataSize = 0;
        const char* pDataFlags = 0;
        int rangeFlags = 0;
        std::function<void(WorkAddressData*)> completeHandler;

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
            std::function<void(WorkAddressData*)>&& completeHandler_in)
            :
            pDataStart(pDataStart_in),
            dataSize(dataSize_in),
            pDataFlags(pDataFlags_in),
            rangeFlags(rangeFlags_in),
            completeHandler(std::move(completeHandler_in))
        {
        }
    };

    struct IPeristentItemStorage;
    struct IWorkPlaceItem
    {
        virtual ~IWorkPlaceItem() {}
        virtual WorkAddressRangeInfo GetRangeInfo(Address_type address) const = 0;
        virtual const std::shared_ptr<CModuleManager> GetModuleManager() const = 0;
        virtual WorkAddressData ReadData(Address_type address, Address_type size) = 0;
        virtual oui::String GetShortName() const = 0;
        virtual void ReloadModules() = 0;
        virtual void GetModules(std::vector<orthia::ModuleInfo>& modules) const = 0;
        virtual int GetModulesCount() const = 0;
        virtual std::shared_ptr<IPeristentItemStorage> GetPersistentStorage() = 0;
        virtual int GetDianaMode() const = 0;
    };


    struct GotoItem
    {
        orthia::CCommonDateTime lastUpdateTime;
        orthia::Address_type address = 0;
        oui::String comment;
        int flags = 0;

        GotoItem()
        {

        }
        GotoItem(const orthia::Address_type & address_in, int flags_in)
            :
            address(address_in), flags(flags_in)
        {
            lastUpdateTime.InitFromCurrentTime();
        }
    };
    using ThreadPtr_type = std::shared_ptr<oui::CWindowThread>;
    using QueryGotoItemHandler_type = std::function<void(std::shared_ptr<oui::BaseOperation> operation,
        const oui::String& filter,
        const std::vector<GotoItem>& data,
        int error)>;
    using GotoCompleteHandler_type = std::function<oui::fsui::OpenResult(orthia::Address_type address, int error)>;
    using FetchCompleteHandler_type = std::function<oui::fsui::OpenResult(orthia::Address_type address, int error, orthia::Address_type pageAddress)>;

    struct IPeristentItemStorage
    {
        virtual ~IPeristentItemStorage() {}

        const static int goto_flags_history_mode = 1;
        virtual void AsyncQueryGotoInfo(ThreadPtr_type targetThread,
            const oui::String& filter,
            oui::OperationPtr_type<QueryGotoItemHandler_type> filterHandler,
            int flags) = 0;
        
        virtual void AsyncUpdateGotoInfo(ThreadPtr_type targetThread,
            oui::OperationPtr_type<GotoCompleteHandler_type> gotoHandler,
            orthia::Address_type address,
            int flags,
            orthia::Address_type pageAddress) = 0;

        virtual void AsyncFetchPrevHistory(ThreadPtr_type targetThread,
            oui::OperationPtr_type<FetchCompleteHandler_type> gotoHandler) = 0;
    };


    class ÑPersistentItemStorage :public IPeristentItemStorage
    {
        std::map<orthia::Address_type, GotoItem> m_dataItems;

        struct HistoryGotoItem :public GotoItem
        {
            orthia::Address_type pageAddress = 0;
            HistoryGotoItem(orthia::Address_type address_in, int flags_in, orthia::Address_type pageAddress_in)
                :
                GotoItem(address_in, flags_in), pageAddress(pageAddress_in)
            {
            }
        };
        std::vector<HistoryGotoItem> m_history;
        int m_historyIndex = -1;
    public:
        void AsyncQueryGotoInfo(ThreadPtr_type targetThread,
            const oui::String& filter,
            oui::OperationPtr_type<QueryGotoItemHandler_type> filterHandler,
            int flags);

        void AsyncUpdateGotoInfo(ThreadPtr_type targetThread,
            oui::OperationPtr_type<GotoCompleteHandler_type> gotoHandler,
            orthia::Address_type address,
            int flags,
            orthia::Address_type pageAddress);

        void AsyncFetchPrevHistory(ThreadPtr_type targetThread,
            oui::OperationPtr_type<FetchCompleteHandler_type> gotoHandler);
    };


    // database flags
    const static int g_database_flags_moduleMetaInfo = 1;
}