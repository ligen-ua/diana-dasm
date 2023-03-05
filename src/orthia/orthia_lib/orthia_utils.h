#ifndef ORTHIA_UTILS_H
#define ORTHIA_UTILS_H

#include "vector"
#include "set"
#include "stdexcept"
#include "sstream"
#include "algorithm"
#include "map"
#include "iomanip"
#include "windows.h"

#include "orthia_pointers.h"
#include "orthia_sha1.h"
#include "diana_win32_cpp.h"

namespace orthia
{
typedef ULONG64 Address_type;
typedef diana::CWin32Exception CWin32Exception;

class CHandleGuard
{
    CHandleGuard(const CHandleGuard&);
    CHandleGuard & operator = (const CHandleGuard&);
    HANDLE m_hFile;
public:
    explicit CHandleGuard(HANDLE hFile)
        : m_hFile(hFile)
    {
    }
    ~CHandleGuard()
    {
        CloseHandle(m_hFile);
    }
};
#define ORTHIA_THROW_WIN32(Text) { ULONG orthia____code = ::GetLastError(); std::stringstream orthia____stream; orthia____stream<<Text; throw orthia::CWin32Exception(orthia____stream.str(), orthia____code);} 
#define ORTHIA_THROW_STD(Text) { std::stringstream orthia____stream; orthia____stream<<Text; throw std::runtime_error(orthia____stream.str());} 


bool IsFileExist(const std::wstring & fullFileName);
Address_type GetSizeOfFile(const std::wstring & fullFileName);
void CreateAllDirectoriesForFile(const std::wstring & fullFileName);

std::wstring ExpandVariable(const std::wstring & possibleVar);
std::wstring ToWideString(const std::string & str, UINT codePage = CP_ACP);
std::wstring ToWideString(Address_type address);

Address_type ToAddress(const std::wstring & sourceStr);
std::string ToAnsiString_Silent(const std::wstring & sourceStr,
                                ULONG codePage = CP_ACP);

template<class CharType>
struct CharTraits
{
};
template<>
struct CharTraits<char>
{
    static const char space = ' ';
    static const char zero = '0';
};
template<>
struct CharTraits<wchar_t>
{
    static const wchar_t space = L' ';
    static const wchar_t zero = L'0';
};

template<class Type, class OutType, template<class> class Traits, template<class> class AllocatorType>
bool HexStringToObject_Silent(const std::basic_string<Type, Traits<Type>, AllocatorType<Type> > & str, OutType * pObject)
{
    typedef std::basic_string<Type, Traits<Type>, AllocatorType<Type> > StringType;

    std::basic_stringstream<Type> stream;
    stream << str;
    std::hex(stream);
    stream >> *pObject;
    if (stream.fail() || stream.bad() || !stream.eof())
        return false;
    return true;
}

inline 
bool HexStringToObject_Silent(const std::wstring & str, long long * pObject)
{
    std::wstringstream stream;
    stream << str;
    std::hex(stream);
    unsigned long long temp = 0;
    stream >> temp;
    if (stream.fail() || stream.bad() || !stream.eof())
        return false;
    *pObject = temp;
    return true;
}
inline 
bool HexStringToObject_Silent(const std::string & str, long long * pObject)
{
    std::stringstream stream;
    stream << str;
    std::hex(stream);
    unsigned long long temp = 0;
    stream >> temp;
    if (stream.fail() || stream.bad() || !stream.eof())
        return false;
    *pObject = temp;
    return true;
}

template<class Type, class OutType, template<class> class Traits, template<class> class AllocatorType>
void HexStringToObject(const std::basic_string<Type, Traits<Type>, AllocatorType<Type> > & str, OutType * pObject)
{
    if (!HexStringToObject_Silent(str, pObject))
    {
        throw std::runtime_error("Can't convert");
    }
}


template<class Type, class OutType, template<class> class Traits, template<class> class AllocatorType>
void StringToObject(const std::basic_string<Type, Traits<Type>, AllocatorType<Type> > & str, OutType * pObject)
{
    typedef std::basic_string<Type, Traits<Type>, AllocatorType<Type> > StringType;

    std::basic_stringstream<Type> stream;
    stream << str;
    stream >> *pObject;
    if (stream.fail() || stream.bad() || !stream.eof())
    {
        throw std::runtime_error("Can't convert");
    }
}

template<class Type, class OutType, template<class> class Traits, template<class> class AllocatorType>
void ObjectToString_t(const OutType & object, std::basic_string<Type, Traits<Type>, AllocatorType<Type> > & result)
{
    typedef std::basic_string<Type, Traits<Type>, AllocatorType<Type> > StringType;

    std::basic_stringstream<Type> stream;
    stream << object;
    stream >> result;
    if (stream.fail() || stream.bad() || !stream.eof())
    {
        throw std::runtime_error("Can't convert");
    }
}

template<class OutType>
inline std::wstring ObjectToString(const OutType & object)
{
    std::wstring res;
    ObjectToString_t(object, res);
    return res;
}
template<class OutType>
inline std::string ObjectToString_Ansi(const OutType & object)
{
    std::string res;
    ObjectToString_t(object, res);
    return res;
}

template<class CharType>
bool IsWhitespace(CharType ch)
{
    switch(ch)
    {
    case CharTraits<CharType>::space:
    case 0xA:
    case 0xD:
    case 0x9:
        return true;
    }
    return false;
}

template<class CharType>
std::basic_string<CharType> Trim(const std::basic_string<CharType> & str)
{
    typedef typename std::basic_string<CharType> String_type;
    typename String_type::const_iterator it = str.begin();
    typename String_type::const_iterator it_end = str.end();
    for(; it != it_end; ++it)
    {
        if (!IsWhitespace(*it))
            break;
    }
    if (it == it_end)
        return String_type(it, it_end);

    typename String_type::const_iterator it2 = it_end;
    --it2;
    for(; it2 != it; --it2)
    {
        if (!IsWhitespace(*it2))
            break;
    }
    return String_type(it, ++it2);
}

template<class CharType>
std::basic_string<CharType> Trim(const CharType * str)
{
    std::basic_string<CharType> arg(str);
    return Trim(arg);
}

// ToStringAsHex
template<class ObjectType, template<class> class Traits, template<class> class AllocatorType, class Type>
void ToStringAsHex(ObjectType id, std::basic_string<Type, Traits<Type>, AllocatorType<Type> > * pStr)
{
    typedef std::basic_string<Type, Traits<Type>, AllocatorType<Type> > StringType;

    std::basic_stringstream<Type> stream;
    std::hex(stream);

    stream << std::setw( sizeof(id)*2 ) << std::setfill( CharTraits<Type>::zero );
    stream << id;
    stream >> *pStr;
    if (stream.fail() || stream.bad() || !stream.eof())
        throw std::runtime_error("Cannot convert");
}


template<template<class> class Traits, template<class> class AllocatorType, class Type>
void ToStringAsHex(long long id, std::basic_string<Type, Traits<Type>, AllocatorType<Type> > * pStr)
{
    LARGE_INTEGER li;
    li.QuadPart = id;
    typedef std::basic_string<Type, Traits<Type>, AllocatorType<Type> > StringType;

    std::basic_stringstream<Type> stream;
    std::hex(stream);


    stream << std::setw( sizeof(li.HighPart)*2 ) << std::setfill( CharTraits<Type>::zero );
    stream << li.HighPart;
    stream << li.LowPart;
    stream >> *pStr;
    if (stream.fail() || stream.bad() || !stream.eof())
        throw std::runtime_error("Cannot convert");
}
template<template<class> class Traits, template<class> class AllocatorType, class Type>
void ToStringAsHex(unsigned long long id, std::basic_string<Type, Traits<Type>, AllocatorType<Type> > * pStr)
{
    ULARGE_INTEGER li;
    li.QuadPart = id;
    typedef std::basic_string<Type, Traits<Type>, AllocatorType<Type> > StringType;

    std::basic_stringstream<Type> stream;
    std::hex(stream);

    stream << std::setw( sizeof(li.HighPart)*2 ) << std::setfill( CharTraits<Type>::zero );
    stream << li.HighPart;   
    stream << li.LowPart;
    stream >> *pStr;
    if (stream.fail() || stream.bad() || !stream.eof())
        throw std::runtime_error("Cannot convert");
}

// ansi
inline std::string ToAnsiStringAsHex(long long id)
{
    std::string res;
    ToStringAsHex(id, &res);
    return res;
}
inline std::string ToAnsiStringAsHex(unsigned long long id)
{
    std::string res;
    ToStringAsHex(id, &res);
    return res;
}
inline std::string ToAnsiStringAsHex(short id)
{
    std::string res;
    ToStringAsHex((unsigned short )id, &res);
    return res;
}
inline std::string ToAnsiStringAsHex(unsigned short id)
{
    std::string res;
    ToStringAsHex(id, &res);
    return res;
}
inline std::string ToAnsiStringAsHex(int id)
{
    std::string res;
    ToStringAsHex((unsigned int)id, &res);
    return res;
}
inline std::string ToAnsiStringAsHex(unsigned int id)
{
    std::string res;
    ToStringAsHex(id, &res);
    return res;
}

// wide
inline std::wstring ToWideStringAsHex(long long id)
{
    std::wstring res;
    ToStringAsHex(id, &res);
    return res;
}
inline std::wstring ToWideStringAsHex(unsigned long long id)
{
    std::wstring res;
    ToStringAsHex(id, &res);
    return res;
}
inline std::wstring ToWideStringAsHex(short id)
{
    std::wstring res;
    ToStringAsHex((unsigned short )id, &res);
    return res;
}
inline std::wstring ToWideStringAsHex(unsigned short id)
{
    std::wstring res;
    ToStringAsHex(id, &res);
    return res;
}
inline std::wstring ToWideStringAsHex(int id)
{
    std::wstring res;
    ToStringAsHex((unsigned int)id, &res);
    return res;
}
inline std::wstring ToWideStringAsHex(unsigned int id)
{
    std::wstring res;
    ToStringAsHex(id, &res);
    return res;
}

template<template<class> class Traits, template<class> class AllocatorType, class Type>
void ToStringAsHex_Short(long long id, std::basic_string<Type, Traits<Type>, AllocatorType<Type> > * pStr)
{
    LARGE_INTEGER li;
    li.QuadPart = id;
    typedef std::basic_string<Type, Traits<Type>, AllocatorType<Type> > StringType;

    std::basic_stringstream<Type> stream;
    std::hex(stream);
    if (li.HighPart)
    {
        stream << li.HighPart;
        stream << std::setw( sizeof(li.HighPart)*2 ) << std::setfill( CharTraits<Type>::zero );
    }
    stream << li.LowPart;
    stream >> *pStr;
    if (stream.fail() || stream.bad() || !stream.eof())
        throw std::runtime_error("Cannot convert");
}
template<template<class> class Traits, template<class> class AllocatorType, class Type>
void ToStringAsHex_Short(unsigned long long id, std::basic_string<Type, Traits<Type>, AllocatorType<Type> > * pStr)
{
    ULARGE_INTEGER li;
    li.QuadPart = id;
    typedef std::basic_string<Type, Traits<Type>, AllocatorType<Type> > StringType;

    std::basic_stringstream<Type> stream;
    std::hex(stream);
    if (li.HighPart)
    {
        stream << li.HighPart;
        stream << std::setw( sizeof(li.HighPart)*2 ) << std::setfill( CharTraits<Type>::zero );
    }
    stream << li.LowPart;
    stream >> *pStr;
    if (stream.fail() || stream.bad() || !stream.eof())
        throw std::runtime_error("Cannot convert");
}

inline std::wstring ToWideStringAsHex_Short(long long id)
{
    std::wstring res;
    ToStringAsHex_Short(id, &res);
    return res;
}
inline std::wstring ToWideStringAsHex_Short(unsigned long long id)
{
    std::wstring res;
    ToStringAsHex_Short(id, &res);
    return res;
}
std::wstring ToHexString(const char * pArray, 
                         size_t size);
template<class Type>
std::wstring ToHexString(const Type & obj)
{
    return ToHexString((const char * )&obj, sizeof(obj));
}
template<class Type>
std::wstring ToHexString(Type * pArray, 
                         size_t count)
{
    return ToHexString((const char * )pArray, sizeof(Type)*count);
}
template<class CharType>
void Split(const std::basic_string<CharType> & sourceString, 
           std::vector<std::basic_string<CharType> > * pArgs,   
           CharType splitChar = CharTraits<CharType>::space)
{
    typedef typename std::basic_string<CharType> String_type;
    typename String_type::const_iterator it = sourceString.begin();
    typename String_type::const_iterator it_end = sourceString.end();
    for(; it != it_end; ++it)
    {
        
        for(; it != it_end; ++it)
        {
            if (*it != splitChar)
                break;
        }
        if (it == it_end)
            return;
        typename String_type::const_iterator wordStart = it++;
        for(; it != it_end; ++it)
        {
            if (*it == splitChar)
                break;
        }
        pArgs->push_back(String_type(wordStart, it));
        if (it == it_end)
            break;
    }
}


// ANSI
struct StringInfo_Ansi
{
    const char * m_pBegin;
    const char * m_pEnd;

