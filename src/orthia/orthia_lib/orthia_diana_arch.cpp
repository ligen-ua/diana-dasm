#include "orthia_diana_arch.h"
extern "C"
{
#include "diana_pe_defs.h"
#include "diana_elfs_defs.h"
}

namespace orthia
{
    bool FindDianaArchOfPeMachine(int machine, int* pDianaMode, int* pDianaArch)
    {
        switch (machine)
        {
        case DIANA_IMAGE_FILE_MACHINE_I386:
            *pDianaMode = DIANA_MODE32;
            *pDianaArch = DIANA_ARCH_X86;
            return true;
        case DIANA_IMAGE_FILE_MACHINE_AMD64:
            *pDianaMode = DIANA_MODE64;
            *pDianaArch = DIANA_ARCH_X86;
            return true;
        case DIANA_IMAGE_FILE_MACHINE_ARM64:
            *pDianaMode = DIANA_MODE64;
            *pDianaArch = DIANA_ARCH_AARCH64;
            return true;
        // Windows runs ARM32 code in Thumb only, as the PE loader takes these images
        case DIANA_IMAGE_FILE_MACHINE_ARMNT:
        case DIANA_IMAGE_FILE_MACHINE_THUMB:
            *pDianaMode = DIANA_MODE32;
            *pDianaArch = DIANA_ARCH_THUMB;
            return true;
        }
        return false;
    }

    int SelectWin32ProcessMachine(int processMachineInfo, int wow64Machine, int nativeMachine)
    {
        if (processMachineInfo)
        {
            return processMachineInfo;
        }
        if (wow64Machine)
        {
            return wow64Machine;
        }
        // not WOW: the process is native. Without ProcessMachineTypeInfo an x64 process emulated on ARM64
        // looks native too, its modules still get their own machines from their headers
        return nativeMachine;
    }

    bool FindDianaArchOfElfHeader(const unsigned char* pHeader, size_t size, int* pDianaMode, int* pDianaArch)
    {
        // e_ident (16 bytes), e_type (2), e_machine (2): the same offsets in both classes
        const size_t machineOffset = 18;
        if (size < machineOffset + 2)
        {
            return false;
        }
        if (pHeader[0] != 0x7f || pHeader[1] != 'E' || pHeader[2] != 'L' || pHeader[3] != 'F')
        {
            return false;
        }
        int mode = 0;
        switch (pHeader[DIANA_EI_CLASS])
        {
        case DIANA_ELFCLASS32:
            mode = DIANA_MODE32;
            break;
        case DIANA_ELFCLASS64:
            mode = DIANA_MODE64;
            break;
        default:
            return false;
        }
        int machine = 0;
        switch (pHeader[DIANA_EI_DATA])
        {
        case DIANA_ELFDATA2LSB:
            machine = pHeader[machineOffset] | (pHeader[machineOffset + 1] << 8);
            break;
        case DIANA_ELFDATA2MSB:
            machine = (pHeader[machineOffset] << 8) | pHeader[machineOffset + 1];
            break;
        default:
            return false;
        }
        int arch = 0;
        switch (machine)
        {
        case DIANA_EM_386:
        case DIANA_EM_X86_64:
            arch = DIANA_ARCH_X86;
            break;
        case DIANA_EM_AARCH64:
            arch = DIANA_ARCH_AARCH64;
            break;
        // A32 or T32 is decided per module and per route, as for the ELF files
        case DIANA_EM_ARM:
            arch = DIANA_ARCH_ARM32;
            break;
        default:
            return false;
        }
        *pDianaMode = mode;
        *pDianaArch = arch;
        return true;
    }

    const std::vector<DianaArchChoice>& GetDianaArchChoices()
    {
        static const std::vector<DianaArchChoice> choices = {
            { "x86",   DIANA_MODE32, DIANA_ARCH_X86 },
            { "x64",   DIANA_MODE64, DIANA_ARCH_X86 },
            { "arm64", DIANA_MODE64, DIANA_ARCH_AARCH64 },
            { "arm",   DIANA_MODE32, DIANA_ARCH_ARM32 },
            { "thumb", DIANA_MODE32, DIANA_ARCH_THUMB },
        };
        return choices;
    }

    const DianaArchChoice* FindDianaArchChoice(const std::string& name)
    {
        for (auto& choice : GetDianaArchChoices())
        {
            if (name == choice.name)
            {
                return &choice;
            }
        }
        return nullptr;
    }

    const DianaArchChoice* FindDianaArchChoice(int dianaMode, int dianaArch)
    {
        for (auto& choice : GetDianaArchChoices())
        {
            if (choice.dianaMode == dianaMode && choice.dianaArch == dianaArch)
            {
                return &choice;
            }
        }
        return nullptr;
    }

    std::string GetDianaArchChoiceNames()
    {
        std::string names;
        for (auto& choice : GetDianaArchChoices())
        {
            if (!names.empty())
            {
                names += ", ";
            }
            names += choice.name;
        }
        return names;
    }

    void InitDianaContext(DianaContext* pContext, int dianaMode, int dianaArch)
    {
        Diana_InitContextEx(pContext, dianaMode, dianaArch);
    }

    const char* GetArmCodeKindName(ArmCodeKind kind)
    {
        switch (kind)
        {
        case ArmCodeKind::T32:
            return "t32";
        case ArmCodeKind::Data:
            return "data";
        case ArmCodeKind::Data16:
            return "data16";
        case ArmCodeKind::Data8:
            return "data8";
        default:
            return "a32";
        }
    }

    bool IsArmCodeKindData(ArmCodeKind kind)
    {
        return kind == ArmCodeKind::Data || kind == ArmCodeKind::Data16 || kind == ArmCodeKind::Data8;
    }

    int GetArmCodeKindDataSize(ArmCodeKind kind)
    {
        return kind == ArmCodeKind::Data8 ? 1 : kind == ArmCodeKind::Data16 ? 2 : 4;
    }

    bool FindArmCodeKind(const std::string& name, ArmCodeKind* pKind)
    {
        const ArmCodeKind kinds[] = { ArmCodeKind::A32, ArmCodeKind::T32, ArmCodeKind::Data, ArmCodeKind::Data16,
            ArmCodeKind::Data8 };
        for (auto kind : kinds)
        {
            if (name == GetArmCodeKindName(kind))
            {
                *pKind = kind;
                return true;
            }
        }
        return false;
    }

    ArmCodeKind GetArmCodeKind(int dianaArch)
    {
        return dianaArch == DIANA_ARCH_THUMB ? ArmCodeKind::T32 : ArmCodeKind::A32;
    }

    bool IsDianaArchArm32(int dianaArch)
    {
        return dianaArch == DIANA_ARCH_ARM32 || dianaArch == DIANA_ARCH_THUMB;
    }

    DI_UINT64 GetDianaInstructionAddress(DI_UINT64 address, int dianaArch)
    {
        return IsDianaArchArm32(dianaArch) ? address & ~(DI_UINT64)1 : address;
    }

    int GetDianaInstructionAlignment(int dianaArch)
    {
        switch (dianaArch)
        {
        case DIANA_ARCH_AARCH64:
        case DIANA_ARCH_ARM32:
            return 4;
        case DIANA_ARCH_THUMB:
            return 2;
        default:
            return 1;
        }
    }
}
