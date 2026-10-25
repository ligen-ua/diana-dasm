# Orthia Disassembler

Orthia is a console disassembler with a text-mode UI (TUI).
It opens executable files and live processes, analyzes their code, and lets you browse the result like a debugger.
A `--cmd` mode runs the same commands without the UI, for scripts and tests.

Supported targets:
- PE files (32 and 64-bit), including drivers and `ntoskrnl.exe`: x86, x64, ARM64 and ARM32 (Thumb-2);
- ELF files (Linux): x86, x64, AArch64 and ARM32 (A32 and Thumb, mixed);
- running processes (Windows and Linux), x86 and x64, and ARM processes when Orthia runs on an ARM machine: each module
  is disassembled as its headers say, other memory as the process's architecture; `threads` shows call stacks of x86
  and x64 processes on Windows only;
- raw shellcode: files in an unknown format can be opened as raw code: x86, x64, arm64, arm (A32) or thumb (T32).

Analysis results (modules, cross-references, symbols) and your comments are kept in an SQLite database for each file,
so a file opens quickly the second time and comments survive restarts. See [databases.md](databases.md) for what is
stored where, and the `.database` commands to list, delete and clean up databases.

## Building

Windows (Visual Studio 2022):
```
build-release-vs.cmd [Win32|x86|x64|ARM64]
```
Without an argument it builds `Release` for the machine it runs on.
The binary is `bin\Release\amd64\orthia.exe` (`i386` for Win32, `arm64` for ARM64).
`win_install.cmd [debug] [x86|arm64] [pdb] [<dest>]` copies it and the docs to `C:\orthia`, `%ORTHIA_INSTALL_DIR%` or `<dest>`.

Linux:
```
./cmake-build.sh
```
The binary is `cmake-release/src/orthia/orthia_disasm_ui/orthia`. To install it (into `/usr/local`, or `--prefix <dir>`):
```
sudo cmake --install cmake-release --strip
```
This gives `bin/orthia`, its private libraries in `lib/orthia/` (found through RUNPATH `$ORIGIN/../lib/orthia`,
so the tree is relocatable) and the docs in `share/doc/orthia/`.

### Linux: opening processes

Orthia reads a process's memory through `/proc/<pid>/mem`, which needs ptrace access to the process.
With Yama's default `kernel.yama.ptrace_scope=1` (Ubuntu, Fedora, ...) a normal user only has it for its own
child processes, so `--pid` and *Open process* fail with "Operation not permitted" for anything else. Either:
- run Orthia with `sudo`; or
- for processes of your own user, `sudo sysctl kernel.yama.ptrace_scope=0` (until reboot; to keep it,
  `echo 'kernel.yama.ptrace_scope = 0' | sudo tee /etc/sysctl.d/10-ptrace.conf`). This applies to every
  program on the system, not only Orthia.

`--pid self` always works. With `--cmd` the error is followed by a hint for the current `ptrace_scope`.

### Linux: what Orthia keeps of root

Reading memory is the only thing Orthia needs root for. When started as root, it drops everything
else before it opens anything:
- **Under `sudo`** (or `pkexec`) it switches back to your user (`SUDO_UID`), so it uses your
  data folder (`~/.local/share/Orthia`) and symbol folders, and the files it creates are yours.
  sudo drops `ORTHIA_SYMBOL_PATH` and `ORTHIA_HOME`, so pass them on the command line if you need them:
  `sudo ORTHIA_SYMBOL_PATH="$HOME/sym" orthia --pid 1234`.
- **Logged in as root** (`su`, a root shell) it stays uid 0 and uses `/root/.local/share/Orthia`.
- Either way only the `CAP_SYS_PTRACE` capability is kept: it can read any process, but it can't
  read or write files your user (or, as root, their owner) can't, and it can't regain root.
  Orthia can't be attached to by your user either (it is not dumpable).
