#define XRT_MODULE_THREAD
#include "xrt.h"
#include "../include/tabulate_native.h"
#include <stdio.h>
#include <string.h>

static int run(void* unused)
{
    const char* header[] = {"Name", "City"};
    const char* row[] = {"Alice", "北京"};
    int i;
    (void)unused;
    for (i = 0; i < 300; ++i) {
        void* table = xtab_create();
        char* output = NULL;
        int valid = table != NULL;
        if (valid) valid = xtab_set_headers(table, header, 2) &&
            xtab_add_row(table, row, 2);
        if (valid) output = (char*)xtab_render(table);
        if (output == NULL || strstr(output, "北京") == NULL) valid = 0;
        xrtFree(output);
        xtab_destroy(table);
        if (!valid) return 1;
    }
    return 0;
}

int main(void)
{
    xthread* threads[4] = {0};
    int i, valid = 1;
    for (i = 0; i < 4; ++i) {
        threads[i] = xrtThreadCreate(run, NULL, 0);
        if (threads[i] == NULL) valid = 0;
    }
    for (i = 0; i < 4; ++i) {
        if (threads[i] == NULL) continue;
        if (xrtThreadWait(threads[i]) != XWAIT_OK ||
            xrtThreadExitCode(threads[i]) != 0) valid = 0;
        xrtThreadDestroy(threads[i]);
    }
    if (valid) puts("tabulate threads: PASS");
    return valid ? 0 : 1;
}
