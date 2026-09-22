/*
 * units_native.c - Human-readable byte sizes, durations, and counts.
 *
 * Conventions:
 *   - bytes uses 1024-based units (KB/MB/GB/TB/PB/EB).
 *   - bytesDecimal uses 1000-based units.
 *   - parseBytes accepts 1024 and 1000 style suffixes (MB and MiB) plus the
 *     bare single letters K/M/G/T; the number may carry a sign.
 *   - duration input is seconds; formatting keeps two significant groups.
 *   - parseDuration accepts d/h/m/s combinations like "1d2h30m".
 *   - parse failures return a negative value (-1) rather than raising.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#define XU_RING 8
#define XU_TEXT 64

static char xu_ring[XU_RING][XU_TEXT];
static int xu_ring_next = 0;

static char *xu_slot(void) {
    char *slot = xu_ring[xu_ring_next];
    xu_ring_next = (xu_ring_next + 1) % XU_RING;
    return slot;
}

/* Manual decimal parser: TCC's strtod accepts FORTRAN 'd' exponents
   ("1d2" == 100), which corrupts duration input like "1d2h30m". */
static double xu_parse_number(const char **pp, int *ok) {
    const char *p = *pp;
    double value = 0;
    int seen = 0;
    int neg = 0;
    double scale = 0.1;
    if (*p == '-') {
        neg = 1;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        value = value * 10.0 + (*p - '0');
        p++;
        seen = 1;
    }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            value += (*p - '0') * scale;
            scale /= 10.0;
            p++;
            seen = 1;
        }
    }
    *pp = p;
    *ok = seen;
    return neg ? -value : value;
}

static const char *xu_bytes_with(double n, double scale) {
    static const char *names[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    char *slot = xu_slot();
    int unit = 0;
    double value = n;
    int neg = value < 0;
    if (neg) value = -value;
    while (value >= scale && unit < 6) {
        value /= scale;
        unit++;
    }
    if (unit == 0) {
        snprintf(slot, XU_TEXT, "%s%.0f %s", neg ? "-" : "", value, names[0]);
    } else {
        snprintf(slot, XU_TEXT, "%s%.2f %s", neg ? "-" : "", value, names[unit]);
    }
    return slot;
}

const char *xu_bytes(double n) {
    return xu_bytes_with(n, 1024.0);
}

const char *xu_bytes_decimal(double n) {
    return xu_bytes_with(n, 1000.0);
}

double xu_parse_bytes(const char *text) {
    static const char *suffixes[] = {
        "", "B", "K", "KB", "KI", "KIB", "M", "MB", "MI", "MIB",
        "G", "GB", "GI", "GIB", "T", "TB", "TI", "TIB",
        "P", "PB", "PI", "PIB", "E", "EB", "EI", "EIB"
    };
    static const double factors[] = {
        1, 1, 1024, 1024, 1024, 1024, 1024 * 1024, 1024 * 1024,
        1024 * 1024, 1024 * 1024,
        1024.0 * 1024 * 1024, 1024.0 * 1024 * 1024, 1024.0 * 1024 * 1024,
        1024.0 * 1024 * 1024,
        1024.0 * 1024 * 1024 * 1024, 1024.0 * 1024 * 1024 * 1024,
        1024.0 * 1024 * 1024 * 1024, 1024.0 * 1024 * 1024 * 1024,
        1024.0 * 1024 * 1024 * 1024 * 1024, 1024.0 * 1024 * 1024 * 1024 * 1024,
        1024.0 * 1024 * 1024 * 1024 * 1024,
        1024.0 * 1024 * 1024 * 1024 * 1024,
        1024.0 * 1024 * 1024 * 1024 * 1024 * 1024,
        1024.0 * 1024 * 1024 * 1024 * 1024 * 1024,
        1024.0 * 1024 * 1024 * 1024 * 1024 * 1024,
        1024.0 * 1024 * 1024 * 1024 * 1024 * 1024
    };
    const char *end = NULL;
    double value;
    char upper[16];
    size_t len;
    size_t i;
    int numOk = 0;
    if (text == NULL) return -1;
    end = text;
    value = xu_parse_number(&end, &numOk);
    if (!numOk) return -1;
    while (*end == ' ') end++;
    len = strlen(end);
    if (len >= sizeof(upper)) return -1;
    for (i = 0; i < len; ++i) {
        upper[i] = (char)toupper((unsigned char)end[i]);
    }
    upper[len] = '\0';
    for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        if (strcmp(upper, suffixes[i]) == 0) {
            return value * factors[i];
        }
    }
    return -1;
}

