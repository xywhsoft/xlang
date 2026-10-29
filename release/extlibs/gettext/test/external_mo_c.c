#define XRT_MODULE_ALL
#include "xrt.h"
#include "../gettext_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "gettext MO interop failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int same(void *result, const char *expected) {
    char *text = result;
    int equal = text != NULL && strcmp(text, expected) == 0;
    xrtFree(text);
    return equal;
}

int main(int argc, char **argv) {
    xvalue *catalog;
    CHECK(argc == 2);
    catalog = xgt_load_mo(argv[1]);
    CHECK(catalog != NULL);
    CHECK(same(xgt_tc(catalog, "menu", "Open"), "打开菜单"));
    CHECK(same(xgt_tn(catalog, "file", "files", 1), "一个文件"));
    CHECK(same(xgt_tn(catalog, "file", "files", 2), "两个文件"));
    CHECK(same(xgt_tn(catalog, "file", "files", 3), "多个文件"));
    CHECK(same(xgt_t(catalog, "Draft"), "Draft"));
    xrtValueRelease(catalog);
    puts("GNU MO to C: PASS");
    return 0;
}
