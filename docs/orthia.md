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
orthia [--file <filename>]... [--pid <pid>]... [--proc-folder <folder|pid>]... [--cmd <command>]... [--no-analysis] [--no-symbols] [--capture] [--overwrite-db] [--no-sandbox]
orthia --file <filename> --format raw --base <address> --arch <arch> --cmd <command>...
```

| Option | Meaning |
|---|---|
| `--file <filename>` | open the given executable file |
| `--pid <pid>` | open the process with the given id: decimal, `0x`-prefixed hex, or `self` for Orthia's own process |
| `--proc-folder <folder\|pid>` | open a saved process folder after the process exited: a `proc/` folder, its `data.db`, or a pid or folder name as `.database list` shows them (see [Saved process folders](#saved-process-folders)) |
| `--cmd <command>` | run `<command>` without the UI and exit; repeat it for more commands, they run in order |
| `--no-analysis` | no code analysis of the opened target; `.analyze` does it later (see [What starts after an open](#what-starts-after-an-open)) |
| `--no-symbols` | no symbol loading for the opened target; `.reload` does it later |
| `--analyze` | accepted for older scripts and ignored: the analysis is the default |
| `--format <format>` | with `--cmd`: open the `--file` as `pe`, `elf` or `raw` (code without headers) instead of the format it is detected as |
| `--base <address>` | with `--format raw`: where the code is placed (ARM code: a multiple of 4, Thumb: of 2) |
| `--arch <arch>` | with `--format raw`: how the code is disassembled and analyzed: `x86`, `x64`, `arm64`, `arm` or `thumb` |
| `--capture` | deep mode for processes: save the memory of each module into the process folder when its analysis is done (see [Capture](#capture-deep-mode)) |
| `--overwrite-db` | give every target opened in this run a new, empty database instead of reusing the one of an earlier open (comments included, see [databases](databases.md)) |
| `--no-sandbox` | don't restrict Orthia: on Linux no privilege drop and no seccomp filter (see [what Orthia keeps of root](#linux-what-orthia-keeps-of-root)), on Windows no privilege removal and no mitigation policies (see [sandbox](#windows-sandbox)) |
| `--run-tests` | run the built-in tests and exit |
| `-h`, `--help` | show help and exit; on Windows also `/?` and `-?` |
| `--version` | show the version (`orthia 1.x.x.xx`) and exit |

The value after `--file`, `--pid`, `--proc-folder` or `--cmd` is taken as it is, even if it starts with `--`.

All arguments are optional: plain `orthia` with no arguments starts the UI with an empty workspace,
and you open files and processes from the **File** menu (*Open executable*, *Open process*, *Open saved process*).
Without `--cmd` the UI starts with all given files and processes open.
With `--cmd`, exactly one `--file`, `--pid` or `--proc-folder` is required, except when every command is a `.database` command:
those work on the data folder and run without a target.

Examples:
```
orthia
orthia --file C:\Windows\System32\notepad.exe
orthia --pid 1234
orthia --proc-folder 1234 --cmd lmf
orthia --file /bin/ls --cmd lm --cmd 'u ls!$entrypoint L5'
orthia --file ntoskrnl.exe --cmd 'x nt!KeBugCheck*'
orthia --file ntoskrnl.exe --no-analysis --no-symbols --cmd lm
orthia --file firmware.bin --format raw --base 0x8000 --arch thumb --cmd 'u 8000 L20'
orthia --cmd '.database list'
```

### What starts after an open

The disassembly shows as soon as the headers, modules, imports and exports are read. Two long tasks then run in
the background: the **code analysis** of the main module (cross-references, the floating code it leads to) and the
**symbols** (PDBs; of every module for a process, of the main module for a file). When both run, the module is analyzed
again with its private symbols.

- **UI, new database:** a *New Database* dialog asks which of the two to start; both are checked.
  Space or a click toggles a box, Tab moves, Enter starts. Esc or Cancel starts nothing, and the target stays open.
  The answer is saved in the folder's [`meta.xml`](databases.md#metaxml).
- **UI, database of an earlier open:** no dialog. The choice made then is used; a database made before the dialog
  existed, or by `--cmd`, gets both. What is done already (an analyzed module) is not done again.
- **UI with `--no-analysis` or `--no-symbols`:** the targets on the command line open with these, without the dialog.
  Targets opened later from the menu still get it.
- **`--cmd`:** both run unless `--no-analysis` or `--no-symbols` turns them off; the commands run after they finish.
  Nothing is saved: the options are for that run only.

Raw code (`--format raw`, or a file the UI opens as shellcode) is analyzed when it opens, as its whole range, so
there is nothing to ask about. `.analyze <module>` and `.reload` start the tasks by hand at any time; a database
created without the analysis is upgraded in place, and nothing in it is thrown away.

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
| `xr [/a] <address> [L<length>]` | List the references to an address: each referrer with its kind (`code`: a call or a jump, `addr`: the address of code taken, `data`: data read, written or its address taken, `?`: saved by an older version) and its name. With `L<length>`, every address of the range that has references, with its referrers under it. `/a` prints the addresses only. References from other modules count, once the analysis of those modules saved them |
| `u <address> [L<count>]` | Disassemble `<count>` instructions |
| `lm` | List modules with their status: `.NET`, `unresolved`, `linked`, `stale`, `unlinked`, `analyzed`, `symbols` (see [Dependencies in file mode](#dependencies-in-file-mode)). `.NET` marks a managed image: an IL-only one has no native entry point and almost no native code |
| `lmf [/v]` | List the floating modules of a process: executable memory no module owns that the analysis led to, with protection, entry points and status (`pending`, `analyzed`, `stale`). `/v` adds what leads to each entry point; see [Floating code](#floating-code) |
| `db`, `dw`, `dd`, `dq`, `dp`, `dps` | Display memory as bytes, words, dwords, qwords, pointers, or pointers with symbols |
| `threads` | Display threads (processes only) |
| `modinfo <module>` | Show file and debug info for a module (PE headers, PDB or ELF build id) and where its image comes from; for a floating module, its allocation, code and entry points with the instructions that lead there |
| `sections [-v] <module\|address>` | List the sections of a module: name, address, size and `RWX` flags; `-v` adds every header field (see [Sections](#sections)) |
| `.reload [<module>]` | Reload symbols for a module |
| `.capture` | Save the memory of the process into its folder: the memory map, every committed readable region, the threads with their stacks (see [Capture](#capture-deep-mode)) |
| `.analyze [/f] <module\|mask>` | Run the full analysis of a module, then find the floating code it leads to (processes). Besides its own entry points, the analysis starts from the calls and jumps that the modules analyzed before saved into it; those saved later count at the next `.analyze /f`. A mask (`k*`, `*`) queues every module `lm` lists that matches. `/f` analyzes an analyzed module again, and checks a stale floating module against the memory again |
| `.symfix [<path>]` | Set the symbols directory |
| `.database list` | List the databases in the data folder; needs no open target (see [databases.md](databases.md#managing-databases)) |
| `.database delete <sel>...` | Delete databases, selected by a SHA-1 prefix, a pid or a file name; an open one is refused |
| `.database cleanup` | Remove the databases of exited processes and broken or half-deleted folders; a process folder with floating code snapshots stays |
| `cls` | Clear the screen (UI only) |
| `exit` | Exit the program (UI only) |

In the UI, press CTRL+C in the command edit box to stop a running command.

### Expressions

The address of `u`, `xr` and of the `db`/`dw`/`dd`/`dq`/`dp`/`dps` commands is an expression, not just a number.
The same expressions work in the UI's *Go to address* dialog (G).
The other commands take plain names or masks: `x` takes a symbol mask, `modinfo`, `.reload` and `.analyze` take a module name (`.analyze` also takes `/f` and a mask).
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
named by their offset in the allocation: `$entry_00h`, `$entry_20h`, `$entry_0800h`; a name stays the same
when the analysis of another module adds an entry), `modinfo fl_7ff7a5d70000`, `.analyze fl_7ff7a5d70000`.
`lm` and the Modules window leave them out; `lmf` lists them.

`lmf /v` also lists what leads to the floating code, one line per instruction and entry point, by address:

```
Referrers:
  fl_7ff74e840000!$entry_00h+1fh  -> fl_7ff74e850000!$entry_00h  RWX private
  ntdll!NtProtectVirtualMemory    -> fl_7ff74e840000!$entry_00h  RWX private [function start]
