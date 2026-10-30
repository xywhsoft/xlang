/* JSON5 转义按 Unicode 字符消费；普通 identity escape 和续行不是同一语义。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../json5_native.c"
#include "../vendor/pdjson5.c"
#include <stdio.h>

static unsigned failures, cases, diagnostics, oomPoints;
static const char* points[] = {
    "\xc2\x80", "\xc2\xa0", "\xc3\xa9", "\xce\xb1", "\xcc\x81",
    "\xe4\xbd\xa0", "\xf0\x9f\x8e\xb6", "\xf4\x8f\xbf\xbf",
    "\xe2\x80\xa8", "\xe2\x80\xa9"
};

static void check(bool ok, const char* message)
{
    if (!ok) { ++failures; fprintf(stderr, "JSON5 Unicode: %s\n", message); }
}

static bool is_text(xvalue* value, const char* expected)
{
    xstrview view;
    size_t size = strlen(expected);
    return value && !xrtGetError() && xrtValueType(value) == XVALUE_STRING &&
        xrtValueGetString(value, &view) && view.Size == size && memcmp(view.Data, expected, size) == 0;
}

static void check_document(const char* doc, const char* path, const char* expected)
{
    xvalue* root = xjson5_parse_len(doc, strlen(doc));
    check(!xrtGetError() && root && is_text(xrtValueObjectGet(root,
        (xstrview){path, strlen(path)}), expected), "parse/key identity differs");
    xrtValueRelease(root);
    check(xjson5_valid_len(doc, strlen(doc)) && !xrtGetError(), "valid rejected Unicode escape");
    xvalue* value = xjson5_get_value_len(doc, strlen(doc), path);
    check(is_text(value, expected), "getValue differs");
    xrtValueRelease(value);
    str text = xjson5_get_text_len(doc, strlen(doc), path);
    check(text && !xrtGetError() && strcmp(text, expected) == 0, "get text differs");
    xrtFree(text);
    xrtClearError();
    ++cases;
}

static void test_values(void)
{
    for (size_t point = 0; point < sizeof(points) / sizeof(*points); ++point)
        for (unsigned quote = 0; quote < 2; ++quote)
            for (unsigned escaped = 0; escaped < 2; ++escaped)
                for (unsigned key = 0; key < 2; ++key) {
                    char doc[128], expected[32], path[32];
                    char delimiter = quote ? '"' : '\'';
                    const char* output = escaped && point >= 8 ? "" : points[point];
                    snprintf(expected, sizeof(expected), "p%sq", output);
                    if (key) {
                        snprintf(doc, sizeof(doc), "{%cp%s%sq%c:'ok'}",
                            delimiter, escaped ? "\\" : "", points[point], delimiter);
                        snprintf(path, sizeof(path), "%s", expected);
                    } else {
                        snprintf(doc, sizeof(doc), "{v:%cp%s%sq%c}",
                            delimiter, escaped ? "\\" : "", points[point], delimiter);
                        strcpy(path, "v");
                    }
                    check_document(doc, path, key ? "ok" : expected);
                }
}

static void test_line_positions(void)
{
    const char* lines[] = {"\n", "\r", "\r\n", "\xe2\x80\xa8", "\xe2\x80\xa9"};
    for (size_t line = 0; line < sizeof(lines) / sizeof(*lines); ++line) {
        char doc[128];
        snprintf(doc, sizeof(doc), "{v:'p\\%sq', bad:@}", lines[line]);
        xvalue* value = xjson5_parse_len(doc, strlen(doc));
        check(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE &&
            strstr(xjson5_error_message(), "JSON5 line 2,") != NULL, "continuation diagnostic line differs");
        xrtValueRelease(value);
        xrtClearError();
        ++diagnostics;
    }
    // 未转义的 LS/PS 可作为字符串内容，但仍是源码行分隔符。
    for (size_t line = 8; line < 10; ++line) {
        char doc[128];
        snprintf(doc, sizeof(doc), "{v:'p%sq', bad:@}", points[line]);
        xvalue* value = xjson5_parse_len(doc, strlen(doc));
        check(xrtGetError() && strstr(xjson5_error_message(), "JSON5 line 2,") != NULL,
            "raw line separator diagnostic differs");
        xrtValueRelease(value);
        xrtClearError();
        ++diagnostics;
    }
}

static void test_comments(void)
{
    const char* lines[] = {"\n", "\r", "\r\n", "\xe2\x80\xa8", "\xe2\x80\xa9"};
    const char* layouts[] = {
        "//你好%s{v:'ok'}", "{//你好%sv:'ok'}", "{v://你好%s'ok'}", "{v:'ok'//你好%s}",
        "/*你好%s*/{v:'ok'}", "{/*你好%s*/v:'ok'}", "{v:/*你好%s*/'ok'}", "{v:'ok'/*你好%s*/}"
    };
    for (size_t line = 0; line < 5; ++line) {
        for (size_t layout = 0; layout < 8; ++layout) {
            char doc[128];
            snprintf(doc, sizeof(doc), layouts[layout], lines[line]);
            check_document(doc, "v", "ok");
        }
        for (unsigned block = 0; block < 2; ++block) {
            char doc[128];
            snprintf(doc, sizeof(doc), block ? "/*你好%s*/{v:@}" : "//你好%s{v:@}", lines[line]);
            xvalue* value = xjson5_parse_len(doc, strlen(doc));
            check(xrtGetError() && strstr(xjson5_error_message(), "JSON5 line 2,") != NULL,
                "comment diagnostic line differs");
            xrtValueRelease(value);
            xrtClearError();
            ++diagnostics;
        }
    }
    static const char doc[] = "/*你好*/{v:@}";
    xvalue* value = xjson5_parse_len(doc, sizeof(doc) - 1);
    check(xrtGetError() && strstr(xjson5_error_message(), "column 10:") != NULL,
        "UTF-8 comment column differs");
    xrtValueRelease(value);
    xrtClearError();
    ++diagnostics;
}

