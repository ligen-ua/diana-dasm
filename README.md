# Diana-Dasm

This repository contains three parts:

- **Orthia Disassembler**: a console disassembler with a text UI for PE and ELF files and live processes;
- **diana_core**: a small, portable x86/amd64 and ARM (AArch64, AArch32) disassembler library in C, with an x86 emulator, which Orthia is built on;
- **Orthia WinDbg Plugin**: a WinDbg extension that adds cross-references and a code emulator.

# Orthia Disassembler

Orthia opens executable files and running processes, analyzes their code, and lets you browse the disassembly with WinDbg-style commands.

- PE (32/64-bit, including drivers and `ntoskrnl.exe`), ELF, running processes, raw shellcode;
- x86, x64, ARM64 and ARM32 (A32 and Thumb) code;
- runs on Windows and Linux, on x86-64 and ARM64 machines;
- cross-references, PDB symbols, comments, navigation history;
- analysis results and comments are kept in a database for each file, so files reopen quickly; `.database list | delete | cleanup` manages them;
- in a process, finds floating code: executable memory outside any module that the analysis leads to, such as hooks and JIT code (`lmf`);
- a process folder can be reopened after the process exited (`--proc-folder`, *File > Open saved process*), and `.capture` / `--capture` save the memory of the process into it;
- `--cmd` mode runs commands without the UI, for scripting.

Path: src/orthia/orthia_disasm_ui

## Quick start

Build (the binary is `bin\Release\<arch>\orthia.exe` on Windows, `cmake-release/src/orthia/orthia_disasm_ui/orthia` on Linux):
```
build-release-vs.cmd          # Windows, Visual Studio 2022
./cmake-build.sh              # Linux
```

### Windows

`build-release-vs.cmd [Win32|x86|x64|ARM64]` builds `Release` for the given platform; without an argument it builds
for the machine it runs on (read from the registry, so it is right under WOW64 or x64 emulation on ARM64).
The output goes to `bin\Release\i386`, `amd64` or `arm64`.

`win_install.cmd` copies the built binaries and docs, the Windows counterpart of `cmake --install`:
```
win_install.cmd                         Release amd64 -> C:\orthia
win_install.cmd D:\tools\orthia         another destination (or set ORTHIA_INSTALL_DIR)
win_install.cmd debug pdb               the Debug build, with .pdb files
win_install.cmd x86 "C:\orthia x86"     the 32-bit build
win_install.cmd arm64                   the ARM64 build, no WinDbg extension (the default on ARM64 Windows)
```

### Windows on ARM

`src\orthia_14_0.sln` has `ARM64` configurations (output in `bin\<Configuration>\arm64`). They use the v143 toolset,
so Visual Studio needs the "MSVC v143 ARM64 build tools" component; the Win32 and x64 builds stay on `v141_xp`.
The ARM64 build has `orthia.exe` and `orthia_disasm.exe`, not the WinDbg extension.
On ARM64 Windows, `run_tests.cmd` and `win_install.cmd` take the `arm64` folder.

### Linux

Needs CMake 3.16+, a C++17 compiler (GCC 9+ or Clang 10+) and make; x86-64 or aarch64. On Ubuntu/Debian:
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
orthia --pid 1234 --cmd '.analyze *' --cmd 'lmf /v' --cmd .capture
orthia --proc-folder 1234 --cmd lm --cmd lmf
```

Command-line options, commands, hotkeys and symbol setup: see [docs/orthia.md](docs/orthia.md).

# diana_core

Diana is a small and fast disassembler library in C, useful for Windows kernel developers.

- highly portable, written in C89, needs only the C runtime;
- includes an x86/amd64 instruction emulator (diana_processor);
- stream-oriented design;
- x86 and amd64: general-purpose, FPU, MMX, SSE to SSE4.2, AVX, AVX2, FMA3/FMA4, F16C and AVX-512;
- AArch64: base instructions, FP/SIMD, SVE and SME, decoded from tables generated from the Arm specification;
- AArch32 (ARMv7): A32 and T32 (Thumb-2, including IT blocks), VFP and NEON;
- code analysis for both: functions, cross-references, and for AArch32 the A32/T32 switches, literal pools and
  `tbb`/`tbh` or `ldr`/`add pc` switch tables;
- PE and ELF parsers;
- define `DIANA_NO_ARM` to build an x86-only library without the ARM decoders and their tables.

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
