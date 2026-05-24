#pragma once

#include "orthia_simple_file.h"

namespace orthia
{
    class CSimpleShellcodeFile : public ISimpleFile
    {
        std::vector<char> m_data;
        DI_UINT64 m_imageBase = 0;
        int m_dianaMode = 0;

    public:
        CSimpleShellcodeFile(const std::vector<char>& data, DI_UINT64 base, int dianaMode);

        // ISimpleFile
        DI_UINT64 GetImageBase() const override;
        DI_UINT64 GetImageEnd() const override;
        const std::vector<char>& GetMappedFile() const override;
        void Relocate(OPERAND_SIZE newAddress) override;
        DI_UINT64 DiGetProcAddress(const char* pFunctionName, OPERAND_SIZE* pForwardOffset = 0, DI_UINT16 ordinal = DIANA_PE_INVALID_ORDINAL_VALUE) override;
        int GetDianaMode() const override;
        DI_UINT64 GetEntryPoint() const override;
        int QueryImports(diana::CBasePeLinkImportsObserver* observer) override;
        int QueryExports(diana::CBasePeLinkImportsObserver* observer) override;
    };
}