const char *xu_duration(double seconds) {
    char *slot = xu_slot();
    double rest = seconds;
    int neg = rest < 0;
    int d;
    int h;
    int m;
    int s;
    int pos = 0;
    if (neg) rest = -rest;
    d = (int)(rest / 86400.0);
    rest -= d * 86400.0;
    h = (int)(rest / 3600.0);
    rest -= h * 3600.0;
    m = (int)(rest / 60.0);
    rest -= m * 60.0;
    s = (int)lround(rest);
    if (s == 60) { s = 0; m++; }
    if (m == 60) { m = 0; h++; }
    if (h == 24) { h = 0; d++; }
    if (neg) slot[pos++] = '-';
    if (d > 0) pos += snprintf(slot + pos, XU_TEXT - pos, "%dd ", d);
    if (h > 0) pos += snprintf(slot + pos, XU_TEXT - pos, "%dh ", h);
    if (m > 0) pos += snprintf(slot + pos, XU_TEXT - pos, "%dm ", m);
    if (pos == (neg ? 1 : 0) || s > 0 || seconds == 0) {
        pos += snprintf(slot + pos, XU_TEXT - pos, "%ds", s);
    } else {
        /* trim a trailing space */
        if (pos > 0 && slot[pos - 1] == ' ') slot[--pos] = '\0';
    }
    slot[pos] = '\0';
    return slot;
}

const char *xu_duration_short(double seconds) {
    char *slot = xu_slot();
    double rest = seconds;
    int neg = rest < 0;
    int d;
    int h;
    int m;
    int s;
    int pos = 0;
    if (neg) rest = -rest;
    d = (int)(rest / 86400.0);
    rest -= d * 86400.0;
    h = (int)(rest / 3600.0);
    rest -= h * 3600.0;
    m = (int)(rest / 60.0);
    rest -= m * 60.0;
    s = (int)lround(rest);
    if (s == 60) { s = 0; m++; }
    if (m == 60) { m = 0; h++; }
    if (h == 24) { h = 0; d++; }
    if (neg) slot[pos++] = '-';
    if (d > 0) {
        pos += snprintf(slot + pos, XU_TEXT - pos, "%dd%02d:%02d:%02d", d, h, m, s);
    } else if (h > 0) {
        pos += snprintf(slot + pos, XU_TEXT - pos, "%d:%02d:%02d", h, m, s);
    } else {
        pos += snprintf(slot + pos, XU_TEXT - pos, "%d:%02d", m, s);
    }
    slot[pos] = '\0';
    return slot;
}

double xu_parse_duration(const char *text) {
    const char *p = text;
    double total = 0;
    int any = 0;
    if (text == NULL) return -1;
    while (*p != '\0') {
        double value;
        int numOk = 0;
        while (*p == ' ') p++;
        if (*p == '\0') break;
        value = xu_parse_number(&p, &numOk);
        if (!numOk) return -1;
        while (*p == ' ') p++;
        if (*p == 'd') { total += value * 86400.0; p++; }
        else if (*p == 'h') { total += value * 3600.0; p++; }
        else if (*p == 'm') { total += value * 60.0; p++; }
        else if (*p == 's') { total += value; p++; }
        else if (*p == '\0') { total += value; }
        else return -1;
        any = 1;
    }
    return any ? total : -1;
}

const char *xu_count(double n) {
    char *slot = xu_slot();
    double value = n;
    int neg = value < 0;
    const char *suffix = "";
    if (neg) value = -value;
    if (value >= 1000000000000.0) { value /= 1000000000000.0; suffix = "T"; }
    else if (value >= 1000000000.0) { value /= 1000000000.0; suffix = "B"; }
    else if (value >= 1000000.0) { value /= 1000000.0; suffix = "M"; }
    else if (value >= 1000.0) { value /= 1000.0; suffix = "K"; }
    if (suffix[0] == '\0') {
        snprintf(slot, XU_TEXT, "%s%.0f", neg ? "-" : "", value);
    } else {
        snprintf(slot, XU_TEXT, "%s%.1f%s", neg ? "-" : "", value, suffix);
    }
    return slot;
}

long long xu_is_pow2(long long n) {
    return n > 0 && (n & (n - 1)) == 0;
}
