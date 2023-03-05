#ifndef DIANA_APPLY_CONFIG_RULES_H
#define DIANA_APPLY_CONFIG_RULES_H


#ifdef DIANA_USE_CRUNTIME
/* we have cruntime */
#include "memory.h"
#include "stdlib.h"
#include "string.h"

#define DIANA_USE_C_SIZET
#define DIANA_MEMCMP  memcmp
#define DIANA_MEMCPY  memcpy
#define DIAND_STRNCMP strncmp
#define DIANA_MEMSET memset

#if defined(_WIN32) || defined(_MSC_VER)
#define DIAND_STRNICMP _strnicmp
#else
#define DIAND_STRNICMP strncasecmp
#endif

#ifndef DIANA_USE_MALLOC_FREE 
#ifndef DIANA_DONT_USE_MALLOC_FREE 
#define DIANA_USE_MALLOC_FREE
#endif
#endif

#else

#endif


#ifdef DIANA_USE_MALLOC_FREE
#define DIANA_MALLOC  malloc
#define DIANA_FREE    free
#else
#define DIANA_MALLOC  Diana_DefaultAllocator_Alloc
#define DIANA_FREE    Diana_DefaultAllocator_Free
#endif


#ifdef DIANA_USE_C_SIZET
#define DIANA_SIZE_T size_t
#else

#ifdef DIANA_CFG_I386
#define DIANA_SIZE_T  DI_UINT32
#else
#define DIANA_SIZE_T  DI_UINT64 
#endif
#endif

#endif

#ifdef _WIN32

#define DIANA_CDECL __cdecl

#else

#define DIANA_CDECL 

#endif



#ifndef _MSC_VER
//#pragma clang diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif


#if __cplusplus < 201103L 
#define DIANA_AUTO_PTR std::auto_ptr
#else
#define DIANA_AUTO_PTR std::unique_ptr
#endif
