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
#define DIANA_STRNCMP strncmp
#define DIANA_MEMSET memset

#if defined(_WIN32) || defined(_MSC_VER)
#define DIANA_STRNICMP _strnicmp
#define DIANA_STRICMP _stricmp

#else
#define DIANA_STRNICMP strncasecmp
#define DIANA_STRICMP strcasecmp

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

#ifdef _WIN32

#define DIANA_CDECL __cdecl

#else

#define DIANA_CDECL 

#endif



#ifndef _MSC_VER
//#pragma clang diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wwrite-strings"
#endif


#if _MSC_VER < 1910 && __cplusplus < 201703L
#define DIANA_AUTO_PTR std::auto_ptr
#else
#define DIANA_AUTO_PTR std::unique_ptr

#ifndef DIANA_HAS_CPP11
#define DIANA_HAS_CPP11
#endif
#endif



#if defined(__has_include) && __has_include(<stdint.h>)
#include <stdint.h>
#define DIANA_HAS_STDINT
#elif defined(_MSC_VER) && (_MSC_VER < 1600)

#define DI_INT8           signed char
#define DI_INT16          short
#define DI_INT32          int
#define DI_INT64          long long

#define DI_UINT8          unsigned char
#define DI_UINT16         unsigned short
#define DI_UINT32         unsigned int
#define DI_UINT64         unsigned long long

#define DI_CHAR           unsigned char
#define DI_SIGNED_CHAR    signed char
#define DI_CHAR_NULL      ((unsigned char)(-1))

#define OPERAND_SIZE         unsigned long long
#define OPERAND_SIZE_SIGNED  long long

#define DI_OPERAND_SIZE         unsigned long long
#define DI_OPERAND_SIZE_SIGNED  long long
#define DI_MAX_OPERAND_SIZE         ((unsigned long long)(-1))

#define DI_FULL_CHAR           unsigned int
#define DI_FULL_CHAR_NULL      ((unsigned int)(-1))

#else
#include <stdint.h>
#define DIANA_HAS_STDINT
#endif


#ifdef DIANA_HAS_STDINT

#define DI_INT8           int8_t
#define DI_INT16          int16_t
#define DI_INT32          int32_t
#define DI_INT64          int64_t

#define DI_UINT8          uint8_t
#define DI_UINT16         uint16_t
#define DI_UINT32         uint32_t
#define DI_UINT64         uint64_t

#define DI_CHAR           uint8_t
#define DI_SIGNED_CHAR    signed char
#define DI_CHAR_NULL      ((uint8_t)(-1))

#define OPERAND_SIZE            uint64_t
#define OPERAND_SIZE_SIGNED     int64_t

#define DI_OPERAND_SIZE         uint64_t
#define DI_OPERAND_SIZE_SIGNED  int64_t
#define DI_MAX_OPERAND_SIZE     ((uint64_t)(-1))

#define DI_FULL_CHAR            uint32_t
#define DI_FULL_CHAR_NULL       ((uint32_t)(-1))

#endif

#endif
