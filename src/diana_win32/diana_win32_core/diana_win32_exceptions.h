#ifndef DIANA_WIN32_EXCEPTIONS_H
#define DIANA_WIN32_EXCEPTIONS_H

#include "diana_win32_processor.h"

int Diana_EnsureWriteAccess(void * pData);
int DianaWin32Processor_InitExceptions();
void DianaWin32Processor_SaveCurrentThreadProcessor(DianaWin32Processor * pRealProcessor);
int DianaWin32Processor_InitProcessorExceptions(DianaWin32Processor * pThis);

#endif