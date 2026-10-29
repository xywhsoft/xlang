/*
 * docx_native.c - DOCX document helpers
 * Tier 3: Office Open XML word processing utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* Paragraph alignment */
const char* xdocx_alignment(long long align) {
    switch (align) {
        case 0: return "left";
        case 1: return "center";
        case 2: return "right";
        case 3: return "both";
        case 4: return "distribute";
        default: return "unknown";
    }
}

/* Heading level */
const char* xdocx_heading_style(long long level) {
    switch (level) {
        case 0: return "Normal";
        case 1: return "Heading 1";
        case 2: return "Heading 2";
        case 3: return "Heading 3";
        case 4: return "Heading 4";
        case 5: return "Heading 5";
        case 6: return "Heading 6";
        case 7: return "Heading 7";
        case 8: return "Heading 8";
        case 9: return "Heading 9";
        default: return "Title";
    }
}

/* Font name (common) */
const char* xdocx_font_name(long long id) {
    switch (id) {
        case 0: return "Calibri";
        case 1: return "Times New Roman";
        case 2: return "Arial";
        case 3: return "Courier New";
        case 4: return "Georgia";
        case 5: return "Verdana";
        case 6: return "Cambria";
        case 7: return "Consolas";
        case 8: return "Segoe UI";
        case 9: return "Tahoma";
        default: return "Unknown";
    }
}

/* Page size */
const char* xdocx_page_size(long long size) {
    switch (size) {
        case 0: return "Letter (8.5x11)";
        case 1: return "Legal (8.5x14)";
        case 2: return "A4 (210x297mm)";
        case 3: return "A3 (297x420mm)";
        case 4: return "A5 (148x210mm)";
        case 5: return "B4 (250x353mm)";
        case 6: return "B5 (176x250mm)";
        case 7: return "Executive (7.25x10.5)";
        case 8: return "Tabloid (11x17)";
        default: return "Custom";
    }
}

/* Section break type */
const char* xdocx_section_break(long long type) {
    switch (type) {
        case 0: return "next page";
        case 1: return "continuous";
        case 2: return "even page";
        case 3: return "odd page";
        default: return "unknown";
    }
}

/* Line spacing rule */
const char* xdocx_line_spacing(long long rule) {
    switch (rule) {
        case 0: return "auto";
        case 1: return "exact";
        case 2: return "atLeast";
        default: return "unknown";
    }
}

/* Tab stop alignment */
const char* xdocx_tab_align(long long align) {
    switch (align) {
        case 0: return "left";
        case 1: return "center";
        case 2: return "right";
        case 3: return "decimal";
        case 4: return "bar";
        default: return "unknown";
    }
}

/* Border style */
const char* xdocx_border_style(long long style) {
    switch (style) {
        case 0: return "none";
        case 1: return "single";
        case 2: return "double";
        case 3: return "dotted";
        case 4: return "dashed";
        case 5: return "thick";
        case 6: return "triple";
        default: return "unknown";
    }
}
