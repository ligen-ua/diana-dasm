#include "orthia_pe.h"
#include "orthia_memory_cache.h"
#include "orthia_files.h"
#include "orthia_streams.h"

#undef max

namespace orthia
{

    CSimplePeFile::CSimplePeFile()
    {
    }
    CSimplePeFile::~CSimplePeFile()
    {
    }
    void CSimplePeFile::Relocate(OPERAND_SIZE newAddress)
    {
        if (m_mappedPeFile.empty() || !m_dianaContext.get() || m_mappedPeFile.empty())
        {
            throw diana::CException(DI_ERROR, "Invalid object state");
        }

        OPERAND_SIZE lastPossibleAddress = DI_MAX_OPERAND_SIZE;
        switch (m_dianaContext->mappedPE.pImpl->dianaMode)
        {
        case 4:
            lastPossibleAddress = std::numeric_limits<uint32_t>::max();
            break;
        case 2:
            lastPossibleAddress = std::numeric_limits<uint16_t>::max();
            break;
        }
        if (newAddress >= lastPossibleAddress ||
            m_mappedPeFile.size() > (lastPossibleAddress - newAddress))
        {
            throw diana::CException(DI_ERROR, "Can't map module");
        }
        ::DianaMemoryStream rwStream;
        Diana_InitMemoryStreamEx2(&rwStream, m_mappedPeFile.data(), m_mappedPeFile.size(), 1, newAddress);

        DI_CHECK_CPP(DianaPeFile_Relocate(&m_dianaContext->mappedPE, newAddress, &rwStream.parent));

        m_dianaContext->mappedPE.pImpl->imageBase = newAddress;
        m_imageBase = newAddress;
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

        DI_CHECK_CPP(DianaPeFile_MapEx(&dianaPeFile,
            &peFileStream.stream,
            imageBase,
            &writeStream,
            &page.front(),
            (ULONG)page.size(),
            params.mapFlags));


        // copy content to vector
        orthia::VmMemoryRangesTargetOverVectorPlain ranges;
        if (!mappedFile.ReportRegions(imageBase, dianaPeFile.pImpl->sizeOfModule, &ranges, false))
        {
            return;
        }

        // load as module
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
    std::string CSimplePeFile::DiReadForwardingString(OPERAND_SIZE forwardingOffset)
    {
        if (m_mappedPeFile.empty() || !m_dianaContext.get())
        {
            throw diana::CException(DI_ERROR, "Invalid object state");
        }
        if (forwardingOffset >= (OPERAND_SIZE)m_mappedPeFile.size())
        {
            throw diana::CException(DI_ERROR, "Invalid RVA");
        }
        const char* pStart = m_mappedPeFile.data() + (size_t)forwardingOffset;
        const char* pEnd = m_mappedPeFile.data() + m_mappedPeFile.size();
        for (auto p = pStart; p != pEnd; ++p)
        {
            if (!*p)
            {
                return std::string(pStart, p);
            }
        }
        throw diana::CException(DI_ERROR, "Invalid forwarging string");
    }
    DI_UINT64 CSimplePeFile::DiGetProcAddress(const char* pFunctionName, OPERAND_SIZE* pForwardOffset, DI_UINT16 ordinal)
    {
        if (m_mappedPeFile.empty() || !m_dianaContext.get())
        {
            return 0;
        }
        auto moduleEnd = m_mappedPeFile.data() + m_mappedPeFile.size();
        OPERAND_SIZE result = 0;
        OPERAND_SIZE forwardOffset = 0;
        int dianaErr = DianaPeFile_GetProcAddressEx(&m_dianaContext->mappedPE,
            m_mappedPeFile.data(),
            moduleEnd,
            pFunctionName,
            &result,
            &forwardOffset,
            ordinal);
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
    const PeDianaContext* CSimplePeFile::GetImpl() const
    {
        return m_dianaContext.get();
    }
    DI_UINT64 CSimplePeFile::GetImageBase() const
    {
        return m_imageBase;
    }
    DI_UINT64 CSimplePeFile::GetImageEnd() const
    {
        auto modAddress = GetImageBase();
        auto modEnd = modAddress;
        DI_CHECK_CPP(Diana_SafeAdd(&modEnd, GetMappedPeFile().size()));
        return modEnd;
    }
    const std::vector<char>& CSimplePeFile::GetMappedPeFile() const
    {
        return m_mappedPeFile;
    }
}