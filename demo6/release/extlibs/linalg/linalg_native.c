/* linalg_native.c - Linear algebra helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Vector dot product (2D) */
double xlin_dot2(double ax, double ay, double bx, double by) {
    return ax * bx + ay * by;
}

/* Vector dot product (3D) */
double xlin_dot3(double ax, double ay, double az, double bx, double by, double bz) {
    return ax * bx + ay * by + az * bz;
}

/* Vector magnitude (2D) */
double xlin_mag2(double x, double y) {
    return sqrt(x * x + y * y);
}

/* Vector magnitude (3D) */
double xlin_mag3(double x, double y, double z) {
    return sqrt(x * x + y * y + z * z);
}

/* Normalize vector (2D) - returns x component, y stored in static */
static double norm_y;
double xlin_norm2_x(double x, double y) {
    double m = sqrt(x * x + y * y);
    if (m < 1e-10) { norm_y = 0; return 0; }
    norm_y = y / m;
    return x / m;
}
double xlin_norm2_y(void) { return norm_y; }

/* Cross product (3D) - returns x component */
static double cross_y, cross_z;
double xlin_cross_x(double ax, double ay, double az, double bx, double by, double bz) {
    cross_y = az * bx - ax * bz;
    cross_z = ax * by - ay * bx;
    return ay * bz - az * by;
}
double xlin_cross_y(void) { return cross_y; }
double xlin_cross_z(void) { return cross_z; }

/* 2x2 matrix determinant */
double xlin_det2(double a, double b, double c, double d) {
    return a * d - b * c;
}

/* 2x2 matrix inverse - returns a component */
static double inv_b, inv_c, inv_d;
double xlin_inv2_a(double a, double b, double c, double d) {
    double det = a * d - b * c;
    if (fabs(det) < 1e-10) { inv_b = inv_c = inv_d = 0; return 0; }
    inv_b = -b / det;
    inv_c = -c / det;
    inv_d = a / det;
    return d / det;
}
double xlin_inv2_b(void) { return inv_b; }
double xlin_inv2_c(void) { return inv_c; }
double xlin_inv2_d(void) { return inv_d; }

/* 2x2 matrix multiply - returns [0][0] component */
static double mul_b, mul_c, mul_d;
double xlin_mul2_a(double a1, double b1, double c1, double d1,
                   double a2, double b2, double c2, double d2) {
    mul_b = a1 * b2 + b1 * d2;
    mul_c = c1 * a2 + d1 * c2;
    mul_d = c1 * b2 + d1 * d2;
    return a1 * a2 + b1 * c2;
}
double xlin_mul2_b(void) { return mul_b; }
double xlin_mul2_c(void) { return mul_c; }
double xlin_mul2_d(void) { return mul_d; }

/* Angle between vectors (2D) in radians */
double xlin_angle2(double ax, double ay, double bx, double by) {
    double dot = ax * bx + ay * by;
    double ma = sqrt(ax * ax + ay * ay);
    double mb = sqrt(bx * bx + by * by);
    if (ma < 1e-10 || mb < 1e-10) return 0;
    double cos_theta = dot / (ma * mb);
    if (cos_theta > 1.0) cos_theta = 1.0;
    if (cos_theta < -1.0) cos_theta = -1.0;
    return acos(cos_theta);
}

/* Distance between points (2D) */
double xlin_dist2(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1, dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

/* Distance between points (3D) */
double xlin_dist3(double x1, double y1, double z1, double x2, double y2, double z2) {
    double dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

/* Lerp (linear interpolation) */
double xlin_lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

/* Clamp value */
double xlin_clamp(double val, double min_val, double max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

/* Degrees to radians */
double xlin_deg2rad(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
}

/* Radians to degrees */
double xlin_rad2deg(double rad) {
    return rad * 180.0 / 3.14159265358979323846;
}
