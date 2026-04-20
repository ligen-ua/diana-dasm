#ifndef DIANA_ELF_DEFS_H
#define DIANA_ELF_DEFS_H

#include "diana_core.h"

// ELF magic and identification
#define DIANA_ELFMAG0     0x7f
#define DIANA_ELFMAG1     'E'
#define DIANA_ELFMAG2     'L'
#define DIANA_ELFMAG3     'F'

#define DIANA_EI_CLASS    4
#define DIANA_EI_DATA     5
#define DIANA_EI_NIDENT   16

// ELF class (32-bit vs 64-bit)
#define DIANA_ELFCLASS32  1
#define DIANA_ELFCLASS64  2

// ELF data encoding (endianness)
#define DIANA_ELFDATA2LSB 1  // Little-endian
#define DIANA_ELFDATA2MSB 2  // Big-endian

// ELF version
#define DIANA_EV_CURRENT  1

// ELF OS/ABI
#define DIANA_ELFOSABI_SYSV       0
#define DIANA_ELFOSABI_LINUX      3

// ELF type
#define DIANA_ET_NONE   0      // No file type
#define DIANA_ET_REL    1      // Relocatable file
#define DIANA_ET_EXEC   2      // Executable file
#define DIANA_ET_DYN    3      // Shared object file
#define DIANA_ET_CORE   4      // Core file

// Machine architecture
#define DIANA_EM_386          3       // Intel 80386
#define DIANA_EM_X86_64       62      // AMD x86-64
#define DIANA_EM_ARM          40      // ARM
#define DIANA_EM_AARCH64      183     // ARM 64-bit

// Program header types
#define DIANA_PT_NULL    0
#define DIANA_PT_LOAD    1
#define DIANA_PT_DYNAMIC 2
#define DIANA_PT_INTERP  3
#define DIANA_PT_NOTE    4
#define DIANA_PT_SHLIB   5
#define DIANA_PT_PHDR    6
#define DIANA_PT_TLS     7

// Program header flags
#define DIANA_PF_X  (1 << 0)   // Execute
#define DIANA_PF_W  (1 << 1)   // Write
#define DIANA_PF_R  (1 << 2)   // Read

// Section header types
#define DIANA_SHT_NULL          0
#define DIANA_SHT_PROGBITS      1
#define DIANA_SHT_SYMTAB        2
#define DIANA_SHT_STRTAB        3
#define DIANA_SHT_RELA          4
#define DIANA_SHT_HASH          5
#define DIANA_SHT_DYNAMIC       6
#define DIANA_SHT_NOTE          7
#define DIANA_SHT_NOBITS        8
#define DIANA_SHT_REL           9
#define DIANA_SHT_SHLIB         10
#define DIANA_SHT_DYNSYM        11
#define DIANA_SHT_INIT_ARRAY    14
#define DIANA_SHT_FINI_ARRAY    15
#define DIANA_SHT_PREINIT_ARRAY 16
#define DIANA_SHT_GROUP         17
#define DIANA_SHT_SYMTAB_SHNDX  18
#define DIANA_SHT_RELR          19

// Section header flags
#define DIANA_SHF_WRITE      (1 << 0)
#define DIANA_SHF_ALLOC      (1 << 1)
#define DIANA_SHF_EXECINSTR  (1 << 2)
#define DIANA_SHF_MERGE      (1 << 4)
#define DIANA_SHF_STRINGS    (1 << 5)
#define DIANA_SHF_INFO_LINK  (1 << 6)
#define DIANA_SHF_LINK_ORDER (1 << 7)
#define DIANA_SHF_OS_NONCONFORMING (1 << 8)
#define DIANA_SHF_GROUP      (1 << 9)
#define DIANA_SHF_TLS        (1 << 10)

