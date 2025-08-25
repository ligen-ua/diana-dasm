#include "orthia_item_file.h"
#include "orthia_pe.h"


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
    }
    void FileWorkplaceItem::GetModules(std::vector<orthia::ModuleInfo>& modules) const
    {
    }
    int FileWorkplaceItem::GetModulesCount() const 
    {
        return 0;
    }
}