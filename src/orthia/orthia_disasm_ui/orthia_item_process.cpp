#include "orthia_item_process.h"
#include "orthia_pe.h"
#include "orthia_memory_cache.h"
#include "orthia_streams.h"
#include "orthia_process_adapter.h"
#include "orthia_log.h"
#include "orthia_common_print.h"
#include "orthia_common_format.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"

namespace orthia
{

    // CProcessWorkplaceItem
    CProcessWorkplaceItem::CProcessWorkplaceItem(std::shared_ptr<oui::IProcess> proc,
        const oui::String& shortName,
        int dianaMode,
        std::shared_ptr<IPeristentItemStorage> persistentStorage)
        :
        m_proc(proc),
        m_shortName(shortName),
        m_dianaMode(dianaMode),
        m_persistentStorage(persistentStorage)
    {
    }
    void CProcessWorkplaceItem::Init(std::shared_ptr<CModuleManager> moduleManager,
        std::shared_ptr<CFilePersistentItemStorage> persistentItemStorage)
    {
        m_moduleManager = moduleManager;
        m_persistentStorage = persistentItemStorage;
    }
    WorkAddressData CProcessWorkplaceItem::ReadData(Address_type address, Address_type size)
    {
        return m_proc->ReadExactEx(address, (size_t)size);
    }

    WorkAddressRangeInfo CProcessWorkplaceItem::GetRangeInfo(Address_type address) const
    {
        orthia::CAutoCriticalSection guard(m_lock);

        for (const auto& module : m_modules)
        {
            if (module.IsInRange(address))
            {
                return module;
            }
        }
        // no modules found, return address space as the big module
        int platformError = 0;
        unsigned long long fileSize = 0;
        std::tie(platformError, fileSize) = m_proc->GetSizeInBytes();
        WorkAddressRangeInfo info;
        info.address = 0;
        info.lastValidAddress = fileSize;
        info.entryPoint = 0;
        info.size = fileSize;
        info.dianaMode = m_dianaMode;
        return info;
    }

    const std::shared_ptr<CModuleManager> CProcessWorkplaceItem::GetModuleManager() const
    {
        return m_moduleManager;
    }
    oui::String CProcessWorkplaceItem::GetShortName() const
    {
        return m_shortName;
    }
    Address_type CProcessWorkplaceItem::GerProcessModuleAddress()
    {
        return m_processModuleAddress;
    }

    struct ExportsCollector:public diana::CBasePeLinkImportsObserver
    {
        const orthia::ModuleInfo* m_moduleInfo = 0;
        orthia::flat_map<orthia::Address_type, NameInfo> m_exports;
        bool m_fixupAddresses = false;