static void test_oom(void)
{
    static const char doc[] = "{v:'p\\\xe4\xbd\xa0q\\\xe2\x80\xa8z'}";
    bool valid = xjson5_valid_len(doc, sizeof(doc) - 1);
    check(valid, "OOM fixture must be valid");
    if (!valid) return;
    for (unsigned budget = 0; budget < 64; ++budget) {
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        check(xrtMemDebugFailAfter(budget), "cannot inject allocation failure");
        xvalue* value = xjson5_parse_len(doc, sizeof(doc) - 1);
        bool injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            ++oomPoints;
            check(value && xrtValueType(value) == XVALUE_NULL && xrtGetError() &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY, "OOM became syntax or a partial value");
        } else check(value && !xrtGetError(), "valid escaped text did not complete");
        xrtValueRelease(value);
        xrtClearError();
        value = xjson5_get_value_len(doc, sizeof(doc) - 1, "v");
        check(is_text(value, "p\xe4\xbd\xa0qz"), "same source did not recover");
        xrtValueRelease(value);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
            before.UseAfterFreeCount == after.UseAfterFreeCount, "allocation failure or recovery leaked");
        if (!injected) return;
    }
    check(false, "allocation scan did not complete");
}

static void test_controls(void)
{
    static const char duplicate[] = "{'p\\\xe2\x80\xa8q':1,pq:2}";
    static const char bad_utf8[] = "{v:'\\\xc2'}";
    for (unsigned test = 0; test < 2; ++test) {
        const char* doc = test ? bad_utf8 : duplicate;
        check(!xjson5_valid_len(doc, strlen(doc)) && !xrtGetError(), "invalid source was accepted");
    }
    // 放开 JSON5 identity escape 不得让严格 JSON 也接受它。
    static const char strict[] = "\"\\\xe4\xbd\xa0\"";
    pdjson_stream parser;
    pdjson_open_buffer(&parser, strict, sizeof(strict) - 1);
    check(pdjson_next(&parser) == PDJSON_ERROR, "strict JSON accepted an identity escape");
    pdjson_close(&parser);
}

int main(void)
{
    test_values();
    test_line_positions();
    test_comments();
    test_oom();
    test_controls();
    xrtClearError();
    if (!failures) printf("JSON5 Unicode: %u cases, %u lines, %u OOM points PASS\n", cases, diagnostics, oomPoints);
    return failures ? 1 : 0;
}
