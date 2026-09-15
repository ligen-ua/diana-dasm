#include "cmd_common.h"
#include "orthia_files.h"
#include "orthia_pe.h"
#include "orthia_shellcode.h"
#include "orthia_diana_print.h"
#include <memory>

namespace orthia
{
    struct DisasmOptions
    {
        std::wstring inputFile;
        DI_UINT64 address = 0;
        bool isRva = false;
        std::optional<DI_UINT64> imageBase;
        orthia::Address_type sizeInCommands = 10;
        std::optional<int> dianaMode; // set => shellcode mode
    };

    struct WcoutTextPrinter : orthia::ITextPrinter
    {
        void PrintLine(const orthia::PlatformString_type& line) override
        {
            std::wcout << line << L"\n";
        }
    };

    static void Disasm(DisasmOptions& options)
    {
        std::vector<char> fileData;
        orthia::LoadFileToVector(options.inputFile, fileData);
        if (fileData.empty())
        {
            throw std::runtime_error("Input file is empty");
        }

        std::unique_ptr<orthia::ISimpleFile> file;
        if (options.dianaMode.has_value())
        {
            file = std::make_unique<orthia::CSimpleShellcodeFile>(
                fileData, options.imageBase.value_or(0), *options.dianaMode);
        }
        else
        {
            auto peFile = std::make_unique<orthia::CSimplePeFile>();
            orthia::MapFileParameters params;
            if (options.imageBase.has_value())
            {
                params.imageBase = *options.imageBase;
            }
            peFile->MapFile(fileData, params);
            file = std::move(peFile);
        }

        DI_UINT64 imageBase = file->GetImageBase();
        DI_UINT64 imageEnd = file->GetImageEnd();
        DI_UINT64 absoluteAddress = options.isRva ? (imageBase + options.address) : options.address;
        if (absoluteAddress < imageBase || absoluteAddress >= imageEnd)
        {
            throw std::runtime_error("Address is out of module range");
        }

        const std::vector<char>& mapped = file->GetMappedFile();
        size_t offset = (size_t)(absoluteAddress - imageBase);

        WcoutTextPrinter printer;
        orthia::CVmAsmMemoryPrinter<diana::CMasmString> asmPrinter(
            &printer, file->GetDianaMode(), options.sizeInCommands);

        asmPrinter.OnRange(
            orthia::VmMemoryRangeInfo(absoluteAddress, mapped.size() - offset,
                                       orthia::VmMemoryRangeInfo::flags_hasData),
            &mapped[offset]);
        std::wcout.flush();
    }

    int ParseAndRunDisasm(int argc, wchar_t* argv[])
    {
        if (argc < 4)
        {
            PrintUsage();
            return 1;
        }
        DisasmOptions options;
        options.inputFile = argv[2];
        if (int res = ValidateArgument(options.inputFile.c_str(), L"Filename expected"))
        {
            return res;
        }
        if (int res = ValidateArgument(argv[3], L"Address expected"))
        {
            return res;
        }
        options.address = CaptureArgument64(argv[3]);

        for (int i = 4; i < argc; )
        {
            int optionIndex = i;

            // --rva is value-less, unlike every other option here
            if (wcscmp(argv[optionIndex], L"--rva") == 0)
            {
                options.isRva = true;
                i += 1;
                continue;
            }

            int argumentIndex = i + 1;
            if (argumentIndex >= argc)
            {
                std::wcerr << L"Argument required: " << argv[i] << L"\n";
                return 1;
            }
            i += 2;

            if (wcscmp(argv[optionIndex], L"--size") == 0)
            {
                options.sizeInCommands = (orthia::Address_type)CaptureArgument64(argv[argumentIndex]);
                continue;
            }
            if (wcscmp(argv[optionIndex], L"--base") == 0)
            {
                options.imageBase = CaptureArgument64(argv[argumentIndex]);
                continue;
            }
            if (wcscmp(argv[optionIndex], L"--mode") == 0)
            {
                if (wcscmp(argv[argumentIndex], L"x86") == 0)
                {
                    options.dianaMode = 4;
                }
                else if (wcscmp(argv[argumentIndex], L"x64") == 0)
                {
                    options.dianaMode = 8;
                }
                else
                {
                    return PrintInvalidArgument(argv[argumentIndex]);
                }
                continue;
            }

            return PrintInvalidArgument(argv[optionIndex]);
        }

        Disasm(options);
        return 0;
    }
}
