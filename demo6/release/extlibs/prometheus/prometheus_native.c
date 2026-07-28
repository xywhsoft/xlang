/*
 * prometheus_native.c - Metrics exposition
 * Tier 4: observability
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xprometheus_metric_type(long long id) {
    switch (id) {
        case 0: return "counter";
        case 1: return "gauge";
        case 2: return "histogram";
        case 3: return "summary";
        case 4: return "untyped";
        default: return "Unknown";
    }
}

const char* xprometheus_aggregation(long long id) {
    switch (id) {
        case 0: return "sum";
        case 1: return "avg";
        case 2: return "min";
        case 3: return "max";
        case 4: return "count";
        case 5: return "stddev";
        case 6: return "stdvar";
        case 7: return "quantile";
        default: return "Unknown";
    }
}

const char* xprometheus_function(long long id) {
    switch (id) {
        case 0: return "rate";
        case 1: return "irate";
        case 2: return "increase";
        case 3: return "delta";
        case 4: return "idelta";
        case 5: return "deriv";
        case 6: return "predict_linear";
        case 7: return "histogram_quantile";
        case 8: return "label_replace";
        case 9: return "absent";
        default: return "Unknown";
    }
}

const char* xprometheus_label(long long id) {
    switch (id) {
        case 0: return "job";
        case 1: return "instance";
        case 2: return "__name__";
        case 3: return "le";
        case 4: return "quantile";
        default: return "Unknown";
    }
}

/* Format a metric line: name{labels} value */
const char* xprometheus_format_metric(const char *name, const char *labels, double value) {
    if (labels[0] == '\0') {
        sprintf(buf, "%s %g", name, value);
    } else {
        sprintf(buf, "%s{%s} %g", name, labels, value);
    }
    return buf;
}

const char* xprometheus_alert_state(long long id) {
    switch (id) {
        case 0: return "inactive";
        case 1: return "pending";
        case 2: return "firing";
        default: return "Unknown";
    }
}

const char* xprometheus_storage(long long id) {
    switch (id) {
        case 0: return "local";
        case 1: return "remote_write";
        case 2: return "remote_read";
        case 3: return "thanos";
        case 4: return "cortex";
        case 5: return "mimir";
        default: return "Unknown";
    }
}
