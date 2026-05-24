#include "orthia_shellcode.h"

namespace orthia
{
    CSimpleShellcodeFile::CSimpleShellcodeFile(const std::vector<char>& data, DI_UINT64 base, int dianaMode)
        : m_data(data), m_imageBase(base), m_dianaMode(dianaMode)
    {
    }

    DI_UINT64 CSimpleShellcodeFile::GetImageBase() const
    {
        return m_imageBase;
    }

    DI_UINT64 CSimpleShellcodeFile::GetImageEnd() const
    {
        if (m_data.empty())
            return m_imageBase;
        return m_imageBase + (DI_UINT64)(m_data.size() - 1);
    }

    const std::vector<char>& CSimpleShellcodeFile::GetMappedFile() const
    {
        return m_data;
    }

    void CSimpleShellcodeFile::Relocate(OPERAND_SIZE newAddress)
    {
        m_imageBase = newAddress;
    }

    DI_UINT64 CSimpleShellcodeFile::DiGetProcAddress(const char*, OPERAND_SIZE*, DI_UINT16)
    {
        return 0;
    }

    int CSimpleShellcodeFile::GetDianaMode() const
    {
        return m_dianaMode;
    }

    DI_UINT64 CSimpleShellcodeFile::GetEntryPoint() const
    {
        return m_imageBase;
    }

    int CSimpleShellcodeFile::QueryImports(diana::CBasePeLinkImportsObserver*)
    {
        return 0;
    }

    int CSimpleShellcodeFile::QueryExports(diana::CBasePeLinkImportsObserver*)
    {
        return 0;
    }
}