    static const int npos = (int)-1;
    StringInfo_Ansi(const char * pBegin,
                    const char * pEnd)
        : 
            m_pBegin(pBegin),
            m_pEnd(pEnd)
    {
    }
    StringInfo_Ansi(const char * pBegin,
               size_t sizeInWchars)
        : 
            m_pBegin(pBegin),
            m_pEnd(pBegin + sizeInWchars)
    {
    }
    StringInfo_Ansi(const std::string & str)
        : 
           m_pBegin(str.c_str()),
           m_pEnd(str.c_str()+str.size())
    {   
    }
    StringInfo_Ansi(const char * pBegin)
        : 
            m_pBegin(pBegin),
            m_pEnd(0)
    {
        m_pEnd = m_pBegin + strlen(pBegin);
    }
    const char * c_str() const;
    int size() const;
    int find(const char * separator,
             int startOffset, 
             int separatorSize) const;
    std::string ToString() const;
    
    bool empty() const
    {
        return size() == 0;
    }
};

inline bool operator == (const StringInfo_Ansi & info1, const StringInfo_Ansi & info2)
{
    int iSize1 = info1.size();
    int iSize2 = info2.size();
    if (iSize1 != iSize2)
        return false;

    return strncmp(info1.c_str(), info2.c_str(), iSize1) == 0;
}

inline bool operator != (const StringInfo_Ansi & info1, const StringInfo_Ansi & info2)
{
    return !(info1 == info2);
}

void SplitString(const StringInfo_Ansi & str,
                 const StringInfo_Ansi & separator,
                 std::vector<StringInfo_Ansi> * pInfo);


// Wide
struct StringInfo
{
    const wchar_t * m_pBegin;
    const wchar_t * m_pEnd;

