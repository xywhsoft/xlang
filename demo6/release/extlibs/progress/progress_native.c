/*
 * progress_native.c - Terminal progress bar
 */
#include <stdio.h>
#include "../xlang_extlib_error.h"
static char prog_buf[4096];

typedef struct {
    long long total;
    long long current;
    int bar_width;
    char prefix[128];
} XProg;

void *xprog_create(long long total) {
    XProg *p;
    if (total <= 0) { xl6ExtlibError(XL6_EXTERR_ARGUMENT, "xlang.extlib.progress", "progress total must be positive"); return NULL; }
    p = (XProg *)calloc(1, sizeof(XProg));
    if (!p) { xl6ExtlibError(XL6_EXTERR_MEMORY, "xlang.extlib.progress", "cannot allocate progress state"); return NULL; }
    p->total = total;
    p->bar_width = 40;
    strcpy(p->prefix, "Progress");
    return p;
}

void xprog_destroy(void *ptr) {
    free(ptr);
}

void xprog_advance(void *ptr, long long n) {
    XProg *p = (XProg *)ptr;
    if (!p) return;
    p->current += n;
    if (p->current > p->total) p->current = p->total;
    if (p->current < 0) p->current = 0;
}

void xprog_set(void *ptr, long long current) {
    XProg *p = (XProg *)ptr;
    if (!p) return;
    p->current = current;
    if (p->current > p->total) p->current = p->total;
    if (p->current < 0) p->current = 0;
}

void xprog_set_prefix(void *ptr, const char *prefix) {
    XProg *p = (XProg *)ptr;
    if (!p || !prefix) return;
    strncpy(p->prefix, prefix, sizeof(p->prefix) - 1);
    p->prefix[sizeof(p->prefix) - 1] = '\0';
}

void xprog_set_width(void *ptr, int width) {
    XProg *p = (XProg *)ptr;
    if (!p) return;
    if (width < 10) width = 10;
    if (width > 100) width = 100;
    p->bar_width = width;
}

int xprog_percent(void *ptr) {
    XProg *p = (XProg *)ptr;
    if (!p || p->total == 0) return 0;
    return (int)(p->current * 100 / p->total);
}

int xprog_is_done(void *ptr) {
    XProg *p = (XProg *)ptr;
    if (!p) return 0;
    return p->current >= p->total;
}

long long xprog_current(void *ptr) {
    XProg *p = (XProg *)ptr;
    return p ? p->current : 0;
}

long long xprog_total(void *ptr) {
    XProg *p = (XProg *)ptr;
    return p ? p->total : 0;
}

const char *xprog_render(void *ptr) {
    XProg *p = (XProg *)ptr;
    int pct, filled, i, pos = 0;
    if (!p) return "";
    pct = (int)(p->current * 100 / p->total);
    filled = (int)(p->current * p->bar_width / p->total);

    pos += snprintf(prog_buf + pos, sizeof(prog_buf) - pos, "%s [", p->prefix);
    for (i = 0; i < p->bar_width; i++) {
        if (i < filled) prog_buf[pos++] = '=';
        else if (i == filled) prog_buf[pos++] = '>';
        else prog_buf[pos++] = ' ';
    }
    pos += snprintf(prog_buf + pos, sizeof(prog_buf) - pos, "] %3d%% (%lld/%lld)",
                    pct, p->current, p->total);
    prog_buf[pos] = '\0';
    return prog_buf;
}
