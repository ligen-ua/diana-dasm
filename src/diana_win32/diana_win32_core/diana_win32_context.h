#ifndef DIANA_WIN32_CONTEXT_H
#define DIANA_WIN32_CONTEXT_H

#include "diana_core.h"
#include "windows.h"

typedef struct _DI_WIN32_CONTEXT
{
    // WARNING: do not change fields position!!! (see functions below)
    DWORD m_retESP;

    DWORD m_EFlags;
 
    DWORD m_EDI;
    DWORD m_ESI;
    DWORD m_EBP;
    DWORD m_ESP;
    DWORD m_EBX;
    DWORD m_EDX;
    DWORD m_ECX;
    DWORD m_EAX;

    DWORD m_Eip;
}
DI_WIN32_CONTEXT;

void __stdcall Di_SetThreadContext(DI_WIN32_CONTEXT context);
void __stdcall Di_GetThreadContext(DI_WIN32_CONTEXT * pThis);

#endif