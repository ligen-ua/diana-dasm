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

#include "softfloat_start.h"
#include "softfloatx80.h"
#include "softfloat-round-pack.h"
#include "softfloat-macros.h"

/*----------------------------------------------------------------------------
| Returns the result of converting the extended double-precision floating-
| point value `a' to the 16-bit two's complement integer format.  The
| conversion is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic - which means in particular that the conversion
| is rounded according to the current rounding mode. If `a' is a NaN or the
| conversion overflows, the integer indefinite value is returned.
*----------------------------------------------------------------------------*/

DI_INT16 floatx80_to_int16(floatx80_t a, float_status_t *status)
{
    DI_INT32 v32 = 0;
   if (floatx80_is_unsupported(a)) {
        float_raise(status, float_flag_invalid);
        return int16_indefinite;
   }

   v32 = floatx80_to_int32(a, status);

   if ((v32 > 32767) || (v32 < -32768)) {
        status->float_exception_flags = float_flag_invalid; // throw away other flags
        return int16_indefinite;
   }

   return (DI_INT16) v32;
}

/*----------------------------------------------------------------------------
| Returns the result of converting the extended double-precision floating-
| point value `a' to the 16-bit two's complement integer format.  The
| conversion is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic, except that the conversion is always rounded
| toward zero.  If `a' is a NaN or the conversion overflows, the integer
| indefinite value is returned.
*----------------------------------------------------------------------------*/

DI_INT16 floatx80_to_int16_round_to_zero(floatx80_t a, float_status_t *status)
{
    DI_INT32 v32 = 0;
   if (floatx80_is_unsupported(a)) {
        float_raise(status, float_flag_invalid);
        return int16_indefinite;
   }

   v32 = floatx80_to_int32_round_to_zero(a, status);

   if ((v32 > 32767) || (v32 < -32768)) {
        status->float_exception_flags = float_flag_invalid; // throw away other flags
        return int16_indefinite;
   }

   return (DI_INT16) v32;
}

/*----------------------------------------------------------------------------
| Separate the source extended double-precision floating point value `a'
| into its exponent and significand, store the significant back to the
| 'a' and return the exponent. The operation performed is a superset of
| the IEC/IEEE recommended logb(x) function.
*----------------------------------------------------------------------------*/

floatx80_t floatx80_extract(floatx80_t * a, float_status_t *status)
{
    DI_UINT64 aSig = extractFloatx80Frac(*a);
    DI_INT32 aExp = extractFloatx80Exp(*a);
    int   aSign = extractFloatx80Sign(*a);

    if (floatx80_is_unsupported(*a))
    {
        float_raise(status, float_flag_invalid);
        *a = floatx80_default_nan;
        return *a;
    }

    if (aExp == 0x7FFF) {
        if ((DI_UINT64) (aSig<<1))
        {
            *a = propagateFloatx80NaN(*a, status);
            return *a;
        }
        return packFloatx80(0, 0x7FFF, DI_CONST64(0x8000000000000000));
    }
    if (aExp == 0)
    {
        if (aSig == 0) {
            float_raise(status, float_flag_divbyzero);
            *a = packFloatx80(aSign, 0, 0);
            return packFloatx80(1, 0x7FFF, DI_CONST64(0x8000000000000000));
        }
        float_raise(status, float_flag_denormal);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
    }

    a->exp = (aSign << 15) + 0x3FFF;
    a->fraction = aSig;
    return int32_to_floatx80(aExp - 0x3FFF);
}

/*----------------------------------------------------------------------------
| Scales extended double-precision floating-point value in operand `a' by
| value `b'. The function truncates the value in the second operand 'b' to
| an integral value and adds that value to the exponent of the operand 'a'.
| The operation performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

floatx80_t floatx80_scale(floatx80_t a, floatx80_t b, float_status_t *status)
{
    DI_INT32 scale;
    int shiftCount;
    int bSign;
    int aSign;
    DI_INT32 aExp, bExp;
    DI_UINT64 aSig, bSig;

    // handle unsupported extended double-precision floating encodings
    if (floatx80_is_unsupported(a) || floatx80_is_unsupported(b))
    {
        float_raise(status, float_flag_invalid);
        return floatx80_default_nan;
    }

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    aSign = extractFloatx80Sign(a);
    bSig = extractFloatx80Frac(b);
    bExp = extractFloatx80Exp(b);
    bSign = extractFloatx80Sign(b);

    if (aExp == 0x7FFF) {
        if ((DI_UINT64) (aSig<<1) || ((bExp == 0x7FFF) && (DI_UINT64) (bSig<<1)))
        {
            return propagateFloatx80NaN_ex(a, b, status);
        }
        if ((bExp == 0x7FFF) && bSign) {
            float_raise(status, float_flag_invalid);
            return floatx80_default_nan;
        }
        if (bSig && (bExp == 0)) float_raise(status, float_flag_denormal);
        return a;
    }
    if (bExp == 0x7FFF) {
        if ((DI_UINT64) (bSig<<1)) return propagateFloatx80NaN_ex(a, b, status);
        if ((aExp | aSig) == 0) {
            if (! bSign) {
                float_raise(status, float_flag_invalid);
                return floatx80_default_nan;
            }
            return a;
        }
        if (aSig && (aExp == 0)) float_raise(status, float_flag_denormal);
        if (bSign) return packFloatx80(aSign, 0, 0);
        return packFloatx80(aSign, 0x7FFF, DI_CONST64(0x8000000000000000));
    }
    if (aExp == 0) {
        if (bSig && (bExp == 0)) float_raise(status, float_flag_denormal);
        if (aSig == 0) return a;
        float_raise(status, float_flag_denormal);
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);
        if (bExp < 0x3FFF)
            return normalizeRoundAndPackFloatx80(80, aSign, aExp, aSig, 0, status);
    }
    if (bExp == 0) {
        if (bSig == 0) return a;
        float_raise(status, float_flag_denormal);
        normalizeFloatx80Subnormal(bSig, &bExp, &bSig);
    }

    if (bExp > 0x400E) {
        /* generate appropriate overflow/underflow */
        return roundAndPackFloatx80(80, aSign,
                          bSign ? -0x3FFF : 0x7FFF, aSig, 0, status);
    }

    if (bExp < 0x3FFF) return a;

    shiftCount = 0x403E - bExp;
    bSig >>= shiftCount;
    scale = (DI_INT32) bSig;
    if (bSign) scale = -scale; /* -32768..32767 */
    return
        roundAndPackFloatx80(80, aSign, aExp+scale, aSig, 0, status);
}

/*----------------------------------------------------------------------------
| Determine extended-precision floating-point number class.
*----------------------------------------------------------------------------*/

float_class_t floatx80_class(floatx80_t a)
{
   DI_INT32 aExp = extractFloatx80Exp(a);
   DI_UINT64 aSig = extractFloatx80Frac(a);

   if(aExp == 0) {
       if (aSig == 0)
           return float_zero;

       /* denormal or pseudo-denormal */
       return float_denormal;
   }

   /* valid numbers have the MS bit set */
   if (!(aSig & DI_CONST64(0x8000000000000000)))
       return float_SNaN; /* report unsupported as SNaNs */

   if(aExp == 0x7fff) {
       int aSign = extractFloatx80Sign(a);

       if (((DI_UINT64) (aSig<< 1)) == 0)
           return (aSign) ? float_negative_inf : float_positive_inf;

       return (aSig & DI_CONST64(0x4000000000000000)) ? float_QNaN : float_SNaN;
   }

   return float_normalized;
}

/*----------------------------------------------------------------------------
| Compare  between  two extended precision  floating  point  numbers. Returns
| 'float_relation_equal'  if the operands are equal, 'float_relation_less' if
| the    value    'a'   is   less   than   the   corresponding   value   `b',
| 'float_relation_greater' if the value 'a' is greater than the corresponding
| value `b', or 'float_relation_unordered' otherwise.
*----------------------------------------------------------------------------*/

