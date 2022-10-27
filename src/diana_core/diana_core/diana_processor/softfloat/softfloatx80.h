/*============================================================================
This source file is an extension to the SoftFloat IEC/IEEE Floating-point
Arithmetic Package, Release 2b, written for Bochs (x86 achitecture simulator)
floating point emulation.

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

#ifndef _SOFTFLOATX80_EXTENSIONS_H_
#define _SOFTFLOATX80_EXTENSIONS_H_

#include "softfloat.h"
#include "softfloat-specialize.h"

/*----------------------------------------------------------------------------
| Software IEC/IEEE integer-to-floating-point conversion routines.
*----------------------------------------------------------------------------*/

DI_INT16 floatx80_to_int16(floatx80_t, float_status_t *status);
DI_INT16 floatx80_to_int16_round_to_zero(floatx80_t, float_status_t *status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision operations.
*----------------------------------------------------------------------------*/

float_class_t floatx80_class(floatx80_t);
floatx80_t floatx80_extract(floatx80_t *a, float_status_t *status);
floatx80_t floatx80_scale(floatx80_t a, floatx80_t b, float_status_t *status);
int floatx80_remainder(floatx80_t a, floatx80_t b, floatx80_t *r, DI_UINT64 *q, float_status_t *status);
int floatx80_ieee754_remainder(floatx80_t a, floatx80_t b, floatx80_t *r, DI_UINT64 *q, float_status_t *status);
floatx80_t f2xm1(floatx80_t a, float_status_t *status);
floatx80_t fyl2x(floatx80_t a, floatx80_t b, float_status_t *status);
floatx80_t fyl2xp1(floatx80_t a, floatx80_t b, float_status_t *status);
floatx80_t fpatan(floatx80_t a, floatx80_t b, float_status_t *status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision trigonometric functions.
*----------------------------------------------------------------------------*/

int fsincos(floatx80_t a, floatx80_t *sin_a, floatx80_t *cos_a, float_status_t *status);
int fsin(floatx80_t *a, float_status_t *status);
int fcos(floatx80_t *a, float_status_t *status);
int ftan(floatx80_t *a, float_status_t *status);

/*----------------------------------------------------------------------------
| Software IEC/IEEE extended double-precision compare.
*----------------------------------------------------------------------------*/

int floatx80_compare(floatx80_t, floatx80_t, float_status_t *status);
int floatx80_compare_quiet(floatx80_t, floatx80_t, float_status_t *status);

/*-----------------------------------------------------------------------------
| Calculates the absolute value of the extended double-precision floating-point
| value `a'.  The operation is performed according to the IEC/IEEE Standard
| for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

DIANA_INLINE_C floatx80_t* floatx80_abs(floatx80_t *reg)
{
    reg->exp &= 0x7FFF;
    return reg;
}

/*-----------------------------------------------------------------------------
| Changes the sign of the extended double-precision floating-point value 'a'.
| The operation is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

DIANA_INLINE_C floatx80_t * floatx80_chs(floatx80_t * reg)
{
    reg->exp ^= 0x8000;
    return reg;
}

/*-----------------------------------------------------------------------------
| Commonly used extended double-precision floating-point constants.
*----------------------------------------------------------------------------*/

extern const floatx80_t Const_Z;
extern const floatx80_t Const_1;

#endif
