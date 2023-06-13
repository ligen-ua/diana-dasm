#include "orthia_utils.h"
#pragma warning(disable:4996)
namespace orthia
{


static int GetWinFolder_Silent(int csidl, PlatformString_type& result)
{
    CDll shell;
    int error = shell.Reset_Silent(L"shell32.dll");
    if (error)
    {
        return error;
    }
    std::vector<wchar_t> dataPath(MAX_PATH + 1);
    dataPath[0] = 0;
    HRESULT (STDAPICALLTYPE* pSHGetFolderPath)(HWND hwnd,
        int csidl, 
        HANDLE hToken, 
        int dwFlags, 
        LPWSTR pszPath) = 0;
    shell.QueryFunction("SHGetFolderPathW", &pSHGetFolderPath, true);
    if (!pSHGetFolderPath)
    {
        return ERROR_INVALID_FUNCTION;
    }
    if (pSHGetFolderPath(NULL,
        csidl,
        0,
        0,
        &dataPath[0]) != S_OK)
    {
        return ERROR_INVALID_FUNCTION;
    }
    result.clear();
    result.append(&dataPath[0]);
    AddSlash(result);
    return 0;
}

int GetAppDataFolderWithSlash_Silent(PlatformString_type& result)
{
    const int cs_CSIDL_APPDATA = 0x001a;
    return GetWinFolder_Silent(cs_CSIDL_APPDATA, result);
}

// systemtime
long long ConvertSystemTimeToFileTime(const SYSTEMTIME * pTime)
{
    FILETIME fileTime = {0,0};
    ::SystemTimeToFileTime(pTime, &fileTime);

    LARGE_INTEGER result;
    result.HighPart = fileTime.dwHighDateTime;
    result.LowPart = fileTime.dwLowDateTime;
    return result.QuadPart;
}
std::wstring SystemTimeToWideString(const SYSTEMTIME & st)
{
    //YYYY-MM-DD HH:MM:SS.SSS 
    wchar_t buffer[64];
    _snwprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), 
        L"%4i-%02i-%02i %02i:%02i:%02i.%03i",
        (int)st.wYear,
        (int)st.wMonth,
        (int)st.wDay,
        (int)st.wHour,
        (int)st.wMinute,
        (int)st.wSecond,
        (int)st.wMilliseconds);
    return buffer;
}
std::wstring SystemTimeToWideStringJustDate(const SYSTEMTIME & st)
{
    //YYYY-MM-DD HH:MM:SS.SSS 
    wchar_t buffer[64];
    _snwprintf(buffer, sizeof(buffer)/sizeof(buffer[0]), 
        L"%4i-%02i-%02i",
        (int)st.wYear,
        (int)st.wMonth,
        (int)st.wDay);
    return buffer;
}

///-----------
std::string StringInfo_Ansi::ToString() const
{
    return std::string(m_pBegin, m_pEnd);
}
const char * StringInfo_Ansi::c_str() const
{
    return m_pBegin;
}
int StringInfo_Ansi::size() const
{
    return (int)(m_pEnd - m_pBegin);
}

int StringInfo_Ansi::find(const char * separator,
                         int startOffset, 
                         int separatorSize) const
{    
    int currentSize = size();

    if (separatorSize == 0 && startOffset <= currentSize)
        return startOffset;    

    typedef std::char_traits<char> _Traits;
    int _Nm;
    if (startOffset < currentSize && separatorSize <= (_Nm = currentSize - startOffset))
    {    // room for match, look for it

        const char *_Uptr, *_Vptr;
        for (_Nm -= separatorSize - 1, _Vptr = m_pBegin + startOffset;
            (_Uptr = _Traits::find(_Vptr, _Nm, *separator)) != 0;
            _Nm -= (int)(_Uptr - _Vptr + 1), _Vptr = _Uptr + 1)
        {

            if (_Traits::compare(_Uptr, separator, separatorSize) == 0)
                return (int)(_Uptr - m_pBegin);    // found a match
        }
    }
    return (npos);    // no match
}

