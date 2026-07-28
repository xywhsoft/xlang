/*
 * hnswlib_native.c - HNSW approximate nearest neighbor
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xhnsw_space(long long id) {
    switch (id) {
        case 0: return "l2";
        case 1: return "ip";
        case 2: return "cosine";
        default: return "Unknown";
    }
}

/* Optimal M parameter based on dimension */
long long xhnsw_optimal_m(long long dim) {
    if (dim <= 32) return 16;
    if (dim <= 128) return 32;
    if (dim <= 512) return 48;
    return 64;
}

/* Memory per element in bytes: M*2*4 + dim*4 + overhead */
long long xhnsw_memory_per_element(long long m, long long dim) {
    return m * 2 * 4 + dim * 4 + 64;
}

/* Estimate search time complexity: O(log(n) * ef * M) */
long long xhnsw_search_ops(long long n, long long ef, long long m) {
    long long log_n = 1;
    long long temp = n;
    while (temp > 1) { log_n++; temp /= 2; }
    return log_n * ef * m;
}

const char* xhnsw_param(long long id) {
    switch (id) {
        case 0: return "M";
        case 1: return "ef_construction";
        case 2: return "ef_search";
        case 3: return "max_elements";
        case 4: return "random_seed";
        default: return "Unknown";
    }
}

/* L2 distance squared */
double xhnsw_l2sq(const char *a, const char *b) {
    double sum = 0.0;
    const char *pa = a, *pb = b;
    while (*pa && *pb) {
        double va = 0, vb = 0;
        while (*pa >= '0' && *pa <= '9') { va = va * 10 + (*pa - '0'); pa++; }
        while (*pb >= '0' && *pb <= '9') { vb = vb * 10 + (*pb - '0'); pb++; }
        double d = va - vb;
        sum += d * d;
        if (*pa == ',') pa++;
        if (*pb == ',') pb++;
    }
    return sum;
}
