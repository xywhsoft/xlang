/*
 * openvino_native.c - Intel inference engine
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xov_device(long long id) {
    switch (id) {
        case 0: return "CPU";
        case 1: return "GPU";
        case 2: return "NPU";
        case 3: return "AUTO";
        case 4: return "MULTI";
        case 5: return "HETERO";
        default: return "Unknown";
    }
}

const char* xov_precision(long long id) {
    switch (id) {
        case 0: return "FP32";
        case 1: return "FP16";
        case 2: return "INT8";
        case 3: return "INT4";
        case 4: return "BF16";
        default: return "Unknown";
    }
}

const char* xov_layout(long long id) {
    switch (id) {
        case 0: return "NCHW";
        case 1: return "NHWC";
        case 2: return "NC";
        case 3: return "C";
        default: return "Unknown";
    }
}

const char* xov_perf_mode(long long id) {
    switch (id) {
        case 0: return "LATENCY";
        case 1: return "THROUGHPUT";
        case 2: return "CUMULATIVE_THROUGHPUT";
        default: return "Unknown";
    }
}

/* Compute memory for tensor: N*C*H*W*bytes_per_element */
long long xov_tensor_bytes(long long n, long long c, long long h, long long w, long long elem_size) {
    return n * c * h * w * elem_size;
}

const char* xov_model_format(long long id) {
    switch (id) {
        case 0: return "IR";
        case 1: return "ONNX";
        case 2: return "PaddlePaddle";
        case 3: return "TensorFlow";
        case 4: return "PyTorch";
        default: return "Unknown";
    }
}
