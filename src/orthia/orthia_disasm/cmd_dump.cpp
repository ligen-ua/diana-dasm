#include "cmd_common.h"
#include "orthia_files.h"
#include "orthia_pe.h"
#include "orthia_pdb_symbols.h"
#include "orthia_match.h"
#include <map>
#include <unordered_map>

namespace orthia
{
    struct DumpOptions
    {
        std::wstring exeModule;
        std::optional<DI_UINT64> imageBase;
        std::wstring pdbFile;
        std::vector<std::string> functions;
    };

    namespace
    {
        bool IsWildcardPattern(const std::string& name)
        {
            return name.find('*') != name.npos || name.find('?') != name.npos;
        }

        // Collects every named PE export so wildcard patterns can be matched
        // against them; ordinal-only exports (no name) can't be matched by a
        // name pattern, so they're skipped, same as the existing literal-name
        // lookup already requires a name.
        struct CExportNameCollector : public diana::CBasePeLinkImportsObserver
        {
            std::vector<std::string> names;
            void QueryFunctionByOrdinal(const char* /*pDllName*/, DI_UINT32 /*ordinal*/, OPERAND_SIZE* /*pAddress*/) override
            {
            }
            void QueryFunctionByName(const char* /*pDllName*/, const char* pFunctionName, DI_UINT32 /*hint*/, OPERAND_SIZE* /*pAddress*/) override
            {
                if (pFunctionName && *pFunctionName)
                {
                    names.push_back(pFunctionName);
                }
            }
        };
    }

