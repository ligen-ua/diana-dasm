"""Test inputs: committed files under data/, unpacked zips, and derived files."""
import hashlib
import os
import shutil
import struct
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

    def pdb_mismatch_copy(self, src: Path, name: str) -> Path:
        """Copy with one byte of the RSDS debug GUID flipped: new SHA1, and its PDB no longer matches."""
        dest = self.work_dir / "derived" / name
        if not dest.exists():
            dest.parent.mkdir(parents=True, exist_ok=True)
            image = bytearray(src.read_bytes())
            image[image.index(b"RSDS") + 4] ^= 0xFF
            dest.write_bytes(image)
        return dest

    def copy_with_dependency(self, exe: Path, dep_name: str, dest_dir: Path) -> "tuple[Path, Path]":
        """Byte-identical copies of `exe` and of the host's System32/<dep_name> side by side.

        The opened file's directory is searched first, so the co-located copy is the
        dependency Orthia links; changing or removing it later makes the dependency stale."""
        dest_dir.mkdir(parents=True, exist_ok=True)
        exe_copy = dest_dir / exe.name
        shutil.copyfile(exe, exe_copy)
        system32 = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
        dep_copy = dest_dir / dep_name
        shutil.copyfile(system32 / dep_name, dep_copy)
        return exe_copy, dep_copy

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


# PE helpers for the dependency tests: small on purpose, they only read what the tests need

def _pe_headers(image: bytes):
    """(e_lfanew, is 64-bit, optional header offset)."""
    assert image[:2] == b"MZ", "not a PE file"
    e_lfanew = struct.unpack_from("<I", image, 0x3C)[0]
    assert image[e_lfanew:e_lfanew + 4] == b"PE\0\0", "not a PE file"
    optional = e_lfanew + 24
    magic = struct.unpack_from("<H", image, optional)[0]
    return e_lfanew, magic == 0x20B, optional


def patch_pe_timestamp(path: Path) -> None:
    """Flip one bit of the file header TimeDateStamp: another build as far as identity goes."""
    image = bytearray(path.read_bytes())
    e_lfanew, _, _ = _pe_headers(image)
    image[e_lfanew + 8] ^= 0x01
    path.write_bytes(image)


def flip_rsds_guid(path: Path) -> None:
    """Flip one byte of the RSDS debug GUID in place; the headers stay the same."""
    image = bytearray(path.read_bytes())
    image[image.index(b"RSDS") + 4] ^= 0xFF
    path.write_bytes(image)


def iat_slot(exe: Path, dll: str, func: str) -> int:
    """Virtual address of the import address table slot of dll!func at the preferred image base."""
    image = exe.read_bytes()
    e_lfanew, is64, optional = _pe_headers(image)
    image_base = struct.unpack_from("<Q" if is64 else "<I", image, optional + 24)[0]
    dir_offset = optional + (112 if is64 else 96)
    import_rva = struct.unpack_from("<I", image, dir_offset + 8)[0]
    sections_offset = optional + struct.unpack_from("<H", image, e_lfanew + 20)[0]
    section_count = struct.unpack_from("<H", image, e_lfanew + 6)[0]
    sections = []
    for i in range(section_count):
        base = sections_offset + i * 40
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from("<IIII", image, base + 8)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_offset))

    def to_offset(rva: int) -> int:
        for virtual_address, size, raw_offset in sections:
            if virtual_address <= rva < virtual_address + size:
                return raw_offset + rva - virtual_address
        raise AssertionError(f"rva {rva:#x} not in any section")

    def c_string(rva: int) -> str:
        offset = to_offset(rva)
        return image[offset:image.index(b"\0", offset)].decode("ascii")

    thunk_size = 8 if is64 else 4
    descriptor = to_offset(import_rva)
    while True:
        names_rva, _, _, name_rva, iat_rva = struct.unpack_from("<IIIII", image, descriptor)
        if not iat_rva:
            break
        if c_string(name_rva).lower() == dll.lower():
            index = 0
            names = to_offset(names_rva or iat_rva)
            while True:
                entry = struct.unpack_from("<Q" if is64 else "<I", image, names + index * thunk_size)[0]
                if not entry:
                    break
                if not entry >> (thunk_size * 8 - 1) and c_string((entry & 0xFFFFFFFF) + 2) == func:
                    return image_base + iat_rva + index * thunk_size
                index += 1
        descriptor += 20
    raise AssertionError(f"{dll}!{func} is not imported by {exe}")