void SplitString(const StringInfo_Ansi & str,
                 const StringInfo_Ansi & separator,
                 std::vector<StringInfo_Ansi> * pInfo)
{
    pInfo->reserve(100);
    pInfo->clear();

    int iSepSize = (int)separator.size();
    int iSearchSize = (int)str.size();

    for(int pos = 0; pos <= iSearchSize; )
    {
        int newPos = str.find(separator.c_str(), pos, iSepSize);
        if (newPos == std::string::npos )
            newPos = str.size();

        pInfo->push_back( StringInfo_Ansi(str.c_str() + pos, 
                                           str.c_str() + newPos));

        pos = (int)newPos + iSepSize;
    }
}

//   StringInfo
std::wstring StringInfo::ToString() const
{
    return std::wstring(m_pBegin, m_pEnd);
}
const wchar_t * StringInfo::c_str() const
{
    return m_pBegin;
}
int StringInfo::size() const
{
    return (int)(m_pEnd - m_pBegin);
}

int StringInfo::find(const wchar_t * separator,
                         int startOffset, 
                         int separatorSize) const
{    
    int currentSize = size();

    if (separatorSize == 0 && startOffset <= currentSize)
        return startOffset;    

    typedef std::char_traits<wchar_t> _Traits;
    int _Nm;
    if (startOffset < currentSize && separatorSize <= (_Nm = currentSize - startOffset))
    {    // room for match, look for it

        const wchar_t *_Uptr, *_Vptr;
        for (_Nm -= separatorSize - 1, _Vptr = m_pBegin + startOffset;
            (_Uptr = _Traits::find(_Vptr, _Nm, *separator)) != 0;
            _Nm -= (int)(_Uptr - _Vptr + 1), _Vptr = _Uptr + 1)
        {

            if (_Traits::compare(_Uptr, separator, separatorSize) == 0)
                return (int)(_Uptr - m_pBegin);    // found a match
        }
    }
    return (npos);    // no match
}

void SplitString(const StringInfo & str,
                 const StringInfo & separator,
                 std::vector<StringInfo> * pInfo)
{
    pInfo->reserve(100);
    pInfo->clear();

    int iSepSize = (int)separator.size();
    int iSearchSize = (int)str.size();

    for(int pos = 0; pos <= iSearchSize; )
    {
        int newPos = str.find(separator.c_str(), pos, iSepSize);
        if (newPos == std::string::npos )
            newPos = str.size();

        pInfo->push_back( StringInfo(str.c_str() + pos, 
                                           str.c_str() + newPos));

        pos = (int)newPos + iSepSize;
    }
}


std::wstring ExpandVariable(const std::wstring & possibleVar)
{
    std::vector<wchar_t> buf(1024);
    std::wstring prevResult;
    const int tryCount = 10;
    for(int i = 0; i < tryCount; ++i)
    {
        DWORD size = ExpandEnvironmentStringsW(possibleVar.c_str(), &buf.front(), (DWORD)buf.size());
        if (!size)
        {
            return possibleVar;
        }
        if (size > (DWORD)buf.size())
        {
            buf.resize(size*2);
            continue;
        }
        std::wstring result(&buf.front());
        if (result.find_first_of(L'%') == result.npos)
            return result;
        if (prevResult == result)
            return std::wstring();
        prevResult = result;
    }
    return prevResult;
}

