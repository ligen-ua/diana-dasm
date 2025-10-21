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
    };


    struct GotoItem
    {
        orthia::CCommonDateTime lastUpdateTime;
        orthia::Address_type address = 0;
        oui::String comment;

        GotoItem()
        {

        }
        GotoItem(const orthia::Address_type & address_in)
            :
            address(address_in)
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

    struct IPeristentItemStorage
    {
        virtual ~IPeristentItemStorage() {}
        virtual void AsyncQueryGotoInfo(ThreadPtr_type targetThread,
            const oui::String& filter,
            oui::OperationPtr_type<QueryGotoItemHandler_type> filterHandler,
            int flags) = 0;
        
        virtual void AsyncUpdateGotoInfo(ThreadPtr_type targetThread,
            oui::OperationPtr_type<GotoCompleteHandler_type> gotoHandler,
            orthia::Address_type address) = 0;
    };


    class ÑPeristentItemStorage :public IPeristentItemStorage
    {
        std::map<orthia::Address_type, GotoItem> m_dataItems;
    public:
        void AsyncQueryGotoInfo(ThreadPtr_type targetThread,
            const oui::String& filter,
            oui::OperationPtr_type<QueryGotoItemHandler_type> filterHandler,
            int flags);

        void AsyncUpdateGotoInfo(ThreadPtr_type targetThread,
            oui::OperationPtr_type<GotoCompleteHandler_type> gotoHandler,
            orthia::Address_type address);
    };
}