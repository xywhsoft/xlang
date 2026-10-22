#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "../gettext_native.h"
#include <stdio.h>
#include <string.h>

static int line(xvalue *catalog, const char *key) {
    char *translated = xgt_t(catalog, key);
    if (!translated) return 0;
    puts(translated);
    xrtFree(translated);
    return 1;
}

int main(void) {
    static const char po[] =
        "msgid \"\"\n"
        "msgstr \"Plural-Forms: nplurals=3; plural=n==1?0:n==2?1:2;\\n\"\n\n"
        "msgctxt \"menu\"\nmsgid \"Open\"\nmsgstr \"打开菜单\"\n\n"
        "msgid \"file\"\nmsgid_plural \"files\"\n"
        "msgstr[0] \"一个文件\"\nmsgstr[1] \"两个文件\"\nmsgstr[2] \"多个文件\"\n\n"
        "msgctxt \"toolbar\"\nmsgid \"file\"\nmsgid_plural \"files\"\n"
        "msgstr[0] \"工具栏文件\"\nmsgstr[1] \"工具栏两个文件\"\n"
        "msgstr[2] \"工具栏多个文件\"\n\n"
        "#, fuzzy\nmsgid \"Draft\"\nmsgstr \"未审核\"\n\n"
        "msgid \"Blank\"\nmsgstr \"\"\n\n"
        "msgid \"escaped\"\nmsgstr \"\\x41\\x42\\?\"\n";
    xvalue *catalog = xgt_parse_po(po);
    xvalue *mo_bytes = NULL, *mo = NULL;
    char *menu, *plural, *toolbar, *mo_text = NULL;
    bytes file_data;
    size_t file_size = 0;
    int ok;
    if (!catalog || xrtValueType(catalog) != XVALUE_OBJECT) {
        xrtValueRelease(catalog);
        return 1;
    }
    menu = xgt_tc(catalog, "menu", "Open");
    plural = xgt_tn(catalog, "file", "files", 3);
    toolbar = xgt_tnc(catalog, "toolbar", "file", "files", 2);
    ok = menu && plural && toolbar &&
        strcmp(menu, "打开菜单") == 0 && strcmp(plural, "多个文件") == 0 &&
        strcmp(toolbar, "工具栏两个文件") == 0;
    if (ok) {
        printf("menu: %s\n", menu);
        printf("many: %s\n", plural);
        printf("toolbar: %s\n", toolbar);
    }
    xrtFree(toolbar);
    xrtFree(menu);
    xrtFree(plural);
    if (ok) ok = line(catalog, "missing");
    if (ok) ok = line(catalog, "Draft");
    if (ok) ok = line(catalog, "Blank");
    if (ok) ok = line(catalog, "escaped");
    xrtValueRelease(catalog);
    file_data = xrtFileReadAll("test/fixtures/gettext_mini.mo", &file_size);
    if (file_data) {
        mo_bytes = xrtValueBytes((xbytesview){file_data, file_size});
        xrtFree(file_data);
    }
    if (mo_bytes) mo = xgt_parse_mo(mo_bytes);
    xrtValueRelease(mo_bytes);
    if (mo && xrtValueType(mo) == XVALUE_OBJECT) mo_text = xgt_t(mo, "hello");
    if (!mo_text || strcmp(mo_text, "world!") != 0) ok = 0;
    if (ok) printf("mo: %s\n", mo_text);
    xrtFree(mo_text);
    xrtValueRelease(mo);
    return ok ? 0 : 1;
}

/* 单文件范例同时包含目录与复数表达式实现；正式包仍按清单分别编译。 */
#include "../gettext_native.c"
#include "../gettext_plural.c"