        ExportsCollector()
        {
        }
        void SetCurrentModule(const orthia::ModuleInfo * moduleInfo)
        {
            m_moduleInfo = moduleInfo;
        }
        void QueryFunctionByOrdinal(const char* pDllName,
            DI_UINT32 ordinal,
            OPERAND_SIZE* pAddress)
        {
            auto name = "$ordinal_" + orthia::ToAnsiStringAsHex(ordinal);
            QueryFunctionByName(pDllName, name.c_str(), 0, pAddress);
        }
        void QueryFunctionByName(const char* pDllName,
            const char* pFunctionName,
            DI_UINT32 hint,
            OPERAND_SIZE* pAddress)
        {
            if (!*pAddress)
            {
                return;
            }
            orthia::Address_type fncAddress =  *pAddress;
            if (m_fixupAddresses)
            {
                fncAddress += m_moduleInfo->address;
            }
            NameInfo exportInfo;
            exportInfo.address = fncAddress;
            exportInfo.name.native = (m_moduleInfo->name + OUI_STR("!") + orthia::Utf8ToPlatformString(pFunctionName));
            exportInfo.flags = NameInfo::flags_Export;
            m_exports.insert(fncAddress, exportInfo);
        }
    };
    template<class T, class Stream>
    void DeliverExtraExports(T& exportsCollector, Address_type moduleAddress, OPERAND_SIZE entryPoint, Stream& stream,
        DianaExecutable& exe)
    {
        if (entryPoint && entryPoint != moduleAddress)
        {
            exportsCollector.QueryFunctionByName("$", "$entrypoint", 0, &entryPoint);
        }
        // report tls callbacks
        void* pTlsCallbacks = 0;
        int tlsCallbacksCount = 0;
        OPERAND_SIZE addressOfTLSIndex = 0;
        if (!DianaExecutable_QueryTLSCallbacks(&exe,
            moduleAddress,
            &stream,
            &pTlsCallbacks,
            &tlsCallbacksCount,
            &addressOfTLSIndex,
            DIANA_ANALYZE_RANDOM_READ_ABSOLUTE))
        {
            char* pTls = (char*)pTlsCallbacks;
            for (int i = 0; i < tlsCallbacksCount; ++i)
            {
                OPERAND_SIZE callback = Diana_ReadValue(pTls, exe.dianaMode);
                auto name = "$tls_" + orthia::ToAnsiStringAsHex((unsigned short)i);
                exportsCollector.QueryFunctionByName("$", name.c_str(), 0, &callback);
                pTls += exe.dianaMode;
            }
            DIANA_FREE(pTlsCallbacks);
        }
    }
    void CProcessWorkplaceItem::ReloadModules()
    {
        std::vector<char> page(0x4000);
        ExportsCollector exportsCollector;
        std::vector<orthia::ModuleInfo> modules;
        int processModuleOffset = 0;
        m_proc->QueryModules(modules, processModuleOffset,
            [&](const orthia::ModuleInfo& moduleInfo, oui::ModuleDisasmContext& context) {

            exportsCollector.SetCurrentModule(&moduleInfo);

            exportsCollector.m_fixupAddresses = false;
            DeliverExtraExports(exportsCollector, moduleInfo.address, moduleInfo.entryPoint, *context.stream, *context.executable);
            exportsCollector.m_fixupAddresses = true;
            DianaExecutable_QueryExports(context.executable, &context.stream->parent, page.data(), (int)page.size(), exportsCollector.GetParent(), 0);
        });

        Address_type processModuleAddress = 0;
        if (!modules.empty() && processModuleOffset != -1 && processModuleOffset < (int)modules.size())
        {
            processModuleAddress = modules[processModuleOffset].entryPoint;
        }
        std::sort(modules.begin(), modules.begin(), [](auto& m1, auto& m2) { return m1.address < m2.address; });

        exportsCollector.m_exports.sort();

        orthia::CAutoCriticalSection guard(m_lock);
        m_modules = std::move(modules);
        m_exports = std::move(exportsCollector.m_exports);
        m_modulesIndex.clear();
        for (int i = 0, size = (int)m_modules.size(); i < size; ++i)
        {
            m_modulesIndex[m_modules[i].address] = i;
        }
        for (auto& mod : m_modules)
        {
            auto it = m_moduleFlags.find(mod.address);
            if (it != m_moduleFlags.end())
                mod.flags |= it->second;
        }
        if (m_processModuleAddress == 0)
        {
            m_processModuleAddress = processModuleAddress;
        }
    }
    void CProcessWorkplaceItem::OnPrivateSymbolLoaded(Address_type addr, const oui::String& name)
    {
        orthia::CAutoCriticalSection guard(m_lock);
        auto it = m_exports.find(addr);
        if (it != m_exports.end())
        {
            it->second.privateSymbol = name;
        }
    }
    void CProcessWorkplaceItem::OnModuleSymbolsLoaded(Address_type moduleAddress)
    {
        orthia::CAutoCriticalSection guard(m_lock);
        m_moduleFlags[moduleAddress] |= ModuleInfo::flags_symbolsLoaded;
        auto it = m_modulesIndex.find(moduleAddress);
        if (it != m_modulesIndex.end())
        {
            m_modules[it->second].flags |= ModuleInfo::flags_symbolsLoaded;
        }
    }
    void CProcessWorkplaceItem::GetModules(std::vector<orthia::ModuleInfo>& modules) const
    {
        orthia::CAutoCriticalSection guard(m_lock);
        modules = m_modules;
    }
    int CProcessWorkplaceItem::GetModulesCount() const
    {
        return (int)m_modules.size();
    }
    std::shared_ptr<IPeristentItemStorage> CProcessWorkplaceItem::GetPersistentStorage()
    {
        return m_persistentStorage;
    }


