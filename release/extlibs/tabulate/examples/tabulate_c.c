#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "../include/tabulate_native.h"
#include <inttypes.h>
#include <stdio.h>

int main(void)
{
    const char* headers[] = {"Language", "Year", "Notes"};
    const char* c[] = {"C", "1972", "systems"};
    const char* python[] = {"Python", "1991", "scripting"};
    const char* xlang[] = {"xlang", "2024", "中文, UTF-8"};
    void* table = xtab_create();
    char* output;
    if (table == NULL) return 1;
    if (!xtab_set_headers(table, headers, 3) ||
        !xtab_add_row(table, c, 3) ||
        !xtab_add_row(table, python, 3) ||
        !xtab_add_row(table, xlang, 3)) {
        xtab_destroy(table);
        return 1;
    }
    output = (char*)xtab_render(table);
    if (output == NULL) { xtab_destroy(table); return 1; }
    puts(output);
    printf("rows: %" PRId64 "\n", xtab_row_count(table));
    printf("cols: %" PRId64 "\n", xtab_col_count(table));
    xrtFree(output);
    xtab_destroy(table);
    return 0;
}

/* TCC 单文件范例包含实现；正式 C 构建仍分别编译两个单元。 */
#ifdef __TINYC__
#include "../tabulate_native.c"
#include "../tabulate_render.c"
#endif
