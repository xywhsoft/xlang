/*
 * faiss_native.c - Vector similarity search
 * Tier 4: machine_learning
 */
#include "xrt.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

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

/* 三个距离辅助函数共用严格的 CRT 数值解析，不承担 FAISS 索引后端。 */
static bool faiss_number(const char** cursor, double* value)
{
    const char* p = *cursor;
    char* end;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    errno = 0;
    *value = strtod(p, &end);
    if (p == end || errno == ERANGE || !isfinite(*value)) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.faiss", 1, "expected a finite vector component");
        return false;
    }
    p = end;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (*p == ',') {
        ++p;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
        if (*p != 0) { *cursor = p; return true; }
    } else if (*p == 0) {
        *cursor = p;
        return true;
    }
    xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.faiss", 1, "invalid vector separator or missing component");
    return false;
}

static double faiss_distance(const char* a, const char* b, int mode)
{
    double result = 0, normA = 0, normB = 0;
    xrtClearError();
    if (a == NULL || b == NULL) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.faiss", 1, "vector text is required");
        return 0;
    }
    while (*a != 0 && *b != 0) {
        double va, vb;
        if (!faiss_number(&a, &va) || !faiss_number(&b, &vb)) return 0;
        if (mode == 0) result = hypot(result, va - vb);
        else {
            result += va * vb;
            if (mode == 2) { normA = hypot(normA, va); normB = hypot(normB, vb); }
        }
        if (!isfinite(result) || !isfinite(normA) || !isfinite(normB)) {
            xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.faiss", 2, "vector calculation exceeds finite double range");
            return 0;
        }
    }
    if (*a != 0 || *b != 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.faiss", 1, "vector dimensions differ");
        return 0;
    }
    if (mode != 2) return result;
    /* 保留零向量的 0 约定；分步除法避免范数乘积溢出。 */
    return normA == 0 || normB == 0 ? 0 : (result / normA) / normB;
}

double xfaiss_l2_distance(const char* a, const char* b) { return faiss_distance(a, b, 0); }
double xfaiss_inner_product(const char* a, const char* b) { return faiss_distance(a, b, 1); }
double xfaiss_cosine(const char* a, const char* b) { return faiss_distance(a, b, 2); }

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
