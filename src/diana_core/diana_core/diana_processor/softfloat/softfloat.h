/*============================================================================
This C header file is part of the SoftFloat IEC/IEEE Floating-point Arithmetic
Package, Release 2b.

Written by John R. Hauser.  This work was made possible in part by the
International Computer Science Institute, located at Suite 600, 1947 Center
Street, Berkeley, California 94704.  Funding was partially provided by the
National Science Foundation under grant MIP-9311980.  The original version
of this code was written as part of a project to build a fixed-point vector
processor in collaboration with the University of California at Berkeley,
overseen by Profs. Nelson Morgan and John Wawrzynek.  More information
is available through the Web page `http://www.cs.berkeley.edu/~jhauser/
arithmetic/SoftFloat.html'.

THIS SOFTWARE IS DISTRIBUTED AS IS, FOR FREE.  Although reasonable effort has
been made to avoid it, THIS SOFTWARE MAY CONTAIN FAULTS THAT WILL AT TIMES
RESULT IN INCORRECT BEHAVIOR.  USE OF THIS SOFTWARE IS RESTRICTED TO PERSONS
AND ORGANIZATIONS WHO CAN AND WILL TAKE FULL RESPONSIBILITY FOR ALL LOSSES,
COSTS, OR OTHER PROBLEMS THEY INCUR DUE TO THE SOFTWARE, AND WHO FURTHERMORE
EFFECTIVELY INDEMNIFY JOHN HAUSER AND THE INTERNATIONAL COMPUTER SCIENCE
INSTITUTE (possibly via similar legal warning) AGAINST ALL LOSSES, COSTS, OR
OTHER PROBLEMS INCURRED BY THEIR CUSTOMERS AND CLIENTS DUE TO THE SOFTWARE.

Derivative works are acceptable, even for commercial purposes, so long as
(1) the source code for the derivative work includes prominent notice that
the work is derivative, and (2) the source code includes prominent notice with
these four paragraphs for those parts of this code that are retained.
=============================================================================*/

#include "diana_core.h"


#include "softfloat_start.h"
#ifndef _SOFTFLOAT_H_
#define _SOFTFLOAT_H_

