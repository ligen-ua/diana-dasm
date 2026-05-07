#pragma once

#include "orthia_utils.h"
#include "orthia_interfaces.h"
#include "orthia_text_manager.h"
#include "oui_string.h"
#include "oui_text_markup.h"
#include "oui_window_thread.h"
#include "oui_filesystem.h"
#include "orthia_common_time.h"
#include "orthia_common_print.h"

extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

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

    struct ModuleInfo :WorkAddressRangeInfo
    {
        static const int flags_analyzeDone = 1;
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
        static const int flags_PrivateSymbol = 8;
        Address_type address;
        oui::String name;
        oui::String privateSymbol;
        oui::String comment;
        int flags = 0;
    };
    oui::String GetPreferredName(const NameInfo& nameInfo);
    oui::String GetPreferredComment(const NameInfo& nameInfo);

    struct NameSelectionKey
    {
        static const int flags_ContinueFrom = 1;
        Address_type address;
        oui::String name;
        int flags = 0;
        int continueMarkNameFlag = 0;
        bool excludeImports = false;
    };

    struct MarkupRangeInfo
    {
        int sizeInLines = 0;
    };

    struct MarkupLine
    {
        static const int flags_HasXRefs = 1;

        oui::String text;
        oui::TextMarkup markup;
        bool hasMarkup = false;
        int flags = 0;

        MarkupLine() = default;
        explicit MarkupLine(const oui::String& t) : text(t) {}
        explicit MarkupLine(oui::String&& t) : text(std::move(t)) {}
    };

    struct MarkupRange
    {
        std::vector<MarkupLine> lines;
    };

    struct ExportLineInfo
    {
        oui::String displayName; // pre-computed "moduleName!exportName"
    };

    struct IMarkupCache
    {
        virtual ~IMarkupCache() {}
        virtual bool QueryReferences(Address_type address,
            const CommonReferenceInfoArray_type** refs) const = 0;
        virtual bool QueryExportInfo(Address_type address,
            const ExportLineInfo** info) const = 0;
    };

    struct MarkupRangeCache : IMarkupCache
    {
        const std::vector<CommonRangeInfo>& m_refs;
        std::vector<std::pair<Address_type, ExportLineInfo>> m_exports; // sorted by address

        MarkupRangeCache(const std::vector<CommonRangeInfo>& refs,
            std::vector<std::pair<Address_type, ExportLineInfo>> exports)
            : m_refs(refs), m_exports(std::move(exports)) {}

        bool QueryReferences(Address_type address,
            const CommonReferenceInfoArray_type** refs) const override
        {
            auto it = std::lower_bound(m_refs.begin(), m_refs.end(), address,
                [](const CommonRangeInfo& r, Address_type a) { return r.address < a; });
            if (it != m_refs.end() && it->address == address)
            {
                *refs = &it->references;
                return true;
            }
            return false;
        }

        bool QueryExportInfo(Address_type address,
            const ExportLineInfo** info) const override
        {
            auto it = std::lower_bound(m_exports.begin(), m_exports.end(), address,
                [](const std::pair<Address_type, ExportLineInfo>& e, Address_type a) { return e.first < a; });
            if (it != m_exports.end() && it->first == address)
            {
                *info = &it->second;
                return true;
            }
            return false;
        }
    };

    struct IUILogInterface
    {
        virtual ~IUILogInterface() {}
        virtual void WriteLog(const oui::String& line) = 0;
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
        virtual void QueryNames(Address_type moduleAddress, const NameSelectionKey& name, int count, std::vector<NameInfo>& names) const = 0;
        virtual int QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const = 0;
        virtual MarkupRangeInfo QueryMarkupRange(Address_type address, IMarkupCache* cache = nullptr) const = 0;
        virtual void QueryMarkupRange(Address_type address, int index, int count, MarkupRange& range, IMarkupCache* cache = nullptr) const = 0;
        virtual NameInfo QueryAddressName(Address_type address) const = 0;
        virtual std::shared_ptr<::DianaMovableReadStream> CreateDisasmStream(Address_type addressStart) = 0;
        virtual Address_type QueryAddressByName(const oui::String& text, Address_type defValue) const = 0;
        virtual std::shared_ptr<oui::IProcess> GetAssociatedProcess() { return nullptr; }
        virtual void OnPrivateSymbolLoaded(Address_type /*addr*/, const oui::String& /*name*/) {}
        virtual void OnModuleSymbolsLoaded(Address_type /*moduleAddress*/) {}
        virtual std::shared_ptr<IMemoryReader> CreateMemoryReader() = 0;
        virtual void UpdateModuleFlags(Address_type moduleAddress, int flagsToSet, int flagsToRemove) = 0;
    };

    void AppendXrefLine(Address_type address, IMarkupCache* cache, CModuleManager* moduleManager, int dianaMode, std::vector<MarkupLine>& allLines);

    template<class PtrType>
    oui::String QueryAddressNameDef(PtrType ptr, Address_type address, int dianaMode)
    {
        auto str = ptr->QueryAddressName(address);
        if (str.name.native.empty())
        {
            return orthia::AddressToString(address, dianaMode);
        }
        return str.name;
    }

    struct GotoItem
    {
        orthia::CCommonDateTime lastUpdateTime;
        orthia::Address_type address = 0;
        orthia::NameInfo nameInfo;
        int flags = 0;

        GotoItem()
        {

        }
        GotoItem(const orthia::Address_type& address_in, int flags_in)
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
    using AsyncCommentCompleteHandler_type = std::function<oui::fsui::OpenResult(orthia::Address_type address, int error, const oui::String& comment)>;

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
        virtual oui::fsui::OpenResult SyncWriteComment(orthia::Address_type address, const oui::String& comment) = 0;
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

        std::weak_ptr<IWorkPlaceItem> m_item;
    public:
        void Init(std::shared_ptr<IWorkPlaceItem> item);
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
    const static int g_database_type_fnc_PrivateSymbol = 4;

    oui::String ComposeName(const oui::String& name, Address_type nameAddress, Address_type address);
    oui::String ComposeName(const oui::String& name, Address_type nameAddress, Address_type address, const oui::String& moduleName, Address_type moduleAddress);

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

    class CXrefItemStorage : public IPeristentItemStorage
    {
        std::shared_ptr<CModuleManager> m_moduleManager;
        Address_type m_targetAddress;
    public:
        CXrefItemStorage(std::shared_ptr<CModuleManager> moduleManager, Address_type targetAddress);

        void AsyncQueryGotoInfo(ThreadPtr_type targetThread,
            const oui::String& filter,
            oui::OperationPtr_type<QueryGotoItemHandler_type> filterHandler,
            int flags) override;

        void AsyncUpdateGotoInfo(ThreadPtr_type targetThread,
            oui::OperationPtr_type<GotoCompleteHandler_type> gotoHandler,
            Address_type address, int flags, Address_type pageAddress) override;

        void AsyncFetchPrevHistory(ThreadPtr_type targetThread,
            oui::OperationPtr_type<FetchCompleteHandler_type> gotoHandler) override;

        oui::String SyncReadComment(Address_type address) override;
        oui::fsui::OpenResult SyncWriteComment(Address_type address, const oui::String& comment) override;
    };

}

namespace oui {
    static const int kMaxXrefs = 64;

    // MARKUP fields aka markup regions
    const std::uint32_t g_region_id_address     = oui::g_id_user_range + 1;
    const std::uint32_t g_region_id_operand     = oui::g_id_user_range + 2;
    const std::uint32_t g_region_id_xref_dialog = oui::g_id_user_range + 3;
    const std::uint32_t g_region_id_xref_0      = oui::g_id_user_range + 4;
    const std::uint32_t g_region_id_xref_last   = g_region_id_xref_0 + kMaxXrefs - 1;

}