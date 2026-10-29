/*
 * imgui_native.c - Immediate Mode GUI
 * Tier 5: gui
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* ximgui_widget(long long id) {
    switch (id) {
        case 0: return "Button";
        case 1: return "Checkbox";
        case 2: return "SliderFloat";
        case 3: return "InputText";
        case 4: return "ColorEdit";
        case 5: return "TreeNode";
        default: return "Unknown";
    }
}

const char* ximgui_layout(long long id) {
    switch (id) {
        case 0: return "SameLine";
        case 1: return "NewLine";
        case 2: return "Separator";
        case 3: return "Spacing";
        case 4: return "Indent";
        default: return "Unknown";
    }
}

/* Compute widget rect area */
double ximgui_rect_area(double x1, double y1, double x2, double y2) {
    double w = x2 - x1;
    double h = y2 - y1;
    if (w < 0) w = -w;
    if (h < 0) h = -h;
    return w * h;
}

const char* ximgui_style_var(long long id) {
    switch (id) {
        case 0: return "Alpha";
        case 1: return "WindowPadding";
        case 2: return "FrameRounding";
        case 3: return "ItemSpacing";
        case 4: return "ScrollbarSize";
        default: return "Unknown";
    }
}

/* Clamp value to [lo, hi] */
double ximgui_clamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
