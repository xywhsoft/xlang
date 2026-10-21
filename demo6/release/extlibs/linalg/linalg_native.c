/* 轻量线性代数：每次调用独立写入调用方输出，不使用全局临时结果。 */
#include <math.h>
#include <float.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_MATH
#define XRT_MODULE_MATH
#endif
#include "xrt.h"
#include "include/linalg_native.h"

#define XLINALG_DOMAIN "xlang.extlib.linalg"

static void xlinalg_error(cstr sMessage)
{
    xrtSetErrorInfo(XERR_VALUE, XLINALG_DOMAIN, 0, sMessage);
}

double xlin_dot2(double ax, double ay, double bx, double by)
{
    xrtClearError();
    return ax * bx + ay * by;
}

double xlin_dot3(double ax, double ay, double az, double bx, double by, double bz)
{
    xrtClearError();
    return ax * bx + ay * by + az * bz;
}

double xlin_mag2(double x, double y)
{
    xrtClearError();
    return xrtMathHypot(x, y);
}

double xlin_mag3(double x, double y, double z)
{
    xrtClearError();
    return xrtMathHypot(xrtMathHypot(x, y), z);
}

static xvalue* xlinalg_values(const double* pValues, size_t iCount)
{
    size_t iIndex;
    xvalue* pResult = xrtValueArray();
    if (pResult == NULL) {
        return xrtValueNull();
    }
    for (iIndex = 0; iIndex < iCount; ++iIndex) {
        if (!xrtValueArrayAppendNew(pResult, xrtValueFloat(pValues[iIndex]))) {
            xrtValueRelease(pResult);
            return xrtValueNull();
        }
    }
    return pResult;
}

int xlin_normalize2_into(double x, double y, double* px, double* py)
{
    double fScale = fmax(fabs(x), fabs(y));
    double fLength;
    if (px == NULL || py == NULL) {
        xlinalg_error("normalize2 output is missing");
        return 0;
    }
    if (px == py) {
        xlinalg_error("normalize2 outputs must be distinct");
        return 0;
    }
    if (!(fScale > 0.0) || !isfinite(x) || !isfinite(y)) {
        xlinalg_error("cannot normalize a zero or non-finite vector");
        return 0;
    }
    x /= fScale;
    y /= fScale;
    fLength = xrtMathHypot(x, y);
    *px = x / fLength;
    *py = y / fLength;
    xrtClearError();
    return 1;
}

int xlin_cross3_into(double ax, double ay, double az, double bx, double by, double bz,
    double* px, double* py, double* pz)
{
    if (px == NULL || py == NULL || pz == NULL) {
        xlinalg_error("cross3 output is missing");
        return 0;
    }
    if (px == py || px == pz || py == pz) {
        xlinalg_error("cross3 outputs must be distinct");
        return 0;
    }
    *px = ay * bz - az * by;
    *py = az * bx - ax * bz;
    *pz = ax * by - ay * bx;
    xrtClearError();
    return 1;
}

double xlin_det2(double a, double b, double c, double d)
{
    xrtClearError();
    return a * d - b * c;
}

int xlin_inverse2_into(double a, double b, double c, double d,
    double* pa, double* pb, double* pc, double* pd)
{
    double fScale1, fScale2;
    double fDeterminant;
    double fProducts;
    double ra, rb, rc, rd;
    if (pa == NULL || pb == NULL || pc == NULL || pd == NULL) {
        xlinalg_error("inverse2 output is missing");
        return 0;
    }
    if (pa == pb || pa == pc || pa == pd || pb == pc || pb == pd || pc == pd) {
        xlinalg_error("inverse2 outputs must be distinct");
        return 0;
    }
    fScale1 = fmax(fabs(a), fabs(b));
    fScale2 = fmax(fabs(c), fabs(d));
    if (!(fScale1 > 0.0) || !(fScale2 > 0.0) ||
        !isfinite(a) || !isfinite(b) || !isfinite(c) || !isfinite(d)) {
        xlinalg_error("matrix is singular or non-finite");
        return 0;
    }
    a /= fScale1;
    b /= fScale1;
    c /= fScale2;
    d /= fScale2;
    fDeterminant = a * d - b * c;
    fProducts = fabs(a * d) + fabs(b * c);
    if (!(fabs(fDeterminant) > 4.0 * DBL_EPSILON * fProducts)) {
        xlinalg_error("matrix is singular or numerically unstable");
        return 0;
    }
    ra = (d / fDeterminant) / fScale1;
    rb = (-b / fDeterminant) / fScale2;
    rc = (-c / fDeterminant) / fScale1;
    rd = (a / fDeterminant) / fScale2;
    if (!isfinite(ra) || !isfinite(rb) || !isfinite(rc) || !isfinite(rd)) {
        xlinalg_error("inverse matrix is not representable");
        return 0;
    }
    *pa = ra;
    *pb = rb;
    *pc = rc;
    *pd = rd;
    xrtClearError();
    return 1;
}

