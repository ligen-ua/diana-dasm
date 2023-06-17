#pragma once

#include "orthia_utils.h"
extern "C"
{
#include "diana_pe_analyzer.h"
#include "diana_pe.h"
}
#include "diana_pe_cpp.h"

namespace orthia
{
    struct MapFileParameters
    {
        DI_UINT64 imageBase = (DI_UINT64)(-1);
    };

    struct PeDianaContext
    {
        Diana_PeFile mappedPE;
        DianaMovableReadStreamOverMemory stream;
        diana::Guard<diana::PeFile> mappedPE_Guard;
    };

    class CSimplePeFile
    {
        std::vector<char> m_mappedPeFile;
        std::unique_ptr<PeDianaContext> m_dianaContext;
        DI_UINT64 m_imageBase = 0;
    public:
        CSimplePeFile();
        ~CSimplePeFile();
        void MapFile(const std::vector<char>& peFile, const MapFileParameters& params);
        DI_UINT64 DiGetProcAddress(const char * pFunctionName, OPERAND_SIZE* pForwardOffset = 0);

        PeDianaContext* GetImpl();
        const PeDianaContext* GetImpl() const;
        DI_UINT64 GetImageBase() const;
        const std::vector<char> & GetMappedPeFile() const;
    };
}