// Dynamic section entry types
#define DIANA_DT_NULL         0
#define DIANA_DT_NEEDED       1
#define DIANA_DT_PLTRELSZ     2
#define DIANA_DT_PLTGOT       3
#define DIANA_DT_HASH         4
#define DIANA_DT_STRTAB       5
#define DIANA_DT_SYMTAB       6
#define DIANA_DT_RELA         7
#define DIANA_DT_RELASZ       8
#define DIANA_DT_RELAENT      9
#define DIANA_DT_STRSZ        10
#define DIANA_DT_SYMENT       11
#define DIANA_DT_INIT         12
#define DIANA_DT_FINI         13
#define DIANA_DT_SONAME       14
#define DIANA_DT_RPATH        15
#define DIANA_DT_SYMBOLIC     16
#define DIANA_DT_REL          17
#define DIANA_DT_RELSZ        18
#define DIANA_DT_RELENT       19
#define DIANA_DT_PLTREL       20
#define DIANA_DT_DEBUG        21
#define DIANA_DT_TEXTREL      22
#define DIANA_DT_JMPREL       23
#define DIANA_DT_BIND_NOW     24
#define DIANA_DT_INIT_ARRAY   25
#define DIANA_DT_FINI_ARRAY   26
#define DIANA_DT_INIT_ARRAYSZ 27
#define DIANA_DT_FINI_ARRAYSZ 28
#define DIANA_DT_RUNPATH      29
#define DIANA_DT_FLAGS        30
#define DIANA_DT_PREINIT_ARRAY 32
#define DIANA_DT_PREINIT_ARRAYSZ 33

// Symbol binding
#define DIANA_STB_LOCAL   0
#define DIANA_STB_GLOBAL  1
#define DIANA_STB_WEAK    2

// Symbol type
#define DIANA_STT_NOTYPE   0
#define DIANA_STT_OBJECT   1
#define DIANA_STT_FUNC     2
#define DIANA_STT_SECTION  3
#define DIANA_STT_FILE     4
#define DIANA_STT_COMMON   5
#define DIANA_STT_TLS      6

// Symbol visibility
#define DIANA_STV_DEFAULT   0
#define DIANA_STV_INTERNAL  1
#define DIANA_STV_HIDDEN    2
#define DIANA_STV_PROTECTED 3

#define DIANA_STN_UNDEF     0

// ELF Header (64-bit - unions both 32 and 64 bit fields)
typedef struct
{
    DI_UINT8  e_ident[16];           // ELF identification
    DI_UINT16 e_type;                // Object file type
    DI_UINT16 e_machine;             // Machine type
    DI_UINT32 e_version;             // Object file version
    DI_UINT64 e_entry;               // Entry point address (64-bit)
    DI_UINT64 e_phoff;               // Program header offset (64-bit)
    DI_UINT64 e_shoff;               // Section header offset (64-bit)
    DI_UINT32 e_flags;               // Processor-specific flags
    DI_UINT16 e_ehsize;              // ELF header size
    DI_UINT16 e_phentsize;           // Program header entry size
    DI_UINT16 e_phnum;               // Program header entry count
    DI_UINT16 e_shentsize;           // Section header entry size
    DI_UINT16 e_shnum;               // Section header entry count
    DI_UINT16 e_shstrndx;            // Section name string table index

} DIANA_ELF_HEADER;

// Program Header (64-bit)
typedef struct
{
    DI_UINT32 p_type;                // Segment type
    DI_UINT32 p_flags;               // Segment flags
    DI_UINT64 p_offset;              // Segment offset in file
    DI_UINT64 p_vaddr;               // Virtual address
    DI_UINT64 p_paddr;               // Physical address (often same as vaddr)
    DI_UINT64 p_filesz;              // Size in file
    DI_UINT64 p_memsz;               // Size in memory
    DI_UINT64 p_align;               // Alignment

} DIANA_ELF_PROGRAM_HEADER;

// 32-bit structures
typedef struct {
    DI_UINT8  e_ident[16];
    DI_UINT16 e_type;
    DI_UINT16 e_machine;
    DI_UINT32 e_version;
    DI_UINT32 e_entry;
    DI_UINT32 e_phoff;
    DI_UINT32 e_shoff;
    DI_UINT32 e_flags;
    DI_UINT16 e_ehsize;
    DI_UINT16 e_phentsize;
    DI_UINT16 e_phnum;
    DI_UINT16 e_shentsize;
    DI_UINT16 e_shnum;
    DI_UINT16 e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    DI_UINT32 p_type;
    DI_UINT32 p_offset;
    DI_UINT32 p_vaddr;
    DI_UINT32 p_paddr;
    DI_UINT32 p_filesz;
    DI_UINT32 p_memsz;
    DI_UINT32 p_flags;
    DI_UINT32 p_align;
} Elf32_Phdr;

