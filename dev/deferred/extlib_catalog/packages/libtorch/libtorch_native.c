/*
 * libtorch_native.c - PyTorch C++ API
 * Tier 5: ai_framework
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xtorch_dtype(long long id) {
    switch (id) {
        case 0: return "float32";
        case 1: return "float64";
        case 2: return "float16";
        case 3: return "int64";
        case 4: return "int32";
        case 5: return "int8";
        case 6: return "bool";
        case 7: return "bfloat16";
        default: return "Unknown";
    }
}

const char* xtorch_device(long long id) {
    switch (id) {
        case 0: return "cpu";
        case 1: return "cuda";
        case 2: return "mps";
        case 3: return "xpu";
        default: return "Unknown";
    }
}

const char* xtorch_activation(long long id) {
    switch (id) {
        case 0: return "ReLU";
        case 1: return "Sigmoid";
        case 2: return "Tanh";
        case 3: return "LeakyReLU";
        case 4: return "GELU";
        case 5: return "SiLU";
        case 6: return "Softmax";
        default: return "Unknown";
    }
}

const char* xtorch_optimizer(long long id) {
    switch (id) {
        case 0: return "SGD";
        case 1: return "Adam";
        case 2: return "AdamW";
        case 3: return "RMSprop";
        case 4: return "Adagrad";
        default: return "Unknown";
    }
}

/* Compute tensor element count from shape "d1,d2,d3" */
long long xtorch_numel(const char *shape) {
    long long result = 1;
    const char *p = shape;
    while (*p) {
        long long val = 0;
        while (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); p++; }
        if (val > 0) result *= val;
        if (*p == ',') p++;
    }
    return result;
}

/* Softmax for single value with max subtraction */
double xtorch_softmax(double x, double max_val) {
    return exp(x - max_val);
}
