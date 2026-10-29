/*
 * tensorrt_native.c - NVIDIA TensorRT inference
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xtrt_precision(long long id) {
    switch (id) {
        case 0: return "FP32";
        case 1: return "FP16";
        case 2: return "INT8";
        case 3: return "INT4";
        case 4: return "FP8";
        default: return "Unknown";
    }
}

const char* xtrt_layer(long long id) {
    switch (id) {
        case 0: return "Convolution";
        case 1: return "Pooling";
        case 2: return "Activation";
        case 3: return "FullyConnected";
        case 4: return "Softmax";
        case 5: return "Normalization";
        case 6: return "ElementWise";
        case 7: return "Plugin";
        default: return "Unknown";
    }
}

const char* xtrt_builder_flag(long long id) {
    switch (id) {
        case 0: return "FP16";
        case 1: return "INT8";
        case 2: return "DEBUG";
        case 3: return "GPU_FALLBACK";
        case 4: return "STRICT_TYPES";
        case 5: return "REFIT";
        case 6: return "DISABLE_TIMING_CACHE";
        case 7: return "TF32";
        default: return "Unknown";
    }
}

const char* xtrt_profile(long long id) {
    switch (id) {
        case 0: return "min";
        case 1: return "opt";
        case 2: return "max";
        default: return "Unknown";
    }
}

/* Compute workspace size in MB */
long long xtrt_workspace_mb(long long bytes) {
    return bytes / (1024 * 1024);
}

const char* xtrt_network_flag(long long id) {
    switch (id) {
        case 0: return "EXPLICIT_BATCH";
        case 1: return "EXPLICIT_PRECISION";
        default: return "Unknown";
    }
}
