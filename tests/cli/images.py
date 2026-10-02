"""Independent readers of the binaries Orthia opens, to compute what the tests expect.

The self tests open the freshly built Orthia binary, whose addresses change with every build,
so the expected values are read here from the file itself (or from dbghelp for PDB symbols),
never from Orthia's own output.
"""
import ctypes
import struct
import sys
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional


class Section(NamedTuple):
    name: str
    address: Optional[int]   # relative to the load bias (ELF) or the image base (PE); None: not loaded
    size: int
    flags: str               # 'R-X' etc., as `sections` prints it


# ELF

PT_LOAD = 1
SHT_NOTE = 7
SHT_NOBITS = 8
SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR = 0x1, 0x2, 0x4
DT_NEEDED, DT_RUNPATH, DT_RPATH = 1, 29, 15
NT_GNU_BUILD_ID = 3


class ElfInfo:
    """ELF64 little-endian: what `lm`, `modinfo`, `sections` and dependency tests need."""

    def __init__(self, path: Path):
        self.path = Path(path)
        data = self.path.read_bytes()
        self._data = data
        assert data[:4] == b"\x7fELF" and data[4] == 2 and data[5] == 1, f"not an ELF64 LE file: {path}"
        (self.e_type, _, _, self.e_entry, e_phoff, e_shoff, _, _, e_phentsize, e_phnum,
         e_shentsize, e_shnum, e_shstrndx) = struct.unpack_from("<HHIQQQIHHHHHH", data, 16)

        self.loads = []   # (p_vaddr, p_memsz, p_offset, p_filesz)
        for i in range(e_phnum):
            p_type, _, p_offset, p_vaddr, _, p_filesz, p_memsz, _ = struct.unpack_from(
                "<IIQQQQQQ", data, e_phoff + i * e_phentsize)
            if p_type == PT_LOAD:
                self.loads.append((p_vaddr, p_memsz, p_offset, p_filesz))

        raw = [struct.unpack_from("<IIQQQQIIQQ", data, e_shoff + i * e_shentsize) for i in range(e_shnum)]
        shstr_offset = raw[e_shstrndx][4]

        def c_string(offset):
            return data[offset:data.index(b"\0", offset)].decode()

        self.sections: List[Section] = []
        self.build_id: Optional[str] = None
        dynamic = None
        by_index = {}
        for index, (name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, _, _, _) in enumerate(raw):
            if index == 0:
                continue
            section_name = c_string(shstr_offset + name)
            by_index[index] = (sh_offset, sh_size)
            loaded = bool(sh_flags & SHF_ALLOC)
            flags = ("R" if loaded else "-") + ("W" if sh_flags & SHF_WRITE else "-") + \
                    ("X" if sh_flags & SHF_EXECINSTR else "-")
            self.sections.append(Section(section_name, sh_addr if loaded else None, sh_size, flags))
            if sh_type == SHT_NOTE and self.build_id is None:
                self.build_id = self._build_id(data[sh_offset:sh_offset + sh_size])
            if section_name == ".dynamic":
                dynamic = (sh_offset, sh_size, sh_link)

        self.needed: List[str] = []
        self.runpath: List[str] = []
        if dynamic:
            offset, size, link = dynamic
            strtab = by_index[link][0]
            for pos in range(offset, offset + size, 16):
                tag, value = struct.unpack_from("<qQ", data, pos)
                if tag == 0:
                    break
                if tag == DT_NEEDED:
                    self.needed.append(c_string(strtab + value))
                elif tag in (DT_RUNPATH, DT_RPATH):
                    self.runpath += [p for p in c_string(strtab + value).split(":") if p]

    @staticmethod
    def _build_id(notes: bytes) -> Optional[str]:
        pos = 0
        while pos + 12 <= len(notes):
            namesz, descsz, note_type = struct.unpack_from("<III", notes, pos)
            name_end = pos + 12 + ((namesz + 3) & ~3)
            if note_type == NT_GNU_BUILD_ID and notes[pos + 12:pos + 12 + namesz] == b"GNU\0":
                return notes[name_end:name_end + descsz].hex()
            pos = name_end + ((descsz + 3) & ~3)
        return None

    @property
    def image_size(self) -> int:
        """End of the highest PT_LOAD: Orthia maps the image from the load bias up to there."""
        return max(vaddr + memsz for vaddr, memsz, _, _ in self.loads)

    def bytes_at(self, vaddr: int, size: int) -> bytes:
        """File bytes that a PT_LOAD maps at vaddr (relative to the load bias)."""
        for p_vaddr, _, p_offset, p_filesz in self.loads:
            if p_vaddr <= vaddr and vaddr + size <= p_vaddr + p_filesz:
                return self._data[p_offset + vaddr - p_vaddr:p_offset + vaddr - p_vaddr + size]
        raise AssertionError(f"{vaddr:#x} is not file-backed in {self.path}")

    def find_needed(self, name: str) -> Optional[Path]:
        """Where the loader would take a DT_NEEDED library from via RUNPATH ($ORIGIN expanded)."""
        for entry in self.runpath:
            candidate = Path(entry.replace("$ORIGIN", str(self.path.resolve().parent))) / name
            if candidate.is_file():
                return candidate
        return None


