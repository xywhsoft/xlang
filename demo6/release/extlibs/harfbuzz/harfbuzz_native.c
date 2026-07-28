/*
 * harfbuzz_native.c - Text shaping engine
 * Tier 5: graphics
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xharfbuzz_direction(long long id) {
    switch (id) {
        case 0: return "LTR";
        case 1: return "RTL";
        case 2: return "TTB";
        case 3: return "BTT";
        default: return "Unknown";
    }
}

const char* xharfbuzz_script(long long id) {
    switch (id) {
        case 0: return "Latin";
        case 1: return "Arabic";
        case 2: return "Han";
        case 3: return "Devanagari";
        case 4: return "Hangul";
        default: return "Unknown";
    }
}

/* Simple glyph advance sum */
double xharfbuzz_total_advance(const char *advances) {
    double total = 0.0;
    const char *p = advances;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            double val = 0.0;
            while (*p >= '0' && *p <= '9') {
                val = val * 10.0 + (*p - '0');
                p++;
            }
            if (*p == '.') {
                p++;
                double frac = 0.1;
                while (*p >= '0' && *p <= '9') {
                    val += (*p - '0') * frac;
                    frac *= 0.1;
                    p++;
                }
            }
            total += val;
        } else {
            p++;
        }
    }
    return total;
}

const char* xharfbuzz_feature(long long id) {
    switch (id) {
        case 0: return "liga";
        case 1: return "kern";
        case 2: return "calt";
        case 3: return "smcp";
        case 4: return "onum";
        default: return "Unknown";
    }
}

/* Detect direction from first char (simplified) */
long long xharfbuzz_detect_dir(const char *text) {
    if (!text || !*text) return 0;
    unsigned char c = (unsigned char)text[0];
    /* Arabic range heuristic: 0xD8-0xDF in UTF-8 */
    if (c == 0xD8 || c == 0xD9 || c == 0xDA || c == 0xDB) return 1;
    return 0;
}
