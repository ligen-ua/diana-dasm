# Command-line tests

Black-box tests that run the built `orthia` binary in `--cmd` mode and check its stdout and exit code.
They don't depend on the build system: the tests work the same whether the binary came from MSBuild or CMake.

## Running

```
pip install -r tests/cli/requirements.txt     # once; only pytest

run_cli_tests.cmd                             # Windows
./run_cli_tests.sh                            # Linux
python -m pytest tests/cli -k expressions     # a subset
python -m pytest tests/cli -m "not slow"      # skip the cold analysis of large binaries
python -m pytest tests/cli -m "not private"   # only what runs in a public clone
python -m pytest tests/cli --orthia path/to/orthia   # a specific binary (or set ORTHIA_BIN)
```

Without `--orthia`, the binary is looked up in `bin/Release/amd64/orthia.exe`, then in `cmake-release/…/orthia`.
`run_tests.cmd` and `cmake-test.sh` run this suite after the C++ tests when pytest is installed.

## Isolation

Every run sets two environment variables that the binary honours:

| Variable | Replaces |
|---|---|
| `ORTHIA_HOME` | `%APPDATA%\Orthia` / `$XDG_DATA_HOME/Orthia` (databases) |
| `ORTHIA_SYMBOL_PATH` | default symbol folders (`C:\Sym;C:\Symbols`, `~/sym;~/symbols`) |

## Quick open vs `--analyze`

By default `--cmd` opens a target quickly: headers, modules, imports and exports only. It skips deep code analysis and symbol loading.
`--analyze` gives the full behaviour the UI uses: cross-references, PDB loading and private-symbol analysis.
A DB created quickly is upgraded in place by a later `--analyze` open or by `.analyze <module>`, and its names are kept.

Fixtures in `conftest.py`:

| Fixture | Mode | DB | Symbols |
|---|---|---|---|
| `orthia` | quick | shared across the session (warm) | none |
| `orthia_full` | `--analyze` | its own, shared across the session | none |
| `orthia_pdb` | `--analyze` | its own, shared across the session | `ntkrnlmp.pdb` from `data/private/pe/nt.zip` |
| `orthia_self_pdb` | `--analyze` | its own, shared across the session | the PDB of the binary under test (Windows) |
| `orthia_cold` | quick | new for each test | none |

`orthia_cold.with_(analyze=True)` or `.with_(symbol_path=…)` gives a variant that uses the same home.
Fixtures never share a home: an `--analyze` open upgrades the DB and loaded symbols persist in it, so shared homes would make results depend on test order.
After the session, every test DB is checked with `PRAGMA foreign_key_check`.

## Test data

Most tests open **the binary under test itself** as a file: the file counterpart of `--pid self`.
Nothing is stored for them; every run checks the current build. `data.self_file` is a copy named `orthia.exe` (Windows)
or `orthia` (Linux) in `_out/.../self/bin/`, with the PDB in the sibling `self/symbols/`, so the "no symbols" tests never
see it. Each host builds its own format, so the `*_self` PE tests run on Windows and the ELF tests on Linux
(fixtures `self_pe` / `self_elf` skip otherwise).

The expected values of the self tests are never taken from Orthia's output. `images.py` reads them from the file
(`PeInfo`, `ElfInfo`: headers, sections, exports, imports, Build ID, DT_NEEDED), and private PDB symbols come
from `dbghelp.dll` (`dbghelp_symbols`).

Other data:
- `data/elf/libexports_*.so` are committed; they are built from `data/elf/exports_lib` (our own source).
- `data/private/` is **gitignored**: third-party binaries that can't be published (`dmesg`, `apt-mark`, `ls.bin`,
  and `nt.zip`, ntoskrnl with its PDB). The tests marked `private` use them and skip when the folder is absent.
  `nt.zip` is unpacked once into `tests/cli/_out/data/`, with binaries in `bin/` and PDBs in `symbols/`.
  The folders must stay siblings: the PDB loader also searches the module's folder and its subfolders.
- Derived files are generated per session: fresh-SHA1 copies, truncated files, non-ASCII paths (see `data.py`).

## Writing a test

```python
def test_something(orthia, data, self_pe):
    res = orthia.run("lm", "x orthia!*", file=data.self_file)   # one --cmd per argument
    res.assert_ok()                                   # exit 0 and no "Error:" lines
    res.assert_line(r"orthia\.exe$")                  # regex against stdout lines
    entry = self_pe.image_base + self_pe.entry_rva    # expected value read from the file
    assert res.symbols["orthia.exe!$entrypoint"] == [entry]   # parsed `x` output
    assert orthia.resolve("orthia+10", file=data.self_file) == self_pe.image_base + 0x10   # expression -> address
```

Assert on addresses, names and exit codes, not on whole screens of output, so tests survive formatting changes.

## Known bugs

Cases for open bugs from `testing/cmdline-exploratory-test-report.md` are written as
`@pytest.mark.xfail(reason="B<n>: …")`. `xfail_strict` is on, so once a bug is fixed its test reports XPASS as a failure.
Then remove the marker.
