#include "cmd_common.h"
#include "orthia_files.h"
#include "orthia_pe.h"

namespace orthia
{
    struct DumpOptions
    {
        std::wstring exeModule;
        std::optional<DI_UINT64> imageBase;
        std::wstring pdbFile;
        std::vector<std::string> functions;
    };

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

        streamToUse->OutVar(L"ImageBase", orthia::ToWideStringAsHex(mappedPE.GetImageBase()));
        for (auto& name : options.functions)
        {
            DI_UINT64 address = mappedPE.DiGetProcAddress(name.c_str()); 
            streamToUse->OutVar(name.c_str(), orthia::ToAnsiStringAsHex(address));
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
                std::cerr << "Argument required: " << argv[i] << "\n";
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

            // image base arg
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