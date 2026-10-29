/*
 * ncnn_native.c - NCNN mobile inference
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xncnn_layer(long long id) {
    switch (id) {
        case 0: return "Convolution";
        case 1: return "Pooling";
        case 2: return "ReLU";
        case 3: return "InnerProduct";
        case 4: return "Softmax";
        case 5: return "Concat";
        case 6: return "Split";
        case 7: return "Dropout";
        case 8: return "BatchNorm";
        default: return "Unknown";
    }
}

const char* xncnn_vulkan_device(long long id) {
    switch (id) {
        case 0: return "default";
        case 1: return "integrated";
        case 2: return "discrete";
        default: return "Unknown";
    }
}

/* Compute output size for conv: (W - K + 2P) / S + 1 */
long long xncnn_conv_output(long long w, long long k, long long p, long long s) {
    return (w - k + 2 * p) / s + 1;
}

/* Pooling output: (W - K) / S + 1 */
long long xncnn_pool_output(long long w, long long k, long long s) {
    return (w - k) / s + 1;
}

const char* xncnn_param(long long id) {
    switch (id) {
        case 0: return "num_output";
        case 1: return "kernel_w";
        case 2: return "stride_w";
        case 3: return "pad_w";
        case 4: return "dilation_w";
        case 5: return "group";
        default: return "Unknown";
    }
}
