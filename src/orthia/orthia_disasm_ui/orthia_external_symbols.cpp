#include "orthia_external_symbols.h"
#include "orthia_item_file.h"

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

// Search symbol folders and the module's own directory for <stem>.pdb.
// Returns the path of the first file found, or empty string.
PlatformString_type FindPdbFile(const ModuleInfo& mod,
    const std::vector<PlatformString_type>& symbolFolders)
{
    PlatformString_type stem = GetPdbStem(mod);
    PlatformString_type pdbName = stem + ORTHIA_TCSTR(".pdb");

    // Search explicit symbol folders first.
    for (const auto& folder : symbolFolders)
    {
        PlatformString_type candidate = folder + ORTHIA_STR_PLATFORM_SLASH + pdbName;
        std::vector<char> buf;
        if (LoadFileToVector_Silent(candidate, buf) == 0)
            return candidate;
    }

    // Try the directory that contains the module itself.
    PlatformString_type modExt;
    GetExtensionOfFile(mod.fullName, &modExt);
    PlatformString_type modPdb;
    if (!modExt.empty())
        modPdb = mod.fullName.substr(0, mod.fullName.size() - modExt.size()) + ORTHIA_TCSTR("pdb");
    else
        modPdb = mod.fullName + ORTHIA_TCSTR(".pdb");

    std::vector<char> buf;
    if (LoadFileToVector_Silent(modPdb, buf) == 0)
        return modPdb;

    return {};
}

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
        return !FindPdbFile(mod, m_symbolFolders).empty();
    }

    void Load(const ModuleInfo& mod, intrusive_ptr<CClassicDatabase> db) override
    {
        PlatformString_type pdbPath = FindPdbFile(mod, m_symbolFolders);
        if (pdbPath.empty())
            return;

        std::vector<char> pdbData;
        if (LoadFileToVector_Silent(pdbPath, pdbData) != 0 || pdbData.empty())
            return;

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
            return;
        }

        void* ctx = pdb_create_context(nullptr, nullptr);
        if (!ctx)
            return;

        if (pdb_load(ctx, pdbData.data(), pdbData.size()) < 0)
        {
            pdb_destroy_context(ctx);
            return;
        }

        uint32_t nrSections = pdb_get_nr_sections(ctx);

        uint32_t nrSymbols = 0;
        if (pdb_get_nr_public_symbols(ctx, &nrSymbols) < 0 || nrSymbols == 0)
        {
            pdb_destroy_context(ctx);
            return;
        }

        std::vector<const PUBSYM32*> syms(nrSymbols);
        if (pdb_get_public_symbols(ctx, syms.data()) < 0)
        {
            pdb_destroy_context(ctx);
            return;
        }

        for (uint32_t i = 0; i < nrSymbols; ++i)
        {
            const PUBSYM32* sym = syms[i];
            if (!sym || sym->seg == 0)
                continue;

            // Dynamic symbols (seg - 1 == nrSections) carry a raw offset instead of RVA.
            uint32_t rva = 0;
            if ((uint32_t)(sym->seg - 1) == nrSections)
            {
                rva = sym->off;
            }
            else
            {
                if (pdb_convert_section_offset_to_rva(ctx, sym->seg, sym->off, &rva) < 0)
                    continue;
            }

            NameInfo info;
            info.address = mod.address + rva;
            info.flags = NameInfo::flags_PrivateSymbol;
            info.name = Utf8ToPlatformString(reinterpret_cast<const char*>(sym->name));

            InsertName(db, mod.address, info, info.address);
        }

        pdb_destroy_context(ctx);
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

    void Load(const ModuleInfo& /*mod*/, intrusive_ptr<CClassicDatabase> /*db*/) override
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
            if (l->CanLoad(mod)) return true;
        return false;
    }

    void Load(const ModuleInfo& mod, intrusive_ptr<CClassicDatabase> db) override
    {
        for (const auto& l : m_loaders)
            if (l->CanLoad(mod)) { l->Load(mod, db); return; }
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