    static void Dump(DumpOptions& options, IToolOutputStream* streamToUse)
    {
        std::vector<char> peFile;
        orthia::LoadFileToVector(options.exeModule, peFile);
        if (peFile.empty())
        {
            throw std::runtime_error("Executable file is empty");
        }
        orthia::CSimplePeFile mappedPE;
        orthia::MapFileParameters params;
        if (options.imageBase.has_value())
        {
            params.imageBase = *options.imageBase;
        }
        mappedPE.MapFile(peFile, params);

        // Optional PDB symbols: exact-name lookups fall back to the PDB only
        // when the export table doesn't have a match; wildcard patterns are
        // matched against both exports and PDB symbols.
        orthia::CPdbSymbols pdbSymbols;
        bool pdbLoaded = false;
        if (!options.pdbFile.empty())
        {
            std::vector<char> pdbFile;
            orthia::LoadFileToVector(options.pdbFile, pdbFile);
            pdbSymbols.Load(pdbFile);
            pdbLoaded = true;

            DIANA_UUID moduleGuid = {};
            DI_UINT32 moduleAge = 0;
            DIANA_UUID pdbGuid = {};
            if (mappedPE.QueryGUID(&moduleGuid, &moduleAge) == DI_SUCCESS &&
                pdbSymbols.QueryGUID(&pdbGuid, nullptr) &&
                DIANA_UUID_Compare(&moduleGuid, &pdbGuid) != 0)
            {
                // Age is intentionally not compared here, matching the UI's
                // PDB loader policy: GUID is authoritative, Age can drift
                // when a PDB is post-processed without relinking the module.
                std::cerr << "warning: PDB GUID does not match module GUID\n";
            }
        }

        // Lazily built on first use: name -> RVA of PDB public/global symbols.
        std::unordered_map<std::string, DI_UINT64> pdbByName;
        bool pdbByNameBuilt = false;
        auto ensurePdbByName = [&]() -> std::unordered_map<std::string, DI_UINT64>&
        {
            if (!pdbByNameBuilt)
            {
                for (auto& sym : pdbSymbols.GetSymbols())
                {
                    pdbByName.emplace(sym.name, sym.rva);
                }
                pdbByNameBuilt = true;
            }
            return pdbByName;
        };

        // Lazily built on first use: every named PE export.
        std::vector<std::string> exportNames;
        bool exportNamesBuilt = false;
        auto ensureExportNames = [&]() -> std::vector<std::string>&
        {
            if (!exportNamesBuilt)
            {
                CExportNameCollector collector;
                mappedPE.QueryExports(&collector);
                exportNames = std::move(collector.names);
                exportNamesBuilt = true;
            }
            return exportNames;
        };

        streamToUse->OutVar(L"ImageBase", orthia::ToWideStringAsHex(mappedPE.GetImageBase()));
        for (auto& name : options.functions)
        {
            if (!IsWildcardPattern(name))
            {
                DI_UINT64 address = mappedPE.DiGetProcAddress(name.c_str());
                if (!address && pdbLoaded)
                {
                    auto& byName = ensurePdbByName();
                    auto it = byName.find(name);
                    if (it != byName.end())
                    {
                        address = mappedPE.GetImageBase() + it->second;
                    }
                }
                streamToUse->OutVar(name.c_str(), orthia::ToAnsiStringAsHex(address));
                continue;
            }

            // Wildcard pattern: match against exports first, then PDB symbols,
            // deduplicating by name (export address wins on collision).
            std::map<std::string, DI_UINT64> matches;
            for (auto& exportName : ensureExportNames())
            {
                if (utils::match(name, exportName))
                {
                    DI_UINT64 address = mappedPE.DiGetProcAddress(exportName.c_str());
                    if (address)
                    {
                        matches.emplace(exportName, address);
                    }
                }
            }
            if (pdbLoaded)
            {
                for (auto& sym : pdbSymbols.GetSymbols())
                {
                    if (utils::match(name, sym.name))
                    {
                        matches.emplace(sym.name, mappedPE.GetImageBase() + sym.rva);
                    }
                }
            }
            for (auto& match : matches)
            {
                streamToUse->OutVar(match.first, orthia::ToAnsiStringAsHex(match.second));
            }
        }
    }
    int ParseAndRunDump(int argc, wchar_t* argv[])
    {
        if (argc < 4)
        {
            PrintUsage();
            return 1;
        }
        DumpOptions dumpOptions;
        dumpOptions.exeModule = argv[2];
        std::wstring functionsRaw = argv[3];

        // sanity check
        if (int res = ValidateArgument(dumpOptions.exeModule.c_str(), L"Filename expected"))
        {
            return res;
        }
        if (int res = ValidateArgument(functionsRaw.c_str(), L"Function names list expected"))
        {
            return res;
        }

        std::transform(functionsRaw.begin(), functionsRaw.end(), functionsRaw.begin(),
            [](auto ch) { 
                if (ch == L',') 
                    return L';'; 
                return ch; 
            });

        std::vector<orthia::StringInfo> functions;
        orthia::SplitString(orthia::StringInfo(functionsRaw), orthia::StringInfo(L";"), &functions);
        dumpOptions.functions.reserve(functions.size());
        for (auto& name : functions)
        {
            if (!name.empty())
            {
                dumpOptions.functions.push_back(orthia::Utf16ToUtf8(name.ToString()));
            }
        }

        CJSONToolOutputStream jsonStream;
        CTextToolOutputStream defaultStream;
        IToolOutputStream* streamToUse = &defaultStream;
        for (int i = 4; i < argc; )
        {
            int optionIndex = i;
            int argumentIndex = i + 1;

            if (argumentIndex >= argc)
            {
                std::wcerr << L"Argument required: " << argv[i] << L"\n";
                return 1;
            }
            i+=2;

            // fmt arg
            if (wcscmp(argv[optionIndex], L"--fmt") == 0)
            {
                if (wcscmp(argv[argumentIndex], L"json") == 0)
                {
                    streamToUse = &jsonStream;
                }
                else
                {
                    return PrintInvalidArgument(argv[argumentIndex]);
                }
                continue;
            }
            // image base arg
            if (wcscmp(argv[optionIndex], L"--base") == 0)
            {
                dumpOptions.imageBase = CaptureArgument64(argv[argumentIndex]);
                continue;
            }

            // pdb file arg
            if (wcscmp(argv[optionIndex], L"--pdb") == 0)
            {
                dumpOptions.pdbFile = argv[argumentIndex];
                continue;
            }

            return PrintInvalidArgument(argv[optionIndex]);
        }

        diana::Guard< CJSONToolOutputStream::FinalFlusher> jsonStreamFlushGuard;
        if (streamToUse == &jsonStream)
        {
            jsonStreamFlushGuard.reset(&jsonStream);
        }
        Dump(dumpOptions, streamToUse);
        return 0;
    }

}