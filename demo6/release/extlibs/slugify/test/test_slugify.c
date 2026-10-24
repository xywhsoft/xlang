#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/slugify_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "slugify C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int same(const char* source, const char* expected)
{
    char* result = (char*)xsl_slugify(source, (long long)strlen(source));
    int valid = result != NULL && strcmp(result, expected) == 0;
    xrtFree(result);
    return valid;
}

static int test_idempotence(void)
{
    static const char* parts[] = {
        "A", "\xc3\xa9", "\xe4\xbd\xa0\xe5\xa5\xbd",
        "\xef\xbc\x9a", " ", "&", "@", "\xf0\x9f\x98\x80",
        "a\xcc\x81"
    };
    size_t i, j;
    for (i = 0u; i < sizeof(parts) / sizeof(parts[0]); ++i) {
        for (j = 0u; j < sizeof(parts) / sizeof(parts[0]); ++j) {
            char source[64];
            char* first;
            char* second;
            size_t length;
            int written = snprintf(source, sizeof(source), "%s%s", parts[i], parts[j]);
            CHECK(written >= 0 && (size_t)written < sizeof(source));
            first = (char*)xsl_slugify(source, written);
            CHECK(first != NULL);
            length = strlen(first);
            second = (char*)xsl_slugify(first, (long long)length);
            CHECK(second != NULL && strcmp(first, second) == 0);
            CHECK((length == 0u || (first[0] != '-' && first[length - 1u] != '-')) &&
                strstr(first, "--") == NULL);
            xrtFree(second);
            xrtFree(first);
        }
    }
    return 0;
}

