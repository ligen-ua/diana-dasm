#include "orthia_utils.h"
namespace orthia
{

Address_type ToAddress(const orthia::PlatformString_type & sourceStr)
{
    ORTHIA_TCHAR * pEndStr = const_cast<ORTHIA_TCHAR*>(sourceStr.c_str() + sourceStr.size());
    ORTHIA_TCHAR * pResStr = pEndStr;
    Address_type res = ORTHIA_STR_TO_ULL(sourceStr.c_str(), &pEndStr, 16);
    if (pEndStr != pEndStr)
    {
        throw std::runtime_error("Invalid address: " + PlatformStringToUtf8(sourceStr));
    }
    return res;
}

orthia::PlatformString_type SystemTimeToString(const orthia::WinSystemTime_type & st)
{
    //YYYY-MM-DD HH:MM:SS.SSS 
    ORTHIA_TCHAR buffer[64];
    ORTHIA_SNTPRINTF(buffer, sizeof(buffer)/sizeof(buffer[0]), 
        ORTHIA_TCSTR("%4i-%02i-%02i %02i:%02i:%02i.%03i"),
        (int)st.wYear,
        (int)st.wMonth,
        (int)st.wDay,
        (int)st.wHour,
        (int)st.wMinute,
        (int)st.wSecond,
        (int)st.wMilliseconds);
    return buffer;
}
orthia::PlatformString_type SystemTimeToStringJustDate(const orthia::WinSystemTime_type & st)
{
    //YYYY-MM-DD HH:MM:SS.SSS 
    ORTHIA_TCHAR buffer[64];
    ORTHIA_SNTPRINTF(buffer, sizeof(buffer)/sizeof(buffer[0]), 
        ORTHIA_TCSTR("%4i-%02i-%02i"),
        (int)st.wYear,
        (int)st.wMonth,
        (int)st.wDay);
    return buffer;
}

//   StringInfo
PlatformString_type StringInfo::ToString() const
{
    return PlatformString_type(m_pBegin, m_pEnd);
}
const ORTHIA_TCHAR * StringInfo::c_str() const
{
    return m_pBegin;
}
int StringInfo::size() const
{
    return (int)(m_pEnd - m_pBegin);
}

int StringInfo::find(const ORTHIA_TCHAR * separator,
                         int startOffset, 
                         int separatorSize) const
{    
    int currentSize = size();

    if (separatorSize == 0 && startOffset <= currentSize)
        return startOffset;    

    typedef std::char_traits<ORTHIA_TCHAR> _Traits;
    int _Nm;
    if (startOffset < currentSize && separatorSize <= (_Nm = currentSize - startOffset))
    {    // room for match, look for it

        const ORTHIA_TCHAR *_Uptr, *_Vptr;
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