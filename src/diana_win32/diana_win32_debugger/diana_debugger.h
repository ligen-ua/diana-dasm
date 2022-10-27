#ifndef DIANA_DEBUGGER_H
#define DIANA_DEBUGGER_H

#include "windows.h"
#include "iostream"
#include "stdexcept"
#include "diana_debug_stream.h"
extern "C"
{
#include "diana_win32_streams.h"
#include "diana_win32_processor.h"
}

namespace diana
{

struct Win32Exception:public std::runtime_error
{
    ULONG m_error;
public:
    Win32Exception(ULONG error)
        : 
            std::runtime_error("Win32Exception"),
            m_error(error)
    {
    }
    Win32Exception(const std::string & str, ULONG error)
        : 
            std::runtime_error(str),
            m_error(error)
    {
    }
};

struct DianaException:public std::runtime_error
{
    ULONG m_error;
public:
    DianaException(ULONG error)
        : 
            std::runtime_error("DianaException"),
            m_error(error)
    {
    }
    DianaException(const std::string & str, ULONG error)
        : 
            std::runtime_error(str),
            m_error(error)
    {
    }
};

class HandleGuard
{
    HANDLE m_hValue;

    HandleGuard(const HandleGuard&);
    HandleGuard & operator= (const HandleGuard&);
public:
    HandleGuard()
        : m_hValue(0)
    {
    }
    explicit HandleGuard(HANDLE hValue)
        : 
            m_hValue(hValue)
    {
    }
    ~HandleGuard()
    {
        reset(0);
    }

    void reset(HANDLE hValue)
    {
        if (m_hValue)
        {
            CloseHandle(m_hValue);
        }
        m_hValue = hValue;
    }
    HANDLE get()
    {
        return m_hValue;
    }
};

class CDianaDebugger
{

    CONTEXT m_context;
    CONTEXT m_dianaContext, m_prevDiContext;
    std::wstring m_exeName;
    ULONG m_address;
    
    HandleGuard m_hThread, m_hProcess;

    CDebugRemoteStream m_remoteStream;
    DianaWin32Processor m_processor;

    long long m_cmdCount;
    bool m_bValidPrevInfo;
    long m_tempCount;
    void DebugLoop();
    void StartProcess();
public:
    CDianaDebugger(const std::wstring & exeName);

    void Start();
};

}
#endif