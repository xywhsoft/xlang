#define XRT_MODULE_ALL
#include "xrt.h"
#include "../gettext_native.h"
#include <stdio.h>
#include <string.h>

/* 查询结果在目录存活时只读借用，不接管、不释放，也不增加正文复制。 */
static bool emit(const char* label, const char* text, const char* expected)
{
    if (!text || strcmp(text, expected)) return false;
    printf("%s: %s\n", label, text);
    return true;
}

int main(int argc, char** argv)
{
    if (argc != 2 && !(argc == 3 && !strcmp(argv[2], "mo"))) {
        fputs("usage: layout_c FILE.po|FILE.mo [mo]\n", stderr);
        return 2;
    }
    xvalue* catalog;
    if (argc == 3) catalog = xgt_load_mo(argv[1]);
    else {
        size_t size = 0;
        char* source = xrtFileReadTextLimit(argv[1], XENCODING_UTF8, XUTF_STRICT,
            XGT_MAX_CATALOG_BYTES, &size);
        if (!source) return 1;
        if (memchr(source, 0, size)) { xrtFree(source); return 1; }
        catalog = xgt_parse_po(source);
        xrtFree(source);
    }
    bool ok = xrtValueType(catalog) == XVALUE_OBJECT &&
        emit("plain", xgt_t_borrowed(catalog, "plain"), "常规") &&
        emit("menu", xgt_tc_borrowed(catalog, "menu", "Open"), "打开") &&
        emit("toolbar", xgt_tc_borrowed(catalog, "toolbar", "Open"), "开启") &&
        emit("file(1)", xgt_tn_borrowed(catalog, "file", "files", 1), "文件") &&
        emit("file(2)", xgt_tn_borrowed(catalog, "file", "files", 2), "文件们") &&
        emit("empty context", xgt_tc_borrowed(catalog, "", "Open"), "空上下文");
    xrtValueRelease(catalog);
    return ok && !ferror(stdout) ? 0 : 1;
}

/* 单文件 C 范例；名称不与 layout.xl 构成同名 companion。 */
#include "../gettext_native.c"
#include "../gettext_plural.c"
