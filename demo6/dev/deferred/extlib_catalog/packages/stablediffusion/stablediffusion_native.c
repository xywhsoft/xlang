/*
 * stablediffusion_native.c - Image generation
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xsd_model(long long id) {
    switch (id) {
        case 0: return "sd-v1-5";
        case 1: return "sd-v2-1";
        case 2: return "sdxl-1.0";
        case 3: return "sd-turbo";
        case 4: return "sdxl-turbo";
        default: return "Unknown";
    }
}

const char* xsd_sampler(long long id) {
    switch (id) {
        case 0: return "euler";
        case 1: return "euler_a";
        case 2: return "heun";
        case 3: return "dpm2";
        case 4: return "dpmpp_2m";
        case 5: return "ddim";
        case 6: return "lcm";
        default: return "Unknown";
    }
}

const char* xsd_scheduler(long long id) {
    switch (id) {
        case 0: return "default";
        case 1: return "discrete";
        case 2: return "karras";
        case 3: return "exponential";
        case 4: return "ays";
        default: return "Unknown";
    }
}

/* Compute total steps from cfg and sampling params */
long long xsd_total_steps(long long n_steps, long long batch) {
    return n_steps * batch;
}

const char* xsd_output_format(long long id) {
    switch (id) {
        case 0: return "png";
        case 1: return "jpeg";
        case 2: return "webp";
        case 3: return "bmp";
        default: return "Unknown";
    }
}

/* Standard resolutions */
long long xsd_resolution(long long id) {
    switch (id) {
        case 0: return 512;   /* SD 1.x */
        case 1: return 768;   /* SD 2.x */
        case 2: return 1024;  /* SDXL */
        default: return 0;
    }
}
