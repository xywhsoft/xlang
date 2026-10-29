/*
 * pdf_native.c - PDF generation helpers
 * Tier 4: document rendering
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xpdf_page_size(long long id) {
    switch (id) {
        case 0: return "A4 (595x842)";
        case 1: return "Letter (612x792)";
        case 2: return "Legal (612x1008)";
        case 3: return "A3 (842x1191)";
        case 4: return "A5 (420x595)";
        case 5: return "B5 (499x709)";
        case 6: return "Executive (522x756)";
        default: return "Unknown";
    }
}

const char* xpdf_font(long long id) {
    switch (id) {
        case 0: return "Helvetica";
        case 1: return "Helvetica-Bold";
        case 2: return "Helvetica-Oblique";
        case 3: return "Times-Roman";
        case 4: return "Times-Bold";
        case 5: return "Times-Italic";
        case 6: return "Courier";
        case 7: return "Courier-Bold";
        case 8: return "Symbol";
        case 9: return "ZapfDingbats";
        default: return "Unknown";
    }
}

const char* xpdf_line_cap(long long id) {
    switch (id) {
        case 0: return "butt";
        case 1: return "round";
        case 2: return "square";
        default: return "Unknown";
    }
}

const char* xpdf_line_join(long long id) {
    switch (id) {
        case 0: return "miter";
        case 1: return "round";
        case 2: return "bevel";
        default: return "Unknown";
    }
}

const char* xpdf_text_align(long long id) {
    switch (id) {
        case 0: return "left";
        case 1: return "center";
        case 2: return "right";
        case 3: return "justify";
        default: return "Unknown";
    }
}

const char* xpdf_color_space(long long id) {
    switch (id) {
        case 0: return "DeviceRGB";
        case 1: return "DeviceCMYK";
        case 2: return "DeviceGray";
        case 3: return "ICCBased";
        case 4: return "Indexed";
        default: return "Unknown";
    }
}

const char* xpdf_compression(long long id) {
    switch (id) {
        case 0: return "none";
        case 1: return "flate";
        case 2: return "lzw";
        case 3: return "run-length";
        default: return "Unknown";
    }
}

/* Points to mm: 1pt = 0.3528mm, return value * 100 for precision */
long long xpdf_pt_to_mm_x100(long long pt) {
    return (pt * 3528) / 100;
}
