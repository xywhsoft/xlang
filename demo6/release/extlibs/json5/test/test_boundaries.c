#define XRT_MODULE_ALL
#include "xrt.h"
#include "../json5_native.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "JSON5 boundary failed at %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int invalid(const char* text, size_t size) {
    xvalue* value = xjson5_parse_len(text, size);
    int rejected = value && xrtValueType(value) == XVALUE_NULL && xjson5_has_error();
    xrtValueRelease(value);
    xrtClearError();
    return rejected;
}

int main(void) {
    static const char source[] = "{ // comment\n name: 'xlang', count: +0x2a, "
        "items: [1,2,], nested: {ok:true}, '你好': '世界', }";
    static const char embedded_nul[] = {'t', 'r', 'u', 'e', 0, 'x'};
    xvalue *root, *item, *selected;
    xstrview text;
    int64 integer;
    double number;
    unsigned char noise[40];
    char huge_hex[260];
    uint32_t seed = 0x5a123456u;
    size_t i, j;

    root = xjson5_parse_len(source, strlen(source));
    if (xjson5_has_error()) fprintf(stderr, "JSON5 parse: %s\n", xjson5_error_message());
    CHECK(root && xrtValueType(root) == XVALUE_OBJECT);
    CHECK(xjson5_valid_len(source, strlen(source)));
    item = xrtValueObjectGet(root, (xstrview){ "name", 4 });
    CHECK(item && xrtValueGetString(item, &text) && text.Size == 5 &&
        memcmp(text.Data, "xlang", 5) == 0);
    item = xrtValueObjectGet(root, (xstrview){ "count", 5 });
    CHECK(item && xrtValueGetInt(item, &integer) && integer == 42);
    item = xrtValueObjectGet(root, (xstrview){ "你好", 6 });
    CHECK(item && xrtValueGetString(item, &text) && text.Size == 6 &&
        memcmp(text.Data, "世界", 6) == 0);
    xrtValueRelease(root);

    /* 外层对象只在最后一个字节闭合；逐处截断均不得接受半份文档。 */
    for (i = 0u; i < sizeof(source) - 1u; ++i) {
        CHECK(invalid(source, i));
        CHECK(!xjson5_valid_len(source, i) && !xjson5_has_error());
    }
    root = xjson5_parse_len(source, sizeof(source) - 1u);
    CHECK(root && !xjson5_has_error() && xrtValueType(root) == XVALUE_OBJECT);
    xrtValueRelease(root);

    /* 转义后的 NUL 是字符串内容；原始 NUL 源字节仍在下方拒绝。 */
    {
        static const char escaped[] = "{'a\\u0000b':'x\\u0000y',v:'x\\u0000y'}";
        static const char key[] = {'a', 0, 'b'};
        static const char expected[] = {'x', 0, 'y'};
        root = xjson5_parse_len(escaped, sizeof(escaped) - 1u);
        CHECK(root && !xjson5_has_error());
        item = xrtValueObjectGet(root, (xstrview){key, sizeof(key)});
        CHECK(xrtValueGetString(item, &text) && text.Size == sizeof(expected) &&
            memcmp(text.Data, expected, sizeof(expected)) == 0);
        xrtValueRelease(root);
        selected = xjson5_get_value_len(escaped, sizeof(escaped) - 1u, "v");
        CHECK(xrtValueGetString(selected, &text) && text.Size == sizeof(expected) &&
            memcmp(text.Data, expected, sizeof(expected)) == 0);
        xrtValueRelease(selected);
        CHECK(xjson5_get_text_len(escaped, sizeof(escaped) - 1u, "v") == NULL &&
            xjson5_has_error());
    }

    selected = xjson5_get_value_len(source, strlen(source), "items.1");
    CHECK(selected && xrtValueGetInt(selected, &integer) && integer == 2);
    xrtValueRelease(selected);
    selected = xjson5_get_value_len("{a:1} trailing", 14u, "a");
    CHECK(selected && xrtValueType(selected) == XVALUE_NULL && xjson5_has_error());
    xrtValueRelease(selected);
    xrtClearError();
    selected = xjson5_get_value_len("null", 4, "");
    CHECK(selected && xrtValueType(selected) == XVALUE_NULL && !xjson5_has_error());
    xrtValueRelease(selected);
    root = xjson5_parse_len("Infinity", 8);
    CHECK(root && xrtValueGetFloat(root, &number) && isinf(number));
    xrtValueRelease(root);
    root = xjson5_parse_len("-Infinity", 9);
    CHECK(root && xrtValueGetFloat(root, &number) && isinf(number) && signbit(number));
    xrtValueRelease(root);
    root = xjson5_parse_len("NaN", 3);
    CHECK(root && xrtValueGetFloat(root, &number) && isnan(number));
    xrtValueRelease(root);
    root = xjson5_parse_len("1.7976931348623157e308", strlen("1.7976931348623157e308"));
    CHECK(root && xrtValueGetFloat(root, &number) && isfinite(number));
    xrtValueRelease(root);
    root = xjson5_parse_len("-0", 2);
    CHECK(root && xrtValueGetFloat(root, &number) && signbit(number));
    xrtValueRelease(root);
    /* 十六进制的 e/E 是数字，不是十进制指数。 */
    root = xjson5_parse_len("0xdead", 6);
    CHECK(root && xrtValueType(root) == XVALUE_INT &&
        xrtValueGetInt(root, &integer) && integer == 0xdead);
    xrtValueRelease(root);
    root = xjson5_parse_len("0xBEEF", 6);
    CHECK(root && xrtValueType(root) == XVALUE_INT &&
        xrtValueGetInt(root, &integer) && integer == 0xBEEF);
    xrtValueRelease(root);
    root = xjson5_parse_len("-0x1e", 5);
    CHECK(root && xrtValueType(root) == XVALUE_INT &&
        xrtValueGetInt(root, &integer) && integer == -0x1e);
    xrtValueRelease(root);
    root = xjson5_parse_len("0x8000000000000000", 18);
    CHECK(root && xrtValueType(root) == XVALUE_FLOAT && !xjson5_has_error());
    CHECK(xrtValueGetFloat(root, &number) && number == 9223372036854775808.0);
    xrtValueRelease(root);
    memcpy(huge_hex, "0x1", 3);
    memset(huge_hex + 3, '0', 256);
    huge_hex[259] = 0;
    CHECK(invalid(huge_hex, 259));
    CHECK(invalid("1e999", 5));
    root = xjson5_parse_len("'\\uD83C\\uDFBC'", strlen("'\\uD83C\\uDFBC'"));
    CHECK(root && xrtValueGetString(root, &text) && text.Size == 4 &&
        memcmp(text.Data, "\xF0\x9F\x8E\xBC", 4) == 0);
    xrtValueRelease(root);

    /* Unicode 键名、转义归一化及补充平面码点使用相同的键比较语义。 */
    {
        const char* unicode = "{你好:1, \\u006Eame:2, a\\u200Cb:3, 𐐀:4, cafe\\u0301:5}";
        const char* keys[] = { "你好", "name", "a\xE2\x80\x8C" "b", "𐐀", "cafe\xCC\x81" };
        root = xjson5_parse_len(unicode, strlen(unicode));
        if (xjson5_has_error()) fprintf(stderr, "Unicode JSON5 parse: %s\n", xjson5_error_message());
        CHECK(root && !xjson5_has_error());
        for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
            item = xrtValueObjectGet(root, (xstrview){ keys[i], strlen(keys[i]) });
            CHECK(item && xrtValueGetInt(item, &integer) && integer == (int64)i + 1);
        }
        xrtValueRelease(root);
    }
    {
        const char* unicode_space = "{\xE1\x9A\x80" "你好\xE3\x80\x80:\xE2\x80\x80" "9}";
        root = xjson5_parse_len(unicode_space, strlen(unicode_space));
        CHECK(root && !xjson5_has_error());
        item = xrtValueObjectGet(root, (xstrview){ "你好", 6 });
        CHECK(item && xrtValueGetInt(item, &integer) && integer == 9);
        xrtValueRelease(root);
    }
    {
        static const uint32_t spaces[] = {
            0x00A0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2003,
            0x2004, 0x2005, 0x2006, 0x2007, 0x2008, 0x2009,
            0x200A, 0x202F, 0x205F, 0x3000, 0x2028, 0x2029, 0xFEFF
        };
        char document[16] = "{a";
        for (i = 0; i < sizeof(spaces) / sizeof(spaces[0]); ++i) {
            size_t width = xrtUtf8Encode(spaces[i], document + 2);
            CHECK(width > 0 && width <= 4);
            document[2 + width] = ':';
            document[3 + width] = '1';
            document[4 + width] = '}';
            root = xjson5_parse_len(document, 5 + width);
            CHECK(root && !xjson5_has_error());
            item = xrtValueObjectGet(root, XRT_STR_LITERAL("a"));
            CHECK(item && xrtValueGetInt(item, &integer) && integer == 1);
            xrtValueRelease(root);
        }
    }

    CHECK(invalid("1e", 2));
    CHECK(invalid("/* unclosed", 11));
    CHECK(invalid("{a:1,a:2}", 9));
    CHECK(invalid("true false", 10));
    CHECK(invalid("{☃:1}", strlen("{☃:1}")));
    CHECK(invalid("{a€:1}", strlen("{a€:1}")));
    CHECK(invalid("{\xFF:1}", sizeof("{\xFF:1}") - 1u));
    CHECK(invalid("{a\xE2\x82:1}", sizeof("{a\xE2\x82:1}") - 1u));
    CHECK(invalid("{'\xFF':1}", sizeof("{'\xFF':1}") - 1u));
    CHECK(invalid("{a:'\xFF'}", sizeof("{a:'\xFF'}") - 1u));
    CHECK(invalid("{a:'\xC3\x28'}", sizeof("{a:'\xC3\x28'}") - 1u));
    CHECK(invalid("//\xFF\n{}", sizeof("//\xFF\n{}") - 1u));
    CHECK(invalid("/*\xFF*/{}", sizeof("/*\xFF*/{}") - 1u));
    {
        static const char bad_comment[] = "//\xFF\n{a:1}";
        CHECK(!xjson5_valid_len(bad_comment, sizeof(bad_comment) - 1u) &&
            !xjson5_has_error());
        selected = xjson5_get_value_len(bad_comment,
            sizeof(bad_comment) - 1u, "a");
        CHECK(selected && xrtValueType(selected) == XVALUE_NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(selected);
        xrtClearError();
        CHECK(xjson5_get_text_len(bad_comment,
            sizeof(bad_comment) - 1u, "a") == NULL &&
            xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtClearError();
    }
    CHECK(invalid("{1abc:1}", strlen("{1abc:1}")));
    CHECK(invalid("{a\\u002D:1}", strlen("{a\\u002D:1}")));
    CHECK(invalid("{\\uD800:1}", strlen("{\\uD800:1}")));
    CHECK(invalid("'\\uD800'", strlen("'\\uD800'")));
    CHECK(invalid("'\\uDC00'", strlen("'\\uDC00'")));
    CHECK(invalid("{'你好':1, \\u4F60\\u597D:2}", strlen("{'你好':1, \\u4F60\\u597D:2}")));
    CHECK(invalid(embedded_nul, sizeof(embedded_nul)));
    CHECK(invalid("", 64u * 1024u * 1024u + 1u));
    root = xjson5_parse_len("", 64u * 1024u * 1024u + 1u);
    CHECK(root && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(root);
    root = xjson5_parse_len("", 0u);
    CHECK(root && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(root);
    {
        char nested[140];
        memset(nested, '[', 64u);
        nested[64] = '0';
        memset(nested + 65u, ']', 64u);
        CHECK(xjson5_valid_len(nested, 129u));
        memset(nested, '[', 65u);
        nested[65] = '0';
        memset(nested + 66u, ']', 65u);
        CHECK(invalid(nested, 131u));
        root = xjson5_parse_len(nested, 131u);
        CHECK(root && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtValueRelease(root);
    }

    /* 分配失败不能伪装成 JSON5 语法错误。 */
    {
        static const char nested[] = "{items:[1,2,3],name:'value'}";
        size_t failures = 0u;
        for (i = 0u; i < 64u; ++i) {
            bool triggered;
            xmemdebugsnapshot before, after;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            CHECK(xrtMemDebugFailAfter(i));
            root = xjson5_parse_len(nested, sizeof(nested) - 1u);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (!triggered) {
                CHECK(root != NULL && !xjson5_has_error());
            } else {
                CHECK(root != NULL && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++failures;
            }
            xrtValueRelease(root);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            if (after.LiveCount != before.LiveCount ||
                after.LiveBytes != before.LiveBytes) {
                fprintf(stderr, "JSON5 parse allocation %zu leaked: %zu -> %zu\n",
                    i, before.LiveCount, after.LiveCount);
                return 1;
            }
            if (!triggered) break;
        }
        CHECK(failures > 0u && i < 64u);
    }
    {
        static const char source[] = "{items:[1,2,3]}";
        CHECK(!xjson5_valid_len("{bad:", 5u) && !xjson5_has_error());
        CHECK(xrtMemDebugFailAfter(0u));
        CHECK(!xjson5_valid_len(source, sizeof(source) - 1u));
        CHECK(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        CHECK(xjson5_valid_len(source, sizeof(source) - 1u) && !xjson5_has_error());
    }

    /* 固定种子畸形输入保证每次覆盖同一组解析边界。 */
    xmemdebugsnapshot before, after;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (i = 0; i < 5000; ++i) {
        size_t size = i % sizeof(noise);
        for (j = 0; j < size; ++j) {
            seed = seed * 1664525u + 1013904223u;
            noise[j] = (unsigned char)(seed >> 24);
        }
        root = xjson5_parse_len((const char*)noise, size);
        CHECK(root != NULL);
        xrtValueRelease(root);
    }
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    CHECK(after.LiveCount == before.LiveCount &&
        after.LiveBytes == before.LiveBytes);
    puts("json5 boundaries: ok");
    return 0;
}
