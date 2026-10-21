#ifndef XLINALG_NATIVE_H
#define XLINALG_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

double xlin_dot2(double ax, double ay, double bx, double by);
double xlin_dot3(double ax, double ay, double az, double bx, double by, double bz);
double xlin_mag2(double x, double y);
double xlin_mag3(double x, double y, double z);
/* 所有 _into 输出指针须非空且两两不同；失败时不写入任何输出。 */
int xlin_normalize2_into(double x, double y, double* px, double* py);
int xlin_cross3_into(double ax, double ay, double az, double bx, double by,
    double bz, double* px, double* py, double* pz);
double xlin_det2(double a, double b, double c, double d);
int xlin_inverse2_into(double a, double b, double c, double d,
    double* pa, double* pb, double* pc, double* pd);
int xlin_multiply2_into(double a1, double b1, double c1, double d1,
    double a2, double b2, double c2, double d2,
    double* pa, double* pb, double* pc, double* pd);
double xlin_angle2(double ax, double ay, double bx, double by);
double xlin_dist2(double x1, double y1, double x2, double y2);
double xlin_dist3(double x1, double y1, double z1,
    double x2, double y2, double z2);
double xlin_lerp(double a, double b, double t);

#ifdef __cplusplus
}
#endif
#endif
