"""Test inputs: committed files under data/, unpacked zips, and derived files."""
import hashlib
import shutil
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DATA_DIR = REPO_ROOT / "data"
OUT_DIR = Path(__file__).resolve().parent / "_out"

ELF_DIR = DATA_DIR / "elf"
NT_ZIP = DATA_DIR / "pe" / "nt.zip"


def _sha1(path: Path) -> str:
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def unpack_zip(zip_path: Path) -> Path:
    """Unpack once into _out/data/<name>-<sha1>/, reused until the zip changes.

    Binaries go to `bin/`, PDBs to `symbols/`. The two folders must be siblings:
    the PDB loader also searches the module's folder and its direct subfolders,
    so a PDB inside the binary's tree would be found whatever the symbol path.
    """
    target = OUT_DIR / "data" / f"{zip_path.stem}-{_sha1(zip_path)[:12]}"
    if target.is_dir():
        return target
    staging = target.with_name(target.name + ".tmp")
    shutil.rmtree(staging, ignore_errors=True)
    with zipfile.ZipFile(zip_path) as zf:
        for member in zf.infolist():
            if member.is_dir():
                continue
            name = Path(member.filename).name
            sub = "symbols" if name.lower().endswith(".pdb") else "bin"
            dest = staging / sub / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(member) as src, open(dest, "wb") as dst:
                shutil.copyfileobj(src, dst)
    (staging / "symbols").mkdir(exist_ok=True)
    staging.rename(target)
    return target


class DataSet:
    def __init__(self, work_dir: Path):
        self.work_dir = work_dir
        self._nt = None

    # committed ELF files
    def elf(self, name: str) -> Path:
        path = ELF_DIR / name
        assert path.is_file(), f"missing test file {path}"
        return path

    # data/pe/nt.zip: ntoskrnl.exe 10.0.14393.9512 + matching ntkrnlmp.pdb
    @property
    def nt_dir(self) -> Path:
        if self._nt is None:
            self._nt = unpack_zip(NT_ZIP)
        return self._nt

    @property
    def ntoskrnl(self) -> Path:
        return self.nt_dir / "bin" / "ntoskrnl.exe"

    @property
    def nt_symbols(self) -> Path:
        return self.nt_dir / "symbols"

    # derived files, created under the session work dir
    def fresh_copy(self, src: Path, name: str = None, subdir: str = "fresh") -> Path:
        """Copy with one byte appended: new SHA1, so no cached DB applies."""
        dest = self.work_dir / subdir / (name or src.name)
        if not dest.exists():
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dest)
            with open(dest, "ab") as f:
                f.write(b"\0")
        return dest

    def copy(self, src: Path, relative: str) -> Path:
        """Byte-identical copy (same SHA1) at another path/name."""
        dest = self.work_dir / relative
        if not dest.exists():
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dest)
        return dest

    def truncated(self, src: Path, size: int, name: str) -> Path:
        dest = self.work_dir / "derived" / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(src.read_bytes()[:size])
        return dest

    def text_file(self, name: str, content: bytes) -> Path:
        dest = self.work_dir / "derived" / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(content)
        return dest
