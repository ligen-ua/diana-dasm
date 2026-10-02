#pragma once

#include "orthia_model.h"

namespace orthia
{
    // Shared bootstrap for both entry points: creates g_textManager, installs the EN
    // language pack and the EditBox context menu provider, creates and initializes the
    // config storage, runs Diana_Init/DianaProcessor_GlobalInit.
    // Platform-only steps (DianaWin32_Init, SetupWin32FSHandlers) stay in main.cpp.
    std::shared_ptr<CConfigOptionsStorage> InitAppCore();

    struct ConsoleModeOptions
    {
        std::vector<PlatformString_type> commands;      // --cmd, in order
        std::vector<PlatformString_type> files;         // --file
        std::vector<unsigned long long> pids;           // --pid
        bool analyze = false;                           // --analyze
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

    // The UI needs stdin and stdout attached to a terminal.
    bool HasInteractiveConsole();
}