```

The import slots (IAT, the GOT on Linux) and the export table entries (PE) of the module are looked at too,
as they are in memory now: a slot that leads into such memory makes an entry point as well, with the slot as
what leads to it. That is how an import or export hook looks:

```
  orthia_floating_helper.exe+2f020h -> fl_7ff604970000!$entry_00h  RWX private [import slot: KERNEL32.dll!GetCurrentProcessId]
```

`[export: <name>]` marks an export table entry. `modinfo` lists the slots after the instructions:
`from app.exe+2f020h (import slot: KERNEL32.dll!GetCurrentProcessId)`.

After the arrow: the protection where the entry is, the allocation type (`private`, `mapped`, `image`), an
`MZ-header` or `ELF-header` when the allocation starts with one, the mapped file, and `stale`.
`[function start]` marks an instruction at a name (an export or a symbol): code at the start of a function
that leaves its module is how an inline hook looks.

Floating modules are saved in the process database. When the same process is opened again, each one is
checked against the memory (the allocation and a hash of its bytes); one that changed or went away is kept
and shown as `stale`, and is not analyzed again. The check covers the executable pages of the allocation
only: data written next to the code doesn't make a module stale, a change of protection does.

The bytes of each floating module are saved when it is found: `floating/<base>_<sha256 prefix>.bin` in the
process database folder (up to 16 MB of the allocation, unreadable pages zero filled). `modinfo` prints the
file and its SHA-256. The folder then never expires, and only `.database delete` removes
it (see [databases.md](databases.md#processes-procpid-name-start)). `--proc-folder` opens such a folder after the
process exited (see [Saved process folders](#saved-process-folders)).

`.analyze /f fl_<base>` on a stale module looks at the memory as it is now: when the allocation is still
there with some of its entry points executable, the module is fingerprinted and analyzed again; when it is
gone (or a module the OS reports took its place) the floating module is removed.

Hooks are usually in the system modules, which are not analyzed on open: `.analyze *` analyzes every
module, `.analyze kernel*` a few.

When the floating code analysis is done, the log has one line for all of it:
`Floating code: 4 modules, 7 referrers (lmf /v lists them)`, with `, 1 stale` when some are; `.capture`
prints the same line.

Targets in zero-filled memory are left out: no code starts with `00 00`. Data read or written there
(`mov rax, [rip+X]`) is no entry point either. A taken address is: `lea`, `adr`, and an immediate the
analysis takes for an address (`push offset callback`), so in a 32-bit process a constant that happens to
point into executable memory adds an entry point too. A taken address outside the module is saved only when it
leads into a module or into committed executable memory; the rest (`0CCCCCCCCh`, NTSTATUS codes, masks) are
constants.

#### TODO

Entry points and targets:
- [ ] More entry points than the static references: thread start addresses, return addresses on thread stacks,
  pointers in data (function pointer tables, vtables, `KernelCallbackTable`), exception handlers and other callbacks.
- [x] Import slots (PE IAT, ELF PLT GOT slots) and PE export table entries that lead into floating code.
- [ ] ELF: GOT slots of `R_*_GLOB_DAT` relocations (`-fno-plt`, a taken function address) are not looked at, only
  the PLT ones; an ELF export whose symbol value was rewritten neither.
- [ ] A slot that leads into memory that is not executable, or into another module than the one it imports from
  (a hook through a DLL loaded for it), is not reported.
- [x] Store the reference kind in `tbl_references` (code, taken address, data access); discovery skips data reads
  and writes.
- [x] x64 `lea reg, [rip+X]` and AArch64 `adr` with X outside the module are saved as taken addresses.
- [x] Mark how each entry was reached (code, address taken) in `modinfo` and `lmf /v`.
- [ ] Report calls and jumps into memory that is not executable (a sleep-masked implant is RW while it
  sleeps). Without the reference kind every constant that falls into a heap would be reported too.

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
- [ ] The check against memory runs only when the process is opened again, or for one module with
  `.analyze /f`. Checking every floating module of the open process at once is missing.
- [ ] A stale module whose allocation is still there can't be dropped, only analyzed again.

Commands and UI:
- [x] The bytes are saved when a floating module is found, with their SHA-256, and the folder stays.
- [x] Open a saved process folder after the process exited (`--proc-folder`): the module files and the snapshots.
- [x] `.capture` and `--capture`: the memory of the modules, and everything else the process has, into the folder.
- [ ] Snapshots: 16 MB per allocation and no budget per process; a JIT heap can make many.
- [ ] The Modules window leaves floating modules out; give them a view of their own.

Architectures and tests:
- [x] ARM patches: `DianaHook_PatchStreamEx` hooks AArch64 (`DIANA_ARCH_AARCH64`) and AArch32 (`DIANA_ARCH_ARM32`,
  `DIANA_ARCH_THUMB`) code, so the test helper `orthia_floating_helper` and `tests/cli/test_floating.py` run on
  ARM64 and ARM too. AArch32 moves b/bl/b&lt;cond&gt;/cbz/cbnz among the overwritten instructions; a pc-relative
  load (`ldr rX, [pc, ...]`), `adr`, or an IT block in that prefix is refused (the hook fails rather than
  corrupts), as is a target in a function too short for the jump.
- [ ] ARM floating code has no test at all: add gtests with an AArch64 and an AArch32 (A32 and T32, bit 0 of the
  entry) module that branches into a fake region, like the x86 ones in `ui_tests.cpp`.
- [ ] The test helper is not checked in a Debug build (incremental linking puts a jump thunk where the hooked
  function's address points).

### Saved process folders

A process folder (`proc/[pid] name <start>/`) can be opened after the process exited:

```
orthia --proc-folder "proc/[1234] app.exe 20261027-125304"
orthia --proc-folder 1234 --cmd lm --cmd 'lmf /v'
```

`--proc-folder` takes the folder, its `data.db`, or a pid or folder name as `.database list` shows them. In the
UI, *File > Open Saved Process* lists the process folders of the data folder, newest first, with their size and
whether they have a capture or floating code snapshots; Enter opens one, and the edit box takes what
`--proc-folder` takes. The item is named `[1234] app.exe (saved)`. What it has:
- the analysis, the comments and the floating modules from `data.db`;
- the module list from `modules.xml`, which the live view writes when the process is opened and again each
  time the module list is reloaded;
- the bytes of each module from its file, while that file is still the build the process ran (the PE
  timestamp and size, or the ELF build-id, read from the headers in memory). `lm` shows `linked`, or `stale`
  when the file is gone or another build; `modinfo` says why. A file has no in-memory changes: an inline hook
  shows the original instructions there;
- the bytes of the floating modules from their snapshots, so `u`, `lmf /v` and `modinfo` work as they did;
- with a [capture](#capture-deep-mode), the memory as it was, over all of the above: `lm` shows `captured`,
  the memory map and `threads` are the capture's. A floating module whose bytes in the capture differ from
  what was found and analyzed (the hook changed before `.capture`) is `stale`; `modinfo` names its snapshot
  (the bytes analyzed) and its capture files. `.analyze /f` on it analyzes the captured bytes.

Anything else reads as missing. A saved folder is never removed when its item closes and is never written
to except by the analysis. Process folders without floating code are removed 48 hours after their last
write, or by `.database cleanup` once the process exited, so open them before that.

### Capture (deep mode)

For something critical and fragile, where the files on disk are not enough (in-memory patches, a module
that unloads or deletes its file, data, stacks), the memory itself goes into the process folder:

- `.capture` saves everything now: the memory map, the bytes of every committed region that can be read
  (unreadable pages zero filled and counted), and the threads with their start addresses and stacks.
  It prints its progress and stops on CTRL+C; what is saved by then stays, marked incomplete.
- `--capture` saves the memory of each module of the process when its analysis is done (`.analyze`, the
  analysis on open). Small, for the modules that matter.

```
orthia --pid 1234 --cmd '.analyze *' --cmd .capture
orthia --pid 1234 --capture --cmd '.analyze app*'
orthia --proc-folder 1234 --cmd 'u app!main L10' --cmd threads
```

Both only read: the process is not stopped, so a thread may change bytes while they are read. In the UI,
`.capture` can run while the analysis on open (or a `.analyze`) still runs: they read the process each on a
thread of its own and don't wait for each other. The capture has the memory as it is, whatever the analysis
has found by then; floating code the analysis finds later has its own snapshots, and `.capture` again
takes in what changed. A newer
capture of a range replaces what was captured over it, and keeps the rest: a module captured before it
unloaded stays. The capture is `capture/` in the process folder ([databases.md](databases.md#capture));
a folder with one is evidence, kept as one with floating code snapshots is. It can be big: a whole
process is often hundreds of MB.

TODO:
- [ ] A switch to stop the threads while `.capture` reads (consistent bytes, but intrusive).
- [ ] Thread registers: only start addresses and stack return addresses are saved.
- [ ] `--capture` for floating modules too (they have their snapshots), and a size limit.

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
