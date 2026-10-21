#include "orthia_elf.h"
#include "orthia_memory_cache.h"
#include "orthia_streams.h"

#undef max

namespace orthia
{

    CSimpleElfFile::CSimpleElfFile()
    {
    }
    CSimpleElfFile::~CSimpleElfFile()
    {
    }

    void CSimpleElfFile::MapFile(const std::vector<char>& elfFile, const MapFileParameters& params)
    {
        DianaMovableReadStreamOverMemory elfFileStream;
        DianaMovableReadStreamOverMemory_Init(&elfFileStream, elfFile.data(), elfFile.size());

        Diana_ElfFile dianaElfFile;
        DI_CHECK_CPP(DianaElfFile_Init(&dianaElfFile,
            &elfFileStream.stream,
            elfFile.size(),
            DIANA_ELF_FILE_FLAGS_FILE_MODE));
        diana::Guard<diana::ElfFile> elfFileGuard(&dianaElfFile);

        orthia::CReaderOverVector reader(0, 0);
        orthia::CMemoryStorageOfModifiedData mappedFile(&reader);
        orthia::DianaAnalyzerReadWriteStream writeStream(&mappedFile);
        std::vector<char> page(0x4000);

        // The image base is where the module starts (its lowest PT_LOAD page):
        // - if the caller specified one, use it
        // - ET_EXEC: its link base, the loader does not move it
        // - ET_DYN (PIE/shared lib): the first nonzero p_vaddr, to keep the image off address 0
        const OPERAND_SIZE linkBase = dianaElfFile.pImpl->linkBase;
        OPERAND_SIZE imageBase = linkBase;
        if (params.imageBase != (DI_UINT64)(-1))
        {
            imageBase = params.imageBase;
        }
        else if (dianaElfFile.pImpl->elfHeader.e_type != DIANA_ET_EXEC)
        {
            for (int i = 0; i < dianaElfFile.pImpl->capturedSegmentCount; ++i)
            {
                const auto& seg = dianaElfFile.pImpl->pCapturedSegments[i];
                if (seg.p_type == DIANA_PT_LOAD && seg.p_vaddr != 0)
                {
                    imageBase = seg.p_vaddr;
                    break;
                }
            }
        }
        // MapEx puts every segment at loadBias + p_vaddr
        const OPERAND_SIZE loadBias = imageBase - linkBase;

        DI_CHECK_CPP(DianaElfFile_MapEx(&dianaElfFile,
            &elfFileStream.stream,
            loadBias,
            &writeStream,
            page.data(),
            (int)page.size(),
            params.mapFlags));

        // Collect flat mapped image
        orthia::VmMemoryRangesTargetOverVectorPlain ranges;
        if (!mappedFile.ReportRegions(imageBase, dianaElfFile.pImpl->sizeOfModule, &ranges, false))
        {
            return;
        }

        char* pModuleStart = ranges.m_data.data();
        size_t moduleSize = ranges.m_data.size();

        // Re-init in MODULE_MODE for analysis
        auto dianaContext = std::make_unique<ElfDianaContext>();
        DianaMovableReadStreamOverMemory_Init(&dianaContext->stream, pModuleStart, moduleSize);
        DI_CHECK_CPP(DianaElfFile_Init(&dianaContext->mappedElf,
            &dianaContext->stream.stream,
            moduleSize,
            DIANA_ELF_FILE_FLAGS_MODULE_MODE));
        dianaContext->mappedElf_Guard.reset(&dianaContext->mappedElf);

        // Commit — store original bytes for Relocate
        m_mappedElfFile.swap(ranges.m_data);
        m_originalElfFile = elfFile;
        m_dianaContext.swap(dianaContext);
        m_imageBase = imageBase;
    }

    void CSimpleElfFile::Relocate(OPERAND_SIZE newAddress)
    {
        if (m_originalElfFile.empty())
        {
            throw diana::CException(DI_ERROR, "Invalid object state");
        }
        MapFileParameters params;
        params.imageBase = newAddress;
        params.mapFlags = 0;
        MapFile(m_originalElfFile, params);
    }

