# Orthia Persistent Cache

Orthia keeps everything it learns about a target on disk, so a file opens quickly the second time and
comments and analysis survive restarts. This document describes what is stored, where, and how to reset it.

## Data folder

| Platform | Default location |
|---|---|
| Windows | `%APPDATA%\Orthia` |
| Linux | `$XDG_DATA_HOME/Orthia` (`~/.local/share/Orthia`) |

The environment variable `ORTHIA_HOME` replaces the whole folder. The command-line tests use it to run
against an empty cache.

```
Orthia/
  db/      one folder per opened file, named by the SHA-1 of the file
  proc/    one folder per open process, deleted when the process is closed
  bin/     helper executables extracted at start
```

The symbol folders (`ORTHIA_SYMBOL_PATH`, by default `C:\Sym;C:\Symbols` or `~/sym;~/symbols`) are not part of
the cache. Orthia reads PDB files from them, it never writes there.

## Files: `db/<sha1>/`

The key is the SHA-1 of the file's bytes, so the same file opened from two paths shares one folder,
and a rebuilt file gets a new one.

| File | Content |
|---|---|
| `data.db` | the SQLite database described below |
| `target.bin` | a byte-identical copy of the opened file. It is verified by SHA-1 on every open and rewritten when it differs, so the folder stays self-describing. Opening `target.bin` itself prints the folder's `DIRINFO` |
| `DIRINFO` | two text lines: the header `Orthia Disasm Database Folder` and `Original Name: "<path as opened>"` |
| `parameters.xml` | raw shellcode only: `base_address` and `diana_mode` chosen in the shellcode dialog, so the file reopens with the same layout |

Dependency files (imported DLLs and shared objects) are **not** copied. See [Dependencies](#dependencies).

### Quick open and `--analyze`

A `--cmd` run without `--analyze` records only headers, modules, imports and exports.
The UI, and `--cmd` with `--analyze`, add cross-references, PDB symbols and the private-symbol analysis.
A database created by a quick open is upgraded in place by a later deep open, or by `.analyze <module>`;
nothing is thrown away.

## Processes: `proc/[pid] name/`

A live process gets `data.db` and `DIRINFO` in a folder named after its pid and short name.
It is temporary: the folder is deleted when the process item is closed, and at start Orthia removes
any `proc/` folder that has not been written for 48 hours (left behind by a crash).

Only the code analysis of the main module and the comments go into that database. The module list,
exports, module flags and loaded private symbols are kept in memory and rebuilt from the live process.

## `bin/`

`orthia_win32_host.exe` is a 32-bit helper extracted from Orthia's resources on Windows. A 64-bit Orthia
uses it to locate 32-bit dependencies the way a 32-bit process would (the WOW64 search path).
It is rewritten when the embedded copy differs.

## The database

`data.db` is an ordinary SQLite database. Any SQLite tool can read it while Orthia is closed.

### Versioning

`tbl_version` holds one row per schema version applied; the current version is **4**.
An older database is migrated step by step inside one transaction the first time it is opened.
A database from a newer Orthia fails to open with `Unknown database version`.

| Step | Change |
|---|---|
| 0 to 1 | emulator tables `tbl_vm_vms`, `tbl_vm_modules` (used by the WinDbg plugin) |
| 1 to 2 | `tbl_metainfo` and `tbl_comments` |
| 2 to 3 | addresses in `tbl_references` encoded for unsigned ordering, index `idx_ref_to` |
| 3 to 4 | the same encoding for `tbl_metainfo.meta_address`, index `idx_metainfo_type_addr` |

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
| `tbl_references` | `ref_address_from`, `ref_address_to` | cross-references found by the code analysis |
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

Import, export and symbol records carry `address` (the target) and `name`.

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

## Resetting the cache

- **Forget one file**: close it in Orthia and delete its `db/<sha1>` folder. The SHA-1 is printed on open
  (`SHA1: ...`), and `DIRINFO` in each folder names the original file.
- **Re-resolve dependencies** after a system update: delete the file's folder and open it again. The
  dependencies are located and identified anew, and the file gets a fresh analysis.
- **Reset everything**: delete the data folder, or point `ORTHIA_HOME` at an empty one.

Deleting `data.db` alone is not enough for a partial reset: the next open recreates it and treats the
file as new, which is the same as deleting the folder.

## Planned

- A shared, content-addressed image store `images/<sha1>` for captured dependency files, referenced from
  the `sha1` attributes, so a target folder can be made independent of the system's DLLs.
- Snapshots of a live process saved as a file-mode folder, with module images taken from process memory.
