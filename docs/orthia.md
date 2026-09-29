# Orthia Disassembler

Orthia is a console disassembler with a text-mode UI (TUI).
It opens executable files and live processes, analyzes their code, and lets you browse the result like a debugger.
A `--cmd` mode runs the same commands without the UI, for scripts and tests.

Supported targets:
- PE files (32 and 64-bit), including drivers and `ntoskrnl.exe`;
- ELF files (Linux);
- running processes (Windows and Linux);
- raw shellcode: files in an unknown format can be opened as raw code.

Analysis results (modules, cross-references, symbols, comments) are cached in an SQLite database for each file,
so a file opens quickly the second time.

## Building

Windows (Visual Studio 2022, Release x64 — the Debug configuration does not build):
```
build-release-vs.cmd
```
The binary is `bin\Release\amd64\orthia.exe`.

Linux:
```
./cmake-build.sh
```
The binary is `cmake-release/src/orthia/orthia_disasm_ui/orthia_disasm_ui`.

## Command line

```
orthia [--file <filename>]... [--pid <pid>]... [--cmd <command>]... [--analyze]
```

| Option | Meaning |
|---|---|
| `--file <filename>` | open the given executable file |
| `--pid <pid>` | open the process with the given id (`self` for Orthia's own process) |
| `--cmd <command>` | run `<command>` without the UI and exit; repeat it for more commands, they run in order |
| `--analyze` | with `--cmd`: deep code analysis and symbol loading on open |
| `--run-tests` | run the built-in tests and exit |
| `-h`, `--help`, `/?` | show help and exit |

Without `--cmd` the UI starts with all given files and processes open.
With `--cmd`, exactly one `--file` or `--pid` is required.

Examples:
```
orthia --file C:\Windows\System32\notepad.exe
orthia --pid 1234
orthia --file data/elf/dmesg --cmd lm --cmd 'u dmesg!$entrypoint L5'
orthia --file ntoskrnl.exe --analyze --cmd 'x nt!KeBugCheck*'
```

### Quick open vs `--analyze`

By default, `--cmd` opens a target quickly: headers, modules, imports and exports only.
`--analyze` does what the UI always does: cross-references, PDB loading and private-symbol analysis.
A database created by a quick open is upgraded in place by a later `--analyze` open, or by `.analyze <module>`.

### Exit codes (`--cmd` mode)

| Code | Meaning |
|---|---|
| 0 | success |
| 1 | at least one command reported an error |
| 2 | bad or incomplete argument |
| 3 | the target failed to open, or no target was given |
| 4 | unexpected error |

### Environment

| Variable | Meaning |
|---|---|
| `ORTHIA_HOME` | data folder, used instead of `%APPDATA%\Orthia` (Windows) or `$XDG_DATA_HOME/Orthia` (Linux) |
| `ORTHIA_SYMBOL_PATH` | symbol folders separated by `;`, used instead of the defaults (`C:\Sym;C:\Symbols` on Windows, `~/sym;~/symbols` on Linux) |

## Commands

The commands follow WinDbg syntax. Type them in the UI's command window, or pass them with `--cmd`.
Addresses are expressions: `module!symbol`, `module+offset`, hex numbers and arithmetic all work.

| Command | Description |
|---|---|
| `x <mask>` | Examine symbols, e.g. `x nt!Ke*`; without `!`, the main module is searched |
| `u <address> [L<count>]` | Disassemble `<count>` instructions |
| `lm` | List loaded modules |
| `db`, `dw`, `dd`, `dq`, `dp`, `dps` | Display memory as bytes, words, dwords, qwords, pointers, or pointers with symbols |
| `threads` | Display threads (processes only) |
| `modinfo <module>` | Show file and debug info for a module (PE headers, PDB or ELF build id) |
| `.reload [<module>]` | Reload symbols for a module |
| `.analyze <module>` | Run the full analysis of a module |
| `.symfix [<path>]` | Set the symbols directory |
| `cls` | Clear the screen (UI only) |
| `exit` | Exit the program (UI only) |

In the UI, press CTRL+C in the command edit box to stop a running command.

## UI hotkeys

Global:

| Key | Action |
|---|---|
| F10 or ALT | Toggle menu |
| ALT+Letter | Enter menu |
| Tab | Focus next panel group |
| CTRL+Tab | Focus next panel |
| ESC | Close modal window |
| CTRL+Left/Right | Scroll list view |

Disassembly view:

| Key | Action |
|---|---|
| G, CTRL+click | Go to address |
| H | Open history |
| ; | Write a comment |
| BACKSPACE | Return to the previous location |
| CTRL+X | Open the cross-references dialog |

## Symbols

PDB files are searched in the symbol folders and in the module's own folder and its subfolders.
A PDB is matched to a module by GUID only, not by age, because public Microsoft symbols often have a different age.

## Testing

`run_tests.cmd` (Windows) and `cmake-test.sh` (Linux) run the C++ tests and then the command-line tests.
The command-line tests are described in [tests/cli/README.md](../tests/cli/README.md).