    ElfDianaContext* CSimpleElfFile::GetImpl()
    {
        return m_dianaContext.get();
    }
    const ElfDianaContext* CSimpleElfFile::GetImpl() const
    {
        return m_dianaContext.get();
    }
    DI_UINT64 CSimpleElfFile::GetImageBase() const
    {
        return m_imageBase;
    }
    DI_UINT64 CSimpleElfFile::GetImageEnd() const
    {
        auto end = GetImageBase();
        DI_CHECK_CPP(Diana_SafeAdd(&end, (OPERAND_SIZE)m_mappedElfFile.size()));
        return end;
    }
    const std::vector<char>& CSimpleElfFile::GetMappedFile() const
    {
        return m_mappedElfFile;
    }
    int CSimpleElfFile::GetDianaMode() const
    {
        if (!m_dianaContext)
            return 0;
        return m_dianaContext->mappedElf.dianaMode;
    }
    int CSimpleElfFile::GetDianaArch() const
    {
        if (!m_dianaContext)
            return DIANA_ARCH_X86;
        return m_dianaContext->mappedElf.arch;
    }
    DI_UINT64 CSimpleElfFile::GetEntryPoint() const
    {
        if (!m_dianaContext)
            return 0;
        const auto* pImpl = m_dianaContext->mappedElf.pImpl;
        if (!pImpl->elfHeader.e_entry)
            return 0;
        // a link-time address, moved by the load bias
        return m_imageBase - pImpl->linkBase + pImpl->elfHeader.e_entry;
    }
    struct SimpleElfFileStreamWithContext : public ::DianaMemoryStream
    {
        CSimpleElfFile* elf;
    };
    DI_UINT64 CSimpleElfFile::DiGetProcAddress(const char* pFunctionName, OPERAND_SIZE* pForwardOffset, DI_UINT16 ordinal)
    {
        if (m_mappedElfFile.empty() || !m_dianaContext)
            return 0;
        // Must use SimpleElfFileStreamWithContext so translateAbsoluteAddress is
        // available for the DIANA_ANALYZE_RANDOM_READ_ABSOLUTE reads inside
        // DianaElfFile_GetSymbolAddress_Memory (gnuHashAddr, symtabAddr, strtabAddr …)
        SimpleElfFileStreamWithContext stream;
        Diana_InitMemoryStreamEx2(&stream, m_mappedElfFile.data(), m_mappedElfFile.size(), 0, 0);
        stream.translateAbsoluteAddress = TranslateAbsoluteAddress;
        stream.elf = this;
        OPERAND_SIZE result = 0;
        int err = DianaElfFile_GetProcAddress(&m_dianaContext->mappedElf,
            &stream.parent.parent,
            pFunctionName,
            &result);
        if (err || !result)
            return 0;
        // st_value is a link-time address: the export is where the image is mapped, as with PE
        return m_imageBase - m_dianaContext->mappedElf.pImpl->linkBase + result;
    }

    int CSimpleElfFile::QueryImports(diana::CBasePeLinkImportsObserver* observer)
    {
        if (m_mappedElfFile.empty() || !m_dianaContext)
            return DI_ERROR;

        std::vector<char> page(4096);
        SimpleElfFileStreamWithContext rwStream;
        Diana_InitMemoryStreamEx2(&rwStream, m_mappedElfFile.data(), m_mappedElfFile.size(), 1, 0);
        rwStream.translateAbsoluteAddress = TranslateAbsoluteAddress;
        rwStream.elf = this;

        return DianaElfFile_QueryImports(&m_dianaContext->mappedElf,
            0,
            &rwStream.parent,
            page.data(),
            (int)page.size(),
            observer->GetParent(),
            0,
            0);
    }
    int CSimpleElfFile::TranslateAbsoluteAddress(struct _dianaMemoryStream* pThis, OPERAND_SIZE* address)
    {
        CSimpleElfFile* elf = ((SimpleElfFileStreamWithContext*)pThis)->elf;
        if (*address < elf->m_imageBase || *address > elf->m_imageBase + elf->GetImpl()->mappedElf.pImpl->sizeOfModule)
        {
            return DI_ERROR;
        }
        *address -= elf->m_imageBase;
        return DI_SUCCESS;
    }
    std::vector<std::string> CSimpleElfFile::GetNeededLibraries() const
    {
        if (m_mappedElfFile.empty() || !m_dianaContext)
            return {};

        // Use the same stream setup as QueryImports/QueryExports so that
        // translateAbsoluteAddress is available for DT_STRTAB VA reads.
        SimpleElfFileStreamWithContext stream;
        Diana_InitMemoryStreamEx2(&stream,
            const_cast<char*>(m_mappedElfFile.data()),
            m_mappedElfFile.size(),
            0,
            0);
        stream.translateAbsoluteAddress = TranslateAbsoluteAddress;
        stream.elf = const_cast<CSimpleElfFile*>(this);

        std::vector<std::string> result;
        DianaElfFile_GetNeededLibraries(
            const_cast<Diana_ElfFile*>(&m_dianaContext->mappedElf),
            &stream.parent.parent,
            [](void* ctx, const char* name) -> int {
                static_cast<std::vector<std::string>*>(ctx)->emplace_back(name);
                return DI_SUCCESS;
            },
            &result,
            0);
        return result;
    }
    bool CSimpleElfFile::WriteImage(DI_UINT64 address, const void* pData, size_t size)
    {
        if (!pData || address < m_imageBase)
            return false;
        const DI_UINT64 offset = address - m_imageBase;
        if (offset > m_mappedElfFile.size() || size > m_mappedElfFile.size() - offset)
            return false;
        memcpy(m_mappedElfFile.data() + offset, pData, size);
        return true;
    }
    int CSimpleElfFile::QueryBuildId(std::vector<DI_UINT8>& buildId) const
    {
        buildId.clear();
        if (m_mappedElfFile.empty() || !m_dianaContext)
            return DI_ERROR;

        // module-mode stream rooted at the image start, like QueryExports
        SimpleElfFileStreamWithContext stream;
        Diana_InitMemoryStreamEx2(&stream,
            const_cast<char*>(m_mappedElfFile.data()),
            m_mappedElfFile.size(),
            0,
            0);
        stream.translateAbsoluteAddress = TranslateAbsoluteAddress;
        stream.elf = const_cast<CSimpleElfFile*>(this);

        DI_UINT8 buffer[256] = { 0, };
        DI_UINT32 buildIdSize = 0;
        int error = DianaElfFile_QueryBuildId(const_cast<Diana_ElfFile*>(&m_dianaContext->mappedElf),
            &stream.parent.parent,
            0,
            buffer,
            sizeof(buffer),
            &buildIdSize);
        if (error)
            return error;
        buildId.assign(buffer, buffer + buildIdSize);
        return DI_SUCCESS;
    }
    int QueryElfStartupRoutines(Diana_ElfFile* pElfFile,
        DianaMovableReadStream* pStream,
        DI_UINT64 moduleAddress,
        std::vector<ElfStartupRoutine>& routines)
    {
        routines.clear();
        // the routines come as module offsets
        struct Context
        {
            std::vector<ElfStartupRoutine>* routines;
            DI_UINT64 bias;
        } context = { &routines, moduleAddress };

        return DianaElfFile_QueryStartupRoutines(pElfFile,
            pStream,
            moduleAddress,
            0,
            0,
            [](void* pContext, const DianaElfStartupRoutine* pRoutine) -> int {
                auto& ctx = *static_cast<Context*>(pContext);
                ElfStartupRoutine routine;
                routine.kind = pRoutine->kind;
                routine.index = pRoutine->index;
                routine.address = ctx.bias + pRoutine->offset;
                routine.slotAddress = pRoutine->slotOffset ? ctx.bias + pRoutine->slotOffset : 0;
                if (pRoutine->pName)
                    routine.name = pRoutine->pName;
                ctx.routines->push_back(routine);
                return DI_SUCCESS;
            },
            &context,
            0);
    }