    class CImportsCollector :public diana::CBasePeLinkImportsObserver
    {
        std::vector<NameInfo>& m_names;
        std::function<NameInfo(OPERAND_SIZE address)> m_getName;
        int m_maxCount = 0;
        int & m_totalCount;
        int m_deliveredCount = 0;
        const NameSelectionKey& m_nameFilter;
        bool m_found = false;
        bool m_skipAll = false;
    public:
        CImportsCollector(const NameSelectionKey& nameFilter,
            std::vector<NameInfo>& names,
            std::function<NameInfo (OPERAND_SIZE address)> getName,
            int maxCount,
            int & totalCount)
            :
            m_names(names),
            m_getName(getName),
            m_maxCount(maxCount),
            m_totalCount(totalCount),
            m_nameFilter(nameFilter)
        {
            if ((m_nameFilter.flags & m_nameFilter.flags_ContinueFrom) &&
                m_nameFilter.continueMarkNameFlag != 0 &&
                m_nameFilter.continueMarkNameFlag != NameInfo::flags_Import)
            {
                m_skipAll = true;
                // m_found intentionally left false so IsMarkFound() returns false
                // and SetFound(false) is passed to the exports collector
            }
        }
        bool IsMarkFound() const
        {
            return m_found;
        }
        void QueryFunctionByOrdinal(const char* pDllName,
            DI_UINT32 ordinal,
            OPERAND_SIZE* pAddress)
        {
            auto name = "$ordinal_" + orthia::ToAnsiStringAsHex(ordinal);
            QueryFunctionByName(pDllName, name.c_str(), 0, pAddress);
        }
        void QueryFunctionByName(const char* pDllName,
            const char* pFunctionName,
            DI_UINT32 hint,
            OPERAND_SIZE* pAddress)
        {
            ++m_totalCount;
            if (m_skipAll) return;

            if (!m_found)
            {
                if (m_nameFilter.flags & m_nameFilter.flags_ContinueFrom)
                {
                    if (m_nameFilter.address == *pAddress)
                    {
                        m_found = true;
                    }
                    return;
                }
                m_found = true;
            }
            if (m_deliveredCount < m_maxCount)
            {
                NameInfo info;
                info.flags = NameInfo::flags_Import;
                info.address = *pAddress;
                auto nameInfo = m_getName(info.address);
                info.name = nameInfo.name;

                if (info.name.native.empty())
                {
                    if (pFunctionName)
                    {
                        info.name.native = orthia::Utf8ToPlatformString(pFunctionName);
                    }
                    else
                    {
                        info.name.native = OUI_TCSTR("<unknown>");
                    }
                }
                m_names.push_back(info);
                ++m_deliveredCount;
            }
        }

        int GetDeliveredCount() const
        {
            return m_deliveredCount;
        }
    };



    struct ModuleExportsCollector :public diana::CBasePeLinkImportsObserver
    {
        const NameSelectionKey& m_nameFilter;
        std::vector<NameInfo>& m_names;
        int m_maxCount = 0;
        int& m_totalCount;
        int m_deliveredCount = 0;
        bool m_found = false;
        OPERAND_SIZE m_moduleStart;

        ModuleExportsCollector(const NameSelectionKey& nameFilter, 
            std::vector<NameInfo>& names,
            int maxCount, 
            int& totalCount, 
            int deliveredCount,
            OPERAND_SIZE moduleStart)
            :
            m_nameFilter(nameFilter),
            m_names(names),
            m_maxCount(maxCount),
            m_totalCount(totalCount),
            m_deliveredCount(deliveredCount),
            m_moduleStart(moduleStart)
        {
        }
        void SetFound(bool markFound)
        {
            m_found = markFound;
        }
        bool IsMarkFound() const
        {
            return m_found;
        }
        void QueryFunctionByOrdinal(const char* pDllName,
            DI_UINT32 ordinal,
            OPERAND_SIZE* pAddress)
        {
            auto name = "$ordinal_" + orthia::ToAnsiStringAsHex(ordinal);
            QueryFunctionByName(pDllName, name.c_str(), 0, pAddress);
        }
        void QueryFunctionByName(const char* pDllName,
            const char* pFunctionName,
            DI_UINT32 hint,
            OPERAND_SIZE* pAddressIn)
        {
            if (!*pAddressIn)
            {
                return;
            }
            auto address = *pAddressIn;
            if (!pDllName || *pDllName != '$') 
            {
                if (Diana_SafeAdd(&address, m_moduleStart))
                {
                    return;
                }
            };
            auto functionName = orthia::Utf8ToPlatformString(pFunctionName);
            ++m_totalCount;
            if (!m_found)
            {
                if (m_nameFilter.flags & m_nameFilter.flags_ContinueFrom)
                {
                    if (m_nameFilter.address == address)
                    {
                        m_found = true;
                    }
                    return;
                }
                m_found = true;
            }
            if (m_deliveredCount < m_maxCount)
            {
                NameInfo info;
                info.flags = NameInfo::flags_Export;
                info.address = address;
                info.name = functionName;
                m_names.push_back(info);
                ++m_deliveredCount;
            }
        }

