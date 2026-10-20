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

    // code the dynamic linker runs before main: IFUNC resolvers and initializers
    struct ElfStartupRoutine
    {
        int kind = 0;                   // DIANA_ELF_STARTUP_*
        int index = 0;
        DI_UINT64 address = 0;          // absolute, like the entry point
        DI_UINT64 slotAddress = 0;      // the GOT slot of an IRELATIVE or the array entry; 0 if none
        std::string name;               // the IFUNC symbol name, empty for the others
    };

    // moduleAddress: where the module is (an ET_DYN's load bias); stream: a module-mode stream over it
    int QueryElfStartupRoutines(Diana_ElfFile* pElfFile,
        DianaMovableReadStream* pStream,
        DI_UINT64 moduleAddress,
        std::vector<ElfStartupRoutine>& routines);

    // the "$" name of a startup routine: $ifunc_<name>, $ifunc_NNNN, $preinit_array_NNNN, $init, $init_array_NNNN
    std::string GetElfStartupRoutineName(const ElfStartupRoutine& routine);

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
        bool WriteImage(DI_UINT64 address, const void* pData, size_t size) override;
        std::vector<std::string> GetNeededLibraries() const;
        // the GNU build-id note bytes; DI_SUCCESS with an empty vector when the file has none
        int QueryBuildId(std::vector<DI_UINT8>& buildId) const;
        // IFUNC resolvers and initializers at their addresses in the mapped image
        int QueryStartupRoutines(std::vector<ElfStartupRoutine>& routines) const;
    };
}
