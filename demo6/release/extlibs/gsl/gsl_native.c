/*
 * gsl_native.c - GNU Scientific Library subset
 * Tier 3: scientific computing
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xgsl_module(long long id) {
    switch (id) {
        case 0: return "statistics";
        case 1: return "random number generation";
        case 2: return "probability distributions";
        case 3: return "linear algebra";
        case 4: return "interpolation";
        case 5: return "numerical integration";
        case 6: return "ODE solvers";
        case 7: return "FFT";
        case 8: return "root finding";
        case 9: return "minimization";
        case 10: return "special functions";
        case 11: return "combinatorics";
        default: return "Unknown";
    }
}

const char* xgsl_rng_name(long long id) {
    switch (id) {
        case 0: return "mt19937";
        case 1: return "taus";
        case 2: return "gfsr4";
        case 3: return "ranlxs0";
        case 4: return "ranlxd1";
        case 5: return "cmrg";
        case 6: return "mrg";
        case 7: return "default";
        default: return "Unknown";
    }
}

const char* xgsl_dist_name(long long id) {
    switch (id) {
        case 0: return "gaussian";
        case 1: return "uniform";
        case 2: return "exponential";
        case 3: return "poisson";
        case 4: return "binomial";
        case 5: return "gamma";
        case 6: return "beta";
        case 7: return "chi-squared";
        case 8: return "lognormal";
        case 9: return "weibull";
        default: return "Unknown";
    }
}

/* Factorial */
double xgsl_factorial(long long n) {
    double result = 1.0;
    long long i;
    if (n < 0) return -1.0;
    for (i = 2; i <= n; i++) result *= (double)i;
    return result;
}

/* Binomial coefficient C(n,k) */
double xgsl_combination(long long n, long long k) {
    double result = 1.0;
    long long i;
    if (k < 0 || k > n) return 0.0;
    if (k > n - k) k = n - k;
    for (i = 0; i < k; i++) {
        result *= (double)(n - i);
        result /= (double)(i + 1);
    }
    return result;
}

/* Gaussian PDF: (1/(sigma*sqrt(2*pi))) * exp(-0.5*((x-mu)/sigma)^2) */
double xgsl_gaussian_pdf(double x, double mu, double sigma) {
    double z = (x - mu) / sigma;
    return (1.0 / (sigma * sqrt(2.0 * 3.14159265358979323846))) * exp(-0.5 * z * z);
}

/* Mean of comma-separated values */
double xgsl_mean(const char *data) {
    double sum = 0.0;
    int count = 0;
    const char *p = data;
    while (*p) {
        sum += atof(p);
        count++;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (count == 0) return 0.0;
    return sum / count;
}

/* Variance of comma-separated values */
double xgsl_variance(const char *data) {
    double vals[128];
    int count = 0;
    const char *p = data;
    double m, sum = 0.0;
    int i;
    while (*p && count < 128) {
        vals[count++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (count < 2) return 0.0;
    m = 0.0;
    for (i = 0; i < count; i++) m += vals[i];
    m /= count;
    for (i = 0; i < count; i++) {
        double d = vals[i] - m;
        sum += d * d;
    }
    return sum / (count - 1);
}

const char* xgsl_interp_type(long long id) {
    switch (id) {
        case 0: return "linear";
        case 1: return "polynomial";
        case 2: return "cubic spline";
        case 3: return "akima";
        case 4: return "steffen";
        default: return "Unknown";
    }
}
