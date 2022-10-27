#ifndef DIANA_PE_DEFS_H
#define DIANA_PE_DEFS_H

#include "diana_core.h"

#pragma pack(push,2)


typedef struct _DIANA_IMAGE_DOS_HEADER 
{      // DOS .EXE header
    char        e_magic[2];                     // Magic number
    DI_UINT16   e_cblp;                      // Bytes on last page of file
    DI_UINT16   e_cp;                        // Pages in file
    DI_UINT16   e_crlc;                      // Relocations
    DI_UINT16   e_cparhdr;                   // Size of header in paragraphs
    DI_UINT16   e_minalloc;                  // Minimum extra paragraphs needed
    DI_UINT16   e_maxalloc;                  // Maximum extra paragraphs needed
    DI_UINT16   e_ss;                        // Initial (relative) SS value
    DI_UINT16   e_sp;                        // Initial SP value
    DI_UINT16   e_csum;                      // Checksum
    DI_UINT16   e_ip;                        // Initial IP value
    DI_UINT16   e_cs;                        // Initial (relative) CS value
    DI_UINT16   e_lfarlc;                    // File address of relocation table
    DI_UINT16   e_ovno;                      // Overlay number
    DI_UINT16   e_res[4];                    // Reserved words
    DI_UINT16   e_oemid;                     // OEM identifier (for e_oeminfo)
    DI_UINT16   e_oeminfo;                   // OEM information; e_oemid specific
    DI_UINT16   e_res2[10];                  // Reserved words
    DI_INT32    e_lfanew;                    // File address of new exe header
}DIANA_IMAGE_DOS_HEADER, *PDIANA_IMAGE_DOS_HEADER;

typedef struct _DIANA_IMAGE_FILE_HEADER 
{
    DI_UINT16    Machine;
    DI_UINT16    NumberOfSections;
    DI_UINT32    TimeDateStamp;
    DI_UINT32    PointerToSymbolTable;
    DI_UINT32    NumberOfSymbols;
    DI_UINT16    SizeOfOptionalHeader;
    DI_UINT16    Characteristics;
}DIANA_IMAGE_FILE_HEADER, *PDIANA_IMAGE_FILE_HEADER;


typedef struct _DIANA_IMAGE_NT_HEADERS 
{
    char                        Signature[4];
    DIANA_IMAGE_FILE_HEADER     FileHeader;
}
DIANA_IMAGE_NT_HEADERS, *PDIANA_IMAGE_NT_HEADERS;


#define DIANA_IMAGE_SIZEOF_SHORT_NAME  8
typedef struct _DIANA_IMAGE_SECTION_HEADER
{
    DI_CHAR  Name[DIANA_IMAGE_SIZEOF_SHORT_NAME];
    union 
    {
        DI_UINT32 PhysicalAddress;
        DI_UINT32 VirtualSize;
    } Misc;
    DI_UINT32 VirtualAddress;
    DI_UINT32 SizeOfRawData;
    DI_UINT32 PointerToRawData;
    DI_UINT32 PointerToRelocations;
    DI_UINT32 PointerToLinenumbers;
    DI_UINT16 NumberOfRelocations;
    DI_UINT16 NumberOfLinenumbers;
    DI_UINT32 Characteristics;
} DIANA_IMAGE_SECTION_HEADER, *PDIANA_IMAGE_SECTION_HEADER;


