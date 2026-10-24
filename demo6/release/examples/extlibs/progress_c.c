#include "../../extlibs/progress/include/progress_native.h"
#include "../../extlibs/progress/progress_native.c"
#include "xrt.h"
#include <stdio.h>

int main(void) {
    void *bar = xprog_create(10);
    int i;
    if (bar == NULL) return 1;
    if (!xprog_set_prefix_n(bar, "Processing", 10) ||
        !xprog_set_width(bar, 30)) { xprog_destroy(bar); return 1; }
    for (i = 0; i < 10; ++i) {
        char *line;
        if (!xprog_advance(bar, 1)) { xprog_destroy(bar); return 1; }
        line = (char *)xprog_render(bar);
        if (line == NULL) { xprog_destroy(bar); return 1; }
        puts(line);
        xrtFree(line);
    }
    printf("done: %s\n", xprog_is_done(bar) ? "true" : "false");
    if (!xprog_set(bar, 3)) { xprog_destroy(bar); return 1; }
    printf("reset current: %lld total: %lld percent: %d\n",
           (long long)xprog_current(bar), (long long)xprog_total(bar),
           xprog_percent(bar));
    if (!xprog_advance(bar, 100)) { xprog_destroy(bar); return 1; }
    printf("saturated current: %lld done: %s\n",
           (long long)xprog_current(bar), xprog_is_done(bar) ? "true" : "false");
    puts("progress example: PASS");
    xprog_destroy(bar);
    return 0;
}
