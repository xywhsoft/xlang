#define XRT_MODULE_ALL
#ifdef __TINYC__
#include "../epub_zip.c"
#include "../epub_native.c"
#else
#include "epub_native.h"
#endif

#include <stdio.h>
#include <string.h>

static int put(xvalue *object, const char *key, const char *value) {
    return xrtValueObjectSetNew(object, (xstrview){key, strlen(key)},
        xrtValueString((xstrview){value, strlen(value)}));
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : NULL;
    char *workdir = NULL;
    char *owned_path = NULL;
    xvalue *meta = xrtValueObject();
    xvalue *chapters = xrtValueArray();
    xvalue *first = xrtValueObject();
    xvalue *second = xrtValueObject();
    char *content = NULL;
    int ok = 0, built = 0;
    if (path == NULL) {
        workdir = xrtDirTemp(NULL, "xlang_epub_example_", NULL);
        if (workdir == NULL) goto done;
        owned_path = xrtPathJoin(workdir, "book.epub");
        if (owned_path == NULL) goto done;
        path = owned_path;
    }
    if (meta == NULL || chapters == NULL || first == NULL || second == NULL)
        goto done;
    if (!put(meta, "title", "xlang 电子书") ||
        !put(meta, "language", "zh-CN") ||
        !put(meta, "identifier", "urn:xlang:example:epub") ||
        !put(meta, "modified", "2026-08-01T00:00:00Z") ||
        !put(first, "title", "第一章") ||
        !put(first, "text", "# 开始\n\n你好，xlang。") ||
        !put(second, "title", "第二章") ||
        !put(second, "text", "# 下一步\n\n使用 C 与 xlang。")) goto done;
    /* AppendNew 无论成功失败都接管条目。 */
    ok = xrtValueArrayAppendNew(chapters, first);
    first = NULL;
    if (!ok) goto done;
    ok = xrtValueArrayAppendNew(chapters, second);
    second = NULL;
    if (!ok) goto done;
    ok = 0;
    if (xep_build(path, meta, chapters) <= 0) goto done;
    built = 1;
    content = xep_read_entry(path, "OEBPS/nav.xhtml");
    ok = content != NULL && strstr(content, "第一章") != NULL &&
        strstr(content, "第二章") != NULL;
done:
    xrtFree(content);
    xrtValueRelease(second);
    xrtValueRelease(first);
    xrtValueRelease(meta);
    xrtValueRelease(chapters);
    if (workdir != NULL) {
        if (built && !xrtFileDelete(path)) ok = 0;
        if (!xrtDirRemove(workdir)) ok = 0;
    }
    xrtFree(owned_path);
    xrtFree(workdir);
    if (!ok) return 1;
    puts("chapters: 2");
    puts("title: xlang 电子书");
    puts("epub example: PASS");
    return 0;
}