#define FLOAT16
#define FLOATX80

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point types.
*----------------------------------------------------------------------------*/
#ifdef FLOAT16
typedef DI_UINT16 float16;
#endif
typedef DI_UINT32 float32;
typedef DI_UINT64 float64;

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point class.
*----------------------------------------------------------------------------*/
typedef enum {
    float_zero,
    float_SNaN,
    float_QNaN,
    float_negative_inf,
    float_positive_inf,
    float_denormal,
    float_normalized
} float_class_t;

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point NaN operands handling mode.
*----------------------------------------------------------------------------*/
enum float_nan_handling_mode_t {
    float_larger_significand_nan = 0,	// this mode used by x87 FPU
    float_first_operand_nan = 1		// this mode used by SSE
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point rounding mode.
*----------------------------------------------------------------------------*/
enum float_round_t {
    float_round_nearest_even = 0,
    float_round_down         = 1,
    float_round_up           = 2,
    float_round_to_zero      = 3
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point exception flags.
*----------------------------------------------------------------------------*/
enum float_exception_flag_t {
    float_flag_invalid   = 0x01,
    float_flag_denormal  = 0x02,
    float_flag_divbyzero = 0x04,
    float_flag_overflow  = 0x08,
    float_flag_underflow = 0x10,
    float_flag_inexact   = 0x20
};

#define float_all_exceptions_mask   0x3f

#ifdef FLOATX80
#define RAISE_SW_C1 0x0200
#endif

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point ordering relations
*----------------------------------------------------------------------------*/
enum {
    float_relation_less      = -1,
    float_relation_equal     =  0,
    float_relation_greater   =  1,
    float_relation_unordered =  2
};

/*----------------------------------------------------------------------------
| Options to indicate which negations to perform in float*_muladd()
| Using these differs from negating an input or output before calling
| the muladd function in that this means that a NaN doesn't have its
| sign bit inverted before it is propagated.
*----------------------------------------------------------------------------*/
enum {
    float_muladd_negate_c       = 1,
    float_muladd_negate_product = 2,
    float_muladd_negate_result  = float_muladd_negate_c | float_muladd_negate_product
};

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point status structure.
*----------------------------------------------------------------------------*/
typedef struct float_status
{
#ifdef FLOATX80
    int float_rounding_precision;	/* floatx80_t only */
#endif
    int float_rounding_mode;
    int float_exception_flags;
    int float_exception_masks;
    int float_suppress_exception;
    int float_nan_handling_mode;	/* flag register */
    int flush_underflow_to_zero;	/* flag register */
    int denormals_are_zeros;            /* flag register */
}float_status_t;

/*----------------------------------------------------------------------------
| Routine to raise any or all of the software IEC/IEEE floating-point
| exception flags.
*----------------------------------------------------------------------------*/

DIANA_INLINE_C void float_raise(float_status_t *status, int flags)
{
    status->float_exception_flags |= flags;
}

/*----------------------------------------------------------------------------
| Returns raised IEC/IEEE floating-point exception flags.
*----------------------------------------------------------------------------*/

DIANA_INLINE_C int get_exception_flags(const float_status_t *status)
{
    return status->float_exception_flags & ~status->float_suppress_exception;
}

/*----------------------------------------------------------------------------
| Routine to check if any or all of the software IEC/IEEE floating-point
| exceptions are masked.
*----------------------------------------------------------------------------*/

DIANA_INLINE_C int float_exception_masked(const float_status_t *status, int flag)
{
    return status->float_exception_masks & flag;
}

/*----------------------------------------------------------------------------
| Returns current floating point rounding mode specified by status word.
*----------------------------------------------------------------------------*/

DIANA_INLINE_C int get_float_rounding_mode(const float_status_t *status)
{
    return status->float_rounding_mode;
}

/*----------------------------------------------------------------------------
| Returns current floating point precision (floatx80_t only).
*----------------------------------------------------------------------------*/

#ifdef FLOATX80
DIANA_INLINE_C int get_float_rounding_precision(const float_status_t *status)
{
    return status->float_rounding_precision;
}
#endif

/*----------------------------------------------------------------------------
| Returns current floating point NaN operands handling mode specified
| by status word.
*----------------------------------------------------------------------------*/

DIANA_INLINE_C int get_float_nan_handling_mode(const float_status_t *status)
{
    return status->float_nan_handling_mode;
}

/*----------------------------------------------------------------------------
| Raise floating point precision lost up flag (floatx80_t only).
*----------------------------------------------------------------------------*/

#ifdef FLOATX80
DIANA_INLINE_C void set_float_rounding_up(float_status_t *status)
{
    status->float_exception_flags |= RAISE_SW_C1;
}
#endif

/*----------------------------------------------------------------------------
| Returns 1 if the <denormals-are-zeros> feature is supported;
| otherwise returns 0.
*----------------------------------------------------------------------------*/

DIANA_INLINE_C int get_denormals_are_zeros(const float_status_t *status)
{
    return status->denormals_are_zeros;
}

/*----------------------------------------------------------------------------
| Returns 1 if the <flush-underflow-to-zero> feature is supported;
| otherwise returns 0.
*----------------------------------------------------------------------------*/

DIANA_INLINE_C int get_flush_underflow_to_zero(const float_status_t *status)
{
    return status->flush_underflow_to_zero;
}

/*----------------------------------------------------------------------------
| Software IEC/IEEE integer-to-floating-point conversion routines.
*----------------------------------------------------------------------------*/
float32 int32_to_float32(DI_INT32, float_status_t *status);
float64 int32_to_float64(DI_INT32);
float32 int64_to_float32(DI_INT64, float_status_t *status);
float64 int64_to_float64(DI_INT64, float_status_t *status);

float32 uint32_to_float32(DI_UINT32, float_status_t *status);
float64 uint32_to_float64(DI_UINT32);
float32 uint64_to_float32(DI_UINT64, float_status_t *status);
float64 uint64_to_float64(DI_UINT64, float_status_t *status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision conversion routines.
*----------------------------------------------------------------------------*/
DI_INT32 float32_to_int32(float32, float_status_t *status);
DI_INT32 float32_to_int32_round_to_zero(float32, float_status_t *status);
DI_INT64 float32_to_int64(float32, float_status_t *status);
DI_INT64 float32_to_int64_round_to_zero(float32, float_status_t *status);
DI_UINT32 float32_to_uint32(float32, float_status_t *status);
DI_UINT32 float32_to_uint32_round_to_zero(float32, float_status_t *status);
DI_UINT64 float32_to_uint64(float32, float_status_t *status);
DI_UINT64 float32_to_uint64_round_to_zero(float32, float_status_t *status);
float64 float32_to_float64(float32, float_status_t *status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE single-precision operations.
*----------------------------------------------------------------------------*/
float32 float32_round_to_int(float32, float_status_t *status);
float32 float32_round_to_int_with_scale(float32, DI_UINT8 scale, float_status_t *status);
float32 float32_add(float32, float32, float_status_t *status);
float32 float32_sub(float32, float32, float_status_t *status);
float32 float32_mul(float32, float32, float_status_t *status);
float32 float32_div(float32, float32, float_status_t *status);
float32 float32_sqrt(float32, float_status_t *status);
float32 float32_frc(float32, float_status_t *status);
float32 float32_muladd(float32, float32, float32, int flags, float_status_t *status);
float32 float32_scalef(float32, float32, float_status_t *status);

DIANA_INLINE_C float32 float32_round_to_int(float32 a, float_status_t *status)
{
  return float32_round_to_int_with_scale(a, 0, status);
}

DIANA_INLINE_C float32 float32_fmadd(float32 a, float32 b, float32 c, float_status_t *status)
{
  return float32_muladd(a, b, c, 0, status);
}

DIANA_INLINE_C float32 float32_fmsub(float32 a, float32 b, float32 c, float_status_t *status)
{
  return float32_muladd(a, b, c, float_muladd_negate_c, status);
}

DIANA_INLINE_C float32 float32_fnmadd(float32 a, float32 b, float32 c, float_status_t *status)
{
  return float32_muladd(a, b, c, float_muladd_negate_product, status);
}

DIANA_INLINE_C float32 float32_fnmsub(float32 a, float32 b, float32 c, float_status_t *status)
{
  return float32_muladd(a, b, c, float_muladd_negate_result, status);
}

int float32_compare(float32, float32, float_status_t *status);
int float32_compare_quiet(float32, float32, float_status_t *status);

float_class_t float32_class(float32);
int float32_is_signaling_nan(float32);
int float32_is_nan(float32);
int float32_is_denormal(float32);

float32 float32_min(float32 a, float32 b, float_status_t *status);
float32 float32_max(float32 a, float32 b, float_status_t *status);

float32 float32_getexp(float32 a, float_status_t *status);
float32 float32_getmant(float32 a, float_status_t *status, int sign_ctrl, int interv);

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision conversion routines.
*----------------------------------------------------------------------------*/
DI_INT32 float64_to_int32(float64, float_status_t *status);
DI_INT32 float64_to_int32_round_to_zero(float64, float_status_t *status);
DI_INT64 float64_to_int64(float64, float_status_t *status);
DI_INT64 float64_to_int64_round_to_zero(float64, float_status_t *status);
DI_UINT32 float64_to_uint32(float64, float_status_t *status);
DI_UINT32 float64_to_uint32_round_to_zero(float64, float_status_t *status);
DI_UINT64 float64_to_uint64(float64, float_status_t *status);
DI_UINT64 float64_to_uint64_round_to_zero(float64, float_status_t *status);
float32 float64_to_float32(float64, float_status_t *status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE double-precision operations.
*----------------------------------------------------------------------------*/
float64 float64_round_to_int(float64, float_status_t *status);
float64 float64_round_to_int_with_scale(float64, DI_UINT8 scale, float_status_t *status);
float64 float64_add(float64, float64, float_status_t *status);
float64 float64_sub(float64, float64, float_status_t *status);
float64 float64_mul(float64, float64, float_status_t *status);
float64 float64_div(float64, float64, float_status_t *status);
float64 float64_sqrt(float64, float_status_t *status);
float64 float64_frc(float64, float_status_t *status);
float64 float64_muladd(float64, float64, float64, int flags, float_status_t *status);
float64 float64_scalef(float64, float64, float_status_t *status);

DIANA_INLINE_C float64 float64_round_to_int(float64 a, float_status_t *status)
{
  return float64_round_to_int_with_scale(a, 0, status);
}

DIANA_INLINE_C float64 float64_fmadd(float64 a, float64 b, float64 c, float_status_t *status)
{
  return float64_muladd(a, b, c, 0, status);
}

DIANA_INLINE_C float64 float64_fmsub(float64 a, float64 b, float64 c, float_status_t *status)
{
  return float64_muladd(a, b, c, float_muladd_negate_c, status);
}

DIANA_INLINE_C float64 float64_fnmadd(float64 a, float64 b, float64 c, float_status_t *status)
{
  return float64_muladd(a, b, c, float_muladd_negate_product, status);
}

DIANA_INLINE_C float64 float64_fnmsub(float64 a, float64 b, float64 c, float_status_t *status)
{
  return float64_muladd(a, b, c, float_muladd_negate_result, status);
}

int float64_compare(float64, float64, float_status_t *status);
int float64_compare_quiet(float64, float64, float_status_t *status);

float_class_t float64_class(float64);
int float64_is_signaling_nan(float64);
int float64_is_nan(float64);
int float64_is_denormal(float64);

float64 float64_min(float64 a, float64 b, float_status_t *status);
float64 float64_max(float64 a, float64 b, float_status_t *status);

float64 float64_getexp(float64 a, float_status_t *status);
float64 float64_getmant(float64 a, float_status_t *status, int sign_ctrl, int interv);

#ifdef FLOAT16
float32 float16_to_float32(float16, float_status_t *status);
float16 float32_to_float16(float32, float_status_t *status);

float_class_t float16_class(float16);
int float16_is_signaling_nan(float16);
int float16_is_nan(float16);
int float16_is_denormal(float16);
#endif

#ifdef FLOATX80

/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point types.
*----------------------------------------------------------------------------*/
#pragma pack(push,1)
#ifdef SFT_BIG_ENDIAN
struct floatx80_t {	// leave alignment to compiler
    DI_UINT16 exp;
    DI_UINT64 fraction;
};
#else
typedef struct floatx80_
{
    DI_UINT64 fraction;
    DI_UINT16 exp;
}floatx80_t;
#endif
#pragma pack(pop)
/*----------------------------------------------------------------------------
| Software IEC/IEEE integer-to-floating-point conversion routines.
*----------------------------------------------------------------------------*/
floatx80_t int32_to_floatx80(DI_INT32);
floatx80_t int64_to_floatx80(DI_INT64);

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision conversion routines.
*----------------------------------------------------------------------------*/
floatx80_t float32_to_floatx80(float32, float_status_t *status);
floatx80_t float64_to_floatx80(float64, float_status_t *status);

DI_INT32 floatx80_to_int32(floatx80_t, float_status_t *status);
DI_INT32 floatx80_to_int32_round_to_zero(floatx80_t, float_status_t *status);
DI_INT64 floatx80_to_int64(floatx80_t, float_status_t *status);
DI_INT64 floatx80_to_int64_round_to_zero(floatx80_t, float_status_t *status);

float32 floatx80_to_float32(floatx80_t, float_status_t *status);
float64 floatx80_to_float64(floatx80_t, float_status_t *status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision operations.
*----------------------------------------------------------------------------*/
floatx80_t floatx80_round_to_int(floatx80_t, float_status_t *status);
floatx80_t floatx80_add(floatx80_t, floatx80_t, float_status_t *status);
floatx80_t floatx80_sub(floatx80_t, floatx80_t, float_status_t *status);
floatx80_t floatx80_mul(floatx80_t, floatx80_t, float_status_t *status);
floatx80_t floatx80_div(floatx80_t, floatx80_t, float_status_t *status);
floatx80_t floatx80_sqrt(floatx80_t, float_status_t *status);

float_class_t floatx80_class(floatx80_t);
int floatx80_is_signaling_nan(floatx80_t);
int floatx80_is_nan(floatx80_t);

#endif  /* FLOATX80 */

#ifdef FLOAT128

#pragma pack(push,1)
#ifdef SFT_BIG_ENDIAN
struct float128_t {
    DI_UINT64 hi, lo;
};
#else
typedef struct float128_
{
    DI_UINT64 lo, hi;
}float128_t;
#endif
#pragma pack(pop)
/*----------------------------------------------------------------------------
| Software IEC/IEEE quadruple-precision conversion routines.
*----------------------------------------------------------------------------*/
float128_t floatx80_to_float128(floatx80_t a, float_status_t *status);
floatx80_t float128_to_floatx80(float128_t a, float_status_t *status);

float128_t int64_to_float128(DI_INT64 a);

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision operations.
*----------------------------------------------------------------------------*/
floatx80_t floatx80_mul_128(floatx80_t a, float128_t b, float_status_t *status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE quadruple-precision operations.
*----------------------------------------------------------------------------*/
float128_t float128_add(float128_t a, float128_t b, float_status_t *status);
float128_t float128_sub(float128_t a, float128_t b, float_status_t *status);
float128_t float128_mul(float128_t a, float128_t b, float_status_t *status);
float128_t float128_div(float128_t a, float128_t b, float_status_t *status);

#endif  /* FLOAT128 */

#include "softfloat_end.h"
#endif
