/*
 * ceres_native.c - Nonlinear least squares solver
 * Tier 4: scientific computing
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xceres_solver_type(long long id) {
    switch (id) {
        case 0: return "dense QR";
        case 1: return "dense normal equations";
        case 2: return "sparse normal equations";
        case 3: return "iterative Schur";
        case 4: return "sparse Schur";
        case 5: return "dense Schur";
        case 6: return "cgnr";
        default: return "Unknown";
    }
}

const char* xceres_loss_function(long long id) {
    switch (id) {
        case 0: return "trivial";
        case 1: return "huber";
        case 2: return "soft l1";
        case 3: return "cauchy";
        case 4: return "arctan";
        case 5: return "tolerant";
        case 6: return "tukey";
        default: return "Unknown";
    }
}

const char* xceres_termination(long long id) {
    switch (id) {
        case 0: return "convergence";
        case 1: return "no convergence";
        case 2: return "failure";
        case 3: return "user success";
        case 4: return "user failure";
        default: return "Unknown";
    }
}

const char* xceres_parameterization(long long id) {
    switch (id) {
        case 0: return "identity";
        case 1: return "quaternion";
        case 2: return "homogeneous vector";
        case 3: return "subspace";
        case 4: return "product";
        default: return "Unknown";
    }
}

/* Simple 1D Newton's method: find root of f(x) = x^2 - target
 * Returns sqrt(target) approximated */
double xceres_sqrt_newton(double target) {
    double x = target > 1.0 ? target : 1.0;
    int i;
    for (i = 0; i < 50; i++) {
        double fx = x * x - target;
        double dfx = 2.0 * x;
        if (fabs(dfx) < 1e-15) break;
        x = x - fx / dfx;
        if (fabs(fx) < 1e-12) break;
    }
    return x;
}

/* Linear regression: y = a*x + b
 * Input: points as "x1,y1,x2,y2,..."
 * Returns "a,b" */
const char* xceres_linear_fit(const char *points) {
    double xs[64], ys[64];
    int n = 0;
    const char *p = points;
    int i;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;

    while (*p && n < 64) {
        xs[n] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        ys[n] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        n++;
    }
    if (n < 2) return "0,0";
    for (i = 0; i < n; i++) {
        sx += xs[i];
        sy += ys[i];
        sxx += xs[i] * xs[i];
        sxy += xs[i] * ys[i];
    }
    double denom = n * sxx - sx * sx;
    if (fabs(denom) < 1e-15) return "0,0";
    double a = (n * sxy - sx * sy) / denom;
    double b = (sy - a * sx) / n;
    sprintf(buf, "%g,%g", a, b);
    return buf;
}

const char* xceres_ordering_type(long long id) {
    switch (id) {
        case 0: return "natural";
        case 1: return "user";
        case 2: return "automatic";
        default: return "Unknown";
    }
}
