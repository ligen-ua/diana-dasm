#ifndef DIANA_CONFIG_H
#define DIANA_CONFIG_H


#ifdef _M_IX86

#define DIANA_CFG_USE_INLINE_ASSEMBLER  

#endif


#if defined(_M_IX86)

#define DIANA_CFG_I386

#endif


#ifndef DIANA_DISABLE_AUTODETECT
#ifndef DIANA_AUTODETECT_DISABLE_WIN32

#ifdef _WIN32

#define DIANA_HAS_WIN32
#define DIANA_DEBUG_BREAK()  __debugbreak()

#endif

#endif
#endif


#define DIANA_PROCESSOR_USE_SOFTFLOAT_FPU


#ifndef DIANA_INLINE_C

#if __GNUC__ 
// GNUC version

#define DIANA_HAS_POSIX
#define DIANA_HAS_LINUX

#define DIANA_DEBUG_BREAK()  assert(0)

#if __GNUC_STDC_INLINE__
#define DIANA_INLINE_C static inline
#else
#define DIANA_INLINE_C extern inline
#endif

#else
// other compilers
#ifdef _MSC_VER
#  define DIANA_INLINE_C __forceinline
#else
#  define DIANA_INLINE_C extern inline __attribute__ ((__gnu_inline__))
#endif

# endif
#endif

// DIANA_NO_ARM (not defined by default): x86 only. The ARM decoders, their tables and text output compile to
// nothing, the ARM fields leave DianaContext, DianaLinkedOperand, Diana_RouteInfo and the analyzer session.
// DIANA_ARCH_AARCH64, DIANA_ARCH_ARM32 and DIANA_ARCH_THUMB get DI_UNSUPPORTED. For small environments,
// such as an i386 kernel driver: define it on the command line or in DIANA_CONFIGURATION_FILE

#ifndef DIANA_CONFIGURATION_FILE
#define DIANA_CONFIGURATION_FILE "./diana_custom_config.inc"
#endif

#ifdef DIANA_CONFIGURATION_FILE
#include DIANA_CONFIGURATION_FILE
#endif 



#include "diana_apply_config_rules.h"

#endif