        int GetDeliveredCount() const
        {
            return m_deliveredCount;
        }
    };

    void CProcessWorkplaceItem::QueryNamesEx(Address_type moduleAddress, const NameSelectionKey& nameFilter, int count, std::vector<NameInfo>& names, int * totalCount)const
    {
        if (totalCount)
        {
            *totalCount = 0;
        }
        names.clear(); 
        Address_type moduleSize = 0, entryPoint = 0;
        {
            orthia::CAutoCriticalSection guard(m_lock);

            auto it = m_modulesIndex.find(moduleAddress);
            if (it == m_modulesIndex.end())
            {
                return;
            }
            auto & module = m_modules.at(it->second);
            moduleSize = module.size;
            entryPoint = module.entryPoint;
        }
       
        // diana PE analyzer uses relative pointers
        orthia::ProcessReaderAdapter memReader(m_proc.get());
        orthia::CMemoryStorageOfModifiedData writeCache(&memReader, 0x1000);
        writeCache.SetCacheReads(true);
        orthia::CMemoryCache module(&writeCache, moduleAddress);
        // adapter to C-code
        orthia::DianaMemoryStream stream(0, &module, moduleSize);

        DianaExecutable exe = {};
        if (int status = DianaExecutable_Init(&exe,
            &stream.parent,
            moduleSize,
            DIANA_EXECUTABLE_FILE_FLAGS_MODULE_MODE))
        {
            ORTHIA_DEV_LOG(orthia::LogSeverity::Debug, "Module: ", orthia::CLogParamEx(moduleAddress, 16), ":",orthia::CLogParamEx(moduleSize, 16), " ->", (long)status);
            return;
        }
        diana::Guard<diana::ExecutableFile> exeGuard(&exe);

        int importsCount = 0;
        CImportsCollector importsCollector(nameFilter, names, [this](auto address) {

            return QueryAddressName(address);
        },
            count,
            importsCount);

        std::vector<char> page(4096);
        if (!nameFilter.excludeImports)
        {
            DianaExecutable_QueryImports(&exe,
                moduleAddress,
                &stream,
                page.data(),
                (int)page.size(),
                importsCollector.GetParent(),
                DIANA_ANALYZE_RANDOM_READ_ABSOLUTE,
                0);
        }
        if (totalCount)
        {
            *totalCount = importsCount;
        }

        int maxCount = count - importsCollector.GetDeliveredCount();
        bool markFound = importsCollector.IsMarkFound();
        if (maxCount || totalCount)
        {
            int exportsCount = 0;
            // deliver exports
            ModuleExportsCollector exportsCollector(nameFilter,
                names,
                maxCount,
                exportsCount,
                0,
                moduleAddress);

            exportsCollector.SetFound(importsCollector.IsMarkFound());

            DeliverExtraExports(exportsCollector, moduleAddress, entryPoint, stream, exe);

            // report regular exports
            DianaExecutable_QueryExports(&exe,
                    &stream.parent,
                    page.data(),
                    (int)page.size(),
                    exportsCollector.GetParent(),
                    0);

            if (totalCount)
            {
                *totalCount += exportsCount;
            }
            maxCount -= exportsCollector.GetDeliveredCount();
            markFound = exportsCollector.IsMarkFound();
        }

        // Also include private symbols from the database (e.g., loaded from PDB).
        if (m_moduleManager)
        {
            auto classicDatabase = m_moduleManager->QueryDatabaseManager()->GetClassicDatabase();
            classicDatabase->QueryMetaInfoModule2(moduleAddress,
                g_database_type_fnc_PrivateSymbol, -1,
                [&, markFound](Address_type, int, const std::string& text, Address_type) mutable -> bool {
                    if (totalCount)
                    {
                        ++(*totalCount);
                        return true;
                    }
                    std::string nameStr;
                    Address_type target = 0;
                    CCommonFormatParser parser;
                    parser.Parse(text);
                    parser.QueryMetadata("address", &target);
                    parser.QueryMetadata("name", &nameStr);

                    if (!markFound)
                    {
                        if (target == nameFilter.address)
                        {
                            markFound = true;
                        }
                        return true;
                    }

                    NameInfo info;
                    info.name = Utf8ToPlatformString(nameStr);
                    info.address = target;
                    info.flags = NameInfo::flags_PrivateSymbol;
                    names.push_back(info);
                    return !count || (int)names.size() < count;
                });
        }
    }
    void CProcessWorkplaceItem::QueryNames(Address_type moduleAddress, const NameSelectionKey& name, int count, std::vector<NameInfo>& names)const
    {
        QueryNamesEx(moduleAddress, name, count, names, nullptr);
    }

