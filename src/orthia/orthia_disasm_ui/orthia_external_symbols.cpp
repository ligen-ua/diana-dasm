#include "orthia_external_symbols.h"
#include "orthia_item_file.h"

extern "C"
{
#include "diana_pdb.h"
}
#include "orthia_files.h"
#include "orthia_utils.h"
#include <filesystem>
#include <optional>

namespace orthia
{

namespace
{

bool IsPeModule(const ModuleInfo& mod)
{
    if (mod.builtInFlags & ModuleInfo::builtInFlags_moduleTypePe)
        return true;
    if (mod.builtInFlags & ModuleInfo::builtInFlags_moduleTypeElf)
        return false;
    // Legacy fallback for modules without builtInFlags (old databases)
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

static constexpr int maxPdbScanDepth = 4;

// Iterates over all candidate PDB paths for a module: explicit symbol folders
// first (each scanned recursively up to maxPdbScanDepth levels), then the
// module's own directory. Call FindNextPdb() until it returns false;
// Current() holds the last successfully located path.
class CPdbFileFinder
{
    PlatformString_type m_pdbName;
    const std::vector<PlatformString_type>& m_symbolFolders;
    PlatformString_type m_modDir;
    size_t m_folderIndex = 0;  // size() == modDir turn
    bool m_checkedRoot = false;
    std::optional<std::filesystem::recursive_directory_iterator> m_dirIt;
    PlatformString_type m_current;

    PlatformString_type CurrentRoot() const
    {
        return m_folderIndex < m_symbolFolders.size()
            ? m_symbolFolders[m_folderIndex]
            : m_modDir;
    }
public:
    CPdbFileFinder(const ModuleInfo& mod, const std::vector<PlatformString_type>& symbolFolders)
        : m_symbolFolders(symbolFolders)
    {
        m_pdbName = GetPdbStem(mod) + ORTHIA_TCSTR(".pdb");
        m_modDir = std::filesystem::path(mod.fullName).parent_path().native();
    }

    bool FindNextPdb()
    {
        namespace fs = std::filesystem;
        std::vector<char> buf;

        while (m_folderIndex <= m_symbolFolders.size())
        {
            if (!m_checkedRoot)
            {
                m_checkedRoot = true;
                PlatformString_type candidate = CurrentRoot() + ORTHIA_STR_PLATFORM_SLASH + m_pdbName;
                std::error_code ec;
                m_dirIt.emplace(CurrentRoot(), fs::directory_options::skip_permission_denied, ec);
                if (ec) m_dirIt.reset();
                if (LoadFileToVector_Silent(candidate, buf) == 0)
                {
                    m_current = std::move(candidate);
                    return true;
                }
            }

            while (m_dirIt && *m_dirIt != fs::recursive_directory_iterator{})
            {
                auto& it = *m_dirIt;
                if (it.depth() >= maxPdbScanDepth - 1)
                    it.disable_recursion_pending();

                std::error_code ec;
                bool isDir = it->is_directory(ec);
                fs::path entryPath = it->path();
                ++it;

                if (isDir && !ec)
                {
                    PlatformString_type candidate = (entryPath / m_pdbName).native();
                    if (LoadFileToVector_Silent(candidate, buf) == 0)
                    {
                        m_current = std::move(candidate);
                        return true;
                    }
                }
            }

            ++m_folderIndex;
            m_checkedRoot = false;
            m_dirIt.reset();
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
        return IsPeModule(mod);
    }

    void Load(const ModuleInfo& mod, ModuleSymbols& out,
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
                    seg = t->seg; off = t->off; name = t->name;
                    break;
                }
                case S_LABEL32:
                {
                    auto* l = reinterpret_cast<const LABELSYM32*>(sym);
                    seg = l->seg; off = l->off;

                    // Labels have no name — synthesize one from the address so
                    // the RVA still gets indexed and is reachable by address lookup.
                    char synth[32];
                    snprintf(synth, sizeof(synth), "__label_%08X", off);
                    // assign to a local std::string/buffer and point `name` at it
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
                out.Insert(info.address, std::move(info), priority);
            }

            out.Finalize();

            if (onSymbol)
            {
                out.ForEach([&](Address_type addr, const NameInfo& info) {
                    onSymbol(addr, info.name);
                });
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

    void Load(const ModuleInfo& /*mod*/, ModuleSymbols& /*out*/,
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

    void Load(const ModuleInfo& mod, ModuleSymbols& out,
              OnPrivateSymbolLoaded onSymbol = nullptr) override
    {
        for (const auto& l : m_loaders)
        {
            if (l->CanLoad(mod))
            {
                l->Load(mod, out, onSymbol);
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
