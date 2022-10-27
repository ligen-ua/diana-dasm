#ifndef DIANA_WIN32_CORE_H
#define DIANA_WIN32_CORE_H

#include "diana_processor.h"
#include "diana_win32_core_init.h"
#include "windows.h"

void __stdcall DianaProcessor_LoadLiveContext32(DianaProcessor * pThis);

void __stdcall DianaProcessor_LoadContext32(DianaProcessor * pThis,
                                            const CONTEXT * pContext);

void __stdcall DianaProcessor_SaveContext32(DianaProcessor * pThis,
                                            CONTEXT * pContext);
#endif