- A seccomp filter denies what `CAP_SYS_PTRACE` would allow besides reading memory (`ptrace`,
  `process_vm_writev`, `pidfd_getfd`, ...), new namespaces, and kernel interfaces Orthia has no
  use for (module loading, `bpf`, `mount`, `io_uring`, ...). Normal runs without sudo get the same filter.
- If the drop fails, Orthia exits instead of going on as root.
- `--no-sandbox` turns all of this off: Orthia keeps full root rights and no seccomp filter, and says so with
  `Warning: running with full root rights and no sandbox`.

Don't install Orthia setuid root or with `setcap cap_sys_ptrace`: every local user could then read
any process's memory. Orthia refuses to run setuid.

The parsers still run with `CAP_SYS_PTRACE`: open only files you trust under sudo.

### Windows: sandbox

Before it opens anything, Orthia restricts its own process:
- Every privilege is removed from its token except `SeDebugPrivilege` (the counterpart of `CAP_SYS_PTRACE`;
  kept as it is, not enabled) and `SeChangeNotifyPrivilege`. On XP before SP2, where privileges can't be
  removed, they are disabled instead.
- On Windows 8 and later it also sets process mitigation policies: no dynamic code; no AppInit DLLs,
  legacy IMEs or window hooks; no DLLs from remote shares or with a low integrity label, System32 searched
  first; and, where Windows supports it, only DLLs signed by Microsoft. Windows XP to 7 get the privilege
  removal only.

The signature policy also keeps out `orthia_proc_win32.dll`, the optional open-process plugin placed next
to `orthia.exe`. If it is there, Orthia warns that it is not loaded; run with `--no-sandbox` to use it.

Problems setting up the sandbox are reported as warnings only when Orthia runs elevated.
`--no-sandbox` turns the sandbox off; an elevated run then warns `running elevated with no sandbox`.

## Command line

```
orthia [--file <filename>]... [--pid <pid>]... [--cmd <command>]... [--analyze] [--overwrite-db] [--no-sandbox]
orthia --file <filename> --format raw --base <address> --arch <arch> --cmd <command>...
```

| Option | Meaning |
|---|---|
| `--file <filename>` | open the given executable file |
| `--pid <pid>` | open the process with the given id: decimal, `0x`-prefixed hex, or `self` for Orthia's own process |
| `--cmd <command>` | run `<command>` without the UI and exit; repeat it for more commands, they run in order |
| `--analyze` | with `--cmd`: deep code analysis and symbol loading on open |
| `--format <format>` | with `--cmd`: open the `--file` as `pe`, `elf` or `raw` (code without headers) instead of the format it is detected as |
| `--base <address>` | with `--format raw`: where the code is placed (ARM code: a multiple of 4, Thumb: of 2) |
| `--arch <arch>` | with `--format raw`: how the code is disassembled and analyzed: `x86`, `x64`, `arm64`, `arm` or `thumb` |
| `--overwrite-db` | give every target opened in this run a new, empty database instead of reusing the one of an earlier open (comments included, see [databases](databases.md)) |
| `--no-sandbox` | don't restrict Orthia: on Linux no privilege drop and no seccomp filter (see [what Orthia keeps of root](#linux-what-orthia-keeps-of-root)), on Windows no privilege removal and no mitigation policies (see [sandbox](#windows-sandbox)) |
| `--run-tests` | run the built-in tests and exit |
| `-h`, `--help` | show help and exit; on Windows also `/?` and `-?` |
| `--version` | show the version (`orthia 1.x.x.xx`) and exit |

