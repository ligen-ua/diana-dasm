# Orthia Databases

Orthia keeps everything it learns about a target on disk, in one database per file or process, so a file
opens quickly the second time and comments and analysis survive restarts. A database is not a cache:
the analysis can be rebuilt, but your comments and the shellcode parameters cannot.
This document describes what is stored, where, and how to list, delete and clean up databases.

## Data folder

| Platform | Default location |
|---|---|
| Windows | `%APPDATA%\Orthia` |
| Linux | `$XDG_DATA_HOME/Orthia` (`~/.local/share/Orthia`) |

The environment variable `ORTHIA_HOME` replaces the whole folder. The command-line tests use it to run
against an empty data folder.

```
Orthia/
  db/      one folder per opened file, named by the SHA-1 of the file
  proc/    one folder per process, kept while the process runs
  bin/     helper executables extracted at start
```

The symbol folders (`ORTHIA_SYMBOL_PATH`, by default `C:\Sym;C:\Symbols` or `~/sym;~/symbols`) are not part of
the data folder. Orthia reads PDB files from them, it never writes there.

## Files: `db/<sha1>/`

The key is the SHA-1 of the file's bytes, so the same file opened from two paths shares one folder,
and a rebuilt file gets a new one.

| File | Content |
|---|---|
| `data.db` | the SQLite database described below |
| `target.bin` | a byte-identical copy of the opened file. It is verified by SHA-1 on every open and rewritten when it differs, so the folder stays self-describing. Opening `target.bin` itself prints the folder's `DIRINFO` |
| `DIRINFO` | two text lines: the header `Orthia Disasm Database Folder` and `Original Name: "<path as opened>"` |
| `meta.xml` | what the folder holds and which Orthia wrote it, see [`meta.xml`](#metaxml) |
| `parameters.xml` | raw shellcode only: `base_address`, `diana_mode` (the pointer size) and `diana_arch` (0 x86, 1 AArch64, 2 A32, 3 T32; missing means x86) chosen in the shellcode dialog or with `--base`/`--arch`, so the file reopens with the same layout |

Dependency files (imported DLLs and shared objects) are **not** copied. See [Dependencies](#dependencies).

### Quick open and `--analyze`

A `--cmd` run without `--analyze` records only headers, modules, imports and exports.
The UI, and `--cmd` with `--analyze`, add cross-references, PDB symbols and the private-symbol analysis.
A database created by a quick open is upgraded in place by a later deep open, or by `.analyze <module>`;
nothing is thrown away.

## Processes: `proc/[pid] name <start>/`

A live process gets `data.db`, `DIRINFO`, `meta.xml` and `modules.xml` in a folder named after its pid, its short name
and when it started, in UTC: `proc/[1234] notepad.exe 20261021-153012/`. A later process that gets the same
pid gets its own folder. When the start can't be read the folder is named `[pid] name`, as before
`meta.xml` existed.

Opening the same process again reuses its database: comments and the analysis survive closing it.
The folder name has the start in seconds only; `meta.xml` has it exactly (the creation `FILETIME` on Windows,
the boot id and start ticks on Linux), and a database is reused only when that matches.
`--overwrite-db` starts with a new, empty database instead.

The folder is deleted when the process item is closed after the process has exited, and when Orthia
opened itself (`--pid self`). At start Orthia removes any `proc/` folder that has not been written for
48 hours. `.database cleanup` removes folders at once, as soon as their process has exited
(see [Managing databases](#managing-databases)).

A folder with snapshots of floating code (`floating/*.bin`) is evidence: it stays when the process exits,
neither the start nor `.database cleanup` removes it, and `.database list` marks it `(floating code saved)`.
Only `.database delete` removes it. `--pid self` still removes its folder.

The code analysis and the comments go into that database, and the analysis flags of each module
(`flags` in its metainfo, with its name, size and the `id_*` identity of its build). The database follows
the module list, on open and each time it is reloaded:
- a module that unloaded keeps its analysis: it is evidence, and the same build loading again at the same
  place gets it back (its flags too);
- a module at that place that is another one (another name, size, start or build) gets none of it: the old
  rows are dropped, with the references made from them and their metainfo;
- comments stay, they belong to addresses. Exports and loaded private symbols are kept
in memory and rebuilt from the live process. The module list is also written to `modules.xml`, see below. Floating modules are saved
(metainfo type 6) and come back on a reopen, `stale` when their memory changed.

When a floating module is found, and when `.analyze /f` checks a stale one again, the bytes of its
allocation (up to 16 MB, from its base) are written to `floating/<base>_<sha256 prefix>.bin`; pages that
can't be read are zero filled. Another snapshot of the same region with other bytes is a new file, and the
old one stays. `modinfo` prints the path and the SHA-256.

### `modules.xml`

What the OS reported as the modules of the process, written by the live view when the process is opened
and again whenever the module list is reloaded (only when it changed). `--proc-folder` reads it to open the
folder after the process exited ([orthia.md](orthia.md#saved-process-folders)).

```xml
<modules format_version="1" main="7ff65ed70000">
    <module base="7ff65ed70000" size="2b6000" name="app.exe" path="C:\dirpp.exe" mode="8" arch="0"
        entry="7ff65ed82844" builtin="1" id_format="1" id_timestamp="6716f0aa" id_sizeofimage="2b6000"/>
</modules>
```

`main` is the base of the process image, `mode`/`arch` are `DIANA_MODE*`/`DIANA_ARCH_*`, `builtin` the
`builtInFlags` (format bits). The `id_*` attributes tell the build of the module's file, as in module metainfo:
PE timestamp and `SizeOfImage` from the headers in memory, or the ELF build-id (`id_buildid`); a module without
them has no bytes when the folder is opened later.

### Capture

`.capture` and `--capture` ([orthia.md](orthia.md#capture-deep-mode)) write `capture/` in the process folder:

| File | What it holds |
|---|---|
| `capture.xml` | the memory map: `<region base size allocation protect committed type name file missing/>` per region, `file` when its bytes were saved; `<threads>` with `<thread tid start created stack/>` (`stack`: return addresses, hex, space separated); `complete="0"` when `.capture` was stopped |
| `<base>_<size>.bin` | the bytes of a region, unreadable chunks zero filled (`missing` bytes of them) |

A newer capture of a range replaces the regions saved over it (their files are removed) and keeps the
rest. A folder with `capture/capture.xml` is evidence: `.database list` marks it `(captured)`, only
`.database delete` removes it.

## `meta.xml`

Every `db/` and `proc/` folder made since `meta.xml` was added has one, in the same XML format as
`parameters.xml`. Folders without it are still read.

| Field | Content |
|---|---|
| `format_version` | the folder layout version of the Orthia that wrote it (1) |
| `min_reader_version` | the lowest layout version that may reuse the folder. An older Orthia refuses a file database it can't read, with a hint to `--overwrite-db`, and replaces a process database |
| `analysis_version` | the analysis version of the writer. Recorded only: a newer analysis does not throw a database away |
| `orthia_version`, `created` | the writer and the time it made the folder (UTC seconds since 1970) |
| `kind` | `file` or `process` |
| `diana_mode`, `diana_arch` | the pointer size and the architecture, as in `parameters.xml` |
| `format`, `image_base`, `size`, `sha1` | files: the format the file opened as (`pe`, `elf`, `raw`), where it is mapped, its size and SHA-1 |
| `pid`, `name`, `start`, `start_key` | processes: the pid, `[pid] name`, the start (UTC seconds) and the exact start that decides reuse |

A file database written before `meta.xml` gets one on its next open. `--overwrite-db` writes a new one with
the new database.

## `bin/`

`orthia_win32_host.exe` is a 32-bit helper extracted from Orthia's resources on Windows. A 64-bit Orthia
uses it to locate 32-bit dependencies the way a 32-bit process would (the WOW64 search path).
It is rewritten when the embedded copy differs.

## The database

`data.db` is an ordinary SQLite database. Any SQLite tool can read it while Orthia is closed.

### Versioning

`tbl_version` holds one row per schema version applied; the current version is **5**.
An older database is migrated step by step inside one transaction the first time it is opened.
A database from a newer Orthia fails to open with `Unknown database version`.

| Step | Change |
|---|---|
| 0 to 1 | emulator tables `tbl_vm_vms`, `tbl_vm_modules` (used by the WinDbg plugin) |
| 1 to 2 | `tbl_metainfo` and `tbl_comments` |
| 2 to 3 | addresses in `tbl_references` encoded for unsigned ordering, index `idx_ref_to` |
| 3 to 4 | the same encoding for `tbl_metainfo.meta_address`, index `idx_metainfo_type_addr` |
| 4 to 5 | `tbl_references.ref_kind`: what a reference does with its target; older rows are 0 (unknown) |

### Address encoding

SQLite integers are signed. So that `ORDER BY` and range queries work on 64-bit addresses above
`0x8000000000000000`, the columns `tbl_references.ref_address_from`, `ref_address_to` and
`tbl_metainfo.meta_address` store `address XOR 0x8000000000000000`. `tbl_modules.mod_address`,
`tbl_metainfo.meta_mod_id` and `tbl_comments.com_address` store the raw value.
When you query the database by hand, apply the XOR to the encoded columns.

### Tables

| Table | Columns | Content |
|---|---|---|
| `tbl_modules` | `mod_address` (primary key), `mod_size`, `mod_name` | every module in the target's address space: the opened file and its dependencies |
| `tbl_metainfo` | `meta_mod_id` (foreign key to `tbl_modules`, cascade delete), `meta_address`, `meta_type`, `meta_info` | per-module records, see below |
| `tbl_references` | `ref_address_from`, `ref_address_to`, `ref_kind` | cross-references found by the code analysis, kept by the module they are made from: clearing or unloading a module removes the rows whose `ref_address_from` is inside it. `ref_kind`: 1 a call or a jump, 2 an address taken (`lea`, an immediate, `ADR`) that is code or outside the module, 3 data read or written, or the address of data in the module taken (it never starts a route), 0 unknown (saved before version 5) |
| `tbl_comments` | `com_address` (primary key), `com_text` | user comments (`;` in the disassembly view) |
| `tbl_vm_vms`, `tbl_vm_modules` | | emulator state of the WinDbg plugin; unused by the disassembler |

Foreign keys are enforced on every connection, so deleting a module row removes its metainfo.

### Metainfo records

`meta_info` is a one-element XML document, `<root attr="value" .../>`, with numbers written in hex.
`meta_type` selects the record kind:

| `meta_type` | Record | `meta_address` |
|---|---|---|
| 1 | module description | the module address |
| 2 | import | the import table slot in the module (see [IAT](#import-table)) |
| 3 | export | the export's address |
| 4 | private symbol (from a PDB) | the symbol's address |
| 5 | AArch32 code map: what the bytes are from here on | the address where it changes |
| 6 | floating module: an allocation of a process that the analysis led to (processes only) | the module address |

Import, export and symbol records carry `address` (the target) and `name`.

The AArch32 code map is written by the analysis of ARM (A32) and Thumb (T32) code, which can mix both and keep
literals and switch tables between the instructions. Its `meta_info` is plain text, not XML: `a32`, `t32`,
`data` (words: literals, tables of addresses), `data16` (a `tbh` table) or `data8` (a `tbb` table). Before the
first record of a module, its own mode applies: T32 for an ARMNT PE, for raw `thumb` code and for an ELF whose
entry point or most functions have the Thumb bit; A32 otherwise. The disassembly reads it to decode
each address in the right mode and to show data as `.word`.

The module description carries:

| Attribute | Meaning |
|---|---|
| `fullname` | the path the module was loaded from (the opened file keeps the Win32 `\\?\` prefix) |
| `flags` | 1 analysis done, 2 symbols loaded, 4 private-symbol analysis done |
| `builtinflags` | 1 PE, 2 ELF, 4 unresolved dependency (a reserved range with no image) |
| `src_kind` | dependencies: `linked` or `none` (planned: `captured`, `memory`) |
| `id_format`, `id_timestamp`, `id_sizeofimage`, `id_guid`, `id_age` | PE identity: 1, the file header `TimeDateStamp`, the optional header `SizeOfImage`, the RSDS debug GUID (raw bytes as hex) and age |
| `id_format`, `id_buildid` | ELF identity: 2 and the GNU build id |
| `sha1` | SHA-1 of the dependency's file, for the planned shared image store |
| `iat_slots` | main module only: import records are keyed by their slot, so they can be replayed |

A floating module (`fl_<base>`) has a `tbl_modules` row for its allocation and a record of type 6:

| Attribute | Meaning |
|---|---|
| `base`, `size` | the OS allocation (hex) |
| `protect`, `type` | protection where the entry points are (1 read, 2 write, 4 execute) and the allocation type (0 private, 1 mapped file, 2 image) |
| `mapped`, `header` | the mapped file or pseudo name, and `MZ`/`ELF` when the allocation starts with such a header |
| `hash`, `hashed` | FNV-1a hash of the executable pages of the allocation with their offsets, `hashed` bytes of them (up to 16 MB); checked on reopen, a mismatch makes the module stale |
| `code_start`, `code_last` | the lowest and the highest instruction start the analysis found (0 before it) |
| `entries` | `<entry>:<referrer>,<referrer>;<entry>:...` in hex: each entry point and the instructions that lead there |
| `slots` | `<entry>:<i\|e><slot>=<name>,...;<entry>:...`: the import slots (`i`) and export entries (`e`) that lead to an entry, the slot in hex, the import or export name with `%`, `,`, `;`, `:`, `=` as `%XX`; missing when there are none |
| `snapshot`, `snapshot_size`, `snapshot_missing` | the file in `floating/` with the bytes as they were found, how many (hex), and how many of them couldn't be read (zero filled); missing when no snapshot was saved |
| `sha256` | SHA-256 of the snapshot file |

## Dependencies

At first open the imports are resolved like a loader would. Each dependency is searched in the directory
of the opened file, then on the system search path (`System32` and `System32\drivers` on Windows), mapped,
and placed after the main image in a synthetic address space at 64 KB alignment. Its exports become
metainfo records, and its path and identity go into the module description. The dependency's bytes are
not kept: they are read again from disk, on demand, when something touches that range.

On every open the file at the recorded path is compared with the recorded identity. Only a matching build
is served; a missing or changed file (after a Windows update, for example) makes the module `stale`:
its names still resolve, its bytes read as `??`, and `modinfo` says why. A module recorded by an Orthia
version without identity shows as `unlinked`. `lm` and the modules window show these states.

The layout never changes: a file always reopens with the module addresses of its first open, so
cross-references into dependencies stay valid whether or not the dependency is still readable.

### Import table

The first open writes the resolved addresses into the main module's import address table, in memory,
before the code analysis runs. On later opens the dependencies are not loaded again; the import records
(type 2, keyed by slot) are replayed into the image instead, so `dps` on a slot shows the same target as
the first time. This replay is enabled for PE files; the module description marks it with `iat_slots`.

## Managing databases

The `.database` commands work on the data folder, not on a target. Type them in the UI's command window,
with or without an open item, or run them from a shell without `--file`/`--pid`:

```
orthia --cmd ".database list"
orthia --cmd ".database delete 3f2a9c0d"
orthia --cmd ".database cleanup"
```

### `.database list`

Prints the data folder, then one row per database, the files (`db/`) first, then the processes (`proc/`):

```
Data folder: C:\Users\me\AppData\Roaming\Orthia
Database      Modified          Size        State           Comments  Name
b1c0f061fc02  2026-09-30 21:46  89.7 KB     quick           0         C:\work\dmesg  (open)
pid 4242      2026-09-30 21:50  1.2 MB      analyzed        0         [4242] notepad.exe
```

| Column | Content |
|---|---|
| Database | the first 12 digits of the SHA-1 of a file, or the pid of a process |
| Modified | the newest write time of the files in the folder |
| Size | the size of the folder on disk |
| State | from the main module's description: `quick` (headers, imports and exports only), `analyzed`, with `+symbols` when a PDB is loaded; `empty` for a folder without `data.db`, `deleting` for a delete that did not finish, `?` when `data.db` can't be read |
| Comments | the number of comments, the part of a database that can't be rebuilt |
| Name | the original file name from `DIRINFO` |

`(open)` marks the databases of the items open in this Orthia. The databases are read with a read-only
connection, so `list` never migrates or changes them.

### `.database delete <selector>...`

Deletes whole folders. Each selector must match exactly one database:

| Selector | Matches |
|---|---|
| 6 or more hex digits | the file database whose SHA-1 starts with them, case-insensitive |
| a decimal number | the process database of that pid (a number that is also a SHA-1 prefix must be unique across both) |
| anything else | a file name: the file is hashed, and its database is selected. Write `./123456` for a file whose name looks like a number |

A name with spaces goes in double quotes. If any selector matches nothing or more than one database,
nothing is deleted. A database open in this Orthia is refused with its item name: close the item first.

The folder is renamed before it is removed. On Windows a folder that another Orthia still has open can't be
renamed, so it is reported and left intact. Another Orthia on Linux, or one that opens the file later,
is not detected: close other instances before deleting their databases.

### `.database cleanup`

Removes what is no longer needed, and prints each folder with the reason:

| Folder | Removed when |
|---|---|
| `proc/[pid] name <start>` | the process has exited, or the pid now runs another process (another start; for a folder without `meta.xml`, another program), or the folder has not been written for 48 hours |
| `db/<sha1>` | it has no `data.db` and has not been written for an hour (a younger one may be being created) |
| `*.deleting` | a `delete` that failed half way |

A file database with a `data.db` is never removed by `cleanup`, and neither is anything open in this Orthia,
nor a process folder with floating code snapshots.
The process check goes through the same process provider that opens processes: a process Orthia can't
open (another user's, for example) keeps its folder until the 48 hours pass. A folder another Orthia still
uses is reported as `Kept` and is not an error.

### By hand

- **Re-resolve dependencies** after a system update: delete the file's database and open the file again. The
  dependencies are located and identified anew, and the file gets a fresh analysis.
- **Reset everything**: close Orthia and delete the data folder, or point `ORTHIA_HOME` at an empty one.
- A database folder can also be deleted directly while Orthia is closed. The SHA-1 is printed on open
  (`SHA1: ...`), and `DIRINFO` in each folder names the original file.

- **Start over with one target**: open it with `--overwrite-db`. Its database is replaced by an empty one,
  comments included; the copy of the file, `DIRINFO` and `parameters.xml` stay.

Deleting `data.db` alone is not enough for a partial reset: the next open recreates it and treats the
file as new, which is the same as deleting the folder.

## Planned

- A shared, content-addressed image store `images/<sha1>` for captured dependency files, referenced from
  the `sha1` attributes, so a target folder can be made independent of the system's DLLs.
- Snapshots of a live process saved as a file-mode folder, with module images taken from process memory.