#define DIANA_IMAGE_SCN_TYPE_NO_PAD             0x00000008
#define DIANA_IMAGE_SCN_CNT_CODE                0x00000020
#define DIANA_IMAGE_SCN_CNT_INITIALIZED_DATA    0x00000040
#define DIANA_IMAGE_SCN_CNT_UNINITIALIZED_DATA  0x00000080
#define DIANA_IMAGE_SCN_LNK_OTHER               0x00000100
#define DIANA_IMAGE_SCN_LNK_INFO                0x00000200
#define DIANA_IMAGE_SCN_LNK_REMOVE              0x00000800
#define DIANA_IMAGE_SCN_LNK_COMDAT              0x00001000
#define DIANA_IMAGE_SCN_NO_DEFER_SPEC_EXC       0x00004000
#define DIANA_IMAGE_SCN_GPREL                   0x00008000
#define DIANA_IMAGE_SCN_MEM_PURGEABLE           0x00020000
#define DIANA_IMAGE_SCN_MEM_LOCKED              0x00040000
#define DIANA_IMAGE_SCN_MEM_PRELOAD             0x00080000
#define DIANA_IMAGE_SCN_ALIGN_1BYTES            0x00100000
#define DIANA_IMAGE_SCN_ALIGN_2BYTES            0x00200000
#define DIANA_IMAGE_SCN_ALIGN_4BYTES            0x00300000
#define DIANA_IMAGE_SCN_ALIGN_8BYTES            0x00400000
#define DIANA_IMAGE_SCN_ALIGN_16BYTES           0x00500000
#define DIANA_IMAGE_SCN_ALIGN_32BYTES           0x00600000
#define DIANA_IMAGE_SCN_ALIGN_64BYTES           0x00700000
#define DIANA_IMAGE_SCN_ALIGN_128BYTES          0x00800000
#define DIANA_IMAGE_SCN_ALIGN_256BYTES          0x00900000
#define DIANA_IMAGE_SCN_ALIGN_512BYTES          0x00A00000
#define DIANA_IMAGE_SCN_ALIGN_1024BYTES         0x00B00000
#define DIANA_IMAGE_SCN_ALIGN_2048BYTES         0x00C00000
#define DIANA_IMAGE_SCN_ALIGN_4096BYTES         0x00D00000
#define DIANA_IMAGE_SCN_ALIGN_8192BYTES         0x00E00000
#define DIANA_IMAGE_SCN_LNK_NRELOC_OVFL         0x01000000
#define DIANA_IMAGE_SCN_MEM_DISCARDABLE         0x02000000
#define DIANA_IMAGE_SCN_MEM_NOT_CACHED          0x04000000
#define DIANA_IMAGE_SCN_MEM_NOT_PAGED           0x08000000
#define DIANA_IMAGE_SCN_MEM_SHARED              0x10000000
#define DIANA_IMAGE_SCN_MEM_EXECUTE             0x20000000
#define DIANA_IMAGE_SCN_MEM_READ                0x40000000 
#define DIANA_IMAGE_SCN_MEM_WRITE               0x80000000


#define DIANA_IMAGE_FILE_RELOCS_STRIPPED           0x0001  // Relocation info stripped from file.
#define DIANA_IMAGE_FILE_EXECUTABLE_IMAGE          0x0002  // File is executable  (i.e. no unresolved externel references).
#define DIANA_IMAGE_FILE_LINE_NUMS_STRIPPED        0x0004  // Line nunbers stripped from file.
#define DIANA_IMAGE_FILE_LOCAL_SYMS_STRIPPED       0x0008  // Local symbols stripped from file.
#define DIANA_IMAGE_FILE_AGGRESIVE_WS_TRIM         0x0010  // Agressively trim working set
#define DIANA_IMAGE_FILE_LARGE_ADDRESS_AWARE       0x0020  // App can handle >2gb addresses
#define DIANA_IMAGE_FILE_BYTES_REVERSED_LO         0x0080  // Bytes of machine word are reversed.
#define DIANA_IMAGE_FILE_32BIT_MACHINE             0x0100  // 32 bit word machine.
#define DIANA_IMAGE_FILE_DEBUG_STRIPPED            0x0200  // Debugging info stripped from file in .DBG file
#define DIANA_IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP   0x0400  // If Image is on removable media, copy and run from the swap file.
#define DIANA_IMAGE_FILE_NET_RUN_FROM_SWAP         0x0800  // If Image is on Net, copy and run from the swap file.
#define DIANA_IMAGE_FILE_SYSTEM                    0x1000  // System File.
#define DIANA_IMAGE_FILE_DLL                       0x2000  // File is a DLL.
#define DIANA_IMAGE_FILE_UP_SYSTEM_ONLY            0x4000  // File should only be run on a UP machine
#define DIANA_IMAGE_FILE_BYTES_REVERSED_HI         0x8000  // Bytes of machine word are reversed.

