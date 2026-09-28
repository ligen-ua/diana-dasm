# Command-line tests

Black-box tests that run the built `orthia` binary in `--cmd` mode and check its stdout and exit code.
They don't depend on the build system: the tests work the same whether the binary came from MSBuild or CMake.

## Running

```
pip install -r tests/cli/requirements.txt     # once; only pytest

run_cli_tests.cmd                             # Windows
./run_cli_tests.sh                            # Linux
python -m pytest tests/cli -k expressions     # a subset
python -m pytest tests/cli -m "not slow"      # skip the cold ntoskrnl analysis
python -m pytest tests/cli --orthia path/to/orthia   # a specific binary (or set ORTHIA_BIN)
```

Without `--orthia`, the binary is looked up in `bin/Release/amd64/orthia.exe`, then in `cmake-release/…/orthia_disasm_ui`.
`run_tests.cmd` and `cmake-test.sh` run this suite after the C++ tests when pytest is installed.

## Isolation

Every run sets two environment variables that the binary honours:

| Variable | Replaces |
|---|---|
| `ORTHIA_HOME` | `%APPDATA%\Orthia` / `$XDG_DATA_HOME/Orthia` (analysis DB cache) |
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
| `orthia_pdb` | `--analyze` | its own, shared across the session | `ntkrnlmp.pdb` from `data/pe/nt.zip` |
| `orthia_cold` | quick | new for each test | none |

`orthia_cold.with_(analyze=True)` or `.with_(symbol_path=…)` gives a variant that uses the same home.
Fixtures never share a home: an `--analyze` open upgrades the DB and loaded symbols persist in it, so shared homes would make results depend on test order.
After the session, every test DB is checked with `PRAGMA foreign_key_check`.

## Test data

- `data/elf/*` is used as is.
- `data/pe/nt.zip` is unpacked once into `tests/cli/_out/data/`, with binaries in `bin/` and PDBs in `symbols/`.
  The folders must stay siblings: the PDB loader also searches the module's folder and its subfolders.
- Derived files are generated per session: fresh-SHA1 copies, truncated files, non-ASCII paths (see `data.py`).

## Writing a test

```python
def test_something(orthia, data):
    res = orthia.run("lm", "x dmesg!*", file=data.elf("dmesg"))   # one --cmd per argument
    res.assert_ok()                                   # exit 0 and no "Error:" lines
    res.assert_line(r"dmesg\s+analysis")              # regex against stdout lines
    assert res.symbols["dmesg!$entrypoint"] == [0xB0E0]   # parsed `x` output
    assert orthia.resolve("dmesg+10", file=data.elf("dmesg")) == 0x4010   # expression -> address
```

Assert on addresses, names and exit codes, not on whole screens of output, so tests survive formatting changes.

## Known bugs

Cases for open bugs from `testing/cmdline-exploratory-test-report.md` are written as
`@pytest.mark.xfail(reason="B<n>: …")`. `xfail_strict` is on, so once a bug is fixed its test reports XPASS as a failure.
Then remove the marker.
