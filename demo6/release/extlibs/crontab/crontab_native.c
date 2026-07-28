/*
 * crontab_native.c - Cron expression parser and matcher
 * Format: minute hour day_of_month month day_of_week
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

extern void xrtSetError(const char *msg);

static char cron_buf[4096];

/* Parse a single cron field into a bitmask.
   Supports: *, N, N-M, N/S, N-M/S, comma lists */
static int parse_field(const char *field, int min, int max, unsigned long long *mask) {
    char buf[256];
    char *tok;
    *mask = 0;
    strncpy(buf, field, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    tok = strtok(buf, ",");
    while (tok) {
        int lo = min, hi = max, step = 1;
        char *slash = strchr(tok, '/');
        if (slash) { *slash = '\0'; step = atoi(slash + 1); if (step < 1) step = 1; }
        if (strcmp(tok, "*") == 0) { lo = min; hi = max; }
        else {
            char *dash = strchr(tok, '-');
            if (dash) { *dash = '\0'; lo = atoi(tok); hi = atoi(dash + 1); }
            else { lo = hi = atoi(tok); }
        }
        if (lo < min) lo = min;
        if (hi > max) hi = max;
        { int v; for (v = lo; v <= hi; v += step) *mask |= (1ULL << v); }
        tok = strtok(NULL, ",");
    }
    return 1;
}

typedef struct {
    unsigned long long minute;   /* 0-59 */
    unsigned long long hour;     /* 0-23 */
    unsigned long long dom;      /* 1-31 */
    unsigned long long month;    /* 1-12 */
    unsigned long long dow;      /* 0-6 (0=Sunday) */
} CronExpr;

static int cron_parse(const char *expr, CronExpr *ce) {
    char buf[512];
    char *fields[5];
    int i = 0;
    char *tok;
    if (!expr) return 0;
    strncpy(buf, expr, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    tok = strtok(buf, " \t");
    while (tok && i < 5) { fields[i++] = tok; tok = strtok(NULL, " \t"); }
    if (i < 5) return 0;
    parse_field(fields[0], 0, 59, &ce->minute);
    parse_field(fields[1], 0, 23, &ce->hour);
    parse_field(fields[2], 1, 31, &ce->dom);
    parse_field(fields[3], 1, 12, &ce->month);
    parse_field(fields[4], 0, 6, &ce->dow);
    return 1;
}

int xcron_matches(const char *expr, long long timestamp) {
    CronExpr ce;
    time_t t = (time_t)timestamp;
    struct tm *tm_info = localtime(&t);
    if (!cron_parse(expr, &ce)) return 0;
    if (!(ce.minute & (1ULL << tm_info->tm_min))) return 0;
    if (!(ce.hour & (1ULL << tm_info->tm_hour))) return 0;
    if (!(ce.dom & (1ULL << tm_info->tm_mday))) return 0;
    if (!(ce.month & (1ULL << (tm_info->tm_mon + 1)))) return 0;
    if (!(ce.dow & (1ULL << tm_info->tm_wday))) return 0;
    return 1;
}

long long xcron_next(const char *expr, long long from_timestamp) {
    CronExpr ce;
    time_t t;
    int iterations = 0;
    if (!cron_parse(expr, &ce)) return -1;
    /* start from next minute */
    t = (time_t)from_timestamp;
    t = t - (t % 60) + 60;
    while (iterations < 525960) {  /* max ~1 year of minutes */
        struct tm *tm_info = localtime(&t);
        if ((ce.minute & (1ULL << tm_info->tm_min)) &&
            (ce.hour & (1ULL << tm_info->tm_hour)) &&
            (ce.dom & (1ULL << tm_info->tm_mday)) &&
            (ce.month & (1ULL << (tm_info->tm_mon + 1))) &&
            (ce.dow & (1ULL << tm_info->tm_wday))) {
            return (long long)t;
        }
        t += 60;
        iterations++;
    }
    return -1;
}

const char *xcron_describe(const char *expr) {
    CronExpr ce;
    int pos = 0;
    if (!cron_parse(expr, &ce)) return "invalid";

    /* simple description */
    if (ce.minute == ~0ULL >> (64 - 60)) pos += snprintf(cron_buf + pos, sizeof(cron_buf) - pos, "every minute");
    else pos += snprintf(cron_buf + pos, sizeof(cron_buf) - pos, "at specific minutes");

    if (ce.hour != (~0ULL >> (64 - 24))) pos += snprintf(cron_buf + pos, sizeof(cron_buf) - pos, ", specific hours");
    if (ce.dom != (~0ULL >> (64 - 32))) pos += snprintf(cron_buf + pos, sizeof(cron_buf) - pos, ", specific days");

    return cron_buf;
}

int xcron_is_valid(const char *expr) {
    CronExpr ce;
    return cron_parse(expr, &ce);
}
