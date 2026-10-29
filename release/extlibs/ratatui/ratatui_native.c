/*
 * ratatui_native.c - Terminal UI framework
 * Tier 5: gui
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xratatui_widget(long long id) {
    switch (id) {
        case 0: return "Block";
        case 1: return "Paragraph";
        case 2: return "List";
        case 3: return "Table";
        case 4: return "Gauge";
        case 5: return "Tabs";
        default: return "Unknown";
    }
}

const char* xratatui_style(long long id) {
    switch (id) {
        case 0: return "Reset";
        case 1: return "Bold";
        case 2: return "Italic";
        case 3: return "Underlined";
        case 4: return "Reversed";
        default: return "Unknown";
    }
}

const char* xratatui_color(long long id) {
    switch (id) {
        case 0: return "Black";
        case 1: return "Red";
        case 2: return "Green";
        case 3: return "Yellow";
        case 4: return "Blue";
        case 5: return "Magenta";
        case 6: return "Cyan";
        case 7: return "White";
        default: return "Unknown";
    }
}

/* Compute gauge percentage */
double xratatui_gauge_pct(double value, double max_val) {
    if (max_val <= 0.0) return 0.0;
    double pct = value / max_val * 100.0;
    if (pct < 0.0) return 0.0;
    if (pct > 100.0) return 100.0;
    return pct;
}

/* Build a border string */
const char* xratatui_border(long long id) {
    switch (id) {
        case 0: return "Plain";
        case 1: return "Rounded";
        case 2: return "Double";
        case 3: return "Thick";
        default: return "Unknown";
    }
}
