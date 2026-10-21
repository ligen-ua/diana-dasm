#pragma once

#include <string>
#include <vector>
extern "C"
{
#include "diana_core.h"
}

namespace orthia
{
    // an architecture the user can name: --arch on the command line, the "open as shellcode" dialog
    struct DianaArchChoice
    {
        const char* name;       // "x86", "x64", "arm64", "arm", "thumb"
        int dianaMode;          // the pointer size, DIANA_MODE32 / DIANA_MODE64
        int dianaArch;          // DIANA_ARCH_*
    };

    // in the order they are offered
    const std::vector<DianaArchChoice>& GetDianaArchChoices();
    // nullptr if the name is unknown; names are lower case
    const DianaArchChoice* FindDianaArchChoice(const std::string& name);
    // the choice of a mode and an architecture, nullptr if there is none (16-bit x86)
    const DianaArchChoice* FindDianaArchChoice(int dianaMode, int dianaArch);
    // "x86, x64, arm64, arm, thumb", for messages
    std::string GetDianaArchChoiceNames();

    // The architecture of a live process. The functions take what the OS reports and touch nothing, so the
    // rules are tested on any host: an ARM64 process is only seen on an ARM machine

    // a PE machine (IMAGE_FILE_MACHINE_*): I386, AMD64, ARM64, ARMNT/THUMB; false for any other
    bool FindDianaArchOfPeMachine(int machine, int* pDianaMode, int* pDianaArch);
    // the machine a Windows process runs as, 0 if nothing is known (Windows without IsWow64Process2):
    //  processMachineInfo  ProcessMachineTypeInfo of GetProcessInformation (Windows 11), 0 if unavailable;
    //                      the only one that tells an x64 process emulated on ARM64 from a native one
    //  wow64Machine        processMachine of IsWow64Process2: the machine of a WOW process, 0 for the others
    //  nativeMachine       nativeMachine of IsWow64Process2, 0 if unavailable
    int SelectWin32ProcessMachine(int processMachineInfo, int wow64Machine, int nativeMachine);
    // the ELF header of the process image (/proc/<pid>/exe): the class gives the mode, e_machine the
    // architecture (EM_386, EM_X86_64, EM_AARCH64, EM_ARM); false if it is not ELF, too short or another machine
    bool FindDianaArchOfElfHeader(const unsigned char* pHeader, size_t size, int* pDianaMode, int* pDianaArch);

    // a decoder context for the architecture, every feature allowed
    void InitDianaContext(DianaContext* pContext, int dianaMode, int dianaArch);
    // where instructions can start: 1 for x86, 4 for AArch64 and ARM, 2 for Thumb
    int GetDianaInstructionAlignment(int dianaArch);

    // AArch32 code mixes A32, T32 and data (literals, tables). The analysis saves what the bytes are from
    // every address where that changes: metainfo of g_database_type_armCodeMap at the address, the text is
    // the kind's name. Before the first change the module's own architecture applies
    enum class ArmCodeKind
    {
        A32,
        T32,
        Data,           // words: literals, tables of addresses
        Data16,         // halfwords: a tbh table
        Data8           // bytes: a tbb table
    };
    const int g_database_type_armCodeMap = 5;
    struct ArmCodeChange
    {
        DI_UINT64 offset;           // in the module
        ArmCodeKind kind;
    };
    // "a32", "t32", "data", "data16", "data8"
    const char* GetArmCodeKindName(ArmCodeKind kind);
    // Data, Data16 or Data8
    bool IsArmCodeKindData(ArmCodeKind kind);
    // the size of the entries of data: 4, 2 or 1
    int GetArmCodeKindDataSize(ArmCodeKind kind);
    bool FindArmCodeKind(const std::string& name, ArmCodeKind* pKind);
    // the kind of a module of the architecture: T32 for DIANA_ARCH_THUMB, A32 otherwise
    ArmCodeKind GetArmCodeKind(int dianaArch);
    bool IsDianaArchArm32(int dianaArch);
    // where the instruction of a code address is: AArch32 addresses of T32 code have bit 0 set (an ELF
    // function symbol, an entry point, a PE export), the instruction starts at the even address
    DI_UINT64 GetDianaInstructionAddress(DI_UINT64 address, int dianaArch);
}
