#pragma once

#include "diana_core_cpp.h"

#ifdef DIANA_HAS_WIN32
struct IUnknown;
#include "windows.h"
#undef max
#undef min

#else

#include <unistd.h>
#include <sys/types.h>
#include <dlfcn.h>

#endif

namespace diana
{

#ifdef DIANA_HAS_WIN32

#define DIANA_PLATFORM_ERROR_CODE   ULONG

#else

#define DIANA_PLATFORM_ERROR_CODE   int


#endif

class CWin32Exception:public std::runtime_error
{
    DIANA_PLATFORM_ERROR_CODE m_errorCode;

    std::string ToErrText(const std::string & text, DIANA_PLATFORM_ERROR_CODE errorCode)
    {
        std::stringstream resStream;
        resStream<<text<<", code: "<<errorCode;
        return resStream.str();
    }
public:
    CWin32Exception(const std::string & text, DIANA_PLATFORM_ERROR_CODE errorCode)
        :
            std::runtime_error(ToErrText(text, errorCode)),
            m_errorCode(errorCode)
    {
    }
    DIANA_PLATFORM_ERROR_CODE GetErrorCode() const { return m_errorCode; }

};

#ifdef DIANA_HAS_WIN32

struct Win32Handle
{
    typedef HANDLE ObjectType;
    static void Free(HANDLE pObject)
    {
        CloseHandle(pObject);
    }
};

#endif

}