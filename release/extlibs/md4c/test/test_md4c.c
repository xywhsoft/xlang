#define XRT_MODULE_ALL
#include "xrt.h"
#include "../md4c_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "md4c test failed at line %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

/* 文件负例使用独占临时目录，避免并行门禁覆盖彼此的夹具。 */
static int test_file_inputs(void) {
    static const uint8_t bad_utf8[] = {'#', ' ', 'x', 0xff, '\n'};
    static const char nul_markdown[] = {'#', ' ', 'a', '\0', 'b'};
    str dir = xrtDirTemp(NULL, "xlang-md4c-test-", NULL);
    str path = NULL;
    void *result = NULL;
    int ok = 0;
    if (dir == NULL) return 0;

    path = xrtPathJoin(dir, "\xe4\xb8\xad\xe6\x96\x87 input.md");
    if (path == NULL || !xrtFileWriteAll(path,
        (xbytesview){(const uint8_t *)"# UTF-8\n", 8u})) goto cleanup;
    result = xmd4c_file_html(path, xmd4c_commonmark_flags(), 0);
    if (result == NULL || strcmp(result, "<h1>UTF-8</h1>\n") ||
        xrtGetError() || !xrtFileDelete(path)) goto cleanup;
    xrtFree(result);
    result = NULL;
    xrtFree(path);
    path = xrtPathJoin(dir, "invalid_utf8.md");
    if (path == NULL || !xrtFileWriteAll(path,
        (xbytesview){bad_utf8, sizeof(bad_utf8)})) goto cleanup;
    result = xmd4c_file_html(path, xmd4c_commonmark_flags(), 0);
    if (result != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE ||
        !xrtFileDelete(path)) goto cleanup;
    xrtFree(path);
    path = xrtPathJoin(dir, "embedded_zero.md");
    if (path == NULL || !xrtFileWriteAll(path,
        (xbytesview){(const uint8_t *)nul_markdown, sizeof(nul_markdown)})) goto cleanup;
    result = xmd4c_file_html(path, xmd4c_commonmark_flags(), 0);
    if (result != NULL || strstr(xmd4c_last_error(), "NUL") == NULL ||
        xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_VALUE ||
        !xrtFileDelete(path)) goto cleanup;
    xrtFree(path);
    path = xrtPathJoin(dir, "over_limit.md");
    if (path == NULL || !xrtFileWriteAll(path, (xbytesview){NULL, 0u}) ||
        !xrtFileSetSize(path, 64u * 1024u * 1024u + 1u)) goto cleanup;
    result = xmd4c_file_html(path, xmd4c_commonmark_flags(), 0);
    if (result != NULL || xrtGetError() == NULL ||
        xrtErrorKind(xrtGetError()) != XERR_RANGE ||
        !xrtFileDelete(path)) goto cleanup;
    ok = 1;

cleanup:
    xrtFree(result);
    if (path != NULL) {
        if (!ok) (void)xrtFileDelete(path);
        xrtFree(path);
    }
    if (!xrtDirRemove(dir)) ok = 0;
    xrtFree(dir);
    return ok;
}

