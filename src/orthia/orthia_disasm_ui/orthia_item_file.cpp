#include "orthia_item_file.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include "orthia_common_format.h"
#include "orthia_common_print.h"

namespace orthia
{
    FileWorkplaceItem::FileWorkplaceItem(std::shared_ptr<CFilePersistentItemStorage> peristentItemStorage_in)
        : persistentItemStorage(peristentItemStorage_in)
    {
    }
    // FileWorkplaceItem
    WorkAddressData FileWorkplaceItem::ReadData(Address_type address, Address_type size)
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
        if (lastValid < file->GetImageBase() ||
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

        if (address >= file->GetImageBase() &&
            lastValid <= moduleLastValidAddress)
        {
            // the entire range is good
            auto offset = address - file->GetImageBase();
            auto pBufferStart = file->GetMappedFile().data() + offset;
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
        if (address < file->GetImageBase())
        {
            startInvalidBytes = file->GetImageBase() - address;
        }
        else
        {
            positiveAddress = address - file->GetImageBase();
        }
        Address_type startValidBytes = (size - startInvalidBytes) - positiveAddress;
        Address_type endInvalidBytes = 0;
        if (startValidBytes > file->GetMappedFile().size())
        {
            endInvalidBytes = startValidBytes - file->GetMappedFile().size();
            startValidBytes = file->GetMappedFile().size();
        }
        if (startInvalidBytes + startValidBytes + endInvalidBytes != size)
        {
            // something is just plain wrong, the main assumption is broken
            return WorkAddressData();
        }

        // ok here we go, prepare the final data
        std::vector<char> buffer(size);
        auto* pBufferStart = buffer.data();
        memcpy(pBufferStart + startInvalidBytes, file->GetMappedFile().data() + positiveAddress, startValidBytes);

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

    WorkAddressRangeInfo FileWorkplaceItem::GetRangeInfo(Address_type address) const
    {
        return {
            file->GetImageBase(),
            moduleLastValidAddress,
            file->GetEntryPoint(),
            file->GetMappedFile().size(),
            file->GetDianaMode()
        };
    }

    const std::shared_ptr<CModuleManager> FileWorkplaceItem::GetModuleManager() const
    {
        return moduleManager;
    }
    oui::String FileWorkplaceItem::GetShortName() const
    {
        return shortName;
    }
    void FileWorkplaceItem::ReloadModules()
    {
        // do nothing
    }
    void FileWorkplaceItem::QueryNames(Address_type moduleAddress, const NameSelectionKey& nameFilter, int count, std::vector<NameInfo>& names) const
    {
        if (!count)
        {
            return;
        }
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        names.clear();
        names.reserve(1024);

        bool pageFound = false;
        auto handler = [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
        {
            std::string name;
            Address_type target = 0;
            CCommonFormatParser parser;
            parser.Parse(text);
            parser.QueryMetadata("address", &target);
            parser.QueryMetadata("name", &name);

            NameInfo info;
            info.name = orthia::Utf8ToPlatformString(name);
            info.address = target;
            if (metaType == g_database_type_fnc_Import)
            {
                info.flags = NameInfo::flags_Import;
            }
            else if (metaType == g_database_type_fnc_Export)
            {
                info.flags = NameInfo::flags_Export;
            }
            else if (metaType == g_database_type_fnc_PrivateSymbol)
            {
                info.flags = NameInfo::flags_PrivateSymbol;
            }
            if (nameFilter.flags & nameFilter.flags_ContinueFrom)
            {
                if (nameFilter.address == target)
                {
                    pageFound = true;
                    return true;
                }
                if (!pageFound)
                {
                    return true;
                }
            }
            names.push_back(info);
            if ((int)names.size() >= count)
            {
                return false;
            }
            return true;
        };

        bool continueFromPrivate = (nameFilter.flags & nameFilter.flags_ContinueFrom) &&
                                   nameFilter.continueMarkNameFlag == NameInfo::flags_PrivateSymbol;
        if (!continueFromPrivate)
        {
            if (nameFilter.excludeImports)
            {
                classicDatabase->QueryMetaInfoModule2(moduleAddress,
                    g_database_type_fnc_Export, -1,
                    handler);
            }
            else
            {
                classicDatabase->QueryMetaInfoModule2(moduleAddress,
                    g_database_type_fnc_Import, g_database_type_fnc_Export,
                    handler);
            }
        }
        classicDatabase->QueryMetaInfoModule2(moduleAddress,
            g_database_type_fnc_PrivateSymbol, -1,
            handler);
    }
    int FileWorkplaceItem::QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const
    {
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        return classicDatabase->QueryMetaInfoModule2_Count(moduleAddress, g_database_type_fnc_Import, g_database_type_fnc_Export)
             + classicDatabase->QueryMetaInfoModule2_Count(moduleAddress, g_database_type_fnc_PrivateSymbol, -1);
    }
    int FileWorkplaceItem::GetModulesEx(bool calcCount, std::vector<orthia::ModuleInfo>& modules) const
    {
        int count = 0;
        modules.clear();
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();

        std::vector<CommonModuleInfo> dbModules;
        classicDatabase->QueryModules(&dbModules);
        if (calcCount)
        {
            return (int)dbModules.size();
        }
        for (auto& dbm : dbModules)
        {
            orthia::ModuleInfo info;
            info.name = dbm.name;
            info.fullName = dbm.name;
            info.address = dbm.address;
            if (dbm.size)
            {
                info.lastValidAddress = dbm.address + (dbm.size - 1);
            }
            else
            {
                info.lastValidAddress = dbm.address;
            }
            info.entryPoint = info.address;
            info.size = dbm.size;
            info.dianaMode = file->GetDianaMode();
            modules.push_back(info);
        }

        auto moduleIt = modules.begin();
        classicDatabase->QueryMetaInfo(g_database_type_moduleMetaInfo, [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
        {
            for (;; ++moduleIt)
            {
                if (moduleIt == modules.end())
                {
                    return false;
                }
                if (moduleIt->address > moduleAddress)
                {
                    return true;
                }
                if (moduleIt->address == moduleAddress)
                {
                    break;
                }
            }

            CCommonFormatParser parser;
            parser.Parse(text);
            parser.QueryMetadata(ORTHIA_TCSTR("fullname"), &moduleIt->fullName);
            return true;
        });


        return count;
    }

    void FileWorkplaceItem::GetModules(std::vector<orthia::ModuleInfo>& modules) const
    {
        GetModulesEx(false, modules);
    }
    int FileWorkplaceItem::GetModulesCount() const
    {
        std::vector<orthia::ModuleInfo> modules;
        return GetModulesEx(true, modules);
    }
    std::shared_ptr<IPeristentItemStorage> FileWorkplaceItem::GetPersistentStorage()
    {
        return persistentItemStorage;
    }
    int FileWorkplaceItem::GetDianaMode() const
    {
        return file->GetDianaMode();
    }
    MarkupRangeInfo FileWorkplaceItem::QueryMarkupRange(Address_type address, IMarkupCache* cache) const
    {
        MarkupRangeInfo result;

        const ExportLineInfo* exportPtr = nullptr;
        if (!cache)
        {
            auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
            classicDatabase->QueryMetaInfoByAddress(g_database_type_fnc_Export,
                address,
                [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
            {
                result.sizeInLines = 1;
                return false;
            });
        }
        else
        {
            cache->QueryExportInfo(address, &exportPtr);
            if (exportPtr)
                result.sizeInLines = 1;
        }

        const CommonReferenceInfoArray_type* refsPtr = nullptr;
        std::vector<CommonReferenceInfo> refsStorage;
        if (!cache)
        {
            moduleManager->QueryReferencesToInstruction(address, &refsStorage);
            refsPtr = &refsStorage;
        }
        else
        {
            cache->QueryReferences(address, &refsPtr);
        }
        if (refsPtr && !refsPtr->empty())
        {
            ++result.sizeInLines;
        }
        return result;
    }
    void FileWorkplaceItem::QueryMarkupRange(Address_type address, int index, int count, MarkupRange& range, IMarkupCache* cache) const
    {
        range.lines.clear();
        if (count == 0)
        {
            return;
        }
        std::vector<MarkupLine> allLines;

        const ExportLineInfo* exportPtr = nullptr;
        if (!cache)
        {
            auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
            Address_type capturedModuleAddress = 0;
            classicDatabase->QueryMetaInfoByAddress(g_database_type_fnc_Export,
                address,
                [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
            {
                capturedModuleAddress = moduleAddress;

                std::string name;
                Address_type target = 0;
                CCommonFormatParser parser;
                parser.Parse(text);
                parser.QueryMetadata("address", &target);
                parser.QueryMetadata("name", &name);

                allLines.push_back(MarkupLine(orthia::Utf8ToPlatformString(name)));
                return false;
            });

            if (!allLines.empty())
            {
                CommonModuleInfo info;
                if (classicDatabase->QueryModule(capturedModuleAddress, &info))
                {
                    for (auto& line : allLines)
                    {
                        line.text.native = info.name + OUI_TCSTR("!") + line.text.native;
                    }
                }
            }
        }
        else
        {
            cache->QueryExportInfo(address, &exportPtr);
            if (exportPtr)
                allLines.push_back(MarkupLine(exportPtr->displayName));
        }

        AppendXrefLine(address, cache, moduleManager.get(), GetDianaMode(), allLines);

        for (int i = index; i < (int)allLines.size() && (int)range.lines.size() < count; ++i)
        {
            range.lines.push_back(allLines[i]);
        }
    }
    NameInfo FileWorkplaceItem::QueryAddressName(Address_type address) const
    {
        auto nameInfo = QueryAddressNameImpl(address);
        if (persistentItemStorage)
        {
            auto comment = persistentItemStorage->SyncReadComment(address);
            if (!comment.native.empty())
            {
                nameInfo.name = comment;
            }
        }
        return nameInfo;
    }
    NameInfo FileWorkplaceItem::QueryAddressNameImpl(Address_type address) const
    {
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        Address_type capturedModuleAddress = 0;
        Address_type capturedMetaAddress = 0;
        std::string capturedMetaName;

        NameInfo result;
        bool exportDone = false;
        classicDatabase->QueryMetaInfoByNearestAddress(g_database_type_fnc_Export,
            address,
            [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
        {
            capturedModuleAddress = moduleAddress;
            capturedMetaAddress = metaAddress;

            if (metaType == g_database_type_fnc_Export)
            {
                if (!exportDone)
                {
                    CCommonFormatParser parser;
                    parser.Parse(text);
                    parser.QueryMetadata("name", &capturedMetaName);
                }
            }
            // PrivateSymbol
            if (metaAddress == address)
            {
                Address_type target = 0;
                std::string nameStr;
                CCommonFormatParser parser;
                parser.Parse(text);
                parser.QueryMetadata("address", &target);
                parser.QueryMetadata("name", &nameStr);
                if (target == address)
                {
                    result.privateSymbol = Utf8ToPlatformString(nameStr);
                }
            }
            return false;
        },
            g_database_type_fnc_PrivateSymbol);

        CommonModuleInfo info;
        if (classicDatabase->QueryNearestModule(address, &info))
        {
            if (capturedMetaAddress > info.address)
            {
                Address_type diff = address - capturedMetaAddress;
                result.name.native = info.name + OUI_TCSTR("!") + orthia::Utf8ToPlatformString(capturedMetaName);
                if (diff)
                {
                    result.name = ComposeName(result.name, capturedMetaAddress, address);
                }
            }
            else
            {
                result.name = ComposeName(info.name, info.address, address);
            }
        }

        return result;
    }
    Address_type FileWorkplaceItem::QueryAddressByName(const oui::String& text, Address_type defValue) const
    {
        Address_type target = 0;
        bool found = false;
        auto downcased = orthia::Downcase(text.native);
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        classicDatabase->QueryMetaInfo(g_database_type_fnc_Export, [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
        {
  
            orthia::PlatformString_type name;
            CCommonFormatParser parser;
            parser.Parse(text);
            parser.QueryMetadata("address", &target);
            parser.QueryMetadata(OUI_TCSTR("name"), &name);

            auto downcased2 = orthia::Downcase(name);
            found = downcased2 == downcased;
            return !found;
        });
        if (found)
        {
            return target;
        }
        std::vector<CommonModuleInfo> dbModules;
        classicDatabase->QueryModules(&dbModules);
        for (auto& dbm : dbModules)
        {
            auto downcased2 = orthia::Downcase(dbm.name);
            found = downcased2 == downcased;

            if (found)
            {
                target = dbm.address; 
                break;
            }
        }
        if (found)
        {
            return target;
        }
        return defValue;
    }
    std::shared_ptr<::DianaMovableReadStream> FileWorkplaceItem::CreateDisasmStream(Address_type addressStart)
    {
        struct DianaReadStreamAdapter
        {
            std::shared_ptr<orthia::ISimpleFile> file;
            ::DianaMemoryStream stream;

            DianaReadStreamAdapter(std::shared_ptr<orthia::ISimpleFile> file_in)
                :
                file(file_in)
            {
            }
        };

        if (addressStart < file->GetImageBase() || addressStart >= file->GetImageEnd())
        {
            return nullptr;
        }

        auto streamAdapter = std::make_shared<DianaReadStreamAdapter>(file);
        auto diff = addressStart - file->GetImageBase();

        auto& data = file->GetMappedFile();
        Diana_InitMemoryStreamEx2(&streamAdapter->stream, (char*)data.data()+diff, data.size()-diff, 0, 0);

        return std::shared_ptr<::DianaMovableReadStream>(streamAdapter, &streamAdapter->stream.parent.parent);
    }

    // InsertModuleMetaInfo
    void InsertModuleMetaInfo(orthia::intrusive_ptr<CClassicDatabase> database, Address_type moduleAddress, const oui::String& fullName)
    {
        orthia::CCommonFormatBuilder builder;
        builder.AddMetadata(ORTHIA_TCSTR("fullname"), fullName.native);
        std::string metaInfo;
        builder.Produce(&metaInfo);

        database->InsertMetaInfo(moduleAddress, g_database_type_moduleMetaInfo, metaInfo, moduleAddress);
    }
    void InsertName(orthia::intrusive_ptr<CClassicDatabase> database, Address_type moduleAddress, const orthia::NameInfo & info, Address_type metaInfoAddres)
    {
        orthia::CCommonFormatBuilder builder;
        builder.AddMetadata(ORTHIA_TCSTR("address"), info.address);
        builder.AddMetadata(ORTHIA_TCSTR("name"), info.name.native);
        std::string metaInfo;
        builder.Produce(&metaInfo);

        if (info.flags & orthia::NameInfo::flags_Export)
        {
            database->InsertMetaInfo(moduleAddress, g_database_type_fnc_Export, metaInfo, metaInfoAddres);
            return;
        }
        if (info.flags & orthia::NameInfo::flags_Import)
        {
            database->InsertMetaInfo(moduleAddress, g_database_type_fnc_Import, metaInfo, moduleAddress);
            return;
        }
        if (info.flags & orthia::NameInfo::flags_PrivateSymbol)
        {
            database->InsertMetaInfo(moduleAddress, g_database_type_fnc_PrivateSymbol, metaInfo, metaInfoAddres);
        }
    }

}