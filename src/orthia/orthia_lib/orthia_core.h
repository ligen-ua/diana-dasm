#pragma once

// Common part
#ifdef  __cplusplus

#include <vector>
#include <set>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <map>
#include <iomanip>
#include "diana_win32_cpp.h"
#include <cstdint>
#include <type_traits>
    
namespace orthia
{
typedef diana::CWin32Exception CWin32Exception;
}

#endif

#define ORTHIA_THROW_STD(Text) { std::stringstream orthia____stream; orthia____stream<<Text; throw std::runtime_error(orthia____stream.str());} 


#ifdef WIN32
// Windows

#include <intrin.h>
#include <stdint.h>


#ifdef  __cplusplus

namespace orthia
{
typedef std::wstring PlatformString_type;
typedef std::wstringstream PlatformStringStream_type;
}

#endif


#define ORTHIA_TCSTR(X) L##X
#define ORTHIA_TSTRNCMP(X1, X2, X3)  wcsncmp(X1, X2, X3)
#define ORTHIA_TCHAR wchar_t
#define ORTHIA_SYM_PLATFORM_SLASH L'\\'
#define ORTHIA_STR_PLATFORM_SLASH L"\\"
#define ORTHIA_STDCALL  __stdcall
#define ORTHIA_THROW_WIN32(Text) { ULONG orthia____code = ::GetLastError(); std::stringstream orthia____stream; orthia____stream<<Text; throw orthia::CWin32Exception(orthia____stream.str(), orthia____code);} 
#define ORTHIA_TLEN   wcslen
#define ORTHIA_SNPRINTF   _snprintf
#define ORTHIA_IS_DEBUGGER_PRESENT() IsDebuggerPresent()
#define ORTHIA_STR_TO_ULL _wcstoui64
#define ORTHIA_TSTRCPY  wcscpy
#define ORTHIA_SNTPRINTF  _snwprintf
#define ORTHIA_INFINITE INFINITE
#define ORTHIA_TSTRICMP(X1, X2)  _wcsicmp(X1, X2)
#define ORTHIA_SQLITE3_OPEN sqlite3_open16

#ifdef  __cplusplus

namespace orthia
{
typedef ULONG64 Address_type;

template<class Type>
Type * Exchange(Type ** ppDestination, Type * pNewData)
{
    // returns old value from *ppDestination
    return (Type *)InterlockedExchangePointer((void**)ppDestination, pNewData);
}
inline long Exchange(long * pDestination, long newData)
{
    // returns old value from *pDestination
    return  InterlockedExchange(pDestination, newData);
}

inline long Increment(long volatile * pValue)
{
    return InterlockedIncrement(pValue);
}
inline long Decrement(long volatile * pValue)
{
    return InterlockedDecrement(pValue);
}


typedef LARGE_INTEGER LargeInteger_type;
typedef ULARGE_INTEGER UnsignedLargeInteger_type;

typedef SYSTEMTIME WinSystemTime_type;


} // orthia

#endif

#else
// Linux

#include <x86intrin.h> 
#define ORTHIA_TCSTR(X) X
#define ORTHIA_TSTRNCMP(X1, X2, X3)  strncmp(X1, X2, X3)
#define ORTHIA_TCHAR char
#define ORTHIA_SYM_PLATFORM_SLASH '/'
#define ORTHIA_STR_PLATFORM_SLASH "/"
#define ORTHIA_STDCALL 
#define ORTHIA_TLEN   strlen
#define ORTHIA_SNPRINTF   snprintf
#define ORTHIA_IS_DEBUGGER_PRESENT() (0)
#define ORTHIA_STR_TO_ULL strtoull
#define ORTHIA_TSTRCPY  strcpy
#define ORTHIA_SNTPRINTF  snprintf
#define ORTHIA_TSTRICMP(X1, X2)  strcasecmp(X1, X2)
#define ORTHIA_SQLITE3_OPEN sqlite3_open


#define ORTHIA_INFINITE (uint32_t)(-1)
#define ORTHIA_THROW_ERRNO(Text) { int e____1 = (errno);  throw orthia::CWin32Exception(Text, e____1);  }
#define ORTHIA_CHECK_LESS_ZERO(Code, Text) { long long e____ = (Code);  if (e____ < 0)  { ORTHIA_THROW_ERRNO(Text); } }

#define HANDLE_EINTR(x) ({ \
  decltype(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
  } while (eintr_wrapper_result == -1 && errno == EINTR); \
  eintr_wrapper_result; \
})

#define IGNORE_EINTR(x) ({ \
  decltype(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
    if (eintr_wrapper_result == -1 && errno == EINTR) { \
      eintr_wrapper_result = 0; \
    } \
  } while (0); \
  eintr_wrapper_result; \
})



#include <inttypes.h>
#include <assert.h>

#ifdef  __cplusplus

namespace orthia
{
typedef std::string PlatformString_type;
typedef std::stringstream PlatformStringStream_type;

typedef OPERAND_SIZE  Address_type;


// try GCC
template<class Type>
Type * CompareAndSwap(Type ** ppDestination, Type * pNewData, Type * pComparand)
{
    // returns old value from *ppDestination
    return (Type *)__sync_val_compare_and_swap((void **)ppDestination,  (void *)pComparand, (void *)pNewData); 
}

inline
long CompareAndSwap(long * pDestination, long newData, long comparand)
{
    // returns old value from *ppDestination
    return __sync_val_compare_and_swap(pDestination, comparand, newData); 
}


template<class Type>
Type * Exchange(Type ** data, Type * value)
{
    intptr_t * data2 = (intptr_t *)data;
    intptr_t value2 = (intptr_t)value;
    // this is sad, but it doesn't work with pointers at my cygwin
    Type * pRes =  (Type *)__sync_lock_test_and_set (data2, value2);
    __sync_synchronize();
    return pRes;
}
inline long Exchange(long * pDestination, long newData)
{
    long res = __sync_lock_test_and_set (pDestination, newData);
    __sync_synchronize();
    return res;
}

inline long Increment(long volatile * pValue)
{
    return __sync_add_and_fetch(pValue, 1);
}
inline long Decrement(long volatile * pValue)
{
    return __sync_add_and_fetch(pValue, -1);
}

typedef union  _LARGE_INTEGER {
        struct {
            uint32_t LowPart;
            int32_t HighPart;
        };
        struct {
            uint32_t LowPart;
            int32_t HighPart;
        } u;
        int64_t QuadPart;
} LargeInteger_type;


typedef union  _ULARGE_INTEGER {
        struct {
            uint32_t LowPart;
            uint32_t HighPart;
        };
        struct {
            uint32_t LowPart;
            uint32_t HighPart;
        } u;
        uint64_t QuadPart;
} UnsignedLargeInteger_type;


struct WinSystemTime
{
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
};

typedef WinSystemTime  WinSystemTime_type;

} // utils

#endif

#endif

