#ifdef WIN32

namespace orthia
{
typedef diana::CWin32Exception CWin32Exception;


class CCriticalSection
{
    CRITICAL_SECTION  m_section;
    CCriticalSection(const CCriticalSection&);
    CCriticalSection&operator = (const CCriticalSection&);

    bool CreateInternal()
    {
        memset(&m_section, 0, sizeof(m_section));
        bool bResult = false;
        __try
        {
            InitializeCriticalSection(&m_section);
            bResult = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            bResult = false;
        }
        return bResult;
    }

public:
    void Lock()
    {
        EnterCriticalSection(&m_section);
    }
    void Unlock()
    {
        LeaveCriticalSection(&m_section);
    }
    CCriticalSection()
    {
        if (!CreateInternal())
            throw std::bad_alloc();
    }
    ~CCriticalSection()
    {
        DeleteCriticalSection(&m_section);
    }
};


class CHandleGuard
{
    CHandleGuard(const CHandleGuard&);
    CHandleGuard & operator = (const CHandleGuard&);
    HANDLE m_hFile;
public:
    CHandleGuard()
        :
            m_hFile(0)
    {
    }
    explicit CHandleGuard(HANDLE hFile)
        : m_hFile(hFile)
    {
    }
    ~CHandleGuard()
    {
        Reset(0);
    }
    HANDLE Get()
    {
        return m_hFile;
    }
    HANDLE Release()
    {
        HANDLE hFile = m_hFile;
        m_hFile = 0;
        return hFile;
    }
    void Reset(HANDLE hFile)
    {
        if (m_hFile && m_hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hFile);
            m_hFile = 0;
        }
        m_hFile = hFile;
    }
};



std::wstring ToWideString(const std::string & str, UINT codePage = CP_ACP);
std::string ToAnsiString_Silent(const std::wstring & sourceStr,
                                ULONG codePage = CP_ACP);


                                
class CDll
{
    CDll(const CDll&);
    CDll&operator = (const CDll&);
    HMODULE m_hLib;
public:
    CDll();
    explicit CDll(const std::wstring & dllName);
    ~CDll();
    HMODULE GetBase();
    void Reset(const std::wstring & dllName);
    void Reset(const wchar_t * pDllName);
    int Reset_Silent(const wchar_t* pName);

    FARPROC QueryFunctionRaw(const char * pFunctionName, 
                            bool bSilent);
    template<class Type>
    void QueryFunction(const char * pFunctionName, 
                       Type * ppFnc,
                       bool bSilent)
    {
        *ppFnc = (Type)QueryFunctionRaw(pFunctionName, bSilent);
    }
};


inline std::wstring Downcase(const std::wstring & str)
{
    if (str.empty())
        return std::wstring();

    std::vector<wchar_t> temp(str.c_str(), str.c_str() + str.size());
    DWORD dwSize = (DWORD)(str.size());
    if (CharLowerBuffW( &temp.front(), dwSize)!=dwSize)
        throw std::runtime_error("Can't convert string");

    return std::wstring(&temp.front(), &temp.front() + dwSize);
}

inline std::string Downcase_Ansi(const std::string & str)
{
    if (str.empty())
        return std::string();

    std::vector<char> temp(str.c_str(), str.c_str() + str.size());
    DWORD dwSize = (DWORD)(str.size());
    if (CharLowerBuffA( &temp.front(), dwSize)!=dwSize)
        throw std::runtime_error("Can't convert string");

    return std::string(&temp.front(), &temp.front() + dwSize);
}



inline std::string Utf16ToUtf8(const std::wstring & wstr)
{
    return ToAnsiString_Silent(wstr, CP_UTF8);
}
inline std::string Utf16ToAcp(const std::wstring & wstr)
{
    return ToAnsiString_Silent(wstr, CP_ACP);
}
inline std::wstring Utf8ToUtf16(const std::string & str)
{
    return ToWideString(str, CP_UTF8);
}
inline std::wstring AcpToUtf16(const std::string & str)
{
    return ToWideString(str, CP_ACP);
}


HMODULE GetCurrentModule();
std::wstring GetModuleName(HMODULE hModule);
std::wstring GetCurrentModuleDir();


inline
void GetFullPathNameX(const std::wstring & name,
                     std::vector<wchar_t> & nameOut,
                     int iResOffset)
{
    wchar_t * pOut = 0;
    nameOut.resize(1024);
    ULONG dwSize = GetFullPathNameW(name.c_str(), 
                     (DWORD)nameOut.size() - iResOffset, 
                     &nameOut.front() + iResOffset,
                     &pOut
                     );
    if (!dwSize)
    {
        ORTHIA_THROW_WIN32("Invalid path: " + Utf16ToAcp(name));
    }
    nameOut.resize( dwSize + iResOffset );
}


inline std::string PlatformStringToAcp(const std::wstring& wstr)
{
    return ToAnsiString_Silent(wstr, CP_ACP);
}
inline std::string PlatformStringToUtf8(const std::wstring& wstr)
{
    return Utf16ToUtf8(wstr);
}
inline std::wstring Utf8ToPlatformString(const std::string& wstr)
{
    return Utf8ToUtf16(wstr);
}

const wchar_t* QueryModuleVersion(HMODULE module);


}

#endif