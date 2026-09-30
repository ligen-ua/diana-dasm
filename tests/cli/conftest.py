"""Fixtures for the orthia command-line tests.

Every Orthia instance gets its own ORTHIA_HOME (databases) and ORTHIA_SYMBOL_PATH,
so results never depend on %APPDATA%\\Orthia, ~/.local/share/Orthia or C:\\Sym.
"""
import os
import sqlite3
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))

from data import REPO_ROOT, DataSet  # noqa: E402
from orthia_runner import Orthia  # noqa: E402

# Where the two build systems put the binary. Override with --orthia or ORTHIA_BIN.
BINARY_CANDIDATES = [
    "bin/Release/amd64/orthia.exe",                                   # MSBuild, orthia_14_0.sln
    "cmake-release/src/orthia/orthia_disasm_ui/orthia_disasm_ui",     # cmake-build.sh
    "cmake-debug/src/orthia/orthia_disasm_ui/orthia_disasm_ui",
]


def pytest_addoption(parser):
    parser.addoption("--orthia", default=None, help="path to the orthia binary under test")


def _find_binary(config) -> Path:
    explicit = config.getoption("--orthia") or os.environ.get("ORTHIA_BIN")
    if explicit:
        path = Path(explicit)
        if not path.is_file():
            pytest.exit(f"orthia binary not found: {path}", returncode=4)
        return path.resolve()
    for candidate in BINARY_CANDIDATES:
        path = REPO_ROOT / candidate
        if path.is_file():
            return path
    pytest.exit("orthia binary not found; build it or pass --orthia <path> / set ORTHIA_BIN.\n"
                "Looked in: " + ", ".join(BINARY_CANDIDATES), returncode=4)


def pytest_report_header(config):
    return f"orthia binary: {_find_binary(config)}"


@pytest.fixture(scope="session")
def orthia_exe(pytestconfig) -> Path:
    return _find_binary(pytestconfig)


@pytest.fixture(scope="session")
def data(tmp_path_factory) -> DataSet:
    return DataSet(tmp_path_factory.mktemp("files"))


@pytest.fixture(scope="session")
def empty_symbols(tmp_path_factory) -> Path:
    return tmp_path_factory.mktemp("no-symbols")


# Every fixture has its own home: an --analyze open upgrades the DB and loaded
# symbols persist in it, so sharing a home would make results depend on test order.

@pytest.fixture(scope="session")
def orthia(orthia_exe, tmp_path_factory, empty_symbols) -> Orthia:
    """Quick open (the --cmd default), no symbols. Most tests use this."""
    return Orthia(orthia_exe, tmp_path_factory.mktemp("home"), empty_symbols)


@pytest.fixture(scope="session")
def orthia_full(orthia_exe, tmp_path_factory, empty_symbols) -> Orthia:
    """--analyze (deep code analysis), no symbols available."""
    return Orthia(orthia_exe, tmp_path_factory.mktemp("home-full"), empty_symbols, analyze=True)


@pytest.fixture(scope="session")
def orthia_pdb(orthia_exe, tmp_path_factory, data) -> Orthia:
    """--analyze with the ntoskrnl PDB on the symbol path."""
    return Orthia(orthia_exe, tmp_path_factory.mktemp("home-pdb"), data.nt_symbols, analyze=True)


@pytest.fixture
def orthia_cold(orthia_exe, tmp_path, empty_symbols) -> Orthia:
    """Quick open with an empty DB for this test only (first open)."""
    return Orthia(orthia_exe, tmp_path / "home", empty_symbols)


@pytest.fixture(scope="session", autouse=True)
def no_foreign_key_orphans(tmp_path_factory):
    """After the session, every test DB must be consistent (FK enforcement on every connection)."""
    yield
    orphans = {}
    for db in Path(tmp_path_factory.getbasetemp()).rglob("data.db"):
        con = sqlite3.connect(f"file:{db}?mode=ro", uri=True)
        try:
            rows = con.execute("PRAGMA foreign_key_check").fetchall()
        finally:
            con.close()
        if rows:
            orphans[str(db)] = rows[:5]
    assert not orphans, f"foreign key orphans: {orphans}"
