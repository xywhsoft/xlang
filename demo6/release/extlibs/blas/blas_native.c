/*
 * blas_native.c - Basic Linear Algebra Subprograms
 * Tier 3: scientific computing
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xblas_level(long long id) {
    switch (id) {
        case 1: return "Level 1 (vector-vector)";
        case 2: return "Level 2 (matrix-vector)";
        case 3: return "Level 3 (matrix-matrix)";
        default: return "Unknown";
    }
}

const char* xblas_operation(long long id) {
    switch (id) {
        case 0: return "no-transpose";
        case 1: return "transpose";
        case 2: return "conjugate-transpose";
        default: return "Unknown";
    }
}

const char* xblas_uplo(long long id) {
    switch (id) {
        case 0: return "upper";
        case 1: return "lower";
        default: return "Unknown";
    }
}

const char* xblas_diag(long long id) {
    switch (id) {
        case 0: return "non-unit";
        case 1: return "unit";
        default: return "Unknown";
    }
}

const char* xblas_side(long long id) {
    switch (id) {
        case 0: return "left";
        case 1: return "right";
        default: return "Unknown";
    }
}

/* Level 1: dot product of two vectors (passed as comma-separated values) */
double xblas_dot(const char *vec_a, const char *vec_b) {
    double sum = 0.0;
    double a[64], b[64];
    int na = 0, nb = 0;
    int i;
    const char *p;

    p = vec_a;
    while (*p && na < 64) {
        a[na++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    p = vec_b;
    while (*p && nb < 64) {
        b[nb++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    int n = na < nb ? na : nb;
    for (i = 0; i < n; i++) sum += a[i] * b[i];
    return sum;
}

/* Level 1: Euclidean norm */
double xblas_nrm2(const char *vec) {
    double sum = 0.0;
    const char *p = vec;
    while (*p) {
        double v = atof(p);
        sum += v * v;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    return sqrt(sum);
}

/* Level 1: axpy y = a*x + y, returns result as string */
const char* xblas_axpy(double alpha, const char *vec_x, const char *vec_y) {
    double x[64], y[64];
    int nx = 0, ny = 0;
    int i, pos = 0;
    const char *p;

    p = vec_x;
    while (*p && nx < 64) {
        x[nx++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    p = vec_y;
    while (*p && ny < 64) {
        y[ny++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    int n = nx < ny ? nx : ny;
    for (i = 0; i < n; i++) {
        double val = alpha * x[i] + y[i];
        if (i > 0) buf[pos++] = ',';
        pos += sprintf(buf + pos, "%g", val);
    }
    buf[pos] = '\0';
    return buf;
}

const char* xblas_routine_name(long long id) {
    switch (id) {
        case 0: return "sdot";
        case 1: return "ddot";
        case 2: return "snrm2";
        case 3: return "dnrm2";
        case 4: return "saxpy";
        case 5: return "daxpy";
        case 6: return "sgemv";
        case 7: return "dgemv";
        case 8: return "sgemm";
        case 9: return "dgemm";
        case 10: return "ssyrk";
        case 11: return "dsyrk";
        default: return "Unknown";
    }
}