int main(void) {
    char *first, *next, *long_markdown, *large;
    int i;
    CHECK(xmd4c_available());
    {
        char *oversized = (char *)malloc(64u * 1024u * 1024u + 1u);
        CHECK(oversized != NULL);
        memset(oversized, 'a', 64u * 1024u * 1024u + 1u);
        /* 超限的 C 缓冲区不以 NUL 终止，也不能继续越界扫描。 */
        CHECK(xmd4c_html(oversized) == NULL);
        CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        CHECK(xmd4c_html_flags(oversized, 0, 0) == NULL);
        CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        free(oversized);
    }
    first = xmd4c_html("# xlang\n\n**runtime**");
    CHECK(first && strstr(first, "<h1>xlang</h1>") &&
        strstr(first, "<strong>runtime</strong>"));
    for (i = 0; i < 20; ++i) {
        next = xmd4c_html("next");
        CHECK(next && strstr(next, "<p>next</p>"));
        xrtFree(next);
    }
    CHECK(strstr(first, "<h1>xlang</h1>"));
    xrtFree(first);

    next = xmd4c_html_flags("<script>x</script>",
        xmd4c_github_flags() | xmd4c_no_html_flag(), 0);
    CHECK(next && strstr(next, "<script>") == NULL);
    xrtFree(next);

    CHECK(xmd4c_html_flags("# x", -1, 0) == NULL &&
        xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xmd4c_html_flags("# x", 0x0080, 0) == NULL &&
        xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xmd4c_html_flags("# x", 0, 0x0010) == NULL &&
        xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xmd4c_file_html("release/extlibs/md4c/test/fixture.md", -1, 0) == NULL &&
        xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    next = xmd4c_html_flags("# x", xmd4c_github_flags(), xmd4c_xhtml_flag());
    CHECK(next && strstr(next, "<h1>x</h1>") && xrtGetError() == NULL);
    xrtFree(next);

    next = xmd4c_file_html("release/extlibs/md4c/test/fixture.md",
        xmd4c_commonmark_flags(), 0);
    CHECK(next && strstr(next, "<strong>xlang</strong>"));
    xrtFree(next);
    CHECK(xmd4c_file_html("release/extlibs/md4c/test/missing.md",
        xmd4c_commonmark_flags(), 0) == NULL);
    CHECK(xrtGetError() != NULL &&
        strcmp(xmd4c_last_error(), xrtErrorMessage(xrtGetError())) == 0);
    {
        void *invalid = xmd4c_html("bad \xff text");
        CHECK(invalid == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtFree(invalid);
    }
    CHECK(test_file_inputs());
    next = xmd4c_html("");
    CHECK(next && *next == '\0' && *xmd4c_last_error() == '\0' &&
        xrtGetError() == NULL);
    xrtFree(next);
    next = xmd4c_html("# \xe4\xb8\xad\xe6\x96\x87\n");
    CHECK(next && strstr(next, "\xe4\xb8\xad\xe6\x96\x87") && xrtGetError() == NULL);
    xrtFree(next);

    CHECK(xrtMemDebugFailAfter(0));
    CHECK(xmd4c_html("retry") == NULL && xrtMemDebugFailTriggered());
    CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    next = xmd4c_html("retry");
    CHECK(next && strstr(next, "<p>retry</p>") && xrtGetError() == NULL);
    xrtFree(next);

    /* 逐点覆盖缓冲增长和结果交还；失败后必须能立即重试。 */
    for (i = 0; i < 16; ++i) {
        bool triggered;
        CHECK(xrtMemDebugFailAfter((size_t)i));
        next = xmd4c_html("# Title\n\n**xlang**\n\n| a | b |\n|---|---|\n| 1 | 2 |\n");
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(next == NULL && xrtGetError() &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else {
            CHECK(next && strstr(next, "<h1>Title</h1>") &&
                xrtGetError() == NULL);
        }
        xrtFree(next);
    }
    next = xmd4c_html("after failure");
    CHECK(next && strstr(next, "<p>after failure</p>") && xrtGetError() == NULL);
    xrtFree(next);

    long_markdown = malloc(20001);
    CHECK(long_markdown != NULL);
    memset(long_markdown, 'x', 20000);
    long_markdown[20000] = '\0';
    for (i = 0; i < 16; ++i) {
        bool triggered;
        CHECK(xrtMemDebugFailAfter((size_t)i));
        large = xmd4c_html(long_markdown);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(large == NULL && xrtGetError() &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else {
            CHECK(large && strlen(large) > 20000 && xrtGetError() == NULL);
        }
        xrtFree(large);
    }
    large = xmd4c_html(long_markdown);
    CHECK(large && strlen(large) > 20000);
    xrtFree(large);
    free(long_markdown);
    return 0;
}