    int CProcessWorkplaceItem::QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const
    {
        int totalCount = 0;
        std::vector<NameInfo> names;
        QueryNamesEx(moduleAddress, name, 0, names, &totalCount);
        return totalCount;
    }

    MarkupRangeInfo CProcessWorkplaceItem::QueryMarkupRange(Address_type address, IMarkupCache* cache) const
    {
        MarkupRangeInfo res;
        auto it = m_exports.find(address);
        if (it != m_exports.end())
        {
            res.sizeInLines = 1;
        }

        if (m_moduleManager)
        {
            const CommonReferenceInfoArray_type* refsPtr = nullptr;
            std::vector<CommonReferenceInfo> refsStorage;
            if (!cache)
            {
                m_moduleManager->QueryReferencesToInstruction(address, &refsStorage);
                refsPtr = &refsStorage;
            }
            else
            {
                cache->QueryReferences(address, &refsPtr);
            }
            if (refsPtr && !refsPtr->empty())
            {
                ++res.sizeInLines;
            }
        }
        return res;
    }
    void CProcessWorkplaceItem::QueryMarkupRange(Address_type address, int index, int count, MarkupRange& range, IMarkupCache* cache) const
    {
        range.lines.clear();
        if (count == 0)
        {
            return;
        }

        std::vector<MarkupLine> allLines;
        auto nameInfo = QueryAddressName(address);
        if ((nameInfo.flags & NameInfo::flags_Export) ||
            (nameInfo.flags & NameInfo::flags_PrivateSymbol))
        {
            allLines.push_back(MarkupLine(GetPreferredName(nameInfo)));
        }
        
        AppendXrefLine(address, cache, m_moduleManager.get(), GetDianaMode(), allLines);

        for (int i = index; i < (int)allLines.size() && (int)range.lines.size() < count; ++i)
        {
            range.lines.push_back(allLines[i]);
        }
    }
    bool CProcessWorkplaceItem::QueryAddressModule(Address_type address, orthia::ModuleInfo & result) const
    {
        orthia::CAutoCriticalSection guard(m_lock);

        auto it = m_modulesIndex.lower_bound(address), it_end = m_modulesIndex.end();
        if (it != m_modulesIndex.begin())
        {
            --it;
        }
        for (; it != it_end; ++it)
        {
            auto& module = m_modules[it->second];
            if (module.address > address)
            {
                break;
            }
            if (module.IsInRange(address))
            {
                result = module;
                return true;
            }
        }
        return false;
    }
    std::shared_ptr<oui::IProcess> CProcessWorkplaceItem::GetAssociatedProcess()
    {
        return m_proc;
    }
    Address_type CProcessWorkplaceItem::QueryAddressByName(const oui::String& text, Address_type defValue) const
    {
        auto downcased = orthia::Downcase(text.native);
        orthia::CAutoCriticalSection guard(m_lock);
        {
            auto it = std::find_if(m_exports.begin(), m_exports.end(), [&](const auto& pair) { return orthia::Downcase(pair.second.name.native) == downcased;  });
            if (it != m_exports.end())
            {
                return it->first;
            }
        }

        {
            auto it = std::find_if(m_modules.begin(), m_modules.end(), [&](const auto& mod) { return orthia::Downcase(mod.name) == downcased;  });
            if (it != m_modules.end())
            {
                return it->address;
            }
        }
        return defValue;
    }