def break_section_headers(path: Path) -> None:
    """Point e_shoff past the end of the file; the program headers stay valid, so Linux still runs it."""
    image = bytearray(path.read_bytes())
    struct.pack_into("<Q", image, 0x28, len(image) + 0x1000)
    path.write_bytes(image)


# PE

IMAGE_SCN_MEM_EXECUTE, IMAGE_SCN_MEM_READ, IMAGE_SCN_MEM_WRITE = 0x20000000, 0x40000000, 0x80000000


class PeInfo:
    """PE32+ / PE32: headers, sections, exports, imports and the RSDS debug record."""

    def __init__(self, path: Path):
        self.path = Path(path)
        image = self.path.read_bytes()
        self._image = image
        assert image[:2] == b"MZ", f"not a PE file: {path}"
        e_lfanew = struct.unpack_from("<I", image, 0x3C)[0]
        assert image[e_lfanew:e_lfanew + 4] == b"PE\0\0", f"not a PE file: {path}"
        section_count = struct.unpack_from("<H", image, e_lfanew + 6)[0]
        optional_size = struct.unpack_from("<H", image, e_lfanew + 20)[0]
        optional = e_lfanew + 24
        self.is64 = struct.unpack_from("<H", image, optional)[0] == 0x20B
        self.entry_rva = struct.unpack_from("<I", image, optional + 16)[0]
        self.image_base = struct.unpack_from("<Q" if self.is64 else "<I", image, optional + 24)[0]
        self.size_of_image = struct.unpack_from("<I", image, optional + 56)[0]
        directories = optional + (112 if self.is64 else 96)
        self._export_dir = struct.unpack_from("<II", image, directories)
        self._import_rva = struct.unpack_from("<I", image, directories + 8)[0]
        self._debug_dir = struct.unpack_from("<II", image, directories + 6 * 8)

        self._raw_sections = []
        self.sections: List[Section] = []
        for i in range(section_count):
            base = optional + optional_size + i * 40
            name = image[base:base + 8].rstrip(b"\0").decode()
            virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from("<IIII", image, base + 8)
            characteristics = struct.unpack_from("<I", image, base + 36)[0]
            self._raw_sections.append((virtual_address, max(virtual_size, raw_size), raw_offset))
            flags = ("R" if characteristics & IMAGE_SCN_MEM_READ else "-") + \
                    ("W" if characteristics & IMAGE_SCN_MEM_WRITE else "-") + \
                    ("X" if characteristics & IMAGE_SCN_MEM_EXECUTE else "-")
            self.sections.append(Section(name, virtual_address, virtual_size, flags))

    def _offset(self, rva: int) -> int:
        for virtual_address, size, raw_offset in self._raw_sections:
            if virtual_address <= rva < virtual_address + size:
                return raw_offset + rva - virtual_address
        raise AssertionError(f"rva {rva:#x} not in any section of {self.path}")

    def _c_string(self, rva: int) -> str:
        offset = self._offset(rva)
        return self._image[offset:self._image.index(b"\0", offset)].decode("ascii")

    def bytes_at(self, rva: int, size: int) -> bytes:
        offset = self._offset(rva)
        return self._image[offset:offset + size]

    @property
    def exports(self) -> Dict[str, int]:
        """Named exports -> RVA; forwarded exports are left out (they have no code here)."""
        rva, size = self._export_dir
        if not rva:
            return {}
        image = self._image
        directory = self._offset(rva)
        functions, names, ordinals = struct.unpack_from("<III", image, directory + 28)
        name_count = struct.unpack_from("<I", image, directory + 24)[0]
        result = {}
        for i in range(name_count):
            name = self._c_string(struct.unpack_from("<I", image, self._offset(names) + i * 4)[0])
            ordinal = struct.unpack_from("<H", image, self._offset(ordinals) + i * 2)[0]
            function = struct.unpack_from("<I", image, self._offset(functions) + ordinal * 4)[0]
            if not rva <= function < rva + size:
                result[name] = function
        return result

    @property
    def imports(self) -> Dict[str, List[str]]:
        """Imported DLL name (as written) -> functions imported by name."""
        result: Dict[str, List[str]] = {}
        image = self._image
        thunk_size = 8 if self.is64 else 4
        descriptor = self._offset(self._import_rva)
        while True:
            names_rva, _, _, name_rva, iat_rva = struct.unpack_from("<IIIII", image, descriptor)
            if not iat_rva:
                break
            functions = result.setdefault(self._c_string(name_rva), [])
            names = self._offset(names_rva or iat_rva)
            index = 0
            while True:
                entry = struct.unpack_from("<Q" if self.is64 else "<I", image, names + index * thunk_size)[0]
                if not entry:
                    break
                if not entry >> (thunk_size * 8 - 1):
                    functions.append(self._c_string((entry & 0xFFFFFFFF) + 2))
                index += 1
            descriptor += 20
        return result

    @property
    def rsds_offset(self) -> int:
        """File offset of the RSDS CodeView record (the bytes 'RSDS' may also occur elsewhere in the file)."""
        rva, size = self._debug_dir
        for entry in range(self._offset(rva), self._offset(rva) + size, 28):
            debug_type, _, _, pointer = struct.unpack_from("<IIII", self._image, entry + 12)
            if debug_type == 2 and self._image[pointer:pointer + 4] == b"RSDS":   # IMAGE_DEBUG_TYPE_CODEVIEW
                return pointer
        raise AssertionError(f"no RSDS record in {self.path}")

    @property
    def pdb_name(self) -> str:
        """The PDB path recorded in the RSDS debug record."""
        offset = self.rsds_offset + 24   # after 'RSDS', the GUID and the age
        return self._image[offset:self._image.index(b"\0", offset)].decode()


