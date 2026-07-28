/*
 * libsvm_native.c - Support Vector Machine
 * Tier 4: machine_learning
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xsvm_type(long long id) {
    switch (id) {
        case 0: return "C_SVC";
        case 1: return "NU_SVC";
        case 2: return "ONE_CLASS";
        case 3: return "EPSILON_SVR";
        case 4: return "NU_SVR";
        default: return "Unknown";
    }
}

const char* xsvm_kernel(long long id) {
    switch (id) {
        case 0: return "linear";
        case 1: return "polynomial";
        case 2: return "rbf";
        case 3: return "sigmoid";
        case 4: return "precomputed";
        default: return "Unknown";
    }
}

/* RBF kernel: K(x,y) = exp(-gamma * ||x-y||^2)
   Input: "x1,x2,...;y1,y2,..." format */
double xsvm_rbf_kernel(const char *x, const char *y, double gamma) {
    double sum_sq = 0.0;
    const char *px = x, *py = y;
    while (*px && *py) {
        double vx = 0, vy = 0;
        while (*px && *px != ',' && *px != ';') { vx = vx * 10 + (*px - '0'); px++; }
        while (*py && *py != ',' && *py != ';') { vy = vy * 10 + (*py - '0'); py++; }
        double d = vx - vy;
        sum_sq += d * d;
        if (*px == ',' || *px == ';') px++;
        if (*py == ',' || *py == ';') py++;
    }
    return exp(-gamma * sum_sq);
}

/* Linear kernel: dot product */
double xsvm_linear_kernel(const char *x, const char *y) {
    double dot = 0.0;
    const char *px = x, *py = y;
    while (*px && *py) {
        double vx = 0, vy = 0;
        while (*px && *px != ',' && *px != ';') { vx = vx * 10 + (*px - '0'); px++; }
        while (*py && *py != ',' && *py != ';') { vy = vy * 10 + (*py - '0'); py++; }
        dot += vx * vy;
        if (*px == ',' || *px == ';') px++;
        if (*py == ',' || *py == ';') py++;
    }
    return dot;
}

const char* xsvm_param(long long id) {
    switch (id) {
        case 0: return "C";
        case 1: return "gamma";
        case 2: return "degree";
        case 3: return "coef0";
        case 4: return "nu";
        case 5: return "epsilon";
        case 6: return "cache_size";
        case 7: return "tol";
        default: return "Unknown";
    }
}

/* Hinge loss: max(0, 1 - y*f(x)) */
double xsvm_hinge_loss(double y, double fx) {
    double margin = y * fx;
    if (margin >= 1.0) return 0.0;
    return 1.0 - margin;
}
