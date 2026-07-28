/*
 * autodiff_native.c - Automatic differentiation
 * Tier 5: ai_framework
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xad_op(long long id) {
    switch (id) {
        case 0: return "add";
        case 1: return "mul";
        case 2: return "matmul";
        case 3: return "relu";
        case 4: return "sigmoid";
        case 5: return "tanh";
        case 6: return "exp";
        case 7: return "log";
        case 8: return "sum";
        case 9: return "mean";
        default: return "Unknown";
    }
}

/* Derivative of sigmoid: sigmoid(x) * (1 - sigmoid(x)) */
double xad_sigmoid_grad(double x) {
    double s = 1.0 / (1.0 + exp(-x));
    return s * (1.0 - s);
}

/* Derivative of tanh: 1 - tanh(x)^2 */
double xad_tanh_grad(double x) {
    double t = tanh(x);
    return 1.0 - t * t;
}

/* Derivative of ReLU */
double xad_relu_grad(double x) {
    return x > 0 ? 1.0 : 0.0;
}

/* Gradient of MSE loss: 2*(pred - target)/n */
double xad_mse_grad(double pred, double target, long long n) {
    return 2.0 * (pred - target) / (double)n;
}

const char* xad_mode(long long id) {
    switch (id) {
        case 0: return "forward";
        case 1: return "reverse";
        default: return "Unknown";
    }
}
