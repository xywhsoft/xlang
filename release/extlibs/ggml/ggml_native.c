/*
 * ggml_native.c - Tensor library
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xggml_type(long long id) {
    switch (id) {
        case 0: return "F32";
        case 1: return "F16";
        case 2: return "Q4_0";
        case 3: return "Q4_1";
        case 4: return "Q5_0";
        case 5: return "Q5_1";
        case 6: return "Q8_0";
        case 7: return "Q8_1";
        case 8: return "I8";
        case 9: return "I16";
        case 10: return "I32";
        default: return "Unknown";
    }
}

const char* xggml_op(long long id) {
    switch (id) {
        case 0: return "ADD";
        case 1: return "MUL";
        case 2: return "MATMUL";
        case 3: return "RELU";
        case 4: return "GELU";
        case 5: return "SOFTMAX";
        case 6: return "RESHAPE";
        case 7: return "TRANSPOSE";
        case 8: return "NORM";
        default: return "Unknown";
    }
}

/* ReLU activation */
double xggml_relu(double x) {
    return x > 0 ? x : 0.0;
}

/* GELU approximation */
double xggml_gelu(double x) {
    return 0.5 * x * (1.0 + tanh(0.7978845608 * (x + 0.044715 * x * x * x)));
}

/* Element size in bytes for type */
long long xggml_type_size(long long type_id) {
    switch (type_id) {
        case 0: return 4;   /* F32 */
        case 1: return 2;   /* F16 */
        case 2: return 1;   /* Q4_0 approx */
        case 3: return 1;   /* Q4_1 approx */
        case 6: return 1;   /* Q8_0 approx */
        case 8: return 1;   /* I8 */
        case 9: return 2;   /* I16 */
        case 10: return 4;  /* I32 */
        default: return 0;
    }
}

const char* xggml_backend(long long id) {
    switch (id) {
        case 0: return "cpu";
        case 1: return "cuda";
        case 2: return "metal";
        case 3: return "vulkan";
        case 4: return "sycl";
        default: return "Unknown";
    }
}