    static const int npos = (int)-1;
    StringInfo(const wchar_t * pBegin,
                    const wchar_t * pEnd)
        : 
            m_pBegin(pBegin),
            m_pEnd(pEnd)
    {
    }
    StringInfo(const wchar_t * pBegin,
               size_t sizeInWchars)
        : 
            m_pBegin(pBegin),
            m_pEnd(pBegin + sizeInWchars)
    {
    }
    StringInfo(const std::wstring & str)
        : 
           m_pBegin(str.c_str()),
           m_pEnd(str.c_str()+str.size())
    {   
    }
    StringInfo(const wchar_t * pBegin)
        : 
            m_pBegin(pBegin),
            m_pEnd(0)
    {
        m_pEnd = m_pBegin + wcslen(pBegin);
    }
    const wchar_t * c_str() const;
    int size() const;
    int find(const wchar_t * separator,
             int startOffset, 
             int separatorSize) const;
    std::wstring ToString() const;
    
    bool empty() const
    {
        return size() == 0;
    }
};

inline bool operator == (const StringInfo & info1, const StringInfo & info2)
{
    int iSize1 = info1.size();
    int iSize2 = info2.size();
    if (iSize1 != iSize2)
        return false;

    return wcsncmp(info1.c_str(), info2.c_str(), iSize1) == 0;
}

inline bool operator != (const StringInfo & info1, const StringInfo & info2)
{
    return !(info1 == info2);
}

void SplitString(const StringInfo & str,
                 const StringInfo & separator,
                 std::vector<StringInfo> * pInfo);


template<class ContainerType>
typename ContainerType::pointer GetFrontPointer(ContainerType & container)
{
    if (container.empty())
        return 0;

    return &container.front();
}
template<class ContainerType>
typename ContainerType::const_pointer GetFrontPointer(const ContainerType & container)
{
    if (container.empty())
        return 0;

    return &container.front();
}

class CDll
{
    CDll(const CDll&);
    CDll&operator = (const CDll&);
    HMODULE m_hLib;
public:
    explicit CDll(const std::wstring & dllName);
    ~CDll();
    HMODULE GetBase();
    void Reset(const std::wstring & dllName);
    void Reset(const wchar_t * pDllName);
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


inline void CalcSHA1(const std::vector<char> & data,
                     std::vector<char> & sha1)
{
    sha1.resize(0);
    sha1.resize(SHA1_HASH_SIZE, 0);
    Sha1Hash((const uint8_t*)(data.empty()?"":&data.front()), (unsigned int)data.size(), (uint8_t *)&sha1.front());
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

long long ConvertSystemTimeToFileTime(const SYSTEMTIME * pTime);
std::wstring SystemTimeToWideString(const SYSTEMTIME & st);
std::wstring SystemTimeToWideStringJustDate(const SYSTEMTIME & st);


HMODULE GetCurrentModule();
std::wstring GetModuleName(HMODULE hModule);
std::wstring GetCurrentModuleDir();

template<class ContainerStr>
void EraseName(ContainerStr & fullName)
{
    int size = (int)fullName.size();
    for(int i = size-1; i > 0; --i)
    {
        wchar_t ch = (wchar_t)fullName[i];
        if (ch == '\\' || ch == L'/')
        {
            if (i < size-1)
            {
                fullName.erase(i+1);
            }
            return;
        }
    }
    fullName.clear();
}

template<class ContainerStr>
void UnparseFileNameFromFullFileName(const ContainerStr & fullName, ContainerStr * pFileName)
{
    pFileName->clear();
    int size = (int)fullName.size();
    for(int i = size-1; i > 0; --i)
    {
        wchar_t ch = (wchar_t)fullName[i];
        if (ch == '\\' || ch == L'/')
        {
            if (i < size-1)
            {
                pFileName->assign(fullName.begin() + i+1, fullName.end());
            }
            return;
        }
    }
    *pFileName = fullName;
}

template<class ContainerStr>
void GetExtensionOfFile(const ContainerStr & fullName, ContainerStr * pExtension)
{
    pExtension->clear();
    int pExtensionsize = (int)fullName.size();
    int size = (int)fullName.size();
    for(int i = size-1; i > 0; --i)
    {
        wchar_t ch = (wchar_t)fullName[i];
        if (ch == '\\' || ch == L'/')
        {
            return;
        }
        if (ch == '.')
        {
            pExtension->assign(fullName.begin() + i+1, fullName.end());
            return;
        }
    }
}

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



struct orthia_pcg32_random
{ 
    unsigned long long state;  
    unsigned long long inc;

    orthia_pcg32_random();
    void swap(orthia_pcg32_random & other);
    unsigned int gen();
};
unsigned int orthia_pcg32_random_r(orthia_pcg32_random * rng);



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

class CAutoCriticalSection
{
    CCriticalSection & m_section;
    CAutoCriticalSection(const CAutoCriticalSection&);
    CAutoCriticalSection&operator = (const CAutoCriticalSection&);
public:
    CAutoCriticalSection(CCriticalSection & section)
        : m_section(section)
    {
        m_section.Lock();
    }
    ~CAutoCriticalSection()
    {
        m_section.Unlock();
    }
};



}
#endif 