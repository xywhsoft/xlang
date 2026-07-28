/*
 * plot_native.c - 2D chart drawing (SVG/PNG)
 * Tier 4: visualization
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xplot_chart_type(long long id) {
    switch (id) {
        case 0: return "line";
        case 1: return "bar";
        case 2: return "scatter";
        case 3: return "area";
        case 4: return "pie";
        case 5: return "histogram";
        case 6: return "boxplot";
        case 7: return "heatmap";
        case 8: return "candlestick";
        case 9: return "radar";
        default: return "Unknown";
    }
}

const char* xplot_axis_scale(long long id) {
    switch (id) {
        case 0: return "linear";
        case 1: return "log";
        case 2: return "symlog";
        case 3: return "time";
        default: return "Unknown";
    }
}

const char* xplot_legend_position(long long id) {
    switch (id) {
        case 0: return "top";
        case 1: return "bottom";
        case 2: return "left";
        case 3: return "right";
        case 4: return "none";
        default: return "Unknown";
    }
}

const char* xplot_color(long long id) {
    switch (id) {
        case 0: return "#1f77b4";
        case 1: return "#ff7f0e";
        case 2: return "#2ca02c";
        case 3: return "#d62728";
        case 4: return "#9467bd";
        case 5: return "#8c564b";
        case 6: return "#e377c2";
        case 7: return "#7f7f7f";
        case 8: return "#bcbd22";
        case 9: return "#17becf";
        default: return "#000000";
    }
}

const char* xplot_line_style(long long id) {
    switch (id) {
        case 0: return "solid";
        case 1: return "dashed";
        case 2: return "dotted";
        case 3: return "dash-dot";
        default: return "Unknown";
    }
}

const char* xplot_marker(long long id) {
    switch (id) {
        case 0: return "circle";
        case 1: return "square";
        case 2: return "triangle";
        case 3: return "diamond";
        case 4: return "cross";
        case 5: return "star";
        case 6: return "none";
        default: return "Unknown";
    }
}

/* Generate SVG viewBox string for given width/height */
const char* xplot_svg_viewbox(long long width, long long height) {
    sprintf(buf, "0 0 %lld %lld", width, height);
    return buf;
}

const char* xplot_output_format(long long id) {
    switch (id) {
        case 0: return "svg";
        case 1: return "png";
        case 2: return "pdf";
        case 3: return "html";
        default: return "Unknown";
    }
}
