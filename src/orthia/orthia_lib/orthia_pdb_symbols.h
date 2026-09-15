#pragma once

#include "orthia_utils.h"
extern "C"
{
#include "diana_pdb.h"
#include "diana_uids.h"
}

namespace orthia
{
    struct PdbSymbolInfo
    {
        std::string name;
        DI_UINT64 rva = 0;
    };

    // UI-free PDB symbol reader built on top of diana_core's libpdb.
    // Loads a PDB already read into memory and exposes its GUID and the
    // resolved (name, RVA) pairs for public/global symbols.
    class CPdbSymbols
    {
        void* m_context = nullptr;
        std::vector<PdbSymbolInfo> m_symbols;
        bool m_haveGuid = false;
        DIANA_UUID m_guid = {};
        DI_UINT32 m_age = 0;
    public:
        CPdbSymbols();
        ~CPdbSymbols();

        void Load(const std::vector<char>& pdbFile);

        bool QueryGUID(DIANA_UUID* pGuid, DI_UINT32* pAge) const;
        const std::vector<PdbSymbolInfo>& GetSymbols() const;
    };
}
