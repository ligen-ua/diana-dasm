#ifndef DIANA_CORE_WIN32_CONTEXT_H
#define DIANA_CORE_WIN32_CONTEXT_H

#include "diana_core.h"

#pragma warning(push)
#pragma warning(disable:4324 4201)



#ifndef DIANA_DECLSPEC_ALIGN
#if (_MSC_VER >= 1300) && !defined(MIDL_PASS)
#define DIANA_DECLSPEC_ALIGN(x)   __declspec(align(x))
#else
#define DIANA_DECLSPEC_ALIGN(x)
#endif
#endif


typedef struct DIANA_DECLSPEC_ALIGN(16) _DIANA_M128A_NTLIKE 
{
    DI_UINT64 Low;
    DI_INT64 High;
}
DIANA_M128A_NTLIKE, *PM128A_NTLIKE;


#define DIANA_MAXIMUM_SUPPORTED_EXTENSION_32     512
#define DIANA_SIZE_OF_80387_REGISTERS_32         80

typedef struct _DIANA_FLOATING_SAVE_AREA_32
{
    DI_UINT32   ControlWord;
    DI_UINT32   StatusWord;
    DI_UINT32   TagWord;
    DI_UINT32   ErrorOffset;
    DI_UINT32   ErrorSelector;
    DI_UINT32   DataOffset;
    DI_UINT32   DataSelector;
    DI_CHAR     RegisterArea[DIANA_SIZE_OF_80387_REGISTERS_32];
    DI_UINT32   Cr0NpxState;
} DIANA_FLOATING_SAVE_AREA_32;

typedef struct _DIANA_CONTEXT_NTLIKE_32
{
    DI_UINT32 ContextFlags;

    //
    // This section is specified/returned if CONTEXT_DEBUG_REGISTERS is
    // set in ContextFlags.  Note that CONTEXT_DEBUG_REGISTERS is NOT
    // included in CONTEXT_FULL.
    //

    DI_UINT32   Dr0;
    DI_UINT32   Dr1;
    DI_UINT32   Dr2;
    DI_UINT32   Dr3;
    DI_UINT32   Dr6;
    DI_UINT32   Dr7;

    //
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_FLOATING_POINT.
    //

    DIANA_FLOATING_SAVE_AREA_32 FloatSave;

    //
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_SEGMENTS.
    //

    DI_UINT32   SegGs;
    DI_UINT32   SegFs;
    DI_UINT32   SegEs;
    DI_UINT32   SegDs;

    //
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_INTEGER.
    //

    DI_UINT32   Edi;
    DI_UINT32   Esi;
    DI_UINT32   Ebx;
    DI_UINT32   Edx;
    DI_UINT32   Ecx;
    DI_UINT32   Eax;

    //
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_CONTROL.
    //

    DI_UINT32   Ebp;
    DI_UINT32   Eip;
    DI_UINT32   SegCs;              // MUST BE SANITIZED
    DI_UINT32   EFlags;             // MUST BE SANITIZED
    DI_UINT32   Esp;
    DI_UINT32   SegSs;

    //
    // This section is specified/returned if the ContextFlags word
    // contains the flag CONTEXT_EXTENDED_REGISTERS.
    // The format and contexts are processor specific
    //

    DI_CHAR    ExtendedRegisters[DIANA_MAXIMUM_SUPPORTED_EXTENSION_32];

}DIANA_CONTEXT_NTLIKE_32;

typedef struct _DIANA_XMM_SAVE_AREA32_NTLIKE 
{
    DI_UINT16   ControlWord;
    DI_UINT16   StatusWord;
    DI_CHAR     TagWord;
    DI_CHAR     Reserved1;
    DI_UINT16   ErrorOpcode;
    DI_UINT32   ErrorOffset;
    DI_UINT16   ErrorSelector;
    DI_UINT16   Reserved2;
    DI_UINT32   DataOffset;
    DI_UINT16   DataSelector;
    DI_UINT16   Reserved3;
    DI_UINT32   MxCsr;
    DI_UINT32   MxCsr_Mask;
    DIANA_M128A_NTLIKE FloatRegisters[8];
    DIANA_M128A_NTLIKE XmmRegisters[16];
    DI_CHAR     Reserved4[96];
} DIANA_XMM_SAVE_AREA32_NTLIKE, *PDIANA_XMM_SAVE_AREA32_NTLIKE;

