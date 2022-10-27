#ifndef DIANA_WIN32_SYS_MEMCPY_H
#define DIANA_WIN32_SYS_MEMCPY_H


#include "diana_core.h"

typedef void (DIANA_CDECL *sys_memcpy_ptr_type)(void * pDestination, 
                                            const void * pSource,
                                            DIANA_SIZE_T bufferSize,        
                                            DI_UINT16 selector16);

sys_memcpy_ptr_type  DianaWin32_AllocSysMemcpy();
void  DianaWin32_FreeSysMemcpy(sys_memcpy_ptr_type p);


#endif