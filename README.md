# Diana-Dasm

This repository contains three parts:

- **Orthia Disassembler**: a console disassembler with a text UI for PE and ELF files and live processes;
- **diana_core**: a small, portable x86/amd64 disassembler and emulator library in C, which Orthia is built on;
- **Orthia WinDbg Plugin**: a WinDbg extension that adds cross-references and a code emulator.

# Orthia Disassembler

Orthia opens executable files and running processes, analyzes their code, and lets you browse the disassembly with WinDbg-style commands.

- PE (32/64-bit, including drivers and `ntoskrnl.exe`), ELF, running processes, raw shellcode;
- runs on Windows and Linux;
- cross-references, PDB symbols, comments, navigation history;
- analysis results and comments are kept in a database for each file, so files reopen quickly; `.database list | delete | cleanup` manages them;
- `--cmd` mode runs commands without the UI, for scripting.

Path: src/orthia/orthia_disasm_ui

## Quick start

Build (the binary is `bin\Release\amd64\orthia.exe` on Windows, `cmake-release/src/orthia/orthia_disasm_ui/orthia` on Linux):
```
build-release-vs.cmd          # Windows, Visual Studio 2022
./cmake-build.sh              # Linux
```

### Linux

Needs CMake 3.16+, a C++17 compiler (GCC 9+ or Clang 10+) and make; x86-64. On Ubuntu/Debian:
```
sudo apt install build-essential cmake
./cmake-build.sh
sudo cmake --install cmake-release --strip                    # /usr/local
cmake --install cmake-release --strip --prefix ~/.local       # or just for you, no sudo
```
This installs `bin/orthia`, its private libraries in `lib/orthia/` and the docs in `share/doc/orthia/`.
The binary finds its libraries relative to itself, so the installed tree can be moved anywhere.
To uninstall: `xargs rm < cmake-release/install_manifest.txt`.
`cpack` in `cmake-release/` makes the same tree as a `.tar.gz`.

Analysis databases and comments go to `~/.local/share/Orthia` (`$XDG_DATA_HOME/Orthia`); see [docs/databases.md](docs/databases.md).

Opening a process (`--pid`, *File > Open process*) reads its memory and needs ptrace access.
Most distributions (Yama `kernel.yama.ptrace_scope=1`) only allow that for your own child processes,
so run Orthia with `sudo`, or allow it for your user's processes until reboot with `sudo sysctl kernel.yama.ptrace_scope=0`.
Under sudo Orthia switches back to your user right away and keeps only the `CAP_SYS_PTRACE` capability;
see [docs/orthia.md](docs/orthia.md#linux-what-orthia-keeps-of-root).

Open a file or a process in the UI:
```
orthia --file C:\Windows\System32\notepad.exe
orthia --pid 1234
```

Run commands without the UI:
```
orthia --file /bin/ls --cmd lm --cmd 'u ls!$entrypoint L5'
orthia --file ntoskrnl.exe --analyze --cmd 'x nt!KeBugCheck*'
```

Command-line options, commands, hotkeys and symbol setup: see [docs/orthia.md](docs/orthia.md).

# diana_core

Diana is a small and fast disassembler library in C, useful for Windows kernel developers.

- highly portable, needs only the C runtime;
- includes an instruction emulator (diana_processor);
- stream-oriented design;
- platforms: i386, amd64;
- instructions: x586/amd64/FPU/MMX/SSE/SSE2.

Path: src/diana_core

# Orthia WinDbg Plugin

`orthia.dll` is a WinDbg extension: it stores cross-references for loaded modules in an SQLite profile
and runs code in an emulator inside virtual machines that read the debuggee's memory.

Setup, commands and examples: see [docs/windbg-plugin.md](docs/windbg-plugin.md).

# Testing

`run_tests.cmd` (Windows) and `cmake-test.sh` (Linux) run the C++ tests and then the command-line tests.
The command-line tests use pytest; see [tests/cli/README.md](tests/cli/README.md).

# Links

Source code and issues: https://github.com/ligen-ua/diana-dasm

The old SourceForge SVN repository (https://sourceforge.net/projects/diana-dasm/) is no longer updated.
