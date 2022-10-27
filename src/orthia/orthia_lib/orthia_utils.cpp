#include "orthia_utils.h"
#pragma warning(disable:4996)
namespace orthia
{
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

}