typedef struct DIANA_DECLSPEC_ALIGN(16) _DIANA_CONTEXT_NTLIKE_64 
{
    //
    // Register parameter home addresses.
    //
    // N.B. These fields are for convience - they could be used to extend the
    //      context record in the future.
    //

    DI_UINT64 P1Home;
    DI_UINT64 P2Home;
    DI_UINT64 P3Home;
    DI_UINT64 P4Home;
    DI_UINT64 P5Home;
    DI_UINT64 P6Home;

    //
    // Control flags.
    //

    DI_UINT32 ContextFlags;
    DI_UINT32 MxCsr;

    //
    // Segment Registers and processor flags.
    //

    DI_UINT16   SegCs;
    DI_UINT16   SegDs;
    DI_UINT16   SegEs;
    DI_UINT16   SegFs;
    DI_UINT16   SegGs;
    DI_UINT16   SegSs;
    DI_UINT32   EFlags;

    //
    // Debug registers
    //

    DI_UINT64 Dr0;
    DI_UINT64 Dr1;
    DI_UINT64 Dr2;
    DI_UINT64 Dr3;
    DI_UINT64 Dr6;
    DI_UINT64 Dr7;

    //
    // Integer registers.
    //

    DI_UINT64 Rax;
    DI_UINT64 Rcx;
    DI_UINT64 Rdx;
    DI_UINT64 Rbx;
    DI_UINT64 Rsp;
    DI_UINT64 Rbp;
    DI_UINT64 Rsi;
    DI_UINT64 Rdi;
    DI_UINT64 R8;
    DI_UINT64 R9;
    DI_UINT64 R10;
    DI_UINT64 R11;
    DI_UINT64 R12;
    DI_UINT64 R13;
    DI_UINT64 R14;
    DI_UINT64 R15;

    //
    // Program counter.
    //

    DI_UINT64 Rip;

    //
    // Floating point state.
    //

    union 
    {
        DIANA_XMM_SAVE_AREA32_NTLIKE FltSave;
        struct 
        {
            DIANA_M128A_NTLIKE Header[2];
            DIANA_M128A_NTLIKE Legacy[8];
            DIANA_M128A_NTLIKE Xmm0;
            DIANA_M128A_NTLIKE Xmm1;
            DIANA_M128A_NTLIKE Xmm2;
            DIANA_M128A_NTLIKE Xmm3;
            DIANA_M128A_NTLIKE Xmm4;
            DIANA_M128A_NTLIKE Xmm5;
            DIANA_M128A_NTLIKE Xmm6;
            DIANA_M128A_NTLIKE Xmm7;
            DIANA_M128A_NTLIKE Xmm8;
            DIANA_M128A_NTLIKE Xmm9;
            DIANA_M128A_NTLIKE Xmm10;
            DIANA_M128A_NTLIKE Xmm11;
            DIANA_M128A_NTLIKE Xmm12;
            DIANA_M128A_NTLIKE Xmm13;
            DIANA_M128A_NTLIKE Xmm14;
            DIANA_M128A_NTLIKE Xmm15;
        };
    };

    //
    // Vector registers.
    //

    DIANA_M128A_NTLIKE VectorRegister[26];
    DI_UINT64 VectorControl;

    //
    // Special debug control registers.
    //

    DI_UINT64 DebugControl;
    DI_UINT64 LastBranchToRip;
    DI_UINT64 LastBranchFromRip;
    DI_UINT64 LastExceptionToRip;
    DI_UINT64 LastExceptionFromRip;
} DIANA_CONTEXT_NTLIKE_64, *PDIANA_CONTEXT_NTLIKE_64;


#pragma warning(pop)

#endif