Address_type ToAddress(const std::wstring & sourceStr)
{
    wchar_t * pEndStr = const_cast<wchar_t*>(sourceStr.c_str() + sourceStr.size());
    wchar_t * pResStr = pEndStr;
    Address_type res = _wcstoui64(sourceStr.c_str(), &pEndStr, 16);
    if (pEndStr != pEndStr)
    {
        throw std::runtime_error("Invalid address: " + ToAnsiString_Silent(sourceStr));
    }
    return res;
}
std::wstring ToWideString(Address_type address)
{
    wchar_t buf[64];
    _ui64tow(address, buf, 16);
    return buf;
}
std::wstring ToWideString(const std::string & str, UINT codePage)
{
    if(str.empty())
    {
        return std::wstring();
    }
    std::vector<wchar_t> tmp;
    tmp.resize(str.size() + 1);
    for(;;)
    {
        int size = MultiByteToWideChar(codePage,
                                       0,
                                       str.c_str(),
                                       (int)str.size(), 
                                       &tmp.front(), 
                                       (int)tmp.size());
        if(size)
        {
            return std::wstring(tmp.begin(), tmp.begin()+size);   
        }
        DWORD error = ::GetLastError();
        if(error != ERROR_INSUFFICIENT_BUFFER)
        {
            throw CWin32Exception("Can't convert: [" + str + "]", error);
        }
        size = MultiByteToWideChar(codePage, 
                                   0, 
                                   str.c_str(), 
                                   (int)str.size(), 
                                   0, 
                                   0);
        if(!size)
        {
            throw CWin32Exception("Can't convert: [" + str + "]", ::GetLastError());
        }
        tmp.resize(size*2);
    }
}

std::string ToAnsiString_Silent(const std::wstring & str,
                                ULONG codePage)
{
    if(str.empty())
    {
        return std::string();
    }
    std::vector<char> tmp;
    tmp.resize(str.size() + 1);
    for(;;)
    {
        int size = WideCharToMultiByte(codePage,
                                       0,
                                       str.c_str(),
                                       (int)str.size(), 
                                       &tmp.front(), 
                                       (int)tmp.size(),
                                       0,
                                       0);
        if(size)
        {
            return std::string(tmp.begin(), tmp.begin()+size);   
        }
        DWORD error = ::GetLastError();
        if(error != ERROR_INSUFFICIENT_BUFFER)
        {
            return std::string();
        }
        size = WideCharToMultiByte(codePage, 
                                   0, 
                                   str.c_str(), 
                                   (int)str.size(), 
                                   0, 
                                   0,
                                   0,
                                   0);
        if(!size)
        {
            return std::string();
        }
        tmp.resize(size*2);
    }
}

static wchar_t g_hexChars[] = L"0123456789abcdef";

std::wstring ToHexString(const char * pArray, 
                         size_t size)
{
    std::wstring res;
    res.reserve(size*2);
    for(size_t i = 0; i < size; ++i)
    {
        unsigned char item = (unsigned char )(pArray[i]);
        res.push_back(g_hexChars[item >> 4]);
        res.push_back(g_hexChars[item &0xF]);
    }
    return res;
}

bool IsFileExist(const std::wstring & fullFileName)
{
    return ::GetFileAttributes(fullFileName.c_str()) != INVALID_FILE_ATTRIBUTES;
}
Address_type GetSizeOfFile(const std::wstring & fullFileName)
{
    HANDLE hFile = CreateFile(fullFileName.c_str(), 
                              GENERIC_READ, 
                              FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, 
                              0, 
                              OPEN_EXISTING, 
                              0, 
                              0);
    if (hFile == INVALID_HANDLE_VALUE)
        ORTHIA_THROW_WIN32("Can't open file: "<<orthia::ToAnsiString_Silent(fullFileName));

    CHandleGuard guard(hFile);

    LARGE_INTEGER size;
    size.QuadPart = 0;
    if (!::GetFileSizeEx(hFile, &size))
    {
        ORTHIA_THROW_WIN32("Can't get file size: "<<orthia::ToAnsiString_Silent(fullFileName));
    }
    return size.QuadPart;
}