#define DIANA_IMAGE_FILE_MACHINE_UNKNOWN           0
#define DIANA_IMAGE_FILE_MACHINE_I386              0x014c  // Intel 386.
#define DIANA_IMAGE_FILE_MACHINE_R3000             0x0162  // MIPS little-endian, 0x160 big-endian
#define DIANA_IMAGE_FILE_MACHINE_R4000             0x0166  // MIPS little-endian
#define DIANA_IMAGE_FILE_MACHINE_R10000            0x0168  // MIPS little-endian
#define DIANA_IMAGE_FILE_MACHINE_WCEMIPSV2         0x0169  // MIPS little-endian WCE v2
#define DIANA_IMAGE_FILE_MACHINE_ALPHA             0x0184  // Alpha_AXP
#define DIANA_IMAGE_FILE_MACHINE_SH3               0x01a2  // SH3 little-endian
#define DIANA_IMAGE_FILE_MACHINE_SH3DSP            0x01a3
#define DIANA_IMAGE_FILE_MACHINE_SH3E              0x01a4  // SH3E little-endian
#define DIANA_IMAGE_FILE_MACHINE_SH4               0x01a6  // SH4 little-endian
#define DIANA_IMAGE_FILE_MACHINE_SH5               0x01a8  // SH5
#define DIANA_IMAGE_FILE_MACHINE_ARM               0x01c0  // ARM Little-Endian
#define DIANA_IMAGE_FILE_MACHINE_THUMB             0x01c2
#define DIANA_IMAGE_FILE_MACHINE_AM33              0x01d3
#define DIANA_IMAGE_FILE_MACHINE_POWERPC           0x01F0  // IBM PowerPC Little-Endian
#define DIANA_IMAGE_FILE_MACHINE_POWERPCFP         0x01f1
#define DIANA_IMAGE_FILE_MACHINE_IA64              0x0200  // Intel 64
#define DIANA_IMAGE_FILE_MACHINE_MIPS16            0x0266  // MIPS
#define DIANA_IMAGE_FILE_MACHINE_ALPHA64           0x0284  // ALPHA64
#define DIANA_IMAGE_FILE_MACHINE_MIPSFPU           0x0366  // MIPS
#define DIANA_IMAGE_FILE_MACHINE_MIPSFPU16         0x0466  // MIPS
#define DIANA_IMAGE_FILE_MACHINE_AXP64             DIANA_IMAGE_FILE_MACHINE_ALPHA64
#define DIANA_IMAGE_FILE_MACHINE_TRICORE           0x0520  // Infineon
#define DIANA_IMAGE_FILE_MACHINE_CEF               0x0CEF
#define DIANA_IMAGE_FILE_MACHINE_EBC               0x0EBC  // EFI Byte Code
#define DIANA_IMAGE_FILE_MACHINE_AMD64             0x8664  // AMD64 (K8)
#define DIANA_IMAGE_FILE_MACHINE_M32R              0x9041  // M32R little-endian
#define DIANA_IMAGE_FILE_MACHINE_CEE               0xC0EE

//
#pragma pack(push,2)

typedef struct _DIANA_IMAGE_DATA_DIRECTORY 
{
    DI_UINT32   VirtualAddress;
    DI_UINT32   Size;
} DIANA_IMAGE_DATA_DIRECTORY, *PDIANA_IMAGE_DATA_DIRECTORY;

#define DIANA_IMAGE_DIRECTORY_ENTRY_EXPORT          0   // Export Directory
#define DIANA_IMAGE_DIRECTORY_ENTRY_IMPORT          1   // Import Directory
#define DIANA_IMAGE_DIRECTORY_ENTRY_RESOURCE        2   // Resource Directory
#define DIANA_IMAGE_DIRECTORY_ENTRY_EXCEPTION       3   // Exception Directory
#define DIANA_IMAGE_DIRECTORY_ENTRY_SECURITY        4   // Security Directory
#define DIANA_IMAGE_DIRECTORY_ENTRY_BASERELOC       5   // Base Relocation Table
#define DIANA_IMAGE_DIRECTORY_ENTRY_DEBUG           6   // Debug Directory
#define DIANA_IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7   // Architecture Specific Data
#define DIANA_IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8   // RVA of GP
#define DIANA_IMAGE_DIRECTORY_ENTRY_TLS             9   // TLS Directory
#define DIANA_IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    10   // Load Configuration Directory
#define DIANA_IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   11   // Bound Import Directory in headers
#define DIANA_IMAGE_DIRECTORY_ENTRY_IAT            12   // Import Address Table
#define DIANA_IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   13   // Delay Load Import Descriptors
#define DIANA_IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14   // COM Runtime descriptor

