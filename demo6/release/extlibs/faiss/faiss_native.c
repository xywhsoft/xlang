/*
 * faiss_native.c - Vector similarity search
 * Tier 4: machine_learning
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xfaiss_index_type(long long id) {
    switch (id) {
        case 0: return "IndexFlatL2";
        case 1: return "IndexFlatIP";
        case 2: return "IndexIVFFlat";
        case 3: return "IndexIVFPQ";
        case 4: return "IndexHNSW";
        case 5: return "IndexLSH";
        case 6: return "IndexScalarQuantizer";
        default: return "Unknown";
    }
}

const char* xfaiss_metric(long long id) {
    switch (id) {
        case 0: return "L2";
        case 1: return "InnerProduct";
        case 2: return "L1";
        case 3: return "Linf";
        case 4: return "Canberra";
        case 5: return "BrayCurtis";
        case 6: return "JensenShannon";
        default: return "Unknown";
    }
}

/* L2 distance between two vectors encoded as "v1,v2,v3" */
double xfaiss_l2_distance(const char *a, const char *b) {
    double sum = 0.0;
    const char *pa = a, *pb = b;
    while (*pa && *pb) {
        double va = 0, vb = 0;
        int neg_a = 0, neg_b = 0;
        if (*pa == '-') { neg_a = 1; pa++; }
        if (*pb == '-') { neg_b = 1; pb++; }
        while (*pa >= '0' && *pa <= '9') { va = va * 10 + (*pa - '0'); pa++; }
        while (*pb >= '0' && *pb <= '9') { vb = vb * 10 + (*pb - '0'); pb++; }
        if (neg_a) va = -va;
        if (neg_b) vb = -vb;
        double d = va - vb;
        sum += d * d;
        if (*pa == ',') pa++;
        if (*pb == ',') pb++;
    }
    return sqrt(sum);
}

/* Inner product between two vectors */
double xfaiss_inner_product(const char *a, const char *b) {
    double dot = 0.0;
    const char *pa = a, *pb = b;
    while (*pa && *pb) {
        double va = 0, vb = 0;
        while (*pa >= '0' && *pa <= '9') { va = va * 10 + (*pa - '0'); pa++; }
        while (*pb >= '0' && *pb <= '9') { vb = vb * 10 + (*pb - '0'); pb++; }
        dot += va * vb;
        if (*pa == ',') pa++;
        if (*pb == ',') pb++;
    }
    return dot;
}

/* Cosine similarity */
double xfaiss_cosine(const char *a, const char *b) {
    double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
    const char *pa = a, *pb = b;
    while (*pa && *pb) {
        double va = 0, vb = 0;
        while (*pa >= '0' && *pa <= '9') { va = va * 10 + (*pa - '0'); pa++; }
        while (*pb >= '0' && *pb <= '9') { vb = vb * 10 + (*pb - '0'); pb++; }
        dot += va * vb;
        norm_a += va * va;
        norm_b += vb * vb;
        if (*pa == ',') pa++;
        if (*pb == ',') pb++;
    }
    if (norm_a == 0 || norm_b == 0) return 0.0;
    return dot / (sqrt(norm_a) * sqrt(norm_b));
}

const char* xfaiss_quantizer(long long id) {
    switch (id) {
        case 0: return "QT_8bit";
        case 1: return "QT_4bit";
        case 2: return "QT_8bit_uniform";
        case 3: return "QT_4bit_uniform";
        case 4: return "QT_fp16";
        default: return "Unknown";
    }
}
