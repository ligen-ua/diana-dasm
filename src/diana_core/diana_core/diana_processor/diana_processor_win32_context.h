#ifndef DIANA_PROCESSOR_WIN32_CONTEXT_H
#define DIANA_PROCESSOR_WIN32_CONTEXT_H

#include "diana_processor_context.h"
#include "diana_core_win32_context.h"

int DianaProcessor_ConvertContextToIndependent_Win32(const DIANA_CONTEXT_NTLIKE_32 * pContextIn, 
                                                     Diana_Processor_Registers_Context * pContextOut);
int DianaProcessor_ConvertContextToIndependent_X64(const DIANA_CONTEXT_NTLIKE_64 * pContextIn, 
                                                   Diana_Processor_Registers_Context * pContextOut);


#endif