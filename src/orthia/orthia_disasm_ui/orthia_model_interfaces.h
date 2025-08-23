#pragma once

#include "orthia_utils.h"
#include "oui_string.h"

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
    struct IWorkPlaceItem
    {
        virtual ~IWorkPlaceItem() {}
        virtual WorkAddressRangeInfo GetRangeInfo(Address_type address) const = 0;
        virtual const std::shared_ptr<CModuleManager> GetModuleManager() const = 0;
        virtual WorkAddressData ReadData(Address_type address, Address_type size) = 0;
        virtual oui::String GetShortName() const = 0;
        virtual void ReloadModules() = 0;
        virtual void GetModules(std::vector<orthia::ModuleInfo>& modules) const = 0;
    };

}