#include "orthia_diana_arch.h"

namespace orthia
{
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
        default:
            return "a32";
        }
    }

    bool FindArmCodeKind(const std::string& name, ArmCodeKind* pKind)
    {
        const ArmCodeKind kinds[] = { ArmCodeKind::A32, ArmCodeKind::T32, ArmCodeKind::Data };
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
