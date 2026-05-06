#include "orthia_external_symbols.h"
#include "orthia_item_file.h"
#include <unordered_map>

extern "C"
{
#include "diana_pdb.h"
}
#include "orthia_files.h"
#include "orthia_utils.h"
#include "orthia_database_impl_classic.h"

namespace orthia
{

namespace
{

bool IsPeModule(const ModuleInfo& mod)
{
    PlatformString_type ext;
    GetExtensionOfFile(mod.fullName, &ext);
    return ext == ORTHIA_TCSTR("dll")
        || ext == ORTHIA_TCSTR("exe")
        || ext == ORTHIA_TCSTR("sys");
}

// Build the PDB stem (filename without path or extension) for a module.
PlatformString_type GetPdbStem(const ModuleInfo& mod)
{
    PlatformString_type fileName;
    UnparseFileNameFromFullFileName(mod.fullName, &fileName);
    PlatformString_type ext;
    GetExtensionOfFile(fileName, &ext);
    if (!ext.empty())
        return fileName.substr(0, fileName.size() - ext.size() - 1);
    return fileName;
}

// Iterates over all candidate PDB paths for a module: explicit symbol folders
// first, then the module's own directory. Call FindNextPdb() until it returns
// false; Current() holds the last successfully located path.
class CPdbFileFinder
{
    PlatformString_type m_pdbName;
    const std::vector<PlatformString_type>& m_symbolFolders;
    PlatformString_type m_modPdb;
    size_t m_folderIndex = 0;
    bool m_triedModDir = false;
    PlatformString_type m_current;
public:
    CPdbFileFinder(const ModuleInfo& mod, const std::vector<PlatformString_type>& symbolFolders)
        : m_symbolFolders(symbolFolders)
    {
        m_pdbName = GetPdbStem(mod) + ORTHIA_TCSTR(".pdb");
        PlatformString_type modExt;
        GetExtensionOfFile(mod.fullName, &modExt);
        if (!modExt.empty())
            m_modPdb = mod.fullName.substr(0, mod.fullName.size() - modExt.size()) + ORTHIA_TCSTR("pdb");
        else
            m_modPdb = mod.fullName + ORTHIA_TCSTR(".pdb");
    }

    bool FindNextPdb()
    {
        std::vector<char> buf;
        while (m_folderIndex < m_symbolFolders.size())
        {
            PlatformString_type candidate = m_symbolFolders[m_folderIndex] + ORTHIA_STR_PLATFORM_SLASH + m_pdbName;
            ++m_folderIndex;
            if (LoadFileToVector_Silent(candidate, buf) == 0)
            {
                m_current = std::move(candidate);
                return true;
            }
        }
        if (!m_triedModDir)
        {
            m_triedModDir = true;
            if (LoadFileToVector_Silent(m_modPdb, buf) == 0)
            {
                m_current = m_modPdb;
                return true;
            }
        }
        return false;
    }

    const PlatformString_type& Current() const { return m_current; }
};

// -----------------------------------------------------------------------
// PDB loader

class CPdbExternalSymbolsLoader : public IExternalSymbolsLoader
{
    std::vector<PlatformString_type> m_symbolFolders;
    std::shared_ptr<CLoaderUILogger> m_logger;
public:
    CPdbExternalSymbolsLoader(std::vector<PlatformString_type> symbolFolders,
                              std::shared_ptr<CLoaderUILogger> logger)
        : m_symbolFolders(std::move(symbolFolders))
        , m_logger(std::move(logger))
    {
    }

    bool CanLoad(const ModuleInfo& mod) const override
    {
        if (!IsPeModule(mod))
            return false;
        CPdbFileFinder finder(mod, m_symbolFolders);
        return finder.FindNextPdb();
    }

    void Load(const ModuleInfo& mod, intrusive_ptr<CClassicDatabase> db,
              OnPrivateSymbolLoaded onSymbol = nullptr) override
    {
        CPdbFileFinder finder(mod, m_symbolFolders);
        while (finder.FindNextPdb())
        {
            const PlatformString_type& pdbPath = finder.Current();

            std::vector<char> pdbData;
            if (LoadFileToVector_Silent(pdbPath, pdbData) != 0 || pdbData.empty())
                continue;

            if (m_logger)
            {
                auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                m_logger->WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("loading-symbols")), mod.name));
            }

