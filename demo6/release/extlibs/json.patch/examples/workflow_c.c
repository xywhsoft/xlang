#define XRT_MODULE_ALL
#include "../include/json_patch.h"

#include <stdio.h>
#include <string.h>

static xvalue *parse(const char *text) {
    return xrtJsonParse((xstrview){text, strlen(text)});
}

static int text_at(xvalue *document, const char *path, const char *expected) {
    xvalue *value = xjsonpatch_get(document, path);
    xstrview text;
    size_t length = strlen(expected);
    int equal = value != NULL && !xjsonpatch_has_error() &&
        xrtValueGetString(value, &text) && text.Size == length &&
        memcmp(text.Data, expected, length) == 0;
    xrtValueRelease(value);
    return equal;
}

int main(void) {
    xvalue *source = parse("{\"name\":\"demo\",\"tags\":[\"one\",\"two\"],\"settings\":{\"old\":true}}");
    xvalue *operations = parse("["
        "{\"op\":\"test\",\"path\":\"/name\",\"value\":\"demo\"},"
        "{\"op\":\"copy\",\"from\":\"/name\",\"path\":\"/label\"},"
        "{\"op\":\"move\",\"from\":\"/tags/0\",\"path\":\"/tags/1\"},"
        "{\"op\":\"replace\",\"path\":\"/name\",\"value\":\"ready\"},"
        "{\"op\":\"add\",\"path\":\"/tags/-\",\"value\":\"three\"},"
        "{\"op\":\"remove\",\"path\":\"/settings/old\"}]");
    xvalue *patch = parse("{\"settings\":{\"enabled\":true}}");
    xvalue *updated = NULL, *merged = NULL, *configured = NULL, *cleaned = NULL;
    xvalue *mode = NULL;
    int ok = 0;

    if (!source || !operations || !patch) goto done;
    updated = xjsonpatch_apply(source, operations);
    if (!updated || xjsonpatch_has_error() ||
        !text_at(updated, "/name", "ready") ||
        !text_at(updated, "/label", "demo") ||
        !text_at(updated, "/tags/0", "two") ||
        !text_at(updated, "/tags/1", "one") ||
        !text_at(updated, "/tags/2", "three") ||
        xjsonpatch_has(updated, "/settings/old")) goto done;

    merged = xjsonpatch_merge(updated, patch);
    if (!merged || xjsonpatch_has_error() ||
        !xjsonpatch_has(merged, "/settings/enabled")) goto done;
    mode = parse("\"fast\"");
    if (!mode) goto done;
    configured = xjsonpatch_set(merged, "/settings/mode", mode);
    if (!configured || xjsonpatch_has_error() ||
        !text_at(configured, "/settings/mode", "fast")) goto done;
    cleaned = xjsonpatch_remove(configured, "/label");
    if (!cleaned || xjsonpatch_has_error() ||
        xjsonpatch_has(cleaned, "/label") ||
        !text_at(source, "/name", "demo")) goto done;

    puts("json.patch workflow: ok");
    ok = 1;
done:
    xrtValueRelease(cleaned);
    xrtValueRelease(configured);
    xrtValueRelease(mode);
    xrtValueRelease(merged);
    xrtValueRelease(updated);
    xrtValueRelease(patch);
    xrtValueRelease(operations);
    xrtValueRelease(source);
    return ok ? 0 : 1;
}

/* 单文件范例直接运行；正式包仍由 manifest 编译原生实现。 */
#include "../json_patch_native.c"
