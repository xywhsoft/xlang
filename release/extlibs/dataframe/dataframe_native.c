/*
 * dataframe_native.c - Tabular data analysis
 * Tier 4: data processing
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xdataframe_agg(long long id) {
    switch (id) {
        case 0: return "count";
        case 1: return "sum";
        case 2: return "mean";
        case 3: return "median";
        case 4: return "std";
        case 5: return "var";
        case 6: return "min";
        case 7: return "max";
        case 8: return "first";
        case 9: return "last";
        case 10: return "nunique";
        case 11: return "prod";
        default: return "Unknown";
    }
}

const char* xdataframe_join_type(long long id) {
    switch (id) {
        case 0: return "inner";
        case 1: return "left";
        case 2: return "right";
        case 3: return "outer";
        case 4: return "cross";
        case 5: return "semi";
        case 6: return "anti";
        default: return "Unknown";
    }
}

const char* xdataframe_sort_order(long long id) {
    switch (id) {
        case 0: return "ascending";
        case 1: return "descending";
        default: return "Unknown";
    }
}

const char* xdataframe_fill_method(long long id) {
    switch (id) {
        case 0: return "ffill";
        case 1: return "bfill";
        case 2: return "interpolate";
        case 3: return "zero";
        case 4: return "mean";
        default: return "Unknown";
    }
}

/* Compute mean of comma-separated values */
double xdataframe_mean(const char *data) {
    double sum = 0.0;
    int count = 0;
    const char *p = data;
    while (*p) {
        sum += atof(p);
        count++;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (count == 0) return 0.0;
    return sum / count;
}

/* Compute median of comma-separated values (simple insertion sort) */
double xdataframe_median(const char *data) {
    double vals[128];
    int count = 0;
    const char *p = data;
    int i, j;
    while (*p && count < 128) {
        vals[count++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (count == 0) return 0.0;
    /* insertion sort */
    for (i = 1; i < count; i++) {
        double key = vals[i];
        j = i - 1;
        while (j >= 0 && vals[j] > key) {
            vals[j+1] = vals[j];
            j--;
        }
        vals[j+1] = key;
    }
    if (count % 2 == 1) return vals[count/2];
    return (vals[count/2 - 1] + vals[count/2]) / 2.0;
}

const char* xdataframe_window_func(long long id) {
    switch (id) {
        case 0: return "rolling_mean";
        case 1: return "rolling_sum";
        case 2: return "rolling_std";
        case 3: return "cumsum";
        case 4: return "cumprod";
        case 5: return "cummax";
        case 6: return "cummin";
        case 7: return "shift";
        case 8: return "diff";
        case 9: return "pct_change";
        default: return "Unknown";
    }
}