            if (!pdb_sig_match(pdbData.data(), pdbData.size()))
            {
                if (m_logger)
                {
                    auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                    m_logger->WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("symbols-mismatch")), pdbPath));
                }
                continue;
            }

            void* ctx = pdb_create_context(nullptr, nullptr);
            if (!ctx)
                continue;
            oui::ScopedGuard pdbContextGuard([&]() {
                pdb_destroy_context(ctx);
            });
            if (pdb_load(ctx, pdbData.data(), pdbData.size()) < 0)
            {
                continue;
            }

            uint32_t nrSections = pdb_get_nr_sections(ctx);

            uint32_t nrSymbols = 0;
            if (pdb_get_nr_symbols(ctx, &nrSymbols) < 0 || nrSymbols == 0)
            {
                continue;
            }

            std::vector<const SYMTYPE*> syms(nrSymbols);
            if (pdb_get_symbols(ctx, syms.data()) < 0)
            {
                continue;
            }

            std::unordered_map<Address_type, std::pair<NameInfo, int>> pending;

            for (uint32_t i = 0; i < nrSymbols; ++i)
            {
                const SYMTYPE* sym = syms[i];
                if (!sym)
                    continue;

                unsigned short seg = 0;
                unsigned long off = 0;
                const unsigned char* name = nullptr;

                switch (sym->rectyp)
                {
                case S_PUB32:
                case S_GDATA32:
                case S_LDATA32:
                {
                    // PUBSYM32 and DATASYM32 share identical off/seg/name layout
                    auto* d = reinterpret_cast<const DATASYM32*>(sym);
                    seg = d->seg;
                    off = d->off;
                    name = d->name;
                    break;
                }
                case S_GPROC32:
                case S_LPROC32:
                {
                    auto* p = reinterpret_cast<const PROCSYM32*>(sym);
                    seg = p->seg;
                    off = p->off;
                    name = p->name;
                    break;
                }
                default:
                    continue;
                }

                if (seg == 0)
                    continue;

                // Dynamic symbols (seg - 1 == nrSections) carry a raw offset instead of RVA.
                uint32_t rva = 0;
                if ((uint32_t)(seg - 1) == nrSections)
                {
                    rva = off;
                }
                else
                {
                    if (pdb_convert_section_offset_to_rva(ctx, seg, off, &rva) < 0)
                        continue;
                }

                NameInfo info;
                info.address = mod.address + rva;
                info.flags = NameInfo::flags_PrivateSymbol;
                info.name = Utf8ToPlatformString(reinterpret_cast<const char*>(name));

                int priority = (sym->rectyp == S_PUB32) ? 0 : 1;
                auto it = pending.find(info.address);
                if (it == pending.end() || priority > it->second.second)
                    pending[info.address] = { info, priority };
            }

            constexpr int kBatchSize = 1000;
            auto it = pending.begin();
            while (it != pending.end())
            {
                auto batch = db->BeginBatch();
                for (int i = 0; i < kBatchSize && it != pending.end(); ++i, ++it)
                {
                    InsertName(db, mod.address, it->second.first, it->first);
                    if (onSymbol)
                        onSymbol(it->first, it->second.first.name);
                }
                batch->Commit();
            }

            if (m_logger)
            {
                auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                m_logger->WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("loading-symbols-done")), mod.name));
            }

            return;
        }
    }
};

// -----------------------------------------------------------------------
// ELF loader (stub — placeholder for future DWARF / .debug support)

class CElfExternalSymbolsLoader : public IExternalSymbolsLoader
{
public:
    bool CanLoad(const ModuleInfo& mod) const override
    {
        return !IsPeModule(mod);
    }

    void Load(const ModuleInfo& /*mod*/, intrusive_ptr<CClassicDatabase> /*db*/,
              OnPrivateSymbolLoaded /*onSymbol*/ = nullptr) override
    {
        // Not yet implemented.
    }
};

// -----------------------------------------------------------------------
// Composite loader — tries loaders in registration order

class CCompositeExternalSymbolsLoader : public IExternalSymbolsLoader
{
    std::vector<std::unique_ptr<IExternalSymbolsLoader>> m_loaders;
public:
    void Add(std::unique_ptr<IExternalSymbolsLoader> loader)
    {
        m_loaders.push_back(std::move(loader));
    }

    bool CanLoad(const ModuleInfo& mod) const override
    {
        for (const auto& l : m_loaders)
        {
            if (l->CanLoad(mod))
            {
                return true;
            }
        }
        return false;
    }

    void Load(const ModuleInfo& mod, intrusive_ptr<CClassicDatabase> db,
              OnPrivateSymbolLoaded onSymbol = nullptr) override
    {
        for (const auto& l : m_loaders)
        {
            if (l->CanLoad(mod))
            {
                l->Load(mod, db, onSymbol);
                return;
            }
        }
    }
};

} // anonymous namespace

std::unique_ptr<IExternalSymbolsLoader> CreateExternalSymbolsLoader(
    const std::vector<PlatformString_type>& symbolFolders,
    std::shared_ptr<CLoaderUILogger> logger)
{
    auto composite = std::make_unique<CCompositeExternalSymbolsLoader>();
    composite->Add(std::make_unique<CPdbExternalSymbolsLoader>(symbolFolders, std::move(logger)));
    composite->Add(std::make_unique<CElfExternalSymbolsLoader>());
    return composite;
}

} // namespace orthia
