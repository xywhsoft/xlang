/* nlopt_native.c - NLopt optimization helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Algorithm names */
const char *xnlopt_algo_name(long long algo) {
    switch(algo) {
        case 0: return "GN_DIRECT";
        case 1: return "GN_DIRECT_L";
        case 2: return "GN_CRS2_LM";
        case 10: return "LN_NELDERMEAD";
        case 11: return "LN_SBPLX";
        case 20: return "LD_MMA";
        case 21: return "LD_CCSAQ";
        case 24: return "LD_LBFGS";
        case 40: return "LN_COBYLA";
        case 41: return "LN_NEWUOA";
        case 42: return "LN_BOBYQA";
        default: return "UNKNOWN";
    }
}

/* Check if algorithm is global */
long long xnlopt_is_global(long long algo) {
    return (algo >= 0 && algo < 10) ? 1 : 0;
}

/* Check if algorithm is local */
long long xnlopt_is_local(long long algo) {
    return (algo >= 10) ? 1 : 0;
}

/* Check if algorithm uses gradient */
long long xnlopt_uses_gradient(long long algo) {
    return (algo >= 20 && algo < 30) ? 1 : 0;
}

/* Result code names */
const char *xnlopt_result_name(long long result) {
    switch(result) {
        case 1: return "SUCCESS";
        case 2: return "STOPVAL_REACHED";
        case 3: return "FTOL_REACHED";
        case 4: return "XTOL_REACHED";
        case 5: return "MAXEVAL_REACHED";
        case 6: return "MAXTIME_REACHED";
        case -1: return "FAILURE";
        case -2: return "INVALID_ARGS";
        case -3: return "OUT_OF_MEMORY";
        case -4: return "ROUNDOFF_LIMITED";
        case -5: return "FORCED_STOP";
        default: return "UNKNOWN";
    }
}

/* Check if result is success */
long long xnlopt_is_success(long long result) {
    return (result > 0) ? 1 : 0;
}

/* Check if result is failure */
long long xnlopt_is_failure(long long result) {
    return (result < 0) ? 1 : 0;
}

/* Rosenbrock function (test function) */
double xnlopt_rosenbrock(double x, double y) {
    double a = 1.0 - x;
    double b = y - x * x;
    return a * a + 100.0 * b * b;
}

/* Sphere function (test function) */
double xnlopt_sphere(double x, double y) {
    return x * x + y * y;
}

/* Rastrigin function (test function) */
double xnlopt_rastrigin(double x, double y) {
    double A = 10.0;
    return 2.0 * A + (x * x - A * cos(2.0 * 3.14159265358979 * x)) +
                     (y * y - A * cos(2.0 * 3.14159265358979 * y));
}

/* Simple gradient descent step */
double xnlopt_grad_step(double x, double grad, double learning_rate) {
    return x - learning_rate * grad;
}

/* Line search (simple backtracking) */
double xnlopt_line_search(double f0, double grad, double alpha, double c) {
    /* Armijo condition: f(x + alpha*d) <= f(x) + c*alpha*grad */
    return f0 + c * alpha * grad;
}

/* Default max evaluations */
long long xnlopt_default_maxeval(void) { return 10000; }

/* Default tolerance */
double xnlopt_default_ftol(void) { return 1e-8; }
double xnlopt_default_xtol(void) { return 1e-8; }

/* Version info */
long long xnlopt_version_major(void) { return 2; }
long long xnlopt_version_minor(void) { return 7; }