    std::string GetElfStartupRoutineName(const ElfStartupRoutine& routine)
    {
        auto number = ToAnsiStringAsHex((unsigned short)routine.index);
        switch (routine.kind)
        {
        case DIANA_ELF_STARTUP_IFUNC:
            return "$ifunc_" + (routine.name.empty() ? number : routine.name);
        case DIANA_ELF_STARTUP_PREINIT_ARRAY:
            return "$preinit_array_" + number;
        case DIANA_ELF_STARTUP_INIT:
            return "$init";
        case DIANA_ELF_STARTUP_INIT_ARRAY:
            return "$init_array_" + number;
        }
        return "$startup_" + number;
    }

    int CSimpleElfFile::QueryStartupRoutines(std::vector<ElfStartupRoutine>& routines) const
    {
        routines.clear();
        if (m_mappedElfFile.empty() || !m_dianaContext)
            return DI_ERROR;

        // module-mode stream rooted at the image start; the mapping relocated the pointers by m_imageBase
        SimpleElfFileStreamWithContext stream;
        Diana_InitMemoryStreamEx2(&stream,
            const_cast<char*>(m_mappedElfFile.data()),
            m_mappedElfFile.size(),
            0,
            0);
        stream.translateAbsoluteAddress = TranslateAbsoluteAddress;
        stream.elf = const_cast<CSimpleElfFile*>(this);

        return QueryElfStartupRoutines(const_cast<Diana_ElfFile*>(&m_dianaContext->mappedElf),
            &stream.parent.parent,
            m_imageBase,
            routines);
    }

    int CSimpleElfFile::QueryExports(diana::CBasePeLinkImportsObserver* observer)
    {
        if (m_mappedElfFile.empty() || !m_dianaContext)
            return DI_ERROR;

        std::vector<char> page(4096);
        SimpleElfFileStreamWithContext rwStream;
        Diana_InitMemoryStreamEx2(&rwStream, m_mappedElfFile.data(), m_mappedElfFile.size(), 0, 0);
        rwStream.translateAbsoluteAddress = TranslateAbsoluteAddress;
        rwStream.elf = this;
        return DianaElfFile_QueryExports(&m_dianaContext->mappedElf,
            &rwStream.parent.parent,
            page.data(),
            (int)page.size(),
            observer->GetParent(),
            0);
    }

}
