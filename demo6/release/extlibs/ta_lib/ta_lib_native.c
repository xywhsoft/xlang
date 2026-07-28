/* ta_lib_native.c - Technical analysis indicators */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Simple Moving Average over last n values (comma-separated) */
double xta_sma(const char *csv, int period) {
    double sum = 0; int count = 0;
    const char *p = csv;
    double vals[256]; int n = 0;
    while (*p && n < 256) {
        vals[n++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (n < period || period <= 0) return 0;
    for (int i = n - period; i < n; i++) sum += vals[i];
    return sum / period;
}

/* Exponential Moving Average (returns last EMA value) */
double xta_ema(const char *csv, int period) {
    double vals[256]; int n = 0;
    const char *p = csv;
    while (*p && n < 256) {
        vals[n++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (n < period || period <= 0) return 0;
    double k = 2.0 / (period + 1.0);
    double ema = vals[0];
    for (int i = 1; i < n; i++) {
        ema = vals[i] * k + ema * (1.0 - k);
    }
    return ema;
}

/* RSI (Relative Strength Index) - returns last RSI */
double xta_rsi(const char *csv, int period) {
    double vals[256]; int n = 0;
    const char *p = csv;
    while (*p && n < 256) {
        vals[n++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (n < period + 1 || period <= 0) return 50.0;
    double gain = 0, loss = 0;
    for (int i = 1; i <= period; i++) {
        double diff = vals[i] - vals[i-1];
        if (diff > 0) gain += diff; else loss -= diff;
    }
    double avg_gain = gain / period;
    double avg_loss = loss / period;
    for (int i = period + 1; i < n; i++) {
        double diff = vals[i] - vals[i-1];
        if (diff > 0) { avg_gain = (avg_gain * (period-1) + diff) / period; avg_loss = (avg_loss * (period-1)) / period; }
        else { avg_gain = (avg_gain * (period-1)) / period; avg_loss = (avg_loss * (period-1) - diff) / period; }
    }
    if (avg_loss < 1e-10) return 100.0;
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

/* MACD: returns macd line value; signal/histogram via getters */
static double macd_signal, macd_hist;
double xta_macd(const char *csv, int fast, int slow, int signal_period) {
    double vals[256]; int n = 0;
    const char *p = csv;
    while (*p && n < 256) {
        vals[n++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (n < slow) { macd_signal = macd_hist = 0; return 0; }
    double kf = 2.0 / (fast + 1), ks = 2.0 / (slow + 1);
    double ema_f = vals[0], ema_s = vals[0];
    double macd_vals[256]; int mn = 0;
    for (int i = 1; i < n; i++) {
        ema_f = vals[i] * kf + ema_f * (1.0 - kf);
        ema_s = vals[i] * ks + ema_s * (1.0 - ks);
        macd_vals[mn++] = ema_f - ema_s;
    }
    double ksig = 2.0 / (signal_period + 1);
    double sig = macd_vals[0];
    for (int i = 1; i < mn; i++) {
        sig = macd_vals[i] * ksig + sig * (1.0 - ksig);
    }
    double macd_line = macd_vals[mn - 1];
    macd_signal = sig;
    macd_hist = macd_line - sig;
    return macd_line;
}
double xta_macd_signal(void) { return macd_signal; }
double xta_macd_hist(void) { return macd_hist; }

/* Bollinger Bands: returns middle; upper/lower via getters */
static double bb_upper, bb_lower;
double xta_bollinger(const char *csv, int period, double num_std) {
    double vals[256]; int n = 0;
    const char *p = csv;
    while (*p && n < 256) {
        vals[n++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (n < period || period <= 0) { bb_upper = bb_lower = 0; return 0; }
    double sum = 0;
    for (int i = n - period; i < n; i++) sum += vals[i];
    double mean = sum / period;
    double var = 0;
    for (int i = n - period; i < n; i++) { double d = vals[i] - mean; var += d * d; }
    double std = sqrt(var / period);
    bb_upper = mean + num_std * std;
    bb_lower = mean - num_std * std;
    return mean;
}
double xta_bollinger_upper(void) { return bb_upper; }
double xta_bollinger_lower(void) { return bb_lower; }

/* Standard deviation of last n values */
double xta_stddev(const char *csv, int period) {
    double vals[256]; int n = 0;
    const char *p = csv;
    while (*p && n < 256) {
        vals[n++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (n < period || period <= 0) return 0;
    double sum = 0;
    for (int i = n - period; i < n; i++) sum += vals[i];
    double mean = sum / period;
    double var = 0;
    for (int i = n - period; i < n; i++) { double d = vals[i] - mean; var += d * d; }
    return sqrt(var / period);
}

/* Highest high / Lowest low over period */
double xta_highest(const char *csv, int period) {
    double vals[256]; int n = 0;
    const char *p = csv;
    while (*p && n < 256) {
        vals[n++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (n == 0) return 0;
    int start = n - period; if (start < 0) start = 0;
    double hi = vals[start];
    for (int i = start + 1; i < n; i++) if (vals[i] > hi) hi = vals[i];
    return hi;
}

double xta_lowest(const char *csv, int period) {
    double vals[256]; int n = 0;
    const char *p = csv;
    while (*p && n < 256) {
        vals[n++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (n == 0) return 0;
    int start = n - period; if (start < 0) start = 0;
    double lo = vals[start];
    for (int i = start + 1; i < n; i++) if (vals[i] < lo) lo = vals[i];
    return lo;
}

/* Rate of change (percent) */
double xta_roc(const char *csv, int period) {
    double vals[256]; int n = 0;
    const char *p = csv;
    while (*p && n < 256) {
        vals[n++] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    if (n <= period) return 0;
    double prev = vals[n - 1 - period];
    if (fabs(prev) < 1e-10) return 0;
    return (vals[n-1] - prev) / prev * 100.0;
}

/* Indicator name lookup */
const char* xta_name(int code) {
    switch (code) {
        case 0: return "SMA";
        case 1: return "EMA";
        case 2: return "RSI";
        case 3: return "MACD";
        case 4: return "BOLLINGER";
        case 5: return "STDDEV";
        case 6: return "HIGHEST";
        case 7: return "LOWEST";
        case 8: return "ROC";
        default: return "UNKNOWN";
    }
}
