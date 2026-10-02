# Orthia Disassembler

Orthia is a console disassembler with a text-mode UI (TUI).
It opens executable files and live processes, analyzes their code, and lets you browse the result like a debugger.
A `--cmd` mode runs the same commands without the UI, for scripts and tests.

Supported targets:
- PE files (32 and 64-bit), including drivers and `ntoskrnl.exe`;
- ELF files (Linux);
- running processes (Windows and Linux);
- raw shellcode: files in an unknown format can be opened as raw code.

Analysis results (modules, cross-references, symbols) and your comments are kept in an SQLite database for each file,
so a file opens quickly the second time and comments survive restarts. See [databases.md](databases.md) for what is
stored where, and the `.database` commands to list, delete and clean up databases.

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

All arguments are optional: plain `orthia` with no arguments starts the UI with an empty workspace,
and you open files and processes from the **File** menu (*Open executable*, *Open process*).
Without `--cmd` the UI starts with all given files and processes open.
With `--cmd`, exactly one `--file` or `--pid` is required, except when every command is a `.database` command:
those work on the data folder and run without a target.

Examples:
```
orthia
orthia --file C:\Windows\System32\notepad.exe
orthia --pid 1234
orthia --file /bin/ls --cmd lm --cmd 'u ls!$entrypoint L5'
orthia --file ntoskrnl.exe --analyze --cmd 'x nt!KeBugCheck*'
orthia --cmd '.database list'
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
| 3 | the target failed to open, or no target was given for a command that needs one |
| 4 | unexpected error |

### Environment

| Variable | Meaning |
|---|---|
| `ORTHIA_HOME` | data folder, used instead of `%APPDATA%\Orthia` (Windows) or `$XDG_DATA_HOME/Orthia` (Linux) |
| `ORTHIA_SYMBOL_PATH` | symbol folders separated by `;`, used instead of the defaults (`C:\Sym;C:\Symbols` on Windows, `~/sym;~/symbols` on Linux) |

## Commands

The commands follow WinDbg syntax. Type them in the UI's command window, or pass them with `--cmd`.

| Command | Description |
|---|---|
| `x <mask>` | Examine symbols, e.g. `x nt!Ke*`; without `!`, the main module is searched |
| `u <address> [L<count>]` | Disassemble `<count>` instructions |
| `lm` | List modules with their status: `unresolved`, `linked`, `stale`, `unlinked`, `analysis`, `symbols` (see [Dependencies in file mode](#dependencies-in-file-mode)) |
| `db`, `dw`, `dd`, `dq`, `dp`, `dps` | Display memory as bytes, words, dwords, qwords, pointers, or pointers with symbols |
| `threads` | Display threads (processes only) |
| `modinfo <module>` | Show file and debug info for a module (PE headers, PDB or ELF build id) and where its image comes from |
| `sections [-v] <module\|address>` | List the sections of a module: name, address, size and `RWX` flags; `-v` adds every header field (see [Sections](#sections)) |
| `.reload [<module>]` | Reload symbols for a module |
| `.analyze <module>` | Run the full analysis of a module |
| `.symfix [<path>]` | Set the symbols directory |
| `.database list` | List the databases in the data folder; needs no open target (see [databases.md](databases.md#managing-databases)) |
| `.database delete <sel>...` | Delete databases, selected by a SHA-1 prefix, a pid or a file name; an open one is refused |
| `.database cleanup` | Remove the databases of exited processes and broken or half-deleted folders |
| `cls` | Clear the screen (UI only) |
| `exit` | Exit the program (UI only) |

In the UI, press CTRL+C in the command edit box to stop a running command.

### Expressions

The address of `u` and of the `db`/`dw`/`dd`/`dq`/`dp`/`dps` commands is an expression, not just a number.
The same expressions work in the UI's *Go to address* dialog (G).
The other commands take plain names or masks: `x` takes a symbol mask, `modinfo`, `.reload` and `.analyze` take a module name.
`sections` takes a module name or an expression, and lists the module that contains the address.

| Syntax | Meaning |
|---|---|
| `7ff76948`, `7ff76948h`, `0x7ff76948` | hex number (numbers are hex by default) |
| ``fffff806`b1458a9e`` | hex number with a WinDbg-style `` ` `` separator |
| `0n15` | decimal number |
| `ntdll!LdrLoadDll` | symbol: an export or a private (PDB) symbol of the module |
| `ntdll`, `ntdll+1000` | module base (name without extension), module base plus offset |
| `ls!$entrypoint` | module entry point |
| `+`, `-`, `*`, `/`, `( )` | arithmetic with the usual precedence, unary `+`/`-` included |
| `poi(<expr>)` | the pointer-sized value stored at `<expr>` |
| `DS:[7ff769486040h]` | a memory operand as the disassembler prints it; evaluates to the address in the brackets, so you can paste it from the disassembly |

The length after `L` (or `l`) is an expression too, and the space after it is optional:
```
u ntdll!LdrLoadDll+10 L0n20
dps poi(ntdll+1000) L(2*4)
db ntdll L 40
```
The length is a count of instructions for `u` and a count of elements for the `d*` commands.
`L?` and negative lengths are not supported.

### Sections

PE section headers are mapped with the image, so `sections` reads them from memory.
ELF section headers are not part of any loaded segment: in a process the memory at `e_shoff` holds something else.
They are read from the module's file on disk instead, and only if the file's ELF and program headers match the loaded image.
When there is no such file, `sections` lists the program headers (segments) from memory and says why.
An ELF section that is not loaded (no `SHF_ALLOC`, e.g. `.comment` or `.symtab`) shows `-` as its address.
The flags are `R` (loaded), `W` (writable) and `X` (executable), for both formats.

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

## Dependencies in file mode

When a file is opened for the first time, its imports are resolved like a loader would: each imported
module is looked up in the directory of the opened file first, then in the system search path
(`System32` and `System32\drivers` on Windows), mapped, and placed after the main image in a synthetic
address space. Their exports go into the database, and the main module's import table is linked to them,
so `dps` on an import slot shows the target and `u kernel32!CreateFileW` disassembles the dependency.

The dependency files themselves are not copied. What is recorded is their path and identity: for PE the
`TimeDateStamp`, `SizeOfImage` and the debug GUID, for ELF the GNU build id. A dependency is read from disk
only when its bytes are first needed, and only if the file at the recorded path is still the same build.
`lm` shows the outcome per module:

| Status | Meaning |
|---|---|
| `linked` | the file is available and matches the recorded identity; bytes are read on demand |
| `stale` | the file is missing or is another build (for example after a Windows update); names still work, bytes do not. `modinfo` says why |
| `unresolved` | the import could not be found at first open (API sets such as `api-ms-win-*`); a range is reserved for it, nothing is mapped |
| `unlinked` | recorded by an older Orthia version without identity; names only |

The layout is fixed by the database: a file always reopens with the same module addresses, whether or not
the dependencies are still readable.

## Symbols

PDB files are searched in the symbol folders and in the module's own folder and its subfolders.
A PDB is matched to a module by GUID only, not by age, because public Microsoft symbols often have a different age.

## Testing

`run_tests.cmd` (Windows) and `cmake-test.sh` (Linux) run the C++ tests and then the command-line tests.
The command-line tests are described in [tests/cli/README.md](../tests/cli/README.md).