void CreateAllDirectoriesForFile(const std::wstring & fullFileName)
{
    if (fullFileName.size() > 3)
    {
        // skip UNC paths and 
        if (wcsncmp(fullFileName.c_str(), L"\\\\", 2) == 0)
        {
            if (fullFileName[2] != L'?' &&
                fullFileName[2] != L'.') 
            {
                return;
            }
        }
    }
    for(std::wstring::const_iterator it = fullFileName.begin(), it_end = fullFileName.end();
        it != it_end;
        ++it)
    {
        if (*it == L'/' || *it == L'\\')
        {
            std::wstring path(fullFileName.begin(), it+1);
            CreateDirectory(path.c_str(), 0);
        }
    }
}

CDll::CDll()
    :
    m_hLib(0)
{
}
CDll::CDll(const std::wstring & name)
    :
        m_hLib(0)
{
    Reset(name);
}
CDll::~CDll()
{
    Reset(0);
}
HMODULE CDll::GetBase()
{
    return m_hLib;
}
void CDll::Reset(const std::wstring & name)
{
    Reset(name.empty()?0:name.c_str());
}

void CDll::Reset(const wchar_t * pName)
{
    if (m_hLib)
    {
        FreeLibrary(m_hLib);
        m_hLib = 0;
    }
    if (!pName)
        return;

    m_hLib = LoadLibraryW(pName);
    if (!m_hLib)
    {
        ORTHIA_THROW_WIN32("Can't load: "<<orthia::ToAnsiString_Silent(pName)<<", code: "<<orthia____code);
    }
}
int CDll::Reset_Silent(const wchar_t* pName)
{
    if (m_hLib)
    {
        FreeLibrary(m_hLib);
        m_hLib = 0;
    }
    if (!pName)
    {
        return 0;
    }
    m_hLib = LoadLibraryW(pName);
    if (!m_hLib)
    {
        return GetLastError();
    }
    return 0;
}
FARPROC CDll::QueryFunctionRaw(const char * pFunctionName, 
                              bool bSilent)
{
    FARPROC pProc = GetProcAddress(m_hLib, pFunctionName);
    if (bSilent)
        return pProc;
    if (!pProc)
    {
        ORTHIA_THROW_WIN32("Can't find: "<<pFunctionName<<", code: "<<orthia____code);
    }
    return pProc;
}

HMODULE GetCurrentModule()
{
    MEMORY_BASIC_INFORMATION mbi = {0}; 
    static int address; 
    VirtualQuery(&address, &mbi, sizeof(mbi)); 
    return (HMODULE)mbi.AllocationBase; 
}

std::wstring GetModuleName(HMODULE hModule)
{
    wchar_t data[1025];
    data[1024] = 0;
    DWORD res = GetModuleFileNameW(hModule, data, 1024);
    if (!res)
    {
         ORTHIA_THROW_WIN32("GetModuleFileName failed");
    }
    data[res] = 0;
    return data;
}
std::wstring GetCurrentModuleDir()
{
    std::wstring module = GetModuleName(GetCurrentModule());
    EraseName(module);
    return module;
}


// orthia_pcg32_random
orthia_pcg32_random::orthia_pcg32_random()
    :
        state(0), inc(0)
{
}

void orthia_pcg32_random::swap(orthia_pcg32_random & other)
{
    std::swap(state, other.state);
    std::swap(inc, other.inc);
}
unsigned int orthia_pcg32_random::gen()
{
    return orthia_pcg32_random_r(this);
}
unsigned int orthia_pcg32_random_r(orthia_pcg32_random * rng)
{
    unsigned long long oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    unsigned int xorshifted = (unsigned int)(((oldstate >> 18u) ^ oldstate) >> 27u);
    unsigned int rot = (unsigned int)(oldstate >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((0-rot) & 31));
}

