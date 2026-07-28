/*
 * freetype_native.c - Font rendering engine
 * Tier 5: graphics
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xfreetype_format(long long id) {
    switch (id) {
        case 0: return "TrueType";
        case 1: return "OpenType";
        case 2: return "Type1";
        case 3: return "BDF";
        case 4: return "PCF";
        default: return "Unknown";
    }
}

const char* xfreetype_style(long long id) {
    switch (id) {
        case 0: return "Regular";
        case 1: return "Bold";
        case 2: return "Italic";
        case 3: return "BoldItalic";
        default: return "Unknown";
    }
}

/* Compute em to pixel conversion: pixels = em * dpi / 72 */
double xfreetype_em_to_px(double em, double dpi) {
    return em * dpi / 72.0;
}

/* Kerning adjustment (simplified) */
double xfreetype_kerning(double advance, double adjust) {
    return advance + adjust;
}

const char* xfreetype_hinting(long long id) {
    switch (id) {
        case 0: return "none";
        case 1: return "slight";
        case 2: return "normal";
        case 3: return "full";
        default: return "Unknown";
    }
}
