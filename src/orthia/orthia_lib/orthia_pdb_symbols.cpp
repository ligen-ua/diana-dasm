#include "orthia_pdb_symbols.h"
#include "diana_core_cpp.h"
#include <unordered_map>
#include <cstring>

namespace orthia
{
    namespace
    {
        struct PdbContextGuard
        {
            void* context = nullptr;
            ~PdbContextGuard()
            {
                if (context)
                {
                    pdb_destroy_context(context);
                }
            }
        };
    }

    CPdbSymbols::CPdbSymbols()
    {
    }
    CPdbSymbols::~CPdbSymbols()
    {
    }

    void CPdbSymbols::Load(const std::vector<char>& pdbFile)
    {
        if (pdbFile.empty() || !pdb_sig_match(const_cast<char*>(pdbFile.data()), pdbFile.size()))
        {
            throw diana::CException(DI_ERROR, "Invalid PDB file");
        }

        PdbContextGuard guard;
        guard.context = pdb_create_context(nullptr, nullptr);
        if (!guard.context)
        {
            throw diana::CException(DI_ERROR, "Can't create PDB context");
        }

        if (pdb_load(guard.context, pdbFile.data(), pdbFile.size()) < 0)
        {
            throw diana::CException(DI_ERROR, "Can't parse PDB file");
        }

        m_haveGuid = false;
        if (const struct guid* pdbGuid = pdb_get_guid(guard.context))
        {
            m_guid.Data1 = pdbGuid->data1;
            m_guid.Data2 = pdbGuid->data2;
            m_guid.Data3 = pdbGuid->data3;
            memcpy(m_guid.Data4, pdbGuid->data4, sizeof(m_guid.Data4));
            m_age = pdb_get_age(guard.context);
            m_haveGuid = true;
        }

        uint32_t nrSections = pdb_get_nr_sections(guard.context);

        uint32_t nrSymbols = 0;
        if (pdb_get_nr_symbols(guard.context, &nrSymbols) < 0 || nrSymbols == 0)
        {
            m_symbols.clear();
            return;
        }

        std::vector<const SYMTYPE*> syms(nrSymbols);
        if (pdb_get_symbols(guard.context, syms.data()) < 0)
        {
            throw diana::CException(DI_ERROR, "Can't enumerate PDB symbols");
        }

        // name -> {rva, priority}; lower priority wins on name collisions
        // (public symbols are preferred over other record kinds sharing a name).
        std::unordered_map<std::string, std::pair<DI_UINT64, int>> collected;
        for (uint32_t i = 0; i < nrSymbols; ++i)
        {
            const SYMTYPE* sym = syms[i];
            if (!sym)
            {
                continue;
            }

            unsigned short seg = 0;
            unsigned long off = 0;
            const unsigned char* name = nullptr;
            int priority = 1;

            switch (sym->rectyp)
            {
            case S_PUB32:
            case S_GDATA32:
            case S_LDATA32:
            case S_GTHREAD32:
            case S_LTHREAD32:
            case S_GMANDATA:
            case S_LMANDATA:
            {
                // PUBSYM32 and DATASYM32 share identical off/seg/name layout
                auto* d = reinterpret_cast<const DATASYM32*>(sym);
                seg = d->seg;
                off = d->off;
                name = d->name;
                priority = (sym->rectyp == S_PUB32) ? 0 : 1;
                break;
            }
            case S_GPROC32:
            case S_LPROC32:
            case S_GPROC32_ID:
            case S_LPROC32_ID:
            {
                auto* p = reinterpret_cast<const PROCSYM32*>(sym);
                seg = p->seg;
                off = p->off;
                name = p->name;
                break;
            }
            case S_THUNK32:
            {
                auto* t = reinterpret_cast<const THUNKSYM32*>(sym);
                seg = t->seg;
                off = t->off;
                name = t->name;
                break;
            }
            default:
                continue;
            }

            if (seg == 0 || !name)
            {
                continue;
            }

            // Dynamic symbols (seg - 1 == nrSections) carry a raw offset instead of RVA.
            uint32_t rva = 0;
            if ((uint32_t)(seg - 1) == nrSections)
            {
                rva = off;
            }
            else
            {
                if (pdb_convert_section_offset_to_rva(guard.context, seg, off, &rva) < 0)
                {
                    continue;
                }
            }

            std::string symbolName(reinterpret_cast<const char*>(name));
            auto it = collected.find(symbolName);
            if (it == collected.end())
            {
                collected.emplace(std::move(symbolName), std::make_pair((DI_UINT64)rva, priority));
            }
            else if (priority < it->second.second)
            {
                it->second = std::make_pair((DI_UINT64)rva, priority);
            }
        }

        m_symbols.clear();
        m_symbols.reserve(collected.size());
        for (auto& entry : collected)
        {
            PdbSymbolInfo info;
            info.name = entry.first;
            info.rva = entry.second.first;
            m_symbols.push_back(std::move(info));
        }
    }

    bool CPdbSymbols::QueryGUID(DIANA_UUID* pGuid, DI_UINT32* pAge) const
    {
        if (!m_haveGuid)
        {
            return false;
        }
        if (pGuid)
        {
            *pGuid = m_guid;
        }
        if (pAge)
        {
            *pAge = m_age;
        }
        return true;
    }

    const std::vector<PdbSymbolInfo>& CPdbSymbols::GetSymbols() const
    {
        return m_symbols;
    }
}