int xlin_multiply2_into(double a1, double b1, double c1, double d1,
    double a2, double b2, double c2, double d2,
    double* pa, double* pb, double* pc, double* pd)
{
    if (pa == NULL || pb == NULL || pc == NULL || pd == NULL) {
        xlinalg_error("multiply2 output is missing");
        return 0;
    }
    if (pa == pb || pa == pc || pa == pd || pb == pc || pb == pd || pc == pd) {
        xlinalg_error("multiply2 outputs must be distinct");
        return 0;
    }
    *pa = a1 * a2 + b1 * c2;
    *pb = a1 * b2 + b1 * d2;
    *pc = c1 * a2 + d1 * c2;
    *pd = c1 * b2 + d1 * d2;
    xrtClearError();
    return 1;
}

/* xlang 多返回值经由 xvalue 数组跨越 ABI，避免 extern byref 的调用器依赖。 */
xvalue* xlin_normalize2(double x, double y)
{
    double aResult[2];
    if (!xlin_normalize2_into(x, y, &aResult[0], &aResult[1])) {
        return xrtValueNull();
    }
    return xlinalg_values(aResult, 2);
}

xvalue* xlin_cross3(double ax, double ay, double az, double bx, double by, double bz)
{
    double aResult[3];
    if (!xlin_cross3_into(ax, ay, az, bx, by, bz,
            &aResult[0], &aResult[1], &aResult[2])) {
        return xrtValueNull();
    }
    return xlinalg_values(aResult, 3);
}

xvalue* xlin_inverse2(double a, double b, double c, double d)
{
    double aResult[4];
    if (!xlin_inverse2_into(a, b, c, d,
            &aResult[0], &aResult[1], &aResult[2], &aResult[3])) {
        return xrtValueNull();
    }
    return xlinalg_values(aResult, 4);
}

xvalue* xlin_multiply2(double a1, double b1, double c1, double d1,
    double a2, double b2, double c2, double d2)
{
    double aResult[4];
    if (!xlin_multiply2_into(a1, b1, c1, d1, a2, b2, c2, d2,
            &aResult[0], &aResult[1], &aResult[2], &aResult[3])) {
        return xrtValueNull();
    }
    return xlinalg_values(aResult, 4);
}

double xlin_angle2(double ax, double ay, double bx, double by)
{
    double fScaleA = fmax(fabs(ax), fabs(ay));
    double fScaleB = fmax(fabs(bx), fabs(by));
    double fCross;
    double fDot;
    if (!(fScaleA > 0.0) || !(fScaleB > 0.0) ||
        !isfinite(ax) || !isfinite(ay) ||
        !isfinite(bx) || !isfinite(by)) {
        xlinalg_error("cannot calculate an angle with a zero or non-finite vector");
        return NAN;
    }
    ax /= fScaleA;
    ay /= fScaleA;
    bx /= fScaleB;
    by /= fScaleB;
    /* atan2 保留近乎平行或反向向量的小角度，避免 acos 在端点舍入。 */
    fCross = ax * by - ay * bx;
    fDot = ax * bx + ay * by;
    xrtClearError();
    return atan2(fabs(fCross), fDot);
}

double xlin_dist2(double x1, double y1, double x2, double y2)
{
    xrtClearError();
    return xrtMathHypot(x2 - x1, y2 - y1);
}

double xlin_dist3(double x1, double y1, double z1, double x2, double y2, double z2)
{
    xrtClearError();
    return xrtMathHypot(xrtMathHypot(x2 - x1, y2 - y1), z2 - z1);
}

double xlin_lerp(double a, double b, double t)
{
    xrtClearError();
    /* 异号端点先相减可能溢出，即使插值或轻微外推的结果仍有限。 */
    if (signbit(a) != signbit(b)) {
        return (1.0 - t) * a + t * b;
    }
    return a + (b - a) * t;
}