The value after `--file`, `--pid` or `--cmd` is taken as it is, even if it starts with `--`.

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
orthia --file firmware.bin --format raw --base 0x8000 --arch thumb --analyze --cmd 'u 8000 L20'
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
| 2 | bad or incomplete argument, or the UI was started without an interactive console (stdin or stdout redirected; use `--cmd`) |
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
| `x [/a\|/n] [/v] <mask>` | Examine symbols, e.g. `x nt!Ke*`; without `!`, the main module is searched. `/a` sorts by address, `/n` by name (default: by type), `/v` shows the symbol type |
| `u <address> [L<count>]` | Disassemble `<count>` instructions |
| `lm` | List modules with their status: `.NET`, `unresolved`, `linked`, `stale`, `unlinked`, `analyzed`, `symbols` (see [Dependencies in file mode](#dependencies-in-file-mode)). `.NET` marks a managed image: an IL-only one has no native entry point and almost no native code |
| `lmf` | List the floating modules of a process: executable memory no module owns that the analysis led to, with protection, entry points and status (`pending`, `analyzed`, `stale`); see [Floating code](#floating-code) |
| `db`, `dw`, `dd`, `dq`, `dp`, `dps` | Display memory as bytes, words, dwords, qwords, pointers, or pointers with symbols |
| `threads` | Display threads (processes only) |
| `modinfo <module>` | Show file and debug info for a module (PE headers, PDB or ELF build id) and where its image comes from; for a floating module, its allocation, code and entry points with the instructions that lead there |
| `sections [-v] <module\|address>` | List the sections of a module: name, address, size and `RWX` flags; `-v` adds every header field (see [Sections](#sections)) |
| `.reload [<module>]` | Reload symbols for a module |
| `.analyze [/f] <module>` | Run the full analysis of a module, then find the floating code it leads to (processes). `/f` analyzes an analyzed module again |
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
The other commands take plain names or masks: `x` takes a symbol mask, `modinfo`, `.reload` and `.analyze` take a module name (`.analyze` also takes `/f`).
`sections` takes a module name or an expression, and lists the module that contains the address.

| Syntax | Meaning |
|---|---|
| `7ff76948`, `7ff76948h`, `0x7ff76948` | hex number (numbers are hex by default) |
| ``fffff806`b1458a9e`` | hex number with a WinDbg-style `` ` `` separator |
| `0n15` | decimal number |
| `ntdll!LdrLoadDll` | symbol: an export or a private (PDB) symbol of the module |
| `ntdll`, `ntdll+1000` | module base (name without extension), module base plus offset |
| `ls!$entrypoint` | module entry point |
| `mylib!$init`, `mylib!$init_array_0000` | code that runs before the entry point, see [Startup routines](#startup-routines) |
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

### Floating code

In a process, `.analyze <module>` also looks at the jumps, calls and taken addresses that leave the module.
Those that land in committed, executable memory no loaded module owns (a hook stub, injected code, a JIT stub)
make a *floating module*, one per OS allocation, named `fl_<allocation base>`: `fl_7ff7a5d70000`.
Each one is then analyzed from its entry points in the background, after the rest of the queue, and the floating
code it leads to is found the same way.

A floating module works like any other: `u fl_7ff7a5d70000`, `x fl_7ff7a5d70000!*` (its entry points are
`$entry_0001`, `$entry_0002`, ... by address), `modinfo fl_7ff7a5d70000`, `.analyze fl_7ff7a5d70000`.
`lm` and the Modules window leave them out; `lmf` lists them.

Floating modules are saved in the process database. When the same process is opened again, each one is
checked against the memory (the allocation and a hash of its bytes); one that changed or went away is kept
and shown as `stale`, and is not analyzed again.

Targets in zero-filled memory are left out: no code starts with `00 00`.

#### TODO

Entry points and targets:
- [ ] More entry points than the static references: thread start addresses, return addresses on thread stacks,
  pointers in data (hooked IAT slots, function pointer tables), exception handlers and other callbacks.
- [ ] Store the reference kind in `tbl_references` (call, jump, taken address, data access); a schema step. The
  database has no kind, so discovery takes every reference that leaves a module, and filters by executable
  memory only. In a 32-bit process an `imm32` constant that falls into such an allocation adds an entry point
  (`push 6A0004h`); the CLI tests allow more than one entry there for that reason.
- [ ] Mark how each entry was reached (call, jump, taken address) in `modinfo`, once the kind is stored.

Targets:
- [ ] Minidumps: there is no minidump support yet.
- [ ] File mode: nothing to find there yet; with captured and memory dependency images it could be.

Regions:
- [ ] Linux: an allocation is one `/proc/<pid>/maps` line. A region that `mprotect` split into several lines
  makes several floating modules; adjacent anonymous mappings are never merged.
- [ ] An allocation that starts with `MZ` or `\x7fELF` (a module mapped by hand) is raw code, with the header only
  reported by `modinfo`. Open it with its format instead: sections, exports, entry point.
- [ ] Big allocations (a JIT heap): the analysis looks at 16 MB from the first entry point, and entry points
  beyond that are not analyzed. The bytes hash covers the first 16 MB only.
- [ ] ARM32: when that 16 MB window doesn't start at the allocation base, the code map metainfo (type 5) is saved
  under the window start, which has no `tbl_modules` row (foreign key). Save it under the module address.
- [ ] Limits per process: 1024 new regions per discovery pass, 256 floating modules analyzed. Make them options,
  or say in `lmf` what was left out.
- [ ] A floating module where the OS reports a module later (one loaded there since) is hidden without a word.

Stale and live memory:
- [ ] The check against memory runs only when the process is opened again. A command to check the floating
  modules of the open process again (and to analyze a changed one again) is missing.
- [ ] A stale module stays until the database is deleted; there is no command to drop it.

Commands and UI:
- [ ] `save`: write a floating module (bytes, base, architecture, entry points) to a file or to another
  workspace item, once process information is persistent.
- [ ] The Modules window leaves floating modules out; give them a view of their own.

Architectures and tests:
- [ ] ARM patches: the diana patcher (`DianaHook_PatchStream`) hooks x86 and x64 code only, so the test helper
  `orthia_floating_helper` prints `unsupported` on ARM64 and ARM, and `tests/cli/test_floating.py` is skipped there.
- [ ] ARM floating code has no test at all: add gtests with an AArch64 and an AArch32 (A32 and T32, bit 0 of the
  entry) module that branches into a fake region, like the x86 ones in `ui_tests.cpp`.
- [ ] The test helper is not checked in a Debug build (incremental linking puts a jump thunk where the hooked
  function's address points).

### Startup routines

Code that runs before the entry point is analyzed like the entry point and gets a name, so `x` and expressions find it:

| Name | Format | What it is |
|---|---|---|
| `$tls_NNNN` | PE | a TLS callback, by its index in the callback array |
| `$preinit_array_NNNN` | ELF | an entry of `DT_PREINIT_ARRAY` |
| `$init` | ELF | the `DT_INIT` function |
| `$init_array_NNNN` | ELF | an entry of `DT_INIT_ARRAY` |
| `$ifunc_<name>` | ELF | the resolver of an IFUNC symbol (`$ifunc_NNNN` when the symbol has no name) |

`NNNN` is a 4-digit hex index. `x mylib!$*` lists them all, together with `$entrypoint`.

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

ELF modules get their names from the exported symbols of the dynamic symbol table and from the [startup routines](#startup-routines).
Separate debug files are not loaded yet. Orthia looks for one (by build id in `<folder>/.build-id/xx/<rest>.debug`,
and by the `.gnu_debuglink` name next to the module, in its `.debug` subfolder and in the symbol folders; on Linux also
under `/usr/lib/debug`), and when it finds one it only reports that ELF symbol loading is not supported yet.

## Raw shellcode in the UI

When *File > Open executable* gets a file that is neither PE nor ELF, the UI asks whether to open it as raw shellcode.
The dialog takes the load address and the architecture (Left/Right switch between x86, x64, arm64, arm and thumb;
x64 by default), the same choices as `--base` and `--arch` on the command line, with the same alignment rule for ARM code. They are saved in the file's database
(`parameters.xml`, see [databases.md](databases.md)), so the file reopens with the same layout without asking.

## Testing

`run_tests.cmd` (Windows) and `cmake-test.sh` (Linux) run the C++ tests and then the command-line tests.
The command-line tests are described in [tests/cli/README.md](../tests/cli/README.md).
