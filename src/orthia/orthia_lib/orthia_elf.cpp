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

        // Determine image base:
        // - if caller specified one, use it
        // - for ET_EXEC: use the lowest PT_LOAD p_vaddr (absolute)
        // - for ET_DYN (PIE/shared lib): load at 0 so the flat image starts from 0
        OPERAND_SIZE imageBase = 0;
        if (params.imageBase != (DI_UINT64)(-1))
        {
            imageBase = params.imageBase;
        }
        else if (dianaElfFile.pImpl->elfHeader.e_type == DIANA_ET_EXEC)
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

        DI_CHECK_CPP(DianaElfFile_MapEx(&dianaElfFile,
            &elfFileStream.stream,
            imageBase,
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
    DI_UINT64 CSimpleElfFile::GetEntryPoint() const
    {
        if (!m_dianaContext)
            return 0;
        auto e_type  = m_dianaContext->mappedElf.pImpl->elfHeader.e_type;
        auto e_entry = m_dianaContext->mappedElf.pImpl->elfHeader.e_entry;
        if (e_type == DIANA_ET_DYN && e_entry)
            return m_imageBase + e_entry;
        return e_entry;
    }
    DI_UINT64 CSimpleElfFile::DiGetProcAddress(const char* pFunctionName, OPERAND_SIZE* pForwardOffset, DI_UINT16 ordinal)
    {
        if (m_mappedElfFile.empty() || !m_dianaContext)
            return 0;
        DianaMovableReadStreamOverMemory stream;
        DianaMovableReadStreamOverMemory_Init(&stream, m_mappedElfFile.data(), m_mappedElfFile.size());
        OPERAND_SIZE result = 0;
        int err = DianaElfFile_GetProcAddress(&m_dianaContext->mappedElf,
            &stream.stream,
            pFunctionName,
            &result);
        if (err)
            return 0;
        return result;
    }
    int CSimpleElfFile::QueryImports(diana::CBasePeLinkImportsObserver* observer)
    {
        if (m_mappedElfFile.empty() || !m_dianaContext)
            return DI_ERROR;

        std::vector<char> page(4096);
        ::DianaMemoryStream rwStream;
        Diana_InitMemoryStreamEx2(&rwStream, m_mappedElfFile.data(), m_mappedElfFile.size(), 0, 0);

        return DianaElfFile_QueryImports(&m_dianaContext->mappedElf,
            0,
            &rwStream.parent,
            page.data(),
            (int)page.size(),
            observer->GetParent(),
            0,
            0);
    }
    int CSimpleElfFile::QueryExports(diana::CBasePeLinkImportsObserver* observer)
    {
        if (m_mappedElfFile.empty() || !m_dianaContext)
            return DI_ERROR;

        std::vector<char> page(4096);
        ::DianaMemoryStream rwStream;
        Diana_InitMemoryStreamEx2(&rwStream, m_mappedElfFile.data(), m_mappedElfFile.size(), 0, 0);

        return DianaElfFile_QueryExports(&m_dianaContext->mappedElf,
            &rwStream.parent.parent,
            page.data(),
            (int)page.size(),
            observer->GetParent(),
            0);
    }

}
