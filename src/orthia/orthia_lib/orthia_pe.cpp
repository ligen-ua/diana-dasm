#include "orthia_pe.h"
#include "orthia_memory_cache.h"
#include "orthia_files.h"
#include "orthia_streams.h"


namespace orthia
{

    CSimplePeFile::CSimplePeFile()
    {
    }
    CSimplePeFile::~CSimplePeFile()
    {
    }
    void CSimplePeFile::MapFile(const std::vector<char>& peFile, const MapFileParameters& params)
    {
        DianaMovableReadStreamOverMemory peFileStream;
        DianaMovableReadStreamOverMemory_Init(&peFileStream, &peFile.front(), peFile.size());

        Diana_PeFile dianaPeFile;
        DI_CHECK_CPP(DianaPeFile_Init(&dianaPeFile,
            &peFileStream.stream,
            peFile.size(),
            DIANA_PE_FILE_FLAGS_FILE_MODE));
        diana::Guard<diana::PeFile> peFileGuard(&dianaPeFile);

        orthia::CReaderOverVector reader(0, 0);
        orthia::CMemoryStorageOfModifiedData mappedFile(&reader);

        orthia::DianaAnalyzerReadWriteStream writeStream(&mappedFile);
        std::vector<char> page(0x4000);

        OPERAND_SIZE imageBase = dianaPeFile.pImpl->imageBase;
        if (params.imageBase != (DI_UINT64)(-1))
        {
            imageBase = params.imageBase;
        }

        DI_CHECK_CPP(DianaPeFile_Map(&dianaPeFile,
            &peFileStream.stream,
            imageBase,
            &writeStream,
            &page.front(),
            (ULONG)page.size()));


        // copy content to vector
        orthia::VmMemoryRangesTargetOverVectorPlain ranges;
        if (!mappedFile.ReportRegions(imageBase, dianaPeFile.pImpl->sizeOfModule, &ranges, false))
        {
            return;
        }

        // module
        char* pModuleStart = ranges.m_data.data();
        size_t moduleSize = ranges.m_data.size();

        auto dianaContext = std::make_unique<PeDianaContext>();

        DianaMovableReadStreamOverMemory_Init(&dianaContext->stream, pModuleStart, moduleSize);
        DI_CHECK_CPP(DianaPeFile_Init(&dianaContext->mappedPE,
            &dianaContext->stream.stream,
            moduleSize,
            DIANA_PE_FILE_FLAGS_MODULE_MODE));

        dianaContext->mappedPE_Guard.reset(&dianaContext->mappedPE);

        // everythins is OK, swap
        m_mappedPeFile.swap(ranges.m_data);
        m_dianaContext.swap(dianaContext);
        m_imageBase = imageBase;
    }
    DI_UINT64 CSimplePeFile::DiGetProcAddress(const char* pFunctionName, OPERAND_SIZE* pForwardOffset)
    {
        if (m_mappedPeFile.empty() || !m_dianaContext.get())
        {
            return 0;
        }
        auto moduleEnd = m_mappedPeFile.data() + m_mappedPeFile.size();
        OPERAND_SIZE result = 0;
        OPERAND_SIZE forwardOffset = 0;
        int dianaErr = DianaPeFile_GetProcAddress(&m_dianaContext->mappedPE,
            m_mappedPeFile.data(),
            moduleEnd,
            pFunctionName,
            &result,
            &forwardOffset);
        if (dianaErr)
        {
            return 0;
        }
        if (pForwardOffset)
        {
            *pForwardOffset = forwardOffset;
        }
        return result + m_imageBase;
    }

    PeDianaContext* CSimplePeFile::GetImpl()
    {
        return m_dianaContext.get();
    }
    DI_UINT64 CSimplePeFile::GetImageBase() const
    {
        return m_imageBase;
    }
    const std::vector<char>& CSimplePeFile::GetMappedPeFile() const
    {
        return m_mappedPeFile;
    }
}