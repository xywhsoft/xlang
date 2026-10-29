/*
 * sciter_native.c - HTML/CSS GUI engine
 * Tier 5: gui
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xsciter_element(long long id) {
    switch (id) {
        case 0: return "div";
        case 1: return "span";
        case 2: return "button";
        case 3: return "input";
        case 4: return "select";
        case 5: return "textarea";
        case 6: return "img";
        case 7: return "a";
        default: return "Unknown";
    }
}

const char* xsciter_event(long long id) {
    switch (id) {
        case 0: return "click";
        case 1: return "change";
        case 2: return "submit";
        case 3: return "keydown";
        case 4: return "keyup";
        case 5: return "focus";
        case 6: return "blur";
        default: return "Unknown";
    }
}

const char* xsciter_style_prop(long long id) {
    switch (id) {
        case 0: return "width";
        case 1: return "height";
        case 2: return "margin";
        case 3: return "padding";
        case 4: return "color";
        case 5: return "background";
        case 6: return "display";
        case 7: return "position";
        default: return "Unknown";
    }
}

const char* xsciter_display(long long id) {
    switch (id) {
        case 0: return "block";
        case 1: return "inline";
        case 2: return "inline-block";
        case 3: return "flex";
        case 4: return "grid";
        case 5: return "none";
        default: return "Unknown";
    }
}

/* Parse simple px value */
long long xsciter_parse_px(const char *val) {
    long long result = 0;
    while (*val >= '0' && *val <= '9') {
        result = result * 10 + (*val - '0');
        val++;
    }
    return result;
}
