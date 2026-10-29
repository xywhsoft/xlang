#define XRT_MODULE_ALL
#include "json_patch.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "json.patch boundary failed at %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static xvalue* parse(const char* text) {
    xstrview source = { text, strlen(text) };
    return xrtJsonParse(source);
}

static int same(const xvalue* actual, const char* expected) {
    xvalue* value = parse(expected);
    int equal = value && actual && xrtValueEqual(actual, value);
    xrtValueRelease(value);
    return equal;
}

static int merge_case(const char* source, const char* patch_text,
    const char* expected) {
    xvalue *doc = parse(source), *patch = parse(patch_text);
    xvalue *result = doc && patch ? xjsonpatch_merge(doc, patch) : NULL;
    int okay = result && !xjsonpatch_has_error() &&
        same(result, expected) && same(doc, source);
    xrtValueRelease(result);
    xrtValueRelease(patch);
    xrtValueRelease(doc);
    return okay;
}

/* 深层合并只应复制目标一次；故障阈值防止每层重新克隆整个子树。 */
static int merge_allocation_shape(void) {
    xstrview branch = {"a", 1u}, valueKey = {"value", 5u};
    xvalue *doc = xrtValueObject(), *patch = xrtValueObject();
    xvalue *result, *node;
    int64 number;
    int triggered, i;
    CHECK(doc && patch &&
        xrtValueObjectSetNew(doc, valueKey, xrtValueInt(1)) &&
        xrtValueObjectSetNew(patch, valueKey, xrtValueInt(2)));
    for (i = 0; i < 32; ++i) {
        xvalue *nextDoc = xrtValueObject(), *nextPatch = xrtValueObject();
        CHECK(nextDoc && nextPatch &&
            xrtValueObjectSetNew(nextDoc, branch, doc) &&
            xrtValueObjectSetNew(nextPatch, branch, patch));
        doc = nextDoc;
        patch = nextPatch;
    }
    CHECK(xrtMemDebugFailAfter(300));
    result = xjsonpatch_merge(doc, patch);
    triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    CHECK(result && !triggered && !xjsonpatch_has_error());
    node = result;
    for (i = 0; i < 32; ++i) node = xrtValueObjectGet(node, branch);
    CHECK(node && xrtValueGetInt(xrtValueObjectGet(node, valueKey), &number) &&
        number == 2);
    node = doc;
    for (i = 0; i < 32; ++i) node = xrtValueObjectGet(node, branch);
    CHECK(node && xrtValueGetInt(xrtValueObjectGet(node, valueKey), &number) &&
        number == 1);
    xrtValueRelease(result);
    xrtValueRelease(patch);
    xrtValueRelease(doc);
    return 0;
}

static int applies(const char* source, const char* operations,
    const char* expected) {
    xvalue* doc = parse(source);
    xvalue* patch = parse(operations);
    xvalue* result = doc && patch ? xjsonpatch_apply(doc, patch) : NULL;
    int okay = result && !xjsonpatch_has_error() && same(result, expected) &&
        same(doc, source);
    xrtValueRelease(result);
    xrtValueRelease(patch);
    xrtValueRelease(doc);
    return okay;
}

static int rejects_unchanged(const char* source, const char* operations) {
    xvalue* doc = parse(source);
    xvalue* patch = parse(operations);
    xvalue* result = doc && patch ? xjsonpatch_apply(doc, patch) : NULL;
    int rejected = result && xjsonpatch_has_error() &&
        xrtValueType(result) == XVALUE_NULL;
    xrtClearError();
    rejected = rejected && same(doc, source);
    xrtValueRelease(result);
    xrtValueRelease(patch);
    xrtValueRelease(doc);
    return rejected;
}

