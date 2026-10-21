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
        Data
    };
    const int g_database_type_armCodeMap = 5;
    struct ArmCodeChange
    {
        DI_UINT64 offset;           // in the module
        ArmCodeKind kind;
    };
    // "a32", "t32", "data"
    const char* GetArmCodeKindName(ArmCodeKind kind);
    bool FindArmCodeKind(const std::string& name, ArmCodeKind* pKind);
    // the kind of a module of the architecture: T32 for DIANA_ARCH_THUMB, A32 otherwise
    ArmCodeKind GetArmCodeKind(int dianaArch);
    bool IsDianaArchArm32(int dianaArch);
}