    std::shared_ptr<::DianaMovableReadStream> CProcessWorkplaceItem::CreateDisasmStream(Address_type addressStart)
    {
        struct DianaReadStreamAdapter
        {
            std::shared_ptr<oui::IProcess> proc;
            orthia::ProcessReaderAdapter memReader;
            orthia::CMemoryStorageOfModifiedData cache;
            orthia::DianaMemoryStream stream;

            DianaReadStreamAdapter(std::shared_ptr<oui::IProcess> proc_in, Address_type addressStart)
                :
                proc(proc_in),
                memReader(proc_in.get()),
                cache(&memReader, 4096),
                stream(addressStart, &cache, 0)
            {
                cache.SetCacheReads(true);
            }
        };

        auto streamAdapter = std::make_shared<DianaReadStreamAdapter>(m_proc, addressStart);
        return std::shared_ptr<::DianaMovableReadStream>(streamAdapter, &streamAdapter->stream.parent);
    }
    NameInfo CProcessWorkplaceItem::QueryAddressName(Address_type address) const
    {
        orthia::CAutoCriticalSection guard(m_lock);
        return QueryAddressNameNoLock(address);
    }
    NameInfo CProcessWorkplaceItem::QueryAddressNameNoLock(Address_type address) const
    {
        orthia::ModuleInfo moduleInfo;
        auto nameInfo = QueryAddressNameImpl(address, moduleInfo);
        if (m_persistentStorage)
        {
            auto comment = m_persistentStorage->SyncReadComment(address);
            if (!comment.native.empty())
            {
                nameInfo.comment = comment;
            }
        }
        if ((moduleInfo.flags & ModuleInfo::flags_symbolsLoaded) && nameInfo.privateSymbol.native.empty())
        {
            if (m_moduleManager)
            {
                auto classicDatabase = m_moduleManager->QueryDatabaseManager()->GetClassicDatabase();
                classicDatabase->QueryMetaInfoByNearestAddress(
                    g_database_type_fnc_PrivateSymbol,
                    address,
                    [&](Address_type, int, const std::string& text, Address_type metaAddress) -> bool {

                        std::string nameStr;
                        Address_type target = 0;
                        CCommonFormatParser parser;
                        parser.Parse(text);
                        parser.QueryMetadata("address", &target);
                        parser.QueryMetadata("name", &nameStr);
                        if (target == address)
                        {
                            nameInfo.flags |= NameInfo::flags_PrivateSymbol;
                            nameInfo.privateSymbol = Utf8ToPlatformString(nameStr);
                        }
                        else
                        {
                            nameInfo.privateSymbol = ComposeName(orthia::Utf8ToPlatformString(nameStr), target, address);
                        }
                        return false;
                    });
            }
        }
        return nameInfo;
    }
    NameInfo CProcessWorkplaceItem::QueryAddressNameImpl(Address_type address, orthia::ModuleInfo & moduleInfo) const
    {
        orthia::Address_type capturedAddress = 0;
        NameInfo capturedInfo;
        {
            auto it = m_exports.upper_bound(address);
            auto it_end = m_exports.end();
            if (it != it_end && it != m_exports.begin())
            {
                --it;
            }
            for(;it != it_end; ++it)
            {
                if (address >= it->first)
                {
                    capturedAddress = it->first;
                    capturedInfo = it->second;
                    break;
                }
            }
        }
        if (address == capturedAddress)
        {
            capturedInfo.flags |= NameInfo::flags_Export;
            return capturedInfo;
        }
        if (QueryAddressModule(address, moduleInfo))
        {
            if (moduleInfo.address > capturedAddress)
            {
                // export function is not found or there is module with a closest address
                NameInfo r;
                r.name = ComposeName(moduleInfo.name, moduleInfo.address, address);
                return r;
            }
        }
        // use found function
        if (!capturedInfo.name.native.empty())
        {
            NameInfo r;
            r.name = ComposeName(capturedInfo.name, capturedAddress, address);
            return r;
        }
        return NameInfo();
    }

}