#include "diana_core_win32_context.h"

#if 0

#include "windows.h"

#ifdef DIANA_CFG_I386

char strArray[(sizeof(CONTEXT) == sizeof(DIANA_CONTEXT_NTLIKE_32)) ? 1:0];

#else

char strArray[(sizeof(CONTEXT) == sizeof(DIANA_CONTEXT_NTLIKE_64)) ? 1:0];

#endif


#endif