#pragma once

#include "orthia_simple_file.h"
#include "orthia_pe.h"
extern "C"
{
#include "diana_elf.h"
}
#include "diana_core_cpp.h"

namespace orthia
{
    struct ElfDianaContext
    {
        Diana_ElfFile mappedElf;
        DianaMovableReadStreamOverMemory stream;
        diana::Guard<diana::ElfFile> mappedElf_Guard;
    };

    class CSimpleElfFile : public ISimpleFile
    {
        std::vector<char> m_mappedElfFile;
        std::vector<char> m_originalElfFile;
        std::unique_ptr<ElfDianaContext> m_dianaContext;
        DI_UINT64 m_imageBase = 0;

        static int TranslateAbsoluteAddress(struct _dianaMemoryStream* pThis, OPERAND_SIZE* address);
    public:
        CSimpleElfFile();
        ~CSimpleElfFile();

        void MapFile(const std::vector<char>& elfFile, const MapFileParameters& params);

        ElfDianaContext* GetImpl();
        const ElfDianaContext* GetImpl() const;

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
        std::vector<std::string> GetNeededLibraries() const;
    };
}