// Section Header (64-bit)
typedef struct
{
    DI_UINT32 sh_name;               // Section name offset in string table (offset 0)
    DI_UINT32 sh_type;               // Section type (offset 4)
    DI_UINT64 sh_flags;              // Section flags (offset 8)
    DI_UINT64 sh_addr;               // Section address (offset 16)
    DI_UINT64 sh_offset;             // Section offset in file (offset 24)
    DI_UINT64 sh_size;               // Section size (offset 32)
    DI_UINT32 sh_link;               // Link to another section (offset 40)
    DI_UINT32 sh_info;               // Extra section information (offset 44)
    DI_UINT64 sh_addralign;          // Section address alignment (offset 48)
    DI_UINT64 sh_entsize;            // Entry size if section holds table (offset 56)
} DIANA_ELF_SECTION_HEADER;

// Symbol Entry (64-bit)
typedef struct
{
    DI_UINT32 st_name;               // Symbol name offset in string table
    DI_UINT8  st_info;               // Symbol binding and type
    DI_UINT8  st_other;              // Symbol visibility
    DI_UINT16 st_shndx;              // Section index
    DI_UINT64 st_value;              // Symbol value
    DI_UINT64 st_size;               // Symbol size

} DIANA_ELF_SYMBOL;

// Relocation Entry (without addend)
typedef struct
{
    DI_UINT64 r_offset;              // Relocation offset
    DI_UINT64 r_info;                // Relocation type and symbol index

} DIANA_ELF_REL;

// Relocation Entry (with addend)
typedef struct
{
    DI_UINT64 r_offset;              // Relocation offset
    DI_UINT64 r_info;                // Relocation type and symbol index
    DI_INT64  r_addend;              // Relocation addend

} DIANA_ELF_RELA;

// Dynamic Entry
typedef struct
{
    DI_INT64  d_tag;                 // Dynamic entry type
    DI_UINT64 d_un;                  // Value

} DIANA_ELF_DYN;

// Helper macros for symbol table entries
#define DIANA_ELF_ST_BIND(info)           (((info) >> 4) & 0xf)
#define DIANA_ELF_ST_TYPE(info)           ((info) & 0xf)
#define DIANA_ELF_ST_INFO(bind, type)     (((bind) << 4) + ((type) & 0xf))

#define DIANA_ELF_ST_VISIBILITY(other)    ((other) & 0x3)

// Helper macros for relocation entries
#define DIANA_ELF_R_SYM(info)             ((info) >> 32)
#define DIANA_ELF_R_TYPE(info)            ((info) & 0xffffffff)
#define DIANA_ELF_R_INFO(sym, type)       (((sym) << 32) + ((type) & 0xffffffff))

// 32-bit versions
#define DIANA_ELF_R_SYM_32(info)          ((info) >> 8)
#define DIANA_ELF_R_TYPE_32(info)         ((info) & 0xff)
#define DIANA_ELF_R_INFO_32(sym, type)    (((sym) << 8) + ((type) & 0xff))


// Relocation type constants for the architectures in diana_elfs_defs.h
// x86-64
#define DIANA_R_X86_64_NONE         0
#define DIANA_R_X86_64_64           1   /* S + A         */
#define DIANA_R_X86_64_RELATIVE     8   /* B + A         */
#define DIANA_R_X86_64_GLOB_DAT     6   /* S             */
#define DIANA_R_X86_64_JUMP_SLOT    7   /* S             */
#define DIANA_R_X86_64_32          10   /* S + A (trunc) */

// i386
#define DIANA_R_386_NONE            0
#define DIANA_R_386_32              1   /* S + A         */
#define DIANA_R_386_RELATIVE        8   /* B + A         */
#define DIANA_R_386_GLOB_DAT        6   /* S             */
#define DIANA_R_386_JMP_SLOT        7   /* S             */

// AArch64
#define DIANA_R_AARCH64_NONE            0
#define DIANA_R_AARCH64_RELATIVE      1027  /* Delta(S) + A  */
#define DIANA_R_AARCH64_GLOB_DAT      1025  /* S + A - P     */
#define DIANA_R_AARCH64_JUMP_SLOT     1026  /* S + A         */
#define DIANA_R_AARCH64_ABS64          257  /* S + A         */

// ARM (32-bit)
#define DIANA_R_ARM_NONE                0
#define DIANA_R_ARM_RELATIVE           23   /* B + A         */
#define DIANA_R_ARM_GLOB_DAT           21   /* S             */
#define DIANA_R_ARM_JUMP_SLOT          22   /* S             */
#define DIANA_R_ARM_ABS32               2   /* S + A         */

#endif

