/*
 * pptx_native.c - PowerPoint OOXML helpers
 * Tier 3: document format utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xpptx_slide_layout(long long id) {
    switch (id) {
        case 0: return "Title Slide";
        case 1: return "Title and Content";
        case 2: return "Section Header";
        case 3: return "Two Content";
        case 4: return "Comparison";
        case 5: return "Title Only";
        case 6: return "Blank";
        case 7: return "Content with Caption";
        case 8: return "Picture with Caption";
        default: return "Unknown";
    }
}

const char* xpptx_transition(long long id) {
    switch (id) {
        case 0: return "none";
        case 1: return "fade";
        case 2: return "push";
        case 3: return "wipe";
        case 4: return "split";
        case 5: return "cut";
        case 6: return "cover";
        case 7: return "uncover";
        case 8: return "zoom";
        case 9: return "morph";
        default: return "Unknown";
    }
}

const char* xpptx_anim_type(long long id) {
    switch (id) {
        case 0: return "appear";
        case 1: return "fade in";
        case 2: return "fly in";
        case 3: return "float in";
        case 4: return "wipe";
        case 5: return "zoom";
        case 6: return "bounce";
        case 7: return "spin";
        case 8: return "grow/shrink";
        case 9: return "pulse";
        default: return "Unknown";
    }
}

const char* xpptx_chart_type(long long id) {
    switch (id) {
        case 0: return "column";
        case 1: return "bar";
        case 2: return "line";
        case 3: return "pie";
        case 4: return "area";
        case 5: return "scatter";
        case 6: return "bubble";
        case 7: return "radar";
        case 8: return "stock";
        case 9: return "surface";
        default: return "Unknown";
    }
}

const char* xpptx_shape_type(long long id) {
    switch (id) {
        case 0: return "rectangle";
        case 1: return "rounded rectangle";
        case 2: return "ellipse";
        case 3: return "triangle";
        case 4: return "diamond";
        case 5: return "pentagon";
        case 6: return "hexagon";
        case 7: return "arrow right";
        case 8: return "star 5";
        case 9: return "cloud";
        default: return "Unknown";
    }
}

long long xpptx_emu_to_cm(long long emu) {
    /* 1 cm = 360000 EMU */
    return (emu * 10) / 360000;
}

const char* xpptx_color_scheme(long long id) {
    switch (id) {
        case 0: return "Office Theme";
        case 1: return "Grayscale";
        case 2: return "Apex";
        case 3: return "Aspect";
        case 4: return "Civic";
        case 5: return "Concourse";
        case 6: return "Equity";
        case 7: return "Flow";
        default: return "Unknown";
    }
}

const char* xpptx_bullet_style(long long id) {
    switch (id) {
        case 0: return "none";
        case 1: return "disc";
        case 2: return "circle";
        case 3: return "square";
        case 4: return "dash";
        case 5: return "arrow";
        case 6: return "check";
        case 7: return "diamond";
        default: return "Unknown";
    }
}
