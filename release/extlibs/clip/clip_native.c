/*
 * clip_native.c - CLIP multimodal model
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xclip_model(long long id) {
    switch (id) {
        case 0: return "ViT-B/32";
        case 1: return "ViT-B/16";
        case 2: return "ViT-L/14";
        case 3: return "ViT-L/14@336px";
        case 4: return "RN50";
        case 5: return "RN101";
        default: return "Unknown";
    }
}

/* Cosine similarity between two normalized vectors */
double xclip_cosine_similarity(const char *a, const char *b) {
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

const char* xclip_preprocess(long long id) {
    switch (id) {
        case 0: return "resize";
        case 1: return "center_crop";
        case 2: return "normalize";
        case 3: return "to_tensor";
        default: return "Unknown";
    }
}

/* Embedding dimensions for model */
long long xclip_embed_dim(long long model_id) {
    switch (model_id) {
        case 0: return 512;   /* ViT-B/32 */
        case 1: return 512;   /* ViT-B/16 */
        case 2: return 768;   /* ViT-L/14 */
        case 3: return 768;   /* ViT-L/14@336px */
        case 4: return 1024;  /* RN50 */
        case 5: return 512;   /* RN101 */
        default: return 0;
    }
}

const char* xclip_tokenizer(long long id) {
    switch (id) {
        case 0: return "bpe";
        case 1: return "simple";
        default: return "Unknown";
    }
}
