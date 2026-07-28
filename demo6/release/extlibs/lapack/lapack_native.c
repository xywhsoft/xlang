/*
 * lapack_native.c - Linear Algebra PACKage
 * Tier 3: scientific computing
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xlapack_routine_category(long long id) {
    switch (id) {
        case 0: return "linear solve";
        case 1: return "least squares";
        case 2: return "eigenvalue";
        case 3: return "singular value";
        case 4: return "matrix factorization";
        default: return "Unknown";
    }
}

const char* xlapack_routine_name(long long id) {
    switch (id) {
        case 0: return "dgesv";
        case 1: return "dgels";
        case 2: return "dsyev";
        case 3: return "dgesvd";
        case 4: return "dgetrf";
        case 5: return "dgetri";
        case 6: return "dpotrf";
        case 7: return "dtrtri";
        case 8: return "dgeev";
        case 9: return "dsyevd";
        case 10: return "dgesdd";
        case 11: return "dgelss";
        default: return "Unknown";
    }
}

const char* xlapack_info_code(long long code) {
    if (code == 0) return "success";
    if (code < 0) return "illegal argument";
    if (code > 0) return "convergence failure";
    return "Unknown";
}

/* Solve 2x2 linear system Ax=b using Cramer's rule
 * Matrix A passed as "a11,a12,a21,a22", b as "b1,b2"
 * Returns solution as "x1,x2" */
const char* xlapack_solve2x2(const char *mat, const char *rhs) {
    double a[4], b[2];
    const char *p;
    int i;

    p = mat;
    for (i = 0; i < 4; i++) {
        a[i] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    p = rhs;
    for (i = 0; i < 2; i++) {
        b[i] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }

    double det = a[0]*a[3] - a[1]*a[2];
    if (fabs(det) < 1e-15) {
        return "singular";
    }
    double x1 = (b[0]*a[3] - b[1]*a[1]) / det;
    double x2 = (a[0]*b[1] - a[2]*b[0]) / det;
    sprintf(buf, "%g,%g", x1, x2);
    return buf;
}

/* Determinant of 2x2 matrix "a11,a12,a21,a22" */
double xlapack_det2x2(const char *mat) {
    double a[4];
    const char *p = mat;
    int i;
    for (i = 0; i < 4; i++) {
        a[i] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    return a[0]*a[3] - a[1]*a[2];
}

/* Trace of matrix passed as comma-separated row-major with given dimension */
double xlapack_trace(const char *mat, long long n) {
    double sum = 0.0;
    const char *p = mat;
    int i, j;
    for (i = 0; i < (int)n; i++) {
        for (j = 0; j < (int)n; j++) {
            double v = atof(p);
            if (i == j) sum += v;
            while (*p && *p != ',') p++;
            if (*p == ',') p++;
        }
    }
    return sum;
}

const char* xlapack_norm_type(long long id) {
    switch (id) {
        case 0: return "1-norm (max column sum)";
        case 1: return "infinity-norm (max row sum)";
        case 2: return "Frobenius norm";
        case 3: return "2-norm (spectral)";
        default: return "Unknown";
    }
}
