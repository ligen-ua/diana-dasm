#pragma once

#include "orthia_utils.h"
#include "orthia_interfaces.h"
#include "oui_string.h"
#include "oui_window_thread.h"
#include "oui_filesystem.h"
#include "orthia_common_time.h"
#include "orthia_common_print.h"

namespace oui
{
    struct IProcess;
}
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
        PlatformString_type name;
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
            :
                rangeFlags(flags_FullInvalid)
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

    struct NameInfo
    {
        static const int flags_Function = 1;
        static const int flags_Import = 2;
        static const int flags_Export = 4;
        Address_type address;
        oui::String name;
        int flags = 0;
    };
    struct NameSelectionKey 
    {
        static const int flags_ContinueFrom = 1;
        Address_type address;
        oui::String name;
        int flags = 0;
        bool excludeImports = false;
    };

    struct MarkupRangeInfo
    {
        int sizeInLines = 0;
    };

    struct MarkupRange
    {
        std::vector<oui::String> lines;
    };

    struct IReferencesCache
    {
        virtual ~IReferencesCache() {}
        virtual bool QueryReferences(Address_type address,
            const CommonReferenceInfoArray_type** refs) const = 0;
    };

    struct ReferencesRangeCache : IReferencesCache
    {
        const std::vector<CommonRangeInfo>& m_data;
        explicit ReferencesRangeCache(const std::vector<CommonRangeInfo>& data)
            : m_data(data) {}

        bool QueryReferences(Address_type address,
            const CommonReferenceInfoArray_type** refs) const override
        {
            auto it = std::lower_bound(m_data.begin(), m_data.end(), address,
                [](const CommonRangeInfo& r, Address_type a) { return r.address < a; });
            if (it != m_data.end() && it->address == address)
            {
                *refs = &it->references;
                return true;
            }
            return false;
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
        virtual void QueryNames(Address_type moduleAddress, const NameSelectionKey& name, int count, std::vector<NameInfo> & names) const = 0;
        virtual int QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const = 0;
        virtual MarkupRangeInfo QueryMarkupRange(Address_type address, IReferencesCache* cache = nullptr) const = 0;
        virtual void QueryMarkupRange(Address_type address, int index, int count, MarkupRange& range, IReferencesCache* cache = nullptr) const = 0;
        virtual oui::String QueryAddressName(Address_type address) const = 0;
        virtual std::shared_ptr<::DianaMovableReadStream> CreateDisasmStream(Address_type addressStart) = 0;
        virtual Address_type QueryAddressByName(const oui::String & text, Address_type defValue) const = 0;
        virtual std::shared_ptr<oui::IProcess> GetAssociatedProcess() { return nullptr;  }
    };

    template<class PtrType>
    oui::String QueryAddressNameDef(PtrType ptr, Address_type address, int dianaMode)
    {
        auto str = ptr->QueryAddressName(address);
        if (str.native.empty())
        {
            return orthia::AddressToString(address, dianaMode);
        }
        return str;
    }

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
    using AsyncCommentCompleteHandler_type = std::function<oui::fsui::OpenResult(orthia::Address_type address, int error, const oui::String & comment)>;

    struct IPeristentItemStorage
    {
        virtual ~IPeristentItemStorage() {}

        inline const static int goto_flags_history_mode = 1;
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

        virtual oui::String SyncReadComment(orthia::Address_type address) = 0;
        virtual oui::fsui::OpenResult SyncWriteComment(orthia::Address_type address, const oui::String & comment) = 0;
    };


    class CPersistentItemStorage :public IPeristentItemStorage
    {
        orthia::CCriticalSection m_lock;

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

        struct CommentInfo
        {
            oui::String text;
        };
        std::unordered_map<orthia::Address_type, CommentInfo> m_comments;
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


        oui::String SyncReadComment(orthia::Address_type address);
        oui::fsui::OpenResult SyncWriteComment(orthia::Address_type address, const oui::String& comment);
    };


    // database flags
    const static int g_database_type_moduleMetaInfo = 1;
    const static int g_database_type_fnc_Import = 2;
    const static int g_database_type_fnc_Export = 3;

    oui::String ComposeName(const oui::String& name, Address_type nameAddress, Address_type address);

    // CFilePersistentItemStorage
    class CDatabaseManager;
    class CFilePersistentItemStorage :public CPersistentItemStorage
    {
        orthia::intrusive_ptr<CDatabaseManager> m_databaseManager;
    public:
        CFilePersistentItemStorage();
        void Init(orthia::intrusive_ptr<CDatabaseManager> databaseManager);
        oui::fsui::OpenResult SyncWriteComment(orthia::Address_type address, const oui::String& comment);
    };
}