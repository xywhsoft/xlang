/*
 * embeddings_native.c - Text embeddings
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xemb_model(long long id) {
    switch (id) {
        case 0: return "all-MiniLM-L6-v2";
        case 1: return "all-mpnet-base-v2";
        case 2: return "bge-small-en";
        case 3: return "bge-large-en";
        case 4: return "e5-small";
        case 5: return "text-embedding-3-small";
        default: return "Unknown";
    }
}

/* Embedding dimensions */
long long xemb_dim(long long model_id) {
    switch (model_id) {
        case 0: return 384;   /* MiniLM */
        case 1: return 768;   /* mpnet */
        case 2: return 384;   /* bge-small */
        case 3: return 1024;  /* bge-large */
        case 4: return 384;   /* e5-small */
        case 5: return 1536;  /* text-embedding-3-small */
        default: return 0;
    }
}

/* Max tokens for model */
long long xemb_max_tokens(long long model_id) {
    switch (model_id) {
        case 0: return 256;
        case 1: return 512;
        case 2: return 512;
        case 3: return 512;
        case 4: return 512;
        case 5: return 8191;
        default: return 0;
    }
}

/* L2 normalize a vector encoded as "v1,v2,v3" - returns normalized */
const char* xemb_normalize(const char *vec) {
    double vals[32];
    int n = 0;
    const char *p = vec;
    double norm = 0.0;
    while (*p && n < 32) {
        double v = 0;
        while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
        vals[n] = v;
        norm += v * v;
        n++;
        if (*p == ',') p++;
    }
    norm = sqrt(norm);
    int j = 0;
    int i;
    for (i = 0; i < n; i++) {
        double nv = (norm > 0) ? vals[i] / norm : 0;
        j += snprintf(buf + j, sizeof(buf) - j, "%.4f", nv);
        if (i < n - 1) buf[j++] = ',';
    }
    buf[j] = '\0';
    return buf;
}

const char* xemb_pooling(long long id) {
    switch (id) {
        case 0: return "mean";
        case 1: return "cls";
        case 2: return "max";
        default: return "Unknown";
    }
}