static
BOOL VerParseTranslationID(LPVOID lpData, UINT unBlockSize, WORD wLangId, DWORD* pdwId, BOOL bPrimaryEnough/*= FALSE*/)
{
    LPWORD lpwData;

    for (lpwData = (LPWORD)lpData; (LPBYTE)lpwData < ((LPBYTE)lpData) + unBlockSize; lpwData += 2) {
        if (*lpwData == wLangId) {
            *pdwId = *((DWORD*)lpwData);
            return TRUE;
        }
    }

    if (!bPrimaryEnough) {
        return FALSE;
    }

    for (lpwData = (LPWORD)lpData; (LPBYTE)lpwData < ((LPBYTE)lpData) + unBlockSize; lpwData += 2) {
        if (((*lpwData) & 0x00FF) == (wLangId & 0x00FF)) {
            *pdwId = *((DWORD*)lpwData);
            return TRUE;
        }
    }

    return FALSE;
}

typedef BOOL
(APIENTRY
    * VerQueryValueW_type)(
        _In_ LPCVOID pBlock,
        _In_ LPCWSTR lpSubBlock,
        _Outptr_result_buffer_(_Inexpressible_("buffer can be PWSTR or DWORD*")) LPVOID* lplpBuffer,
        _Out_ PUINT puLen
        );
const wchar_t* QueryModuleVersion(HMODULE module)
{
    HMODULE hVersionDll = 0;
    HRSRC hRsrc = 0;
    HGLOBAL hGlobal = 0;
    void* pData = 0;
    DWORD size = 0;
    LPVOID lpInfo = 0;
    UINT unInfoLen = 0;
    DWORD dwLangCode = 0;
    DWORD dwCodePage = 0;
    const wchar_t* pResult = 0;
    VerQueryValueW_type verQueryValueW = 0;
    wchar_t textBuffer[256];
    LPVOID pFileVersion = 0;

    struct LangCodePage
    {
        WORD wLanguage;
        WORD wCodePage;
    } *lcp = 0;
    UINT len = 0;

    hVersionDll = LoadLibraryW(L"version.dll");
    if (!hVersionDll)
    {
        goto cleanup;
    }

    verQueryValueW = (VerQueryValueW_type)GetProcAddress(hVersionDll, "VerQueryValueW");

    hRsrc = FindResourceExW(module, MAKEINTRESOURCEW(16), MAKEINTRESOURCEW(1), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
    if (hRsrc == NULL)
    {
        hRsrc = FindResourceW(module, MAKEINTRESOURCEW(VS_VERSION_INFO), (LPWSTR)VS_FILE_INFO);
    }
    if (hRsrc == NULL)
        goto cleanup;
    hGlobal = LoadResource(module, hRsrc);
    if (hGlobal == NULL)
        goto cleanup;

    pData = LockResource(hGlobal);
    if (pData == NULL)
        goto cleanup;

    size = SizeofResource(module, hRsrc);
    lpInfo = pData;
    unInfoLen = size;
    dwLangCode = 0;
    dwCodePage = 0;

    // Get the string file info

    if (verQueryValueW(pData, L"\\VarFileInfo\\Translation", (LPVOID*)(&lcp), &len) &&
        len >= 4)
    {
        dwLangCode = lcp->wLanguage;
        dwCodePage = lcp->wCodePage;
    }
    else
    {
        if (!VerParseTranslationID(lpInfo, unInfoLen, GetUserDefaultLangID(), &dwLangCode, FALSE))
        {
            if (!VerParseTranslationID(lpInfo, unInfoLen, GetUserDefaultLangID(), &dwLangCode, TRUE))
            {
                if (!VerParseTranslationID(lpInfo, unInfoLen, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), &dwLangCode, TRUE))
                {
                    if (!VerParseTranslationID(lpInfo, unInfoLen, MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), &dwLangCode, TRUE))
                    {
                        dwLangCode = *((DWORD*)lpInfo);
                    }
                }
            }
        }
        dwCodePage = (dwLangCode & 0xFFFF0000) >> 16;
        dwLangCode &= 0x0000FFFF;
    }

    _snwprintf_s(textBuffer, 256, 256, L"\\StringFileInfo\\%04X%04X\\", dwLangCode, dwCodePage);
    wcscat_s(textBuffer, 256, L"FileVersion");
    if (verQueryValueW(pData, textBuffer, &pFileVersion, &len))
    {
        pResult = (wchar_t*)pFileVersion;
    }
