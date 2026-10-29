/*
 * llama_native.c - Local LLM inference (llama.cpp style)
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xllama_model_type(long long id) {
    switch (id) {
        case 0: return "llama";
        case 1: return "mistral";
        case 2: return "phi";
        case 3: return "gemma";
        case 4: return "qwen";
        case 5: return "chatglm";
        case 6: return "falcon";
        case 7: return "mpt";
        default: return "Unknown";
    }
}

const char* xllama_quant_type(long long id) {
    switch (id) {
        case 0: return "Q4_0";
        case 1: return "Q4_1";
        case 2: return "Q5_0";
        case 3: return "Q5_1";
        case 4: return "Q8_0";
        case 5: return "F16";
        case 6: return "F32";
        default: return "Unknown";
    }
}

const char* xllama_sampler(long long id) {
    switch (id) {
        case 0: return "greedy";
        case 1: return "top_k";
        case 2: return "top_p";
        case 3: return "min_p";
        case 4: return "temperature";
        case 5: return "mirostat";
        default: return "Unknown";
    }
}

/* Softmax temperature scaling */
double xllama_apply_temperature(double logit, double temp) {
    if (temp <= 0.0) return logit;
    return logit / temp;
}

/* Top-p nucleus: returns threshold index given sorted probs "p1,p2,p3" */
long long xllama_top_p_index(const char *probs, double p) {
    double cumulative = 0.0;
    const char *ptr = probs;
    long long idx = 0;
    while (*ptr) {
        double val = 0;
        while (*ptr >= '0' && *ptr <= '9') { val = val * 10 + (*ptr - '0'); ptr++; }
        if (*ptr == '.') { ptr++; double frac = 0.1; while (*ptr >= '0' && *ptr <= '9') { val += (*ptr - '0') * frac; frac *= 0.1; ptr++; } }
        cumulative += val;
        if (cumulative >= p) return idx;
        if (*ptr == ',') ptr++;
        idx++;
    }
    return idx - 1;
}

const char* xllama_pooling(long long id) {
    switch (id) {
        case 0: return "none";
        case 1: return "mean";
        case 2: return "cls";
        default: return "Unknown";
    }
}

/* Compute perplexity from log-likelihood */
double xllama_perplexity(double avg_neg_loglik) {
    return exp(avg_neg_loglik);
}
