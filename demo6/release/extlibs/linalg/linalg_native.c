/* 轻量线性代数：每次调用独立写入调用方输出，不使用全局临时结果。 */
#include <math.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XLINALG_DOMAIN "xlang.extlib.linalg"
#define XLINALG_EPSILON 1e-12

static void xlinalg_error(cstr sMessage)
{
    xrtSetErrorInfo(XERR_VALUE, XLINALG_DOMAIN, 0, sMessage);
}

double xlin_dot2(double ax, double ay, double bx, double by)
{
    return ax * bx + ay * by;
}

double xlin_dot3(double ax, double ay, double az, double bx, double by, double bz)
{
    return ax * bx + ay * by + az * bz;
}

double xlin_mag2(double x, double y)
{
    return hypot(x, y);
}

double xlin_mag3(double x, double y, double z)
{
    return hypot(hypot(x, y), z);
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
    double fLength = hypot(x, y);
    if (px == NULL || py == NULL) {
        xlinalg_error("normalize2 output is missing");
        return 0;
    }
    if (!(fLength > XLINALG_EPSILON) || !isfinite(fLength)) {
        xlinalg_error("cannot normalize a zero or non-finite vector");
        return 0;
    }
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
    *px = ay * bz - az * by;
    *py = az * bx - ax * bz;
    *pz = ax * by - ay * bx;
    xrtClearError();
    return 1;
}

double xlin_det2(double a, double b, double c, double d)
{
    return a * d - b * c;
}

int xlin_inverse2_into(double a, double b, double c, double d,
    double* pa, double* pb, double* pc, double* pd)
{
    double fDeterminant = a * d - b * c;
    if (pa == NULL || pb == NULL || pc == NULL || pd == NULL) {
        xlinalg_error("inverse2 output is missing");
        return 0;
    }
    if (!(fabs(fDeterminant) > XLINALG_EPSILON) || !isfinite(fDeterminant)) {
        xlinalg_error("matrix is singular or non-finite");
        return 0;
    }
    *pa = d / fDeterminant;
    *pb = -b / fDeterminant;
    *pc = -c / fDeterminant;
    *pd = a / fDeterminant;
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
    double fLength = hypot(ax, ay) * hypot(bx, by);
    double fCosine;
    if (!(fLength > XLINALG_EPSILON) || !isfinite(fLength)) {
        xlinalg_error("cannot calculate an angle with a zero or non-finite vector");
        return NAN;
    }
    fCosine = (ax * bx + ay * by) / fLength;
    if (fCosine > 1.0) { fCosine = 1.0; }
    if (fCosine < -1.0) { fCosine = -1.0; }
    xrtClearError();
    return acos(fCosine);
}

double xlin_dist2(double x1, double y1, double x2, double y2)
{
    return hypot(x2 - x1, y2 - y1);
}

double xlin_dist3(double x1, double y1, double z1, double x2, double y2, double z2)
{
    return hypot(hypot(x2 - x1, y2 - y1), z2 - z1);
}

double xlin_lerp(double a, double b, double t)
{
    return a + (b - a) * t;
}

double xlin_clamp(double fValue, double fMinimum, double fMaximum)
{
    if (fMinimum > fMaximum) {
        xlinalg_error("clamp minimum must not exceed maximum");
        return NAN;
    }
    if (fValue < fMinimum) { return fMinimum; }
    if (fValue > fMaximum) { return fMaximum; }
    return fValue;
}

double xlin_deg2rad(double fDegrees)
{
    return fDegrees * 0.01745329251994329576923690768489;
}

double xlin_rad2deg(double fRadians)
{
    return fRadians * 57.295779513082320876798154814105;
}

const char* xlin_error_message(void)
{
    const xerror* pError = xrtGetError();
    return pError != NULL ? xrtErrorMessage(pError) : "linear algebra operation failed";
}
