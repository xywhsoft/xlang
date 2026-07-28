/* mpfr_native.c - MPFR-style multi-precision float helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Rounding mode names */
const char *xmpfr_rnd_name(long long mode) {
    switch(mode) {
        case 0: return "MPFR_RNDN"; /* round to nearest */
        case 1: return "MPFR_RNDZ"; /* round toward zero */
        case 2: return "MPFR_RNDU"; /* round toward +inf */
        case 3: return "MPFR_RNDD"; /* round toward -inf */
        case 4: return "MPFR_RNDA"; /* round away from zero */
        default: return "UNKNOWN";
    }
}

/* Default precision (bits) */
long long xmpfr_default_prec(void) { return 53; }

/* Precision for decimal digits */
long long xmpfr_prec_for_digits(long long digits) {
    /* bits = digits * log2(10) ≈ digits * 3.322 */
    return (long long)(digits * 3.322 + 1);
}

/* Decimal digits for precision */
long long xmpfr_digits_for_prec(long long prec) {
    /* digits = bits / log2(10) ≈ bits * 0.301 */
    return (long long)(prec * 0.30103);
}

/* Check if value is NaN */
long long xmpfr_is_nan(double val) {
    return (val != val) ? 1 : 0;
}

/* Check if value is infinite */
long long xmpfr_is_inf(double val) {
    if (val != val) return 0; /* NaN is not inf */
    return (val == val * 2 && val != 0) ? 1 : 0;
}

/* Check if value is zero */
long long xmpfr_is_zero(double val) {
    return (val == 0.0) ? 1 : 0;
}

/* Get sign (-1, 0, or 1) */
long long xmpfr_sign(double val) {
    if (val > 0) return 1;
    if (val < 0) return -1;
    return 0;
}

/* Compare two floats with tolerance */
long long xmpfr_cmp(double a, double b, double tol) {
    double diff = a - b;
    if (diff < 0) diff = -diff;
    if (diff <= tol) return 0;
    return (a < b) ? -1 : 1;
}

/* Format float with precision */
const char *xmpfr_format(double val, long long precision) {
    sprintf(str_buf, "%.*f", (int)precision, val);
    return str_buf;
}

/* Format in scientific notation */
const char *xmpfr_scientific(double val, long long precision) {
    sprintf(str_buf, "%.*e", (int)precision, val);
    return str_buf;
}

/* Constants */
double xmpfr_const_pi(void) { return 3.14159265358979323846; }
double xmpfr_const_e(void) { return 2.71828182845904523536; }
double xmpfr_const_log2(void) { return 0.69314718055994530942; }
double xmpfr_const_sqrt2(void) { return 1.41421356237309504880; }

/* Machine epsilon for double */
double xmpfr_epsilon(void) { return 2.220446049250313e-16; }

/* Min/max representable */
double xmpfr_min_normal(void) { return 2.2250738585072014e-308; }
double xmpfr_max_value(void) { return 1.7976931348623157e+308; }

/* Version info */
long long xmpfr_version_major(void) { return 4; }
long long xmpfr_version_minor(void) { return 2; }