static int allocation_boundaries(xvalue* doc, xvalue* operations) {
    xvalue* mergePatch = parse("{\"name\":\"changed\"}");
    xvalue* mergeDelete = parse("{\"name\":null}");
    xvalue* movePatch = parse("[{\"op\":\"move\",\"from\":\"/arr/0\",\"path\":\"/arr/1\"}]");
    xvalue* copyPatch = parse("[{\"op\":\"copy\",\"from\":\"/name\",\"path\":\"/copy\"}]");
    /* test 的结构相等比较也会分配，故障不能被改报为比较失败。 */
    xvalue* testPatch = parse("[{\"op\":\"test\",\"path\":\"/arr\",\"value\":[1,2]}]");
    int operation;
    CHECK(mergePatch != NULL && mergeDelete != NULL &&
        movePatch != NULL && copyPatch != NULL && testPatch != NULL);
    CHECK(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    for (operation = 0; operation < 8; ++operation) {
        int successes = 0;
        int failures = 0;
        int index;
        for (index = 0; index < 256 && successes < 2; ++index) {
            xmemdebugsnapshot before, after;
            xvalue* result;
            bool injected;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            CHECK(xrtMemDebugFailAfter(index));
            if (operation == 0) result = xjsonpatch_apply(doc, operations);
            else if (operation == 1) result = xjsonpatch_set(doc, "/name", operations);
            else if (operation == 2) result = xjsonpatch_remove(doc, "/name");
            else if (operation == 3) result = xjsonpatch_merge(doc, mergePatch);
            else if (operation == 4) result = xjsonpatch_merge(doc, mergeDelete);
            else if (operation == 5) result = xjsonpatch_apply(doc, movePatch);
            else if (operation == 6) result = xjsonpatch_apply(doc, copyPatch);
            else result = xjsonpatch_apply(doc, testPatch);
            injected = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (injected) {
                CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                CHECK(result == NULL || xrtValueType(result) == XVALUE_NULL);
                ++failures;
            } else {
                CHECK(result && xrtGetError() == NULL);
                if (operation == 4) {
                    CHECK(same(result, "{\"arr\":[1,2],\"nil\":null}"));
                } else if (operation == 5) {
                    CHECK(same(result, "{\"arr\":[2,1],\"name\":\"ok\",\"nil\":null}"));
                } else if (operation == 6) {
                    CHECK(same(result, "{\"arr\":[1,2],\"name\":\"ok\",\"nil\":null,\"copy\":\"ok\"}"));
                } else if (operation == 7) {
                    CHECK(same(result, "{\"arr\":[1,2],\"name\":\"ok\",\"nil\":null}"));
                }
                ++successes;
            }
            xrtValueRelease(result);
            xrtClearError();
            CHECK(same(doc, "{\"arr\":[1,2],\"name\":\"ok\",\"nil\":null}"));
            xrtMemDebugSnapshot(&after);
            CHECK(before.LiveCount == after.LiveCount &&
                before.LiveBytes == after.LiveBytes);
        }
        CHECK(failures > 0 && successes == 2);
    }
    xrtValueRelease(mergePatch);
    xrtValueRelease(mergeDelete);
    xrtValueRelease(movePatch);
    xrtValueRelease(copyPatch);
    xrtValueRelease(testPatch);
    return 1;
}

static int invalid_pointer_error(xvalue* doc) {
    xvalue* bad = xrtValueInt(1);
    xvalue* value = xrtValueInt(2);
    int okay = bad != NULL && value != NULL;
    int kind;
    for (kind = 0; okay && kind < 2; ++kind) {
        int operation;
        xvalue* pointer = kind == 0 ? NULL : bad;
        xerrkind expected = kind == 0 ? XERR_VALUE : XERR_TYPE;
        for (operation = 0; okay && operation < 4; ++operation) {
            xvalue* result = NULL;
            bool found = false;
            xrtSetErrorInfo(XERR_MEMORY, "json.patch.test", 0,
                "previous operation failed");
            if (operation == 0) result = xjsonpatch_get_value(doc, pointer);
            else if (operation == 1) found = xjsonpatch_has_value(doc, pointer);
            else if (operation == 2) result = xjsonpatch_set_value(doc, pointer, value);
            else result = xjsonpatch_remove_value(doc, pointer);
            okay = !found && (result == NULL ||
                xrtValueType(result) == XVALUE_NULL) &&
                xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == expected;
            if (!okay) fprintf(stderr, "invalid pointer kind %d operation %d: error %d\n",
                kind, operation, xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : -1);
            xrtValueRelease(result);
            xrtClearError();
        }
    }
    xrtValueRelease(value);
    xrtValueRelease(bad);
    return okay;
}

int main(void) {
    xvalue* original = parse("{\"arr\":[1,2],\"name\":\"ok\",\"nil\":null}");
    xvalue* patch = parse("[{\"op\":\"add\",\"path\":\"/arr/0\",\"value\":0}]");
    xvalue* changed;
    xvalue* item;
    char long_path[260];
    char long_key[301];
    char *encoded, *encoded_again;

    CHECK(original && patch);
    /* 合并补丁的对象递归、数组替换及根类型切换互不混淆。 */
    CHECK(merge_case("{\"a\":{\"b\":1,\"c\":2},\"d\":4}",
        "{\"a\":{\"b\":null,\"e\":[1,2]},\"d\":null}",
        "{\"a\":{\"c\":2,\"e\":[1,2]}}"));
    CHECK(merge_case("{\"a\":[1]}", "{\"a\":[2,3]}",
        "{\"a\":[2,3]}"));
    CHECK(merge_case("[1,2]", "{\"a\":1}", "{\"a\":1}"));
    CHECK(merge_case("{\"a\":1}", "[3]", "[3]"));
    {
        static const char *invalid[] = {"[null]", "[1]", "[[]]", "[\"text\"]"};
        size_t i;
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            xvalue *bad = parse(invalid[i]);
            CHECK(bad != NULL);
            changed = xjsonpatch_apply(original, bad);
            CHECK(changed && xrtValueType(changed) == XVALUE_NULL &&
                xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE &&
                same(original, "{\"arr\":[1,2],\"name\":\"ok\",\"nil\":null}"));
            xrtValueRelease(changed);
            xrtValueRelease(bad);
        }
    }
    CHECK(!xjsonpatch_has_len(NULL, "", 0u) &&
        xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    {
        const char slash[] = {'/'};
        item = xjsonpatch_get_len(original, slash, SIZE_MAX);
        CHECK(item && xrtValueType(item) == XVALUE_NULL && xrtGetError());
        xrtValueRelease(item);
        xrtClearError();
        CHECK(!xjsonpatch_has_len(original, slash, SIZE_MAX) && xrtGetError());
        xrtClearError();
        changed = xjsonpatch_set_len(original, slash, SIZE_MAX, original);
        CHECK(changed && xrtValueType(changed) == XVALUE_NULL && xrtGetError());
        xrtValueRelease(changed);
        xrtClearError();
        changed = xjsonpatch_remove_len(original, slash, SIZE_MAX);
        CHECK(changed && xrtValueType(changed) == XVALUE_NULL && xrtGetError());
        xrtValueRelease(changed);
        xrtClearError();
    }
    {
        char overlong[32u * (1u + 2u * 255u) + 1u];
        memset(overlong, '/', sizeof(overlong));
        item = xjsonpatch_get(original, overlong);
        CHECK(item && xrtValueType(item) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(item);
        CHECK(!xjsonpatch_has(original, overlong) &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        changed = xjsonpatch_set(original, overlong, original);
        CHECK(changed && xrtValueType(changed) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(changed);
        changed = xjsonpatch_remove(original, overlong);
        CHECK(changed && xrtValueType(changed) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(changed);
    }
    {
        char full[32u * (1u + 2u * 255u) + 1u];
        size_t at = 0;
        unsigned int segment, byte;
        for (segment = 0; segment < 32u; ++segment) {
            full[at++] = '/';
            for (byte = 0; byte < 255u; ++byte) {
                full[at++] = '~';
                full[at++] = '1';
            }
        }
        CHECK(at == sizeof(full) - 1u);
        full[at] = '\0';
        CHECK(!xjsonpatch_has_len(original, full, at) && xrtGetError() == NULL);
        CHECK(!xjsonpatch_has(original, full) && xrtGetError() == NULL);
    }
    CHECK(invalid_pointer_error(original));
    CHECK(allocation_boundaries(original, patch));
    changed = xjsonpatch_apply(original, patch);
    CHECK(same(changed, "{\"arr\":[0,1,2],\"name\":\"ok\",\"nil\":null}"));
    CHECK(same(original, "{\"arr\":[1,2],\"name\":\"ok\",\"nil\":null}"));
    xrtValueRelease(changed);
    xrtValueRelease(patch);

    item = xjsonpatch_get(original, "/nil");
    CHECK(item && xrtValueType(item) == XVALUE_NULL && !xjsonpatch_has_error());
    xrtValueRelease(item);
    item = xrtValueNull();
    changed = xjsonpatch_merge(original, item);
    CHECK(changed && xrtValueType(changed) == XVALUE_NULL &&
        !xjsonpatch_has_error());
    xrtValueRelease(changed);
    xrtValueRelease(item);

    item = xjsonpatch_get(original, "/arr/01");
    CHECK(item && xrtValueType(item) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(item);
    xrtClearError();

    item = xjsonpatch_get(original, "/arr/9999999999999999999999999999");
    CHECK(item && xrtValueType(item) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(item);
    xrtClearError();

    long_path[0] = '/';
    memset(long_path + 1, 'a', 255);
    long_path[256] = '\0';
    item = xjsonpatch_get(original, long_path);
    CHECK(item && xrtValueType(item) == XVALUE_NULL && xrtGetError());
    xrtValueRelease(item);
    xrtClearError();

    CHECK(xjsonpatch_has(original, "/name"));
    {
        xvalue* pointer = xrtValueString((xstrview){"/name", 5});
        xstrview text = {0};
        CHECK(pointer && xjsonpatch_has_value(original, pointer));
        item = xjsonpatch_get_value(original, pointer);
        CHECK(item && xrtValueType(item) == XVALUE_STRING &&
            xrtValueGetString(item, &text) && text.Size == 2 &&
            memcmp(text.Data, "ok", 2) == 0 &&
            !xjsonpatch_has_error());
        xrtValueRelease(item);
        xrtValueRelease(pointer);
    }
    CHECK(!xjsonpatch_has(original, "/missing"));
    memset(long_key, 'x', 255);
    long_key[255] = '\0';
    encoded = xjsonpatch_encode_pointer_len(long_key, 255);
    encoded_again = xjsonpatch_encode_pointer_len("a/b~c", 5);
    CHECK(encoded && encoded_again && strlen(encoded) == 256 &&
        encoded[0] == '/' && encoded[255] == 'x' &&
        strcmp(encoded_again, "/a~1b~0c") == 0);
    xrtFree(encoded);
    xrtFree(encoded_again);
    long_key[255] = 'x';
    long_key[256] = '\0';
    CHECK(xjsonpatch_encode_pointer_len(long_key, 256) == NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    {
        const char tiny[] = {'x'};
        CHECK(xjsonpatch_encode_pointer_len(tiny, SIZE_MAX) == NULL &&
            xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtClearError();
    }
    long_key[254] = '/';
    long_key[255] = '\0';
    encoded = xjsonpatch_encode_pointer_len(long_key, 255);
    CHECK(encoded && strlen(encoded) == 257 &&
        !xjsonpatch_has(original, encoded) && !xjsonpatch_has_error());
    xrtFree(encoded);
    memset(long_key, 'x', 300);
    long_key[300] = '\0';
    CHECK(xjsonpatch_encode_pointer_len("a\0b", 3) == NULL &&
        xjsonpatch_has_error());
    xrtClearError();
    {
        xvalue* key = xrtValueString((xstrview){"a\0b", 3});
        xmemdebugsnapshot before, after;
        int failures = 0, successes = 0;
        CHECK(key != NULL);
        xrtMemDebugSnapshot(&before);
        for (int i = 0; i < 64 && successes < 2; ++i) {
            xvalue* encoded_value;
            xstrview text;
            bool injected;
            CHECK(xrtMemDebugFailAfter(i));
            encoded_value = xjsonpatch_encode_pointer_value(key);
            injected = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (injected) {
                CHECK(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
                    encoded_value && xrtValueType(encoded_value) == XVALUE_NULL);
                ++failures;
            } else {
                CHECK(encoded_value && xrtValueGetString(encoded_value, &text) &&
                    text.Size == 4u && memcmp(text.Data, "/a\0b", 4u) == 0);
                ++successes;
            }
            xrtValueRelease(encoded_value);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        }
        CHECK(failures > 0 && successes == 2);
        xrtValueRelease(key);
    }
    CHECK(xjsonpatch_encode_pointer_len("\xff", 1u) == NULL &&
        xjsonpatch_has_error());
    xrtClearError();

    CHECK(applies("{\"a\":1}",
        "[{\"op\":\"add\",\"path\":\"\",\"value\":{\"b\":2}},"
        "{\"op\":\"add\",\"path\":\"/b\",\"value\":3}]",
        "{\"b\":3}"));
    CHECK(applies("{\"a\":1}",
        "[{\"op\":\"replace\",\"path\":\"\",\"value\":[4]}]", "[4]"));
    CHECK(applies("{\"a\":1}",
        "[{\"op\":\"remove\",\"path\":\"\"}]", "null"));
    CHECK(applies("{\"a\":1}",
        "[{\"op\":\"move\",\"from\":\"/a\",\"path\":\"\"}]", "1"));
    CHECK(applies("{\"a\":1}",
        "[{\"op\":\"copy\",\"from\":\"\",\"path\":\"/b\"}]",
        "{\"a\":1,\"b\":{\"a\":1}}"));
    CHECK(applies("{\"a\":1}",
        "[{\"op\":\"copy\",\"from\":\"/a\",\"path\":\"\"}]", "1"));
    CHECK(applies("{\"a\":1}",
        "[{\"op\":\"test\",\"path\":\"/a\",\"value\":1.0}]",
        "{\"a\":1}"));
    CHECK(applies("[\"a\",\"b\",\"c\",\"d\"]",
        "[{\"op\":\"move\",\"from\":\"/1\",\"path\":\"/3\"}]",
        "[\"a\",\"c\",\"d\",\"b\"]"));
    CHECK(applies("[\"a\",\"b\",\"c\",\"d\"]",
        "[{\"op\":\"move\",\"from\":\"/3\",\"path\":\"/1\"}]",
        "[\"a\",\"d\",\"b\",\"c\"]"));
    CHECK(applies("{\"a\":1,\"b\":2}",
        "[{\"op\":\"move\",\"from\":\"/a\",\"path\":\"/a\"}]",
        "{\"a\":1,\"b\":2}"));
    CHECK(applies("[\"a\",\"b\"]",
        "[{\"op\":\"move\",\"from\":\"/0\",\"path\":\"/0\"}]",
        "[\"a\",\"b\"]"));
    CHECK(applies("[\"a\",\"b\"]",
        "[{\"op\":\"copy\",\"from\":\"/0\",\"path\":\"/0\"}]",
        "[\"a\",\"a\",\"b\"]"));
    CHECK(applies("{\"a\":1}",
        "[{\"op\":\"move\",\"from\":\"\",\"path\":\"\"}]",
        "{\"a\":1}"));
    CHECK(applies("{\"nested\":{\"a\":[1],\"b\":true}}",
        "[{\"op\":\"test\",\"path\":\"/nested\","
        "\"value\":{\"b\":true,\"a\":[1.0]}}]",
        "{\"nested\":{\"a\":[1],\"b\":true}}"));
    CHECK(applies("{\"/\":9,\"~1\":10}",
        "[{\"op\":\"test\",\"path\":\"/~01\",\"value\":10}]",
        "{\"/\":9,\"~1\":10}"));
    CHECK(applies("{\"foo\":[\"bar\"]}",
        "[{\"op\":\"add\",\"path\":\"/foo/-\",\"value\":[\"abc\",\"def\"]}]",
        "{\"foo\":[\"bar\",[\"abc\",\"def\"]]}"));
    CHECK(applies("{\"foo\":\"bar\"}",
        "[{\"op\":\"add\",\"path\":\"/baz\",\"value\":\"qux\",\"xyz\":123}]",
        "{\"foo\":\"bar\",\"baz\":\"qux\"}"));
    CHECK(rejects_unchanged("{\"foo\":\"bar\"}",
        "[{\"op\":\"add\",\"path\":\"/baz/bat\",\"value\":\"qux\"}]"));
    CHECK(rejects_unchanged("{\"foo\":\"bar\"}",
        "[{\"op\":\"replace\",\"path\":\"/foo\",\"value\":\"qux\"},"
        "{\"op\":\"test\",\"path\":\"/foo\",\"value\":\"bar\"}]"));
    CHECK(rejects_unchanged("{\"~1\":10}",
        "[{\"op\":\"test\",\"path\":\"/~01\",\"value\":\"10\"}]"));

    patch = parse("[{\"op\":\"move\",\"from\":\"\",\"path\":\"/child\"}]");
    CHECK(patch != NULL);
    changed = xjsonpatch_apply(original, patch);
    CHECK(changed && xrtValueType(changed) == XVALUE_NULL &&
        xjsonpatch_has_error());
    xrtValueRelease(changed);
    xrtValueRelease(patch);
    xrtClearError();
    patch = parse("[{\"op\":\"move\",\"from\":\"/missing\",\"path\":\"/missing\"}]");
    CHECK(patch != NULL);
    changed = xjsonpatch_apply(original, patch);
    CHECK(changed != NULL && xrtValueType(changed) == XVALUE_NULL &&
        xjsonpatch_has_error() &&
        same(original, "{\"arr\":[1,2],\"name\":\"ok\",\"nil\":null}"));
    xrtValueRelease(changed);
    xrtValueRelease(patch);
    xrtClearError();
    {
        xvalue *escaped_doc = parse("{\"~1\":{\"child\":1}}");
        xvalue *escaped_move = parse("[{\"op\":\"move\",\"from\":\"/~01\","
            "\"path\":\"/~01/child\"}]");
        CHECK(escaped_doc != NULL && escaped_move != NULL);
        changed = xjsonpatch_apply(escaped_doc, escaped_move);
        CHECK(changed != NULL && xrtValueType(changed) == XVALUE_NULL &&
            xjsonpatch_has_error() &&
            same(escaped_doc, "{\"~1\":{\"child\":1}}"));
        xrtValueRelease(changed);
        xrtValueRelease(escaped_move);
        xrtValueRelease(escaped_doc);
        xrtClearError();
    }
    patch = parse("[{\"op\":\"replace\",\"path\":\"/name\",\"value\":3},"
        "{\"op\":\"remove\",\"path\":\"/missing\"}]");
    CHECK(patch);
    changed = xjsonpatch_apply(original, patch);
    CHECK(changed && xrtValueType(changed) == XVALUE_NULL &&
        xjsonpatch_has_error() &&
        same(original, "{\"arr\":[1,2],\"name\":\"ok\",\"nil\":null}"));
    xrtValueRelease(changed);
    xrtValueRelease(patch);
    xrtClearError();

    {
        const char pointer[] = {'/', 'a', '\0', 'b'};
        xvalue* nul_doc = parse("{\"a\\u0000b\":1,\"arr\":[9]}");
        xvalue* two = xrtValueInt(2);
        CHECK(nul_doc && two && xjsonpatch_has_len(nul_doc, pointer, sizeof(pointer)));
        item = xjsonpatch_get_len(nul_doc, pointer, sizeof(pointer));
        CHECK(item && same(item, "1"));
        xrtValueRelease(item);
        changed = xjsonpatch_set_len(nul_doc, pointer, sizeof(pointer), two);
        CHECK(changed && same(changed, "{\"a\\u0000b\":2,\"arr\":[9]}") &&
            same(nul_doc, "{\"a\\u0000b\":1,\"arr\":[9]}"));
        xrtValueRelease(changed);
        changed = xjsonpatch_remove_len(nul_doc, pointer, sizeof(pointer));
        CHECK(changed && same(changed, "{\"arr\":[9]}"));
        xrtValueRelease(changed);
        xrtValueRelease(two);
        xrtValueRelease(nul_doc);
    }
    CHECK(applies("{\"a\\u0000b\":1}",
        "[{\"op\":\"replace\",\"path\":\"/a\\u0000b\",\"value\":2}]",
        "{\"a\\u0000b\":2}"));
    {
        const char bad_utf8[] = {'/', (char)0xff};
        CHECK(!xjsonpatch_has_len(original, bad_utf8, sizeof(bad_utf8)) &&
            xjsonpatch_has_error());
        xrtClearError();
    }
    {
        xvalue* doc = parse("{\"a\\u0000b\":1}");
        xvalue* merge = parse("{\"a\\u0000b\":2}");
        changed = xjsonpatch_merge(doc, merge);
        CHECK(changed && !xjsonpatch_has_error() &&
            same(changed, "{\"a\\u0000b\":2}") &&
            same(doc, "{\"a\\u0000b\":1}"));
        xrtValueRelease(changed);
        xrtValueRelease(merge);
        xrtValueRelease(doc);
    }
    {
        xvalue* doc = parse("{\"a\":1}");
        xvalue* merge = parse("{\"missing\":null}");
        changed = xjsonpatch_merge(doc, merge);
        CHECK(changed && same(changed, "{\"a\":1}") &&
            !xjsonpatch_has_error());
        xrtValueRelease(changed);
        xrtValueRelease(merge);
        xrtValueRelease(doc);
    }
    {
        char pointer[302];
        xvalue* doc = xrtValueObject();
        pointer[0] = '/';
        memcpy(pointer + 1, long_key, 300u);
        CHECK(doc && xrtValueObjectSetNew(doc,
            (xstrview){long_key, 255u}, xrtValueInt(5)));
        item = xjsonpatch_get_len(doc, pointer, 256u);
        CHECK(item && same(item, "5") && !xjsonpatch_has_error());
        xrtValueRelease(item);
        item = xjsonpatch_get_len(doc, pointer, 257u);
        CHECK(item && xrtValueType(item) == XVALUE_NULL &&
            xjsonpatch_has_error());
        xrtValueRelease(item);
        xrtValueRelease(doc);
        xrtClearError();
    }
    {
        xvalue* doc = xrtValueObject();
        xvalue* merge = xrtValueObject();
        int64 number = 0;
        xstrview key = {long_key, 300u};
        CHECK(doc && merge &&
            xrtValueObjectSetNew(merge, key, xrtValueInt(17)));
        changed = xjsonpatch_merge(doc, merge);
        item = changed ? xrtValueObjectGet(changed, key) : NULL;
        CHECK(item && xrtValueGetInt(item, &number) && number == 17 &&
            !xjsonpatch_has_error());
        xrtValueRelease(changed);
        xrtValueRelease(merge);
        xrtValueRelease(doc);
    }
    {
        xvalue* deep = xrtValueObject();
        int i;
        CHECK(deep);
        for (i = 0; i < 65; ++i) {
            xvalue* next = xrtValueObject();
            CHECK(next && xrtValueObjectSetNew(next, (xstrview){"a", 1u}, deep));
            deep = next;
        }
        changed = xjsonpatch_merge(original, deep);
        CHECK(changed && xrtValueType(changed) == XVALUE_NULL &&
            xjsonpatch_has_error());
        xrtValueRelease(changed);
        xrtValueRelease(deep);
        xrtClearError();
    }
    xrtValueRelease(original);
    CHECK(merge_allocation_shape() == 0);
    puts("json.patch boundaries: ok");
    return 0;
}
