#include "orthia_item_file.h"
#include "orthia_image_source.h"
#include "orthia_helpers.h"
#include <map>
extern "C"
{
#include "diana_executable.h"
}
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include "orthia_common_format.h"
#include "orthia_common_print.h"
#include "orthia_sections.h"

namespace orthia
{
    FileWorkplaceItem::FileWorkplaceItem(std::shared_ptr<CFilePersistentItemStorage> peristentItemStorage_in)
        : persistentItemStorage(peristentItemStorage_in)
    {
    }
    // FileWorkplaceItem
    static WorkAddressData MakeInvalidData(Address_type size)
    {
        std::vector<char> buffer((size_t)size);
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
        if (!imageSources)
        {
            return MakeInvalidData(size);
        }

        // fast case: the entire range is inside one module
        if (auto* source = imageSources->Find(address))
        {
            if (lastValid <= source->GetLastValidAddress())
            {
                auto image = source->GetImage();
                if (!image)
                {
                    // a stale or unresolved dependency owns the range with nothing mapped there
                    return MakeInvalidData(size);
                }
                auto offset = address - image->GetImageBase();
                auto pBufferStart = image->GetMappedFile().data() + offset;
                return WorkAddressData(
                    pBufferStart,
                    size,
                    nullptr,
                    WorkAddressData::flags_FullValid,
                    [image = std::move(image)](WorkAddressData*) mutable {
                    image.reset();
                }
                );
            }
        }

        // the range spans module boundaries: copy what is mapped, flag the rest
        std::vector<char> buffer((size_t)size);
        std::vector<char> flags((size_t)size, WorkAddressData::dataFlags_Invalid);
        bool anyValid = false;
        Address_type pos = address;
        for (;;)
        {
            auto* source = imageSources->Find(pos);
            if (!source)
            {
                source = imageSources->FindNext(pos);
                if (!source || source->GetBase() > lastValid)
                {
                    break;
                }
                pos = source->GetBase();
            }
            const Address_type sourceLast = source->GetLastValidAddress();
            const Address_type chunkLast = std::min(sourceLast, lastValid);
            if (auto image = source->GetImage())
            {
                const auto& data = image->GetMappedFile();
                const Address_type imageOffset = pos - image->GetImageBase();
                if (imageOffset < data.size())
                {
                    const Address_type available = std::min<Address_type>(chunkLast - pos + 1, data.size() - imageOffset);
                    const size_t bufferOffset = (size_t)(pos - address);
                    memcpy(buffer.data() + bufferOffset, data.data() + imageOffset, (size_t)available);
                    memset(flags.data() + bufferOffset, 0, (size_t)available);
                    anyValid = true;
                }
            }
            if (chunkLast >= lastValid)
            {
                break;
            }
            pos = chunkLast + 1;
        }
        if (!anyValid)
        {
            return MakeInvalidData(size);
        }
        auto* pBufferStart = buffer.data();
        auto* pFlagsStart = flags.data();
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
        // the module owning the address; the main module when the address is in no module,
        // so the initial view (address 0) lands on the main entry point
        IImageSource* source = imageSources ? imageSources->Find(address) : nullptr;
        if (source && source->GetKind() != ImageSourceKind::Main)
        {
            return {
                source->GetBase(),
                source->GetLastValidAddress(),
                source->GetBase(),
                source->GetSize(),
                file->GetDianaMode()
            };
        }
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
            if ((int)names.size() >= count)
            {
                return false;
            }
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
        if (!nameFilter.privateSymbolsOnly)
        {
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
            if ((int)names.size() >= count)
            {
                return;
            }
            if (!pageFound &&
                (nameFilter.flags & nameFilter.flags_ContinueFrom) &&
                nameFilter.continueMarkNameFlag != 0 &&
                nameFilter.continueMarkNameFlag != NameInfo::flags_PrivateSymbol)
            {
                pageFound = true;
                continueFromPrivate = false;
            }
        }
        classicDatabase->QueryMetaInfoModule2(moduleAddress,
            g_database_type_fnc_PrivateSymbol, -1,
            handler,
            continueFromPrivate ? nameFilter.address : 0);
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
            parser.QueryMetadata("flags", &moduleIt->flags);
            parser.QueryMetadata("builtinflags", &moduleIt->builtInFlags);
            return true;
        });

        if (imageSources)
        {
            for (auto& mod : modules)
            {
                mod.imageSourceKnown = true;
                if (auto* source = imageSources->Find(mod.address))
                {
                    mod.imageSourceKind = (int)source->GetKind();
                    mod.imageState = (int)source->GetState();
                    mod.imageStateReason = source->GetStateReason();
                }
            }
        }

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
    Address_type FileWorkplaceItem::GetMainModuleAddress() const
    {
        return file ? file->GetImageBase() : 0;
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
        Address_type capturedExportModuleAddress = 0;
        Address_type capturedMetaExportAddress = 0;
        std::string capturedExportMetaName;
        Address_type capturedPrivateModuleAddress = 0;
        Address_type capturedMetaPrivateAddress = 0;
        std::string capturedPrivateMetaName;

        NameInfo result;
        CommonModuleInfo addressModule;
        if (!classicDatabase->QueryNearestModule(address, &addressModule))
        {
            return result;
        }

        classicDatabase->QueryMetaInfoByNearestAddress(g_database_type_fnc_Export,
            address,
            [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
        {
            if (moduleAddress == addressModule.address)
            {
                capturedExportModuleAddress = moduleAddress;
                capturedMetaExportAddress = metaAddress;
                CCommonFormatParser parser;
                parser.Parse(text);
                parser.QueryMetadata("name", &capturedExportMetaName);
            }
            return false;
        });

        classicDatabase->QueryMetaInfoByNearestAddress(g_database_type_fnc_PrivateSymbol,
            address,
            [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
        {
            if (moduleAddress == addressModule.address)
            {
                capturedPrivateModuleAddress = moduleAddress;
                capturedMetaPrivateAddress = metaAddress;
                CCommonFormatParser parser;
                parser.Parse(text);
                parser.QueryMetadata("name", &capturedPrivateMetaName);
            }
            return false;
        });

        Address_type moduleAddressHint = address;
        if (capturedExportModuleAddress)
        {
            moduleAddressHint = capturedExportModuleAddress;
        }
        else if (capturedPrivateModuleAddress)
        {
            moduleAddressHint = capturedPrivateModuleAddress;
        }


        result.name = ComposeName(orthia::Utf8ToPlatformString(capturedExportMetaName), capturedMetaExportAddress, address, addressModule.name, addressModule.address);
        if (!capturedPrivateMetaName.empty())
        {
            result.privateSymbol = ComposeName(orthia::Utf8ToPlatformString(capturedPrivateMetaName), capturedMetaPrivateAddress, address, addressModule.name, addressModule.address);
        }
        return result;
    }
    Address_type FileWorkplaceItem::QueryAddressByName(const oui::String& text, Address_type defValue) const
    {
        Address_type target = 0;
        bool found = false;
        auto downcased = orthia::Downcase(text.native);
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        // prefer the export of the opened file over same-named exports of its dependencies
        const Address_type mainModuleAddress = GetMainModuleAddress();
        classicDatabase->QueryMetaInfo(g_database_type_fnc_Export, [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
        {
            orthia::PlatformString_type name;
            CCommonFormatParser parser;
            parser.Parse(text);
            parser.QueryMetadata(OUI_TCSTR("name"), &name);
            if (orthia::Downcase(name) != downcased)
            {
                return true;
            }
            if (!found || moduleAddress == mainModuleAddress)
            {
                parser.QueryMetadata("address", &target);
                found = true;
            }
            return moduleAddress != mainModuleAddress;
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
            std::shared_ptr<const orthia::ISimpleFile> file;
            ::DianaMemoryStream stream;

            DianaReadStreamAdapter(std::shared_ptr<const orthia::ISimpleFile> file_in)
                :
                file(std::move(file_in))
            {
            }
        };

        IImageSource* source = imageSources ? imageSources->Find(addressStart) : nullptr;
        if (!source)
        {
            return nullptr;
        }
        auto image = source->GetImage();
        if (!image || addressStart < image->GetImageBase() || addressStart >= image->GetImageEnd())
        {
            return nullptr;
        }

        auto diff = addressStart - image->GetImageBase();
        auto& data = image->GetMappedFile();
        auto streamAdapter = std::make_shared<DianaReadStreamAdapter>(std::move(image));
        Diana_InitMemoryStreamEx2(&streamAdapter->stream, (char*)data.data()+diff, data.size()-diff, 0, 0);

        return std::shared_ptr<::DianaMovableReadStream>(streamAdapter, &streamAdapter->stream.parent.parent);
    }
    std::shared_ptr<IMemoryReader> FileWorkplaceItem::CreateMemoryReader()
    {
        if (imageSources)
        {
            return std::make_shared<CCompositeMemoryReader>(imageSources);
        }
        const auto& mapped = file->GetMappedFile();
        return std::make_shared<CMemoryReaderOnLoadedData>(
            file->GetImageBase(), mapped.data(), mapped.size());
    }
    void FileWorkplaceItem::OnModuleSymbolsLoaded(Address_type moduleAddress)
    {
        UpdateModuleFlags(moduleAddress, ModuleInfo::flags_symbolsLoaded, 0);
    }
    void FileWorkplaceItem::UpdateModuleFlags(Address_type moduleAddress, int flagsToSet, int flagsToRemove)
    {
        UpdateModuleMetaInfo(GetModuleManager()->QueryDatabaseManager()->GetClassicDatabase(),
            moduleAddress,
            [&](CCommonFormatParser& parser, CCommonFormatBuilder& builder) {
                int flags = 0;
                parser.QueryMetadata("flags", &flags);
                flags |= flagsToSet;
                flags &= ~flagsToRemove;
                builder.AddMetadata("flags", flags);
                return true;
            });
    }
    // InsertModuleMetaInfo
    void InsertModuleMetaInfo(orthia::intrusive_ptr<CClassicDatabase> database, Address_type moduleAddress, const oui::String& fullName, int moduleFlags, int builtInModuleFlags, const ModuleSourceMeta* source)
    {
        orthia::CCommonFormatBuilder builder;
        builder.AddMetadata(ORTHIA_TCSTR("fullname"), fullName.native);
        builder.AddMetadata("flags", moduleFlags);
        builder.AddMetadata("builtinflags", builtInModuleFlags);
        if (source)
        {
            if (source->srcKind)
            {
                builder.AddMetadata(std::string(g_meta_src_kind), std::string(source->srcKind));
            }
            if (source->identity)
            {
                WriteImageIdentity(builder, *source->identity);
            }
            if (source->iatSlots)
            {
                builder.AddMetadata(std::string(g_meta_iat_slots), 1);
            }
        }
        std::string metaInfo;
        builder.Produce(&metaInfo);

        database->InsertMetaInfo(moduleAddress, g_database_type_moduleMetaInfo, metaInfo, moduleAddress, true);
    }
    void UpdateModuleMetaInfo(orthia::intrusive_ptr<CClassicDatabase> database, Address_type moduleAddress, std::function<bool (CCommonFormatParser&, CCommonFormatBuilder&)> handler)
    {
        std::string newText;
        bool needUpdate = false;
        database->QueryMetaInfoModule2(moduleAddress, g_database_type_moduleMetaInfo, -1,
            [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
        {            
            CCommonFormatParser parser;
            parser.Parse(text);
            orthia::CCommonFormatBuilder builder(&parser);
            needUpdate = handler(parser, builder);
            if (needUpdate)
            {
                builder.Produce(&newText);
            }
            return false;
        });

        if (needUpdate)
        {
            database->InsertMetaInfo(moduleAddress, g_database_type_moduleMetaInfo, newText, moduleAddress, true);
        }
    }
    void FileWorkplaceItem::InitImageSources(int executableType)
    {
        auto table = std::make_shared<ImageSourceTable>();
        table->SetMain(std::make_shared<CMainImageSource>(file));

        // identity and source kind per dependency, from the module metainfo
        struct SourceRecord
        {
            std::string srcKind;
            ImageIdentity identity;
            PlatformString_type fullName;
            int builtInFlags = 0;
        };
        std::map<Address_type, SourceRecord> records;
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        classicDatabase->QueryMetaInfo(g_database_type_moduleMetaInfo,
            [&](Address_type moduleAddress, int, const std::string& text, Address_type)
        {
            CCommonFormatParser parser;
            parser.Parse(text);
            SourceRecord record;
            parser.QueryMetadata(std::string(g_meta_src_kind), &record.srcKind);
            parser.QueryMetadata(ORTHIA_TCSTR("fullname"), &record.fullName);
            parser.QueryMetadata("builtinflags", &record.builtInFlags);
            ReadImageIdentity(parser, record.identity);
            records[moduleAddress] = std::move(record);
            return true;
        });

        std::vector<CommonModuleInfo> dbModules;
        classicDatabase->QueryModules(&dbModules);
        const Address_type mainBase = file->GetImageBase();
        std::vector<std::shared_ptr<CLinkedFileImageSource>> linked;
        for (auto& dbm : dbModules)
        {
            if (dbm.address == mainBase || !dbm.size)
            {
                continue;
            }
            auto recordIt = records.find(dbm.address);
            if (recordIt == records.end() || recordIt->second.srcKind.empty())
            {
                // recorded before image sources existed: names only
                table->Add(std::make_shared<CNullImageSource>(dbm.address, dbm.size, ORTHIA_TCSTR("recorded without identity")));
                continue;
            }
            const auto& record = recordIt->second;
            if (record.srcKind != g_meta_src_kind_linked || !record.identity.IsValid())
            {
                table->Add(std::make_shared<CNullImageSource>(dbm.address, dbm.size, PlatformString_type()));
                continue;
            }
            int moduleType = executableType;
            if (record.builtInFlags & ModuleInfo::builtInFlags_moduleTypeElf)
            {
                moduleType = DIANA_EXECUTABLE_TYPE_ELF;
            }
            else if (record.builtInFlags & ModuleInfo::builtInFlags_moduleTypePe)
            {
                moduleType = DIANA_EXECUTABLE_TYPE_PE;
            }
            auto source = std::make_shared<CLinkedFileImageSource>(dbm.address,
                dbm.size,
                record.fullName,
                record.identity,
                moduleType,
                file->GetDianaMode());
            linked.push_back(source);
            table->Add(source);
        }
        table->Sort();

        // headers only: `lm` shows stale dependencies right after open without mapping anything
        for (auto& source : linked)
        {
            source->PreCheckHeader();
        }
        imageSources = table;
    }

    int FileWorkplaceItem::ApplyLinkedImports()
    {
        auto database = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        const Address_type mainBase = file->GetImageBase();

        // old databases keyed the import rows by the module address, nothing to replay there
        bool slotsRecorded = false;
        database->QueryMetaInfoModule2(mainBase, g_database_type_moduleMetaInfo, -1,
            [&](Address_type, int, const std::string& text, Address_type)
        {
            CCommonFormatParser parser;
            parser.Parse(text);
            int value = 0;
            slotsRecorded = parser.QueryMetadata(std::string(g_meta_iat_slots), &value) && value != 0;
            return false;
        });
        if (!slotsRecorded)
        {
            return 0;
        }

        const int pointerSize = file->GetDianaMode();
        if (pointerSize != 4 && pointerSize != 8)
        {
            return 0;
        }
        int applied = 0;
        database->QueryMetaInfoModule2(mainBase, g_database_type_fnc_Import, -1,
            [&](Address_type, int, const std::string& text, Address_type slot)
        {
            CCommonFormatParser parser;
            parser.Parse(text);
            Address_type target = 0;
            parser.QueryMetadata("address", &target);

            // little-endian, like the image itself
            DI_UINT64 value = target;
            if (file->WriteImage(slot, &value, (size_t)pointerSize))
            {
                ++applied;
            }
            return true;
        });
        return applied;
    }

    void FileWorkplaceItem::QuerySections(Address_type moduleBase, ImageSections& sections)
    {
        if (!moduleBase)
        {
            moduleBase = file->GetImageBase();
        }
        // the file the image was mapped from: ELF section headers are not in the mapped image
        PlatformString_type imageFile;
        if (moduleBase == file->GetImageBase())
        {
            imageFile = fullName.native;
        }
        else if (imageSources)
        {
            auto source = imageSources->Find(moduleBase);
            if (source && source->GetBase() == moduleBase && source->GetKind() == ImageSourceKind::LinkedFile)
            {
                imageFile = static_cast<CLinkedFileImageSource*>(source)->GetPath();
            }
        }
        auto reader = CreateMemoryReader();
        QueryImageSections(reader.get(), moduleBase, imageFile, sections);
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
            // keyed by the IAT slot, so the linked value can be replayed on reopen (g_meta_iat_slots)
            database->InsertMetaInfo(moduleAddress, g_database_type_fnc_Import, metaInfo, metaInfoAddres);
            return;
        }
        if (info.flags & orthia::NameInfo::flags_PrivateSymbol)
        {
            database->InsertMetaInfo(moduleAddress, g_database_type_fnc_PrivateSymbol, metaInfo, metaInfoAddres);
        }
    }

}