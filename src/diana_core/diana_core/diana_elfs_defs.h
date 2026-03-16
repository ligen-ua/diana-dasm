#ifndef DIANA_ELF_DEFS_H
#define DIANA_ELF_DEFS_H

#include <stdint.h>

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
    uint8_t  e_ident[16];           // ELF identification
    uint16_t e_type;                // Object file type
    uint16_t e_machine;             // Machine type
    uint32_t e_version;             // Object file version
    uint64_t e_entry;               // Entry point address (64-bit)
    uint64_t e_phoff;               // Program header offset (64-bit)
    uint64_t e_shoff;               // Section header offset (64-bit)
    uint32_t e_flags;               // Processor-specific flags
    uint16_t e_ehsize;              // ELF header size
    uint16_t e_phentsize;           // Program header entry size
    uint16_t e_phnum;               // Program header entry count
    uint16_t e_shentsize;           // Section header entry size
    uint16_t e_shnum;               // Section header entry count
    uint16_t e_shstrndx;            // Section name string table index

} DIANA_ELF_HEADER;

// Program Header (64-bit)
typedef struct
{
    uint32_t p_type;                // Segment type
    uint32_t p_flags;               // Segment flags
    uint64_t p_offset;              // Segment offset in file
    uint64_t p_vaddr;               // Virtual address
    uint64_t p_paddr;               // Physical address (often same as vaddr)
    uint64_t p_filesz;              // Size in file
    uint64_t p_memsz;               // Size in memory
    uint64_t p_align;               // Alignment

} DIANA_ELF_PROGRAM_HEADER;

// 32-bit structures
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

// Section Header (64-bit)
typedef struct
{
    uint32_t sh_name;               // Section name offset in string table (offset 0)
    uint32_t sh_type;               // Section type (offset 4)
    uint64_t sh_flags;              // Section flags (offset 8)
    uint64_t sh_addr;               // Section address (offset 16)
    uint64_t sh_offset;             // Section offset in file (offset 24)
    uint64_t sh_size;               // Section size (offset 32)
    uint32_t sh_link;               // Link to another section (offset 40)
    uint32_t sh_info;               // Extra section information (offset 44)
    uint64_t sh_addralign;          // Section address alignment (offset 48)
    uint64_t sh_entsize;            // Entry size if section holds table (offset 56)
} DIANA_ELF_SECTION_HEADER;

// Symbol Entry (64-bit)
typedef struct
{
    uint32_t st_name;               // Symbol name offset in string table
    uint8_t  st_info;               // Symbol binding and type
    uint8_t  st_other;              // Symbol visibility
    uint16_t st_shndx;              // Section index
    uint64_t st_value;              // Symbol value
    uint64_t st_size;               // Symbol size

} DIANA_ELF_SYMBOL;

// Relocation Entry (without addend)
typedef struct
{
    uint64_t r_offset;              // Relocation offset
    uint64_t r_info;                // Relocation type and symbol index

} DIANA_ELF_REL;

// Relocation Entry (with addend)
typedef struct
{
    uint64_t r_offset;              // Relocation offset
    uint64_t r_info;                // Relocation type and symbol index
    int64_t  r_addend;              // Relocation addend

} DIANA_ELF_RELA;

// Dynamic Entry
typedef struct
{
    int64_t  d_tag;                 // Dynamic entry type
    uint64_t d_un;                  // Value

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

