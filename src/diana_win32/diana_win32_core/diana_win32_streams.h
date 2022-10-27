#ifndef DIANA_WIN32_STREAMS_H
#define DIANA_WIN32_STREAMS_H

#include "diana_processor/diana_processor_streams.h"
#include "windows.h"


typedef struct _dianaWin32Stream
{
    DianaRandomReadWriteStream m_parent;
}DianaWin32Stream;

void DianaWin32Stream_Init(DianaWin32Stream * pStream);

//------------ REMOTE
typedef struct _dianaWin32RemoteStream
{
    DianaRandomReadWriteStream m_parent;
    HANDLE m_hProcess;
}DianaWin32RemoteStream;

void DianaWin32RemoteStream_Init(DianaWin32RemoteStream * pStream,
                                 HANDLE hProcess);


#endif