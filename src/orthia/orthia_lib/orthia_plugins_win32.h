#pragma once

#include "orthia_utils.h"

namespace orthia
{


    typedef HANDLE(WINAPI* OpenProcess_type)(
        __in DWORD dwDesiredAccess,
        __in BOOL bInheritHandle,
        __in DWORD dwProcessId
        );
    typedef int(WINAPI* OrthiaInit_type)(
        );
    typedef void(WINAPI* OrthiaUninit_type)(
        );

    typedef NTSTATUS(WINAPI* NtGetNextThread_type)(
        _In_ HANDLE  	ProcessHandle,
        _In_ HANDLE  	ThreadHandle,
        _In_ ACCESS_MASK  	DesiredAccess,
        _In_ ULONG  	HandleAttributes,
        _In_ ULONG  	Flags,
        _Out_ PHANDLE  	NewThreadHandle
        );

    class CWin32OpenProcessPlugin
    {
        orthia::CDll m_plugin;
        OrthiaInit_type m_pluginInit = 0;
        OrthiaUninit_type m_pluginUninit = 0;
        OpenProcess_type m_openProcess = 0;
        NtGetNextThread_type m_getNextThread = 0;

    public:
        CWin32OpenProcessPlugin();
        ~CWin32OpenProcessPlugin();
        OpenProcess_type GetOpenProcess();
        NtGetNextThread_type GetGetNextThread();

        bool Load();
    };
}

