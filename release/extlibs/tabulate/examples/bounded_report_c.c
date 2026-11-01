#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "../include/tabulate_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char wide[4097];
    const char* headers[] = {wide, wide, wide, wide};
    const char* row[] = {"ok"};
    const char* shortHeader[] = {"Value"};
    void* table = xtab_create();
    char* report = NULL;
    int result = 1;
    memset(wide, 'x', sizeof(wide) - 1u); wide[sizeof(wide) - 1u] = 0;
    if (table == NULL || !xtab_set_headers(table, headers, 4u)) goto done;
    for (unsigned i = 0u; i < 256u; ++i)
        if (!xtab_add_row(table, row, 1u)) goto done;
    /* 缺列也参与补齐；超限不能破坏已提交的表。 */
    report = xtab_render(table);
    if (report != NULL || xrtErrorKind(xrtGetError()) != XERR_RANGE ||
        xtab_row_count(table) != 256 || xtab_col_count(table) != 4) goto done;
    if (!xtab_set_headers(table, shortHeader, 1u)) goto done;
    report = xtab_render(table);
    if (report == NULL || strlen(report) >= 4096u || strstr(report, "ok") == NULL) goto done;
    puts("rows: 256\noversized render: rejected\ntable preserved: true\nrecovered render: true");
    result = 0;
done:
    xrtFree(report);
    xtab_destroy(table);
    return result;
}

#ifdef __TINYC__
#include "../tabulate_native.c"
#include "../tabulate_render.c"
#endif