cleanup:
    if (pData)
    {
        UnlockResource(pData);
    }
    if (hVersionDll)
    {
        FreeLibrary(hVersionDll);
    }
    return pResult;
}

// whitespace
bool IsSpace(ORTHIA_TCHAR symbol)
{
    return symbol == L' ';
}
bool IsWhiteSpace(ORTHIA_TCHAR symbol)
{
    switch (symbol)
    {
    case L' ':
    case 9:
    case 10:
    case 13:
        return true;
    }
    return false;
}
bool IsWhiteSpace_Ansi(char symbol)
{
    switch (symbol)
    {
    case ' ':
    case 9:
    case 10:
    case 13:
        return true;
    }
    return false;
}
bool IsFileNameSeparator(ORTHIA_TCHAR ch)
{
    return (ch == L'\\' || ch == L'/');
}
bool IsEOL(ORTHIA_TCHAR symbol)
{
    switch (symbol)
    {
    case 10:
    case 13:
        return true;
    }
    return false;
}
bool IsEOL_Ansi(char symbol)
{
    switch (symbol)
    {
    case 10:
    case 13:
        return true;
    }
    return false;
}

void AddSlash(PlatformString_type& str)
{
    EraseLastSlash(str);
    str.append(1, ORTHIA_SYM_PLATFORM_SLASH);
}
PlatformString_type AddSlash2(const PlatformString_type& str)
{
    PlatformString_type copy(str);
    AddSlash(copy);
    return copy;
}

void EraseLastSlash(PlatformString_type& str)
{
    for (;;)
    {
        if (str.empty())
            return;

        if (!IsFileNameSeparator(*str.rbegin()))
            break;

        str.resize(str.size() - 1);
    }
}

void TrimString(std::wstring& str)
{
    TrimStringIf(str, IsSpace);
}
void TrimString(std::string& str)
{
    TrimStringIf(str, IsSpace);
}
std::wstring TrimString2(const std::wstring& str)
{
    std::wstring str2(str);
    TrimString(str2);
    return str2;
}
std::string TrimString2(const std::string& str)
{
    std::string str2(str);
    TrimString(str2);
    return str2;
}
int TrimStringAllWhiteSpace(std::wstring& str)
{
    return TrimStringIf(str, IsWhiteSpace);
}
int TrimStringAllWhiteSpace(std::string& str)
{
    return TrimStringIf(str, IsWhiteSpace_Ansi);
}

void SplitStringWithoutWhitespace(const StringInfo& str,
    const StringInfo& separator,
    std::set<orthia::PlatformString_type>* pInfo)
{
    std::vector<StringInfo> info;
    SplitString(str,
        separator,
        &info);

    for (std::vector<StringInfo>::iterator it = info.begin(), it_end = info.end();
        it != it_end;
        ++it)
    {
        orthia::PlatformString_type tmp = it->ToString();
        orthia::TrimStringAllWhiteSpace(tmp);
        if (tmp.empty())
            continue;
        pInfo->insert(tmp);
    }
}

void SplitStringWithoutWhitespace(const StringInfo& str_in,
    const StringInfo& separator,
    std::vector<orthia::PlatformString_type>* pInfo)
{
    orthia::PlatformString_type str(str_in.ToString());
    orthia::TrimStringAllWhiteSpace(str);
    std::vector<StringInfo> info;
    SplitString(str,
        separator,
        &info);

    for (std::vector<StringInfo>::iterator it = info.begin(), it_end = info.end();
        it != it_end;
        ++it)
    {
        orthia::PlatformString_type tmp = it->ToString();
        orthia::TrimStringAllWhiteSpace(tmp);
        if (tmp.empty())
            continue;
        pInfo->push_back(tmp);
    }

}

}