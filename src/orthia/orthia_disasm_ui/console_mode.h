#pragma once

#include "orthia_model.h"

namespace orthia
{
    // Shared bootstrap for both entry points: creates g_textManager, installs the EN
    // language pack and the EditBox context menu provider, creates and initializes the
    // config storage, runs Diana_Init/DianaProcessor_GlobalInit.
    // Platform-only steps (DianaWin32_Init, SetupWin32FSHandlers) stay in main.cpp.
    std::shared_ptr<CConfigOptionsStorage> InitAppCore();

    // --format/--base/--arch: how to open the --file. Empty format = detect it from the file
    struct FileFormatOptions
    {
        PlatformString_type format;                     // a format id (pe, elf, raw) or empty
        unsigned long long base = 0;                    // formats opened by hand (raw): where the bytes go
        int dianaMode = 0;                              // and the pointer size, DIANA_MODE32 / DIANA_MODE64
        int dianaArch = DIANA_ARCH_X86;                 // and how the code decodes, DIANA_ARCH_*
    };

    struct ConsoleModeOptions
    {
        std::vector<PlatformString_type> commands;      // --cmd, in order
        std::vector<PlatformString_type> files;         // --file
        std::vector<unsigned long long> pids;           // --pid
        std::vector<PlatformString_type> procFolders;   // --proc-folder
        // --no-analysis, --no-symbols: the config, CConfigOptionsStorage::GetCliAutoTasks
        FileFormatOptions fileFormat;
    };

    enum ConsoleExitCode
    {
        consoleExit_Ok = 0,
        consoleExit_CommandFail = 1,    // at least one command reported an error
        consoleExit_Usage = 2,          // bad or incomplete argument
        consoleExit_OpenFail = 3,       // target failed to open, or no target given
        consoleExit_Exception = 4
    };

    // Headless entry point: opens the targets, runs the commands and returns an exit code.
    // Creates no windows and no oui::CConsoleApp.
    int RunConsoleMode(std::shared_ptr<CProgramModel> model, const ConsoleModeOptions& options);

    // --pid value: decimal, or hex with a 0x prefix. Rejects signs, junk and overflow.
    bool ParsePidArgument(const PlatformString_type& text, unsigned long long* pid);

    // Checks --format/--base/--arch together (nullptr = not given) and fills `result`.
    // On a bad combination or value returns false with the message for the user in `error`.
    bool ParseFileFormatOptions(const PlatformString_type* format,
        const PlatformString_type* base,
        const PlatformString_type* arch,
        FileFormatOptions& result,
        std::string& error);

    // The UI needs stdin and stdout attached to a terminal.
    bool HasInteractiveConsole();
}