#define DIANA_IMAGE_NUMBEROF_DIRECTORY_ENTRIES    16

typedef struct _DIANA_IMAGE_BASE_RELOCATION 
{
    DI_UINT32   VirtualAddress;
    DI_UINT32   SizeOfBlock;
} 
DIANA_IMAGE_BASE_RELOCATION;
typedef DIANA_IMAGE_BASE_RELOCATION * PDIANA_IMAGE_BASE_RELOCATION;

typedef struct _DIANA_IMAGE_IMPORT_DESCRIPTOR 
{
    union 
    {
        DI_UINT32   Characteristics;
        DI_UINT32   OriginalFirstThunk;
    } Misc;
    DI_UINT32   TimeDateStamp;
    DI_UINT32   ForwarderChain;
    DI_UINT32   Name;
    DI_UINT32   FirstThunk;
} DIANA_IMAGE_IMPORT_DESCRIPTOR;
typedef DIANA_IMAGE_IMPORT_DESCRIPTOR  *PDIANA_IMAGE_IMPORT_DESCRIPTOR;


typedef struct _DIANA_IMAGE_OPTIONAL_HEADER32 {
    //
    // Standard fields.
    //

    DI_UINT16    Magic;
    DI_CHAR    MajorLinkerVersion;
    DI_CHAR    MinorLinkerVersion;
    DI_UINT32   SizeOfCode;
    DI_UINT32   SizeOfInitializedData;
    DI_UINT32   SizeOfUninitializedData;
    DI_UINT32   AddressOfEntryPoint;
    DI_UINT32   BaseOfCode;
    DI_UINT32   BaseOfData;

    //
    // NT additional fields.
    //

    DI_UINT32   ImageBase;
    DI_UINT32   SectionAlignment;
    DI_UINT32   FileAlignment;
    DI_UINT16    MajorOperatingSystemVersion;
    DI_UINT16    MinorOperatingSystemVersion;
    DI_UINT16    MajorImageVersion;
    DI_UINT16    MinorImageVersion;
    DI_UINT16    MajorSubsystemVersion;
    DI_UINT16    MinorSubsystemVersion;
    DI_UINT32   Win32VersionValue;
    DI_UINT32   SizeOfImage;
    DI_UINT32   SizeOfHeaders;
    DI_UINT32   CheckSum;
    DI_UINT16    Subsystem;
    DI_UINT16    DllCharacteristics;
    DI_UINT32   SizeOfStackReserve;
    DI_UINT32   SizeOfStackCommit;
    DI_UINT32   SizeOfHeapReserve;
    DI_UINT32   SizeOfHeapCommit;
    DI_UINT32   LoaderFlags;
    DI_UINT32   NumberOfRvaAndSizes;
    DIANA_IMAGE_DATA_DIRECTORY DataDirectory[DIANA_IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} DIANA_IMAGE_OPTIONAL_HEADER32, *PDIANA_IMAGE_OPTIONAL_HEADER32;


typedef struct DIANA_IMAGE_OPTIONAL_HEADER64 {
    DI_UINT16        Magic;
    DI_CHAR        MajorLinkerVersion;
    DI_CHAR        MinorLinkerVersion;
    DI_UINT32       SizeOfCode;
    DI_UINT32       SizeOfInitializedData;
    DI_UINT32       SizeOfUninitializedData;
    DI_UINT32       AddressOfEntryPoint;
    DI_UINT32       BaseOfCode;
    OPERAND_SIZE   ImageBase;
    DI_UINT32       SectionAlignment;
    DI_UINT32       FileAlignment;
    DI_UINT16        MajorOperatingSystemVersion;
    DI_UINT16        MinorOperatingSystemVersion;
    DI_UINT16        MajorImageVersion;
    DI_UINT16        MinorImageVersion;
    DI_UINT16        MajorSubsystemVersion;
    DI_UINT16        MinorSubsystemVersion;
    DI_UINT32       Win32VersionValue;
    DI_UINT32       SizeOfImage;
    DI_UINT32       SizeOfHeaders;
    DI_UINT32       CheckSum;
    DI_UINT16        Subsystem;
    DI_UINT16        DllCharacteristics;
    OPERAND_SIZE   SizeOfStackReserve;
    OPERAND_SIZE   SizeOfStackCommit;
    OPERAND_SIZE   SizeOfHeapReserve;
    OPERAND_SIZE   SizeOfHeapCommit;
    DI_UINT32       LoaderFlags;
    DI_UINT32       NumberOfRvaAndSizes;
    DIANA_IMAGE_DATA_DIRECTORY DataDirectory[DIANA_IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} DIANA_IMAGE_OPTIONAL_HEADER64, *PDIANA_IMAGE_OPTIONAL_HEADER64;


typedef struct _DIANA_IMAGE_EXPORT_DIRECTORY {
    DI_UINT32   Characteristics;
    DI_UINT32   TimeDateStamp;
    DI_UINT16   MajorVersion;
    DI_UINT16   MinorVersion;
    DI_UINT32   Name;
    DI_UINT32   Base;
    DI_UINT32   NumberOfFunctions;
    DI_UINT32   NumberOfNames;
    DI_UINT32   AddressOfFunctions;// RVA 
    DI_UINT32   AddressOfNames;// RVA
    DI_UINT32   AddressOfNameOrdinals;// RVA
} DIANA_IMAGE_EXPORT_DIRECTORY, *PDIANA_IMAGE_EXPORT_DIRECTORY;

typedef struct _DIANA_IMAGE_IMPORT_BY_NAME {
    DI_UINT16   Hint;
    char        Name[1];
} DIANA_IMAGE_IMPORT_BY_NAME, *PDIANA_IMAGE_IMPORT_BY_NAME;

#pragma pack(pop)
#pragma pack(pop)

#pragma pack(push,8)

typedef struct _DIANA_IMAGE_THUNK_DATA64 {
    union {
        DI_UINT64 ForwarderString;  // PUCHAR
        DI_UINT64 Function;         // PULONG
        DI_UINT64 Ordinal;
        DI_UINT64 AddressOfData;    // PIMAGE_IMPORT_BY_NAME
    } u1;
} DIANA_IMAGE_THUNK_DATA64;
typedef DIANA_IMAGE_THUNK_DATA64 * PDIANA_IMAGE_THUNK_DATA64;

#pragma pack(pop)

typedef struct _DIANA_IMAGE_THUNK_DATA32 {
    union {
        DI_UINT32 ForwarderString;      // PUCHAR
        DI_UINT32 Function;             // PULONG
        DI_UINT32 Ordinal;
        DI_UINT32 AddressOfData;        // PIMAGE_IMPORT_BY_NAME
    } u1;
} DIANA_IMAGE_THUNK_DATA32;
typedef DIANA_IMAGE_THUNK_DATA32 * PDIANA_IMAGE_THUNK_DATA32;

#pragma pack(push,4)
// tls callbacks
typedef struct _DIANA_IMAGE_TLS_DIRECTORY64 {
    DI_UINT64    StartAddressOfRawData;
    DI_UINT64    EndAddressOfRawData;
    DI_UINT64    AddressOfIndex;         // PULONG
    DI_UINT64    AddressOfCallBacks;     // PIMAGE_TLS_CALLBACK *;
    DI_UINT32    SizeOfZeroFill;
    DI_UINT32    Characteristics;
} DIANA_IMAGE_TLS_DIRECTORY64;
typedef DIANA_IMAGE_TLS_DIRECTORY64 * PDIANA_IMAGE_TLS_DIRECTORY64;

typedef struct _DIANA_IMAGE_TLS_DIRECTORY32 {
    DI_UINT32    StartAddressOfRawData;
    DI_UINT32    EndAddressOfRawData;
    DI_UINT32    AddressOfIndex;             // PULONG
    DI_UINT32    AddressOfCallBacks;         // PIMAGE_TLS_CALLBACK *
    DI_UINT32    SizeOfZeroFill;
    DI_UINT32    Characteristics;
} DIANA_IMAGE_TLS_DIRECTORY32;
typedef DIANA_IMAGE_TLS_DIRECTORY32 * PDIANA_IMAGE_TLS_DIRECTORY32;

#pragma pack(pop)

#endif