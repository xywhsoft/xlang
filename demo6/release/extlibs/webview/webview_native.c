/*
 * webview_native.c - WebView embedding
 * Tier 5: gui
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xwebview_engine(long long id) {
    switch (id) {
        case 0: return "WebView2";
        case 1: return "WebKit";
        case 2: return "WebKitGTK";
        default: return "Unknown";
    }
}

const char* xwebview_event(long long id) {
    switch (id) {
        case 0: return "navigation";
        case 1: return "message";
        case 2: return "resize";
        case 3: return "close";
        default: return "Unknown";
    }
}

/* Build a simple JS eval script */
const char* xwebview_eval_script(const char *code) {
    snprintf(buf, sizeof(buf), "javascript:%s", code);
    return buf;
}

const char* xwebview_nav_action(long long id) {
    switch (id) {
        case 0: return "back";
        case 1: return "forward";
        case 2: return "reload";
        case 3: return "stop";
        default: return "Unknown";
    }
}

/* Validate URL scheme */
long long xwebview_valid_url(const char *url) {
    if (strncmp(url, "http://", 7) == 0) return 1;
    if (strncmp(url, "https://", 8) == 0) return 1;
    if (strncmp(url, "file://", 7) == 0) return 1;
    if (strncmp(url, "data:", 5) == 0) return 1;
    return 0;
}