int main(void)
{
    char long_text[12001];
    char long_han[30001];
    char* first;
    char* second;
    char* max_text;
    int i;
    CHECK(test_idempotence() == 0);
    memset(long_text, 'A', sizeof(long_text) - 1u);
    long_text[sizeof(long_text) - 1u] = '\0';
    for (i = 0; i < 10000; ++i)
        memcpy(long_han + 3 * i, "\xe4\xbd\xa0", 3u);
    long_han[30000] = '\0';

    CHECK(same("Hello  --  World", "hello-world"));
    CHECK(same("It's fine!", "its-fine"));
    CHECK(same("Tom & Jerry", "tom-and-jerry"));
    CHECK(same("a&b", "a-and-b"));
    CHECK(same("a\vb", "a-b"));
    CHECK(same("a\fb", "a-b"));
    CHECK(same("a\xc2\x85" "b", "a-b")); /* U+0085 NEL */
    CHECK(same("a\xe1\x9a\x80" "b", "a-b")); /* U+1680 Ogham space */
    CHECK(same("&a", "and-a"));
    CHECK(same("a@", "a-at"));
    CHECK(same("user@example.com", "user-at-example-com"));
    CHECK(same("caf\xc3\xa9 m\xc3\xbcnchen", "cafe-munchen"));
    CHECK(same("\xc2\xa9\xc4\x80\xc4\x81\xc9\xa8\xe1\xba\x99\xe2\x99\xa6", "caaiy"));
    CHECK(same("cafe\xcc\x81", "cafe"));
    CHECK(same("\xe4\xbd\xa0\xe5\xa5\xbd\xef\xbc\x9a\xe4\xb8\x96\xe7\x95\x8c", "\xe4\xbd\xa0\xe5\xa5\xbd-\xe4\xb8\x96\xe7\x95\x8c"));
    CHECK(same("\xef\xbc\xa6\xef\xbd\x95\xef\xbd\x8c\xef\xbd\x8c\xef\xbc\x91\xef\xbc\x92\xef\xbc\x93", "full123"));
    CHECK(same("a\xc3\xb7" "b", "a-b"));
    CHECK(same("日本・語", "日本-語"));
    /* Unicode 17 的 CJK 扩展 G、H、J 必须与常用汉字一样保留。 */
    CHECK(same("\xf0\xb0\x80\x80", "\xf0\xb0\x80\x80")); /* U+30000 */
    CHECK(same("\xf0\xb1\x8d\x90", "\xf0\xb1\x8d\x90")); /* U+31350 */
    CHECK(same("\xf0\xb2\x8e\xb0", "\xf0\xb2\x8e\xb0")); /* U+323B0 */
    CHECK(same("\xf0\xb3\x91\xb9", "\xf0\xb3\x91\xb9")); /* U+33479 */
    CHECK(same("hi\xf0\x9f\x98\x80there", "hithere"));
    CHECK(same("", ""));
    CHECK(same("!!!", ""));

    first = (char*)xsl_slugify(long_text, 12000);
    second = (char*)xsl_slugify("Other", 5);
    CHECK(first != NULL && second != NULL && first != second);
    CHECK(strlen(first) == 12000u && first[0] == 'a' && first[11999] == 'a');
    CHECK(strcmp(second, "other") == 0 && first[0] == 'a');
    xrtFree(first);
    xrtFree(second);

    first = (char*)xsl_slugify(long_han, 30000);
    CHECK(first != NULL && strlen(first) == 30000u &&
        memcmp(first, long_han, 30000u) == 0);
    xrtFree(first);

    max_text = (char*)xrtMalloc(1048578u);
    CHECK(max_text != NULL);
    memset(max_text, 'A', 1048576u);
    max_text[1048576] = '\0';
    first = (char*)xsl_slugify(max_text, 1048576);
    CHECK(first != NULL && strlen(first) == 1048576u &&
        first[0] == 'a' && first[1048575] == 'a');
    xrtFree(first);
    max_text[1048576] = 'A';
    max_text[1048577] = '\0';
    CHECK(xsl_slugify(max_text, 1048577) == NULL);
    xrtFree(max_text);

    CHECK(xsl_slugify("a\0b", 3) == NULL);
    CHECK(xsl_slugify("a\xc0\xaf", 3) == NULL); /* 非最短 UTF-8 */
    CHECK(xsl_slugify("a\xe4\xb8", 3) == NULL); /* 截断的 UTF-8 */
    CHECK(xsl_slugify(NULL, 0) == NULL);
    CHECK(xsl_slugify("a", 1048577) == NULL);
    CHECK(xsl_is_ascii("abc", 3) == 1);
    CHECK(xsl_is_ascii("a\xc3\xa9", 3) == 0);
    CHECK(xsl_is_ascii("a\xc0\xaf", 3) == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xsl_is_ascii("a\xe4\xb8", 3) == -1);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xsl_is_ascii("a\xc3\xa9", 3) == 0 && xrtGetError() == NULL);
    CHECK(xsl_is_ascii("a\0b", 3) == -1);
    CHECK(xsl_is_ascii(NULL, 0) == -1);

    /* 公开入口失败后可直接调用另一入口，不遗留旧错误。 */
    CHECK(xsl_slugify(NULL, 0) == NULL && xrtGetError() != NULL);
    CHECK(xsl_is_ascii("abc", 3) == 1);
    CHECK(xrtGetError() == NULL);
    CHECK(xsl_is_ascii(NULL, 0) == -1 && xrtGetError() != NULL);
    CHECK(same("Hello", "hello"));
    CHECK(xrtGetError() == NULL);

    /* 输出缓冲每个分配点失败后，下次调用仍可正常完成。 */
    {
        unsigned failure, rejected = 0u;
        for (failure = 0u; failure < 16u; ++failure) {
            int triggered;
            CHECK(xrtMemDebugFailAfter(failure));
            first = (char*)xsl_slugify("Caf\xc3\xa9 & \xe4\xbd\xa0\xe5\xa5\xbd", 14);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(first == NULL);
                CHECK(xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                xrtClearError();
                CHECK(same("Caf\xc3\xa9 & \xe4\xbd\xa0\xe5\xa5\xbd",
                    "cafe-and-\xe4\xbd\xa0\xe5\xa5\xbd"));
                ++rejected;
            } else {
                CHECK(first != NULL && strcmp(first,
                    "cafe-and-\xe4\xbd\xa0\xe5\xa5\xbd") == 0);
                xrtFree(first);
                break;
            }
        }
        CHECK(rejected >= 1u && failure < 16u);
    }

    puts("slugify C boundaries: PASS");
    return 0;
}
