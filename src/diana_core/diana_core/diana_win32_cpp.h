#pragma once

#include "diana_core_cpp.h"

namespace diana
{

class CWin32Exception:public std::runtime_error
{
    ULONG m_errorCode;

    std::string ToErrText(const std::string & text, ULONG errorCode)
    {
        std::stringstream resStream;
        resStream<<text<<", code: "<<errorCode;
        return resStream.str();
    }
public:
    CWin32Exception(const std::string & text, ULONG errorCode)
        :
            std::runtime_error(ToErrText(text, errorCode)),
            m_errorCode(errorCode)
    {
    }
};

struct Win32Handle
{
    typedef HANDLE ObjectType;
    static void Free(HANDLE pObject)
    {
        CloseHandle(pObject);
    }
};


}