int floatx80_compare(floatx80_t a, floatx80_t b, float_status_t *status)
{
    int aSign = 0;
    int bSign = 0;
    float_class_t aClass = floatx80_class(a);
    float_class_t bClass = floatx80_class(b);
    DI_UINT64 aSig = 0;
    DI_INT32 aExp = 0;
    DI_UINT64 bSig = 0;
    DI_INT32 bExp = 0;
    int less_than = 0;

    if (aClass == float_SNaN || aClass == float_QNaN || bClass == float_SNaN || bClass == float_QNaN)
    {
        float_raise(status, float_flag_invalid);
        return float_relation_unordered;
    }

    if (aClass == float_denormal || bClass == float_denormal) {
        float_raise(status, float_flag_denormal);
    }

    aSign = extractFloatx80Sign(a);
    bSign = extractFloatx80Sign(b);

    if (aClass == float_zero) {
        if (bClass == float_zero) return float_relation_equal;
        return bSign ? float_relation_greater : float_relation_less;
    }

    if (bClass == float_zero || aSign != bSign) {
        return aSign ? float_relation_less : float_relation_greater;
    }

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    bSig = extractFloatx80Frac(b);
    bExp = extractFloatx80Exp(b);

    if (aClass == float_denormal)
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);

    if (bClass == float_denormal)
        normalizeFloatx80Subnormal(bSig, &bExp, &bSig);

    if (aExp == bExp && aSig == bSig)
        return float_relation_equal;

    less_than =
        aSign ? ((bExp < aExp) || ((bExp == aExp) && (bSig < aSig)))
              : ((aExp < bExp) || ((aExp == bExp) && (aSig < bSig)));

    if (less_than) return float_relation_less;
    return float_relation_greater;
}

/*----------------------------------------------------------------------------
| Compare  between  two extended precision  floating  point  numbers. Returns
| 'float_relation_equal'  if the operands are equal, 'float_relation_less' if
| the    value    'a'   is   less   than   the   corresponding   value   `b',
| 'float_relation_greater' if the value 'a' is greater than the corresponding
| value `b', or 'float_relation_unordered' otherwise. Quiet NaNs do not cause
| an exception.
*----------------------------------------------------------------------------*/

int floatx80_compare_quiet(floatx80_t a, floatx80_t b, float_status_t *status)
{
    int aSign = 0;
    int bSign = 0;
    float_class_t aClass = floatx80_class(a);
    float_class_t bClass = floatx80_class(b);
    DI_UINT64 aSig = 0;
    DI_INT32 aExp = 0;
    DI_UINT64 bSig = 0;
    DI_INT32 bExp = 0;
    int less_than = 0;

    if (aClass == float_SNaN || bClass == float_SNaN)
    {
        /* unsupported reported as SNaN */
        float_raise(status, float_flag_invalid);
        return float_relation_unordered;
    }

    if (aClass == float_QNaN || bClass == float_QNaN) {
        return float_relation_unordered;
    }

    if (aClass == float_denormal || bClass == float_denormal) {
        float_raise(status, float_flag_denormal);
    }

    aSign = extractFloatx80Sign(a);
    bSign = extractFloatx80Sign(b);

    if (aClass == float_zero) {
        if (bClass == float_zero) return float_relation_equal;
        return bSign ? float_relation_greater : float_relation_less;
    }

    if (bClass == float_zero || aSign != bSign) {
        return aSign ? float_relation_less : float_relation_greater;
    }

    aSig = extractFloatx80Frac(a);
    aExp = extractFloatx80Exp(a);
    bSig = extractFloatx80Frac(b);
    bExp = extractFloatx80Exp(b);

    if (aClass == float_denormal)
        normalizeFloatx80Subnormal(aSig, &aExp, &aSig);

    if (bClass == float_denormal)
        normalizeFloatx80Subnormal(bSig, &bExp, &bSig);

    if (aExp == bExp && aSig == bSig)
        return float_relation_equal;

    less_than =
        aSign ? ((bExp < aExp) || ((bExp == aExp) && (bSig < aSig)))
              : ((aExp < bExp) || ((aExp == bExp) && (aSig < bSig)));

    if (less_than) return float_relation_less;
    return float_relation_greater;
}
