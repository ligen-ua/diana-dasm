#pragma once
#include <vector>
extern "C"
{
#include "diana_core.h"
}
#include "diana_pe_cpp.h"

namespace orthia
{
    class ISimpleFile
    {
    public:
        virtual ~ISimpleFile() = default;
        virtual DI_UINT64 GetImageBase() const = 0;
        virtual DI_UINT64 GetImageEnd() const = 0;
        virtual const std::vector<char>& GetMappedFile() const = 0;
        virtual void Relocate(OPERAND_SIZE newAddress) = 0;
        virtual DI_UINT64 DiGetProcAddress(const char* pFunctionName, OPERAND_SIZE* pForwardOffset = 0, DI_UINT16 ordinal = DIANA_PE_INVALID_ORDINAL_VALUE)=0;
        virtual int GetDianaMode() const = 0;
        virtual DI_UINT64 GetEntryPoint() const = 0;
        virtual int QueryImports(diana::CBasePeLinkImportsObserver* observer) = 0;
        virtual int QueryExports(diana::CBasePeLinkImportsObserver* observer) = 0;
        // Patches the mapped image in place (e.g. replaying linked import slots). The range
        // must lie inside the image; returns false otherwise or when the format is read-only.
        virtual bool WriteImage(DI_UINT64 address, const void* pData, size_t size) = 0;
    };
}