# PDB symbols through dbghelp (Windows only): an oracle that shares no code with Orthia's PDB reader

def dbghelp_symbols(exe: Path, symbol_path: Path, names: List[str]) -> Dict[str, int]:
    """name -> address at the preferred image base, as dbghelp resolves it from the PDB on symbol_path."""
    assert sys.platform == "win32"
    from ctypes import wintypes

    class SYMBOL_INFOW(ctypes.Structure):
        _fields_ = [("SizeOfStruct", wintypes.ULONG), ("TypeIndex", wintypes.ULONG),
                    ("Reserved", ctypes.c_ulonglong * 2), ("Index", wintypes.ULONG), ("Size", wintypes.ULONG),
                    ("ModBase", ctypes.c_ulonglong), ("Flags", wintypes.ULONG), ("Value", ctypes.c_ulonglong),
                    ("Address", ctypes.c_ulonglong), ("Register", wintypes.ULONG), ("Scope", wintypes.ULONG),
                    ("Tag", wintypes.ULONG), ("NameLen", wintypes.ULONG), ("MaxNameLen", wintypes.ULONG),
                    ("Name", wintypes.WCHAR * 1024)]

    SYMOPT_UNDNAME, SYMOPT_DEFERRED_LOADS, SYMOPT_IGNORE_CVREC, SYMOPT_EXACT_SYMBOLS = 0x2, 0x4, 0x80, 0x400
    dbghelp = ctypes.WinDLL("dbghelp", use_last_error=True)
    dbghelp.SymInitializeW.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, wintypes.BOOL]
    dbghelp.SymLoadModuleExW.restype = ctypes.c_ulonglong
    dbghelp.SymLoadModuleExW.argtypes = [wintypes.HANDLE, wintypes.HANDLE, wintypes.LPCWSTR, wintypes.LPCWSTR,
                                         ctypes.c_ulonglong, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
    dbghelp.SymFromNameW.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, ctypes.POINTER(SYMBOL_INFOW)]
    dbghelp.SymCleanup.argtypes = [wintypes.HANDLE]

    process = wintypes.HANDLE(0x0DB6E1F)   # any unique value: no real process is attached
    dbghelp.SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_IGNORE_CVREC | SYMOPT_EXACT_SYMBOLS)
    # only the given folder: the path recorded in the RSDS record must not be used
    assert dbghelp.SymInitializeW(process, str(symbol_path), False), ctypes.get_last_error()
    try:
        assert dbghelp.SymLoadModuleExW(process, None, str(exe), None, 0, 0, None, 0), ctypes.get_last_error()
        result = {}
        module = Path(exe).stem
        for name in names:
            info = SYMBOL_INFOW()
            info.SizeOfStruct = SYMBOL_INFOW.Name.offset + ctypes.sizeof(wintypes.WCHAR)
            info.MaxNameLen = 1024
            assert dbghelp.SymFromNameW(process, f"{module}!{name}", ctypes.byref(info)), (name, ctypes.get_last_error())
            result[name] = info.Address
        return result
    finally:
        dbghelp.SymCleanup(process)
