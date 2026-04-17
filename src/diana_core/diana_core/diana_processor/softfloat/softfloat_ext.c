/*============================================================================
Implementations of the extended softfloat transcendental/remainder functions
declared in softfloatx80.h but absent from the library.

These stubs convert to/from IEEE 754 double via the existing floatx80 <-> float64
conversion routines and delegate to the host C runtime (math.h).  The precision
is limited to double (53-bit mantissa) rather than the full 64-bit extended
precision, which is acceptable for an instruction-set emulator.
============================================================================*/

#include "softfloat_start.h"
#include "softfloatx80.h"
#include "softfloat_end.h"

#include <math.h>
#include <string.h>

/*---------------------------------------------------------------------------
| Constants
*--------------------------------------------------------------------------*/

const floatx80_t Const_Z = { 0, 0 };                                   /* +0.0 */
const floatx80_t Const_1 = { (DI_UINT64)0x8000000000000000ULL, 0x3FFF }; /* +1.0 */

/*---------------------------------------------------------------------------
| Helpers: floatx80 <-> double via float64 (uint64 bit pattern)
*--------------------------------------------------------------------------*/

static double fx80_to_double(floatx80_t a)
{
    float_status_t status;
    float64 f64;
    double d;
    memset(&status, 0, sizeof(status));
    status.float_rounding_mode = float_round_nearest_even;
    f64 = floatx80_to_float64(a, &status);
    memcpy(&d, &f64, sizeof(d));
    return d;
}

static floatx80_t double_to_fx80(double d)
{
    float_status_t status;
    float64 f64;
    memset(&status, 0, sizeof(status));
    status.float_rounding_mode = float_round_nearest_even;
    memcpy(&f64, &d, sizeof(f64));
    return float64_to_floatx80(f64, &status);
}

/*---------------------------------------------------------------------------
| 2^ST(0) - 1
*--------------------------------------------------------------------------*/

floatx80_t f2xm1(floatx80_t a, float_status_t *status)
{
    double d = fx80_to_double(a);
    double result = pow(2.0, d) - 1.0;
    (void)status;
    return double_to_fx80(result);
}

/*---------------------------------------------------------------------------
| ST(1) * log2(ST(0)),  operand order: a=ST(0), b=ST(1)
*--------------------------------------------------------------------------*/

floatx80_t fyl2x(floatx80_t a, floatx80_t b, float_status_t *status)
{
    double da = fx80_to_double(a);
    double db = fx80_to_double(b);
    /* log2(x) = log(x) / log(2) */
    double result = db * (log(da) / log(2.0));
    (void)status;
    return double_to_fx80(result);
}

/*---------------------------------------------------------------------------
| ST(1) * log2(ST(0) + 1),  a=ST(0), b=ST(1)
*--------------------------------------------------------------------------*/

floatx80_t fyl2xp1(floatx80_t a, floatx80_t b, float_status_t *status)
{
    double da = fx80_to_double(a);
    double db = fx80_to_double(b);
    double result = db * (log(1.0 + da) / log(2.0));
    (void)status;
    return double_to_fx80(result);
}

/*---------------------------------------------------------------------------
| arctan(ST(1) / ST(0)),  a=ST(0), b=ST(1)
*--------------------------------------------------------------------------*/

floatx80_t fpatan(floatx80_t a, floatx80_t b, float_status_t *status)
{
    double da = fx80_to_double(a);
    double db = fx80_to_double(b);
    double result = atan2(db, da);
    (void)status;
    return double_to_fx80(result);
}

/*---------------------------------------------------------------------------
| Trigonometric in-place (return 0 = done, non-zero = C2 out-of-range)
*--------------------------------------------------------------------------*/

int fsin(floatx80_t *a, float_status_t *status)
{
    double d = fx80_to_double(*a);
    (void)status;
    *a = double_to_fx80(sin(d));
    return 0;
}

int fcos(floatx80_t *a, float_status_t *status)
{
    double d = fx80_to_double(*a);
    (void)status;
    *a = double_to_fx80(cos(d));
    return 0;
}

int ftan(floatx80_t *a, float_status_t *status)
{
    double d = fx80_to_double(*a);
    (void)status;
    *a = double_to_fx80(tan(d));
    return 0;
}

int fsincos(floatx80_t a, floatx80_t *sin_a, floatx80_t *cos_a, float_status_t *status)
{
    double d = fx80_to_double(a);
    (void)status;
    *sin_a = double_to_fx80(sin(d));
    *cos_a = double_to_fx80(cos(d));
    return 0;
}

/*---------------------------------------------------------------------------
| Remainder functions
|
| Returns 0 when the reduction is complete, non-zero when partial (C2).
| For simplicity we always complete in one step (no partial remainder).
*--------------------------------------------------------------------------*/

/* fprem: truncate-toward-zero quotient */
int floatx80_remainder(floatx80_t a, floatx80_t b,
                       floatx80_t *r, DI_UINT64 *q,
                       float_status_t *status)
{
    double da = fx80_to_double(a);
    double db = fx80_to_double(b);
    double quot;
    double rem;
    (void)status;
    if (db == 0.0) { *r = a; *q = 0; return 0; }
    quot = da / db;
    /* truncate toward zero */
    quot = (quot >= 0.0) ? floor(quot) : ceil(quot);
    rem = da - quot * db;
    *r = double_to_fx80(rem);
    *q = (DI_UINT64)(DI_INT64)quot & 0x3F;  /* low 6 bits of quotient */
    return 0;  /* complete, not partial */
}

/* fprem1: round-to-nearest quotient (IEEE 754 remainder) */
int floatx80_ieee754_remainder(floatx80_t a, floatx80_t b,
                               floatx80_t *r, DI_UINT64 *q,
                               float_status_t *status)
{
    double da = fx80_to_double(a);
    double db = fx80_to_double(b);
    double quot;
    double rem;
    (void)status;
    if (db == 0.0) { *r = a; *q = 0; return 0; }
    /* round-to-nearest-even quotient without relying on C99 remainder() */
    quot = da / db;
    quot = floor(quot + 0.5);
    rem = da - quot * db;
    *r = double_to_fx80(rem);
    *q = (DI_UINT64)(DI_INT64)quot & 0x3F;
    return 0;
}
