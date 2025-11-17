#include "orthia_item_file.h"
#include "orthia_pe.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include "orthia_common_format.h"

namespace orthia
{

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

    WorkAddressRangeInfo FileWorkplaceItem::GetRangeInfo(Address_type address) const
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
        classicDatabase->QueryMetaInfoModule2(moduleAddress, g_database_type_fnc_Import, g_database_type_fnc_Export, 
            [&](Address_type moduleAddress, int metaType, const std::string& text, Address_type metaAddress)
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
            if (metaType == g_database_type_fnc_Export)
            {
                info.flags = NameInfo::flags_Export;
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
            if (names.size() >= count)
            {
                return false;
            }
            return true;
        });
    }
    int FileWorkplaceItem::QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const
    {
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        std::vector<CommonModuleInfo> dbModules;
        return classicDatabase->QueryMetaInfoModule2_Count(moduleAddress, g_database_type_fnc_Import, g_database_type_fnc_Export);
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
        for (auto& dbm:dbModules)
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
            info.dianaMode = peFile->GetImpl()->mappedPE.pImpl->dianaMode;
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
        return peFile->GetImpl()->mappedPE.pImpl->dianaMode;
    }
    MarkupRangeInfo FileWorkplaceItem::QueryMarkupRange(Address_type address) const
    {
        return MarkupRangeInfo();
    }
    void FileWorkplaceItem::QueryMarkupRange(Address_type address, int index, int count, MarkupRange& range) const
    {
        range.lines.clear();
    }
    oui::String FileWorkplaceItem::QueryAddressName(Address_type address) const
    {
        return oui::String();
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
        }
    }

}