/*
 * lightgbm_native.c - LightGBM gradient boosting
 * Tier 4: machine_learning
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xlgbm_objective(long long id) {
    switch (id) {
        case 0: return "regression";
        case 1: return "binary";
        case 2: return "multiclass";
        case 3: return "cross_entropy";
        case 4: return "lambdarank";
        case 5: return "mape";
        case 6: return "huber";
        case 7: return "fair";
        default: return "Unknown";
    }
}

const char* xlgbm_boosting(long long id) {
    switch (id) {
        case 0: return "gbdt";
        case 1: return "rf";
        case 2: return "dart";
        case 3: return "goss";
        default: return "Unknown";
    }
}

const char* xlgbm_metric(long long id) {
    switch (id) {
        case 0: return "l1";
        case 1: return "l2";
        case 2: return "rmse";
        case 3: return "auc";
        case 4: return "binary_logloss";
        case 5: return "multi_logloss";
        case 6: return "ndcg";
        case 7: return "map";
        default: return "Unknown";
    }
}

const char* xlgbm_param(long long id) {
    switch (id) {
        case 0: return "num_leaves";
        case 1: return "max_depth";
        case 2: return "learning_rate";
        case 3: return "n_estimators";
        case 4: return "min_child_samples";
        case 5: return "subsample";
        case 6: return "colsample_bytree";
        case 7: return "reg_alpha";
        case 8: return "reg_lambda";
        case 9: return "feature_fraction";
        default: return "Unknown";
    }
}

/* Leaf-wise growth: compute split gain */
double xlgbm_split_gain(double gl, double hl, double gr, double hr, double lambda) {
    double left = (gl * gl) / (hl + lambda);
    double right = (gr * gr) / (hr + lambda);
    double total = (gl + gr) * (gl + gr) / (hl + hr + lambda);
    return 0.5 * (left + right - total);
}

/* Compute leaf weight */
double xlgbm_leaf_weight(double grad, double hess, double lambda) {
    return -grad / (hess + lambda);
}

const char* xlgbm_device(long long id) {
    switch (id) {
        case 0: return "cpu";
        case 1: return "gpu";
        case 2: return "cuda";
        default: return "Unknown";
    }
}
