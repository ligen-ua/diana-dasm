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
            (DI_UINT32)page.size(),
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

    int CSimplePeFile::QueryImports(diana::CBasePeLinkImportsObserver* observer)
    {
        if (m_mappedPeFile.empty() || !m_dianaContext.get())
        {
            return DI_ERROR;
        }

        std::vector<char> page(4096);
        ::DianaMemoryStream2 rwStream;
        Diana_InitMemoryStream2(&rwStream, m_mappedPeFile.data(), m_mappedPeFile.size(), 0, 0, m_imageBase);

        return DianaPeFile_QueryImports(&m_dianaContext->mappedPE,
            0,
            &rwStream.parent.parent,
            page.data(),
            (int)page.size(),
            observer->GetParent(),
            0,
            DIANA_LINK_IMPORT_READ_FULL_INFO);
    }
    int CSimplePeFile::QueryExports(diana::CBasePeLinkImportsObserver* observer)
    {
        if (m_mappedPeFile.empty() || !m_dianaContext.get())
        {
            return DI_ERROR;
        }

        std::vector<char> page(4096);
        ::DianaMemoryStream2 rwStream;
        Diana_InitMemoryStream2(&rwStream, m_mappedPeFile.data(), m_mappedPeFile.size(), 0, 0, m_imageBase);

        return DianaPeFile_QueryExports(&m_dianaContext->mappedPE,
            &rwStream.parent.parent.parent,
            page.data(),
            (int)page.size(),
            observer->GetParent(),
            0);
    }

    int CSimplePeFile::QueryTLSCallbacks(std::vector<OPERAND_SIZE>& callbacks)
    {
        callbacks.clear();

        ::DianaMemoryStream2 stream;
        Diana_InitMemoryStream2(&stream, m_mappedPeFile.data(), m_mappedPeFile.size(), 0, 0, m_imageBase);

        void* pTlsCallbacks = 0;
        int tlsCallbacksCount = 0;
        OPERAND_SIZE addressOfTLSIndex = 0;
        if (auto error = DianaPeFile_QueryTLSCallbacks(&GetImpl()->mappedPE,
            GetImageBase(),
            &stream.parent.parent,
            &pTlsCallbacks,
            &tlsCallbacksCount,
            &addressOfTLSIndex,
            0)) 
        {
            return error;
        }
        char* pTls = (char*)pTlsCallbacks;
        callbacks.reserve(tlsCallbacksCount);
        for (int i = 0; i < tlsCallbacksCount; ++i)
        {
            OPERAND_SIZE callback = Diana_ReadValue(pTls, GetImpl()->mappedPE.pImpl->dianaMode);
            callbacks.push_back(callback);
            pTls += GetImpl()->mappedPE.pImpl->dianaMode;
        }
        DIANA_FREE(pTlsCallbacks);
        return DI_SUCCESS;
    }

    const std::vector<char>& CSimplePeFile::GetMappedFile() const
    {
        return m_mappedPeFile;
    }
    int CSimplePeFile::GetDianaMode() const
    {
        if (!m_dianaContext)
            return 0;
        return m_dianaContext->mappedPE.pImpl->dianaMode;
    }
    DI_UINT64 CSimplePeFile::GetEntryPoint() const
    {
        if (!m_dianaContext)
            return 0;
        DI_UINT64 ep = m_imageBase;
        Diana_SafeAdd(&ep, m_dianaContext->mappedPE.pImpl->addressOfEntryPoint);
        return ep;
    }
}