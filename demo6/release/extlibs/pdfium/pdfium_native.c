/*
 * pdfium_native.c - PDF rendering/parsing helpers
 * Tier 4: document rendering
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xpdfium_object_type(long long id) {
    switch (id) {
        case 0: return "unknown";
        case 1: return "boolean";
        case 2: return "number";
        case 3: return "string";
        case 4: return "name";
        case 5: return "array";
        case 6: return "dictionary";
        case 7: return "stream";
        case 8: return "null";
        case 9: return "reference";
        default: return "Unknown";
    }
}

const char* xpdfium_page_object(long long id) {
    switch (id) {
        case 0: return "text";
        case 1: return "path";
        case 2: return "image";
        case 3: return "shading";
        case 4: return "form";
        default: return "Unknown";
    }
}

const char* xpdfium_annotation(long long id) {
    switch (id) {
        case 0: return "text";
        case 1: return "link";
        case 2: return "freetext";
        case 3: return "line";
        case 4: return "square";
        case 5: return "circle";
        case 6: return "highlight";
        case 7: return "underline";
        case 8: return "stamp";
        case 9: return "widget";
        default: return "Unknown";
    }
}

const char* xpdfium_form_field(long long id) {
    switch (id) {
        case 0: return "unknown";
        case 1: return "pushbutton";
        case 2: return "checkbox";
        case 3: return "radiobutton";
        case 4: return "combobox";
        case 5: return "listbox";
        case 6: return "textfield";
        case 7: return "signature";
        default: return "Unknown";
    }
}

const char* xpdfium_render_flag(long long id) {
    switch (id) {
        case 0: return "annotation";
        case 1: return "printing";
        case 2: return "no_smoothpath";
        case 3: return "no_smoothimage";
        case 4: return "no_smoothtype";
        case 5: return "grayscale";
        case 6: return "lcd_text";
        default: return "Unknown";
    }
}

/* Calculate DPI scale factor: target_dpi / 72.0, return x1000 */
long long xpdfium_dpi_scale_x1000(long long target_dpi) {
    return (target_dpi * 1000) / 72;
}

const char* xpdfium_error_code(long long code) {
    switch (code) {
        case 0: return "success";
        case 1: return "unknown error";
        case 2: return "file not found";
        case 3: return "file format error";
        case 4: return "password error";
        case 5: return "security error";
        case 6: return "page not found";
        default: return "Unknown";
    }
}
