/*
 * xge_math_compat.c - TCC 数学函数兼容性
 *
 * TCC 的 msvcrt.def 不导出 float 后缀的数学函数。
 * tcc_libm.h 提供 static inline 版本，但某些情况下
 * 编译器仍会生成外部引用。此文件提供外部链接版本。
 *
 * 注意：不包含 math.h，避免与 TCC 内联定义冲突。
 */

/* 提供 double 版本的前向声明 */
extern double sin(double);
extern double cos(double);
extern double tan(double);
extern double asin(double);
extern double acos(double);
extern double atan(double);
extern double atan2(double, double);
extern double sqrt(double);
extern double fabs(double);
extern double floor(double);
extern double ceil(double);
extern double fmod(double, double);
extern double pow(double, double);
extern double log(double);
extern double exp(double);

float sinf(float x) { return (float)sin((double)x); }
float cosf(float x) { return (float)cos((double)x); }
float tanf(float x) { return (float)tan((double)x); }
float asinf(float x) { return (float)asin((double)x); }
float acosf(float x) { return (float)acos((double)x); }
float atanf(float x) { return (float)atan((double)x); }
float atan2f(float y, float x) { return (float)atan2((double)y, (double)x); }
float sqrtf(float x) { return (float)sqrt((double)x); }
float fabsf(float x) { return (float)fabs((double)x); }
float floorf(float x) { return (float)floor((double)x); }
float ceilf(float x) { return (float)ceil((double)x); }
float fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }
float powf(float x, float y) { return (float)pow((double)x, (double)y); }
float logf(float x) { return (float)log((double)x); }
float expf(float x) { return (float)exp((double)x); }

/* hypot/round/trunc 的 double 版本也不在 msvcrt.def 中，手动实现 */
float hypotf(float x, float y) {
    float a = x < 0.0f ? -x : x;
    float b = y < 0.0f ? -y : y;
    if (a < b) { float t = a; a = b; b = t; }
    if (a == 0.0f) return 0.0f;
    b /= a;
    return a * (float)sqrt((double)(1.0f + b * b));
}
float roundf(float x) {
    if (x >= 0.0f) return (float)floor((double)(x + 0.5f));
    return (float)ceil((double)(x - 0.5f));
}
float truncf(float x) {
    if (x >= 0.0f) return (float)floor((double)x);
    return (float)ceil((double)x);
}
