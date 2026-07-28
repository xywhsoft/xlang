/*
 * xgboost_native.c - Gradient boosting framework
 * Tier 4: machine_learning
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xxgb_objective(long long id) {
    switch (id) {
        case 0: return "reg:squarederror";
        case 1: return "reg:logistic";
        case 2: return "binary:logistic";
        case 3: return "binary:logitraw";
        case 4: return "multi:softmax";
        case 5: return "multi:softprob";
        case 6: return "rank:pairwise";
        default: return "Unknown";
    }
}

const char* xxgb_booster(long long id) {
    switch (id) {
        case 0: return "gbtree";
        case 1: return "gblinear";
        case 2: return "dart";
        default: return "Unknown";
    }
}

const char* xxgb_tree_method(long long id) {
    switch (id) {
        case 0: return "auto";
        case 1: return "exact";
        case 2: return "approx";
        case 3: return "hist";
        case 4: return "gpu_hist";
        default: return "Unknown";
    }
}

const char* xxgb_importance_type(long long id) {
    switch (id) {
        case 0: return "weight";
        case 1: return "gain";
        case 2: return "cover";
        case 3: return "total_gain";
        case 4: return "total_cover";
        default: return "Unknown";
    }
}

/* Sigmoid function */
double xxgb_sigmoid(double x) {
    return 1.0 / (1.0 + exp(-x));
}

/* Softmax for 2 classes, returns probability of class 1 */
double xxgb_predict_binary(double raw_score) {
    return 1.0 / (1.0 + exp(-raw_score));
}

/* Log loss for binary classification */
double xxgb_logloss(double y_true, double y_pred) {
    double p = y_pred;
    if (p < 1e-15) p = 1e-15;
    if (p > 1.0 - 1e-15) p = 1.0 - 1e-15;
    return -(y_true * log(p) + (1.0 - y_true) * log(1.0 - p));
}

const char* xxgb_param(long long id) {
    switch (id) {
        case 0: return "max_depth";
        case 1: return "learning_rate";
        case 2: return "n_estimators";
        case 3: return "subsample";
        case 4: return "colsample_bytree";
        case 5: return "min_child_weight";
        case 6: return "gamma";
        case 7: return "reg_alpha";
        case 8: return "reg_lambda";
        default: return "Unknown";
    }
}
