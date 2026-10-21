#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../fix_native.h"
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "FIX test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

/* FIX 浮点字段是机器协议文本，不能使用本机的小数点。 */
static int test_numeric_locale(void)
{
    static const char *locales[] = {
        "French_France.1252", "fr-FR", "German_Germany.1252", "de-DE",
        "fr_FR.UTF-8", "de_DE.UTF-8"
    };
    const char *current = setlocale(LC_NUMERIC, NULL);
    char saved[128];
    size_t i;
    int ok = 0;
    xvalue *fields, *field;
    char *message;
    if (current == NULL || strlen(current) >= sizeof(saved)) return 0;
    strcpy(saved, current);
    for (i = 0; i < sizeof(locales) / sizeof(locales[0]); ++i)
        if (setlocale(LC_NUMERIC, locales[i]) != NULL &&
            strcmp(localeconv()->decimal_point, ",") == 0) break;
    if (i == sizeof(locales) / sizeof(locales[0])) {
        setlocale(LC_NUMERIC, saved);
        return 1;
    }
    fields = xrtValueArray();
    field = xfix_field(44, "placeholder");
    if (fields != NULL && field != NULL &&
        xrtValueObjectSetNew(field, XRT_STR_LITERAL("value"), xrtValueFloat(1.5)) &&
        xrtValueArrayAppendTake(fields, &field)) {
        message = (char *)xfix_build("FIX.4.4", fields);
        ok = message != NULL && strstr(message, "44=1.5\x01") != NULL &&
            xfix_validate(message);
        if (!ok) fprintf(stderr, "locale FIX frame: %s\n", message ? message : "<null>");
        xrtFree(message);
    }
    xrtValueRelease(field);
    xrtValueRelease(fields);
    setlocale(LC_NUMERIC, saved);
    return ok;
}

/* 扫描每个分配点，防止中途失败被误报为畸形报文或交出半成品。 */
static int test_allocation_paths(xvalue *fields, const char *message)
{
    size_t failure;
    size_t injected = 0u;
    int completed = 0;
    for (failure = 0u; failure < 128u; ++failure) {
        char *built;
        int triggered;
        CHECK(xrtMemDebugFailAfter(failure));
        built = (char *)xfix_build("FIX.4.4", fields);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (!triggered) {
            CHECK(built != NULL && xfix_validate(built));
            xrtFree(built);
            completed = 1;
            break;
        }
        CHECK(built == NULL && xfix_memory_error());
        ++injected;
    }
    CHECK(completed && injected > 1u);
    injected = 0u;
    completed = 0;
    for (failure = 0u; failure < 128u; ++failure) {
        xvalue *parsed;
        int triggered;
        CHECK(xrtMemDebugFailAfter(failure));
        parsed = xfix_parse(message);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (!triggered) {
            CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_OBJECT);
            xrtValueRelease(parsed);
            completed = 1;
            break;
        }
        CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL &&
            xfix_memory_error());
        xrtValueRelease(parsed);
        ++injected;
    }
    CHECK(completed && injected > 1u);
    CHECK(xfix_validate(message) && !xfix_has_error());
    return 1;
}

int main(void)
{
    xvalue *fields = xrtValueArray();
    xvalue *first = xfix_field(35, "D");
    xvalue *second = xfix_field(49, "SENDER");
    xvalue *third = xfix_field(49, "ALIAS");
    char *message;
    char *corrupted;
    char *checksum;
    xvalue *parsed;
    xvalue *parsed_fields;
    xvalue *bulk_fields;
    int i;

    CHECK(fields && first && second && third);
    CHECK(xrtValueArrayAppendTake(fields, &first));
    CHECK(xrtValueArrayAppendTake(fields, &second));
    CHECK(xrtValueArrayAppendTake(fields, &third));
    message = xfix_build("FIX.4.4", fields);
    CHECK(message && xfix_validate(message));
    CHECK(test_allocation_paths(fields, message));
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(!xfix_validate(message));
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(xfix_has_error() && xfix_memory_error());
    CHECK(xfix_validate(message) && !xfix_has_error() && !xfix_memory_error());
    CHECK(xrtMemDebugFailAfter(0u));
    parsed = xfix_field(58, "value");
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(parsed && xrtValueType(parsed) == XVALUE_NULL && xfix_memory_error());
    xrtValueRelease(parsed);
    parsed = xfix_field(58, "recovered");
    CHECK(parsed && xrtValueType(parsed) == XVALUE_OBJECT && !xfix_has_error());
    xrtValueRelease(parsed);
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(xfix_build("FIX.4.4", fields) == NULL);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(xfix_memory_error());
    CHECK(xfix_validate(message) && !xfix_has_error());
    CHECK(xrtMemDebugFailAfter(0u));
    parsed = xfix_parse(message);
    CHECK(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(parsed && xrtValueType(parsed) == XVALUE_NULL && xfix_memory_error());
    xrtValueRelease(parsed);
    CHECK(xfix_validate(message) && !xfix_has_error());
    CHECK(strstr(message, "8=FIX.4.4\x01") == message);
    CHECK(strstr(message, "49=SENDER\x01") != NULL);
    CHECK(strstr(message, "49=ALIAS\x01") != NULL);

    parsed = xfix_parse(message);
    CHECK(parsed && xrtValueType(parsed) == XVALUE_OBJECT);
    parsed_fields = xrtValueObjectGet(parsed, XRT_STR_LITERAL("fields"));
    CHECK(parsed_fields && xrtValueCount(parsed_fields) == 6);
    xrtValueRelease(parsed);

    corrupted = xrtMalloc(strlen(message) + 1);
    CHECK(corrupted);
    strcpy(corrupted, message);
    checksum = strstr(corrupted, "10=");
    CHECK(checksum);
    checksum[3] = checksum[3] == '0' ? '1' : '0';
    CHECK(!xfix_validate(corrupted));
    CHECK(!xfix_has_error());
    CHECK(!xfix_validate("35=D\x01"));
    CHECK(xrtGetError() == NULL);
    {
        size_t size = 16u * 1024u * 1024u + 1u;
        char *oversized = (char *)malloc(size);
        CHECK(oversized != NULL);
        memset(oversized, 'x', size);
        parsed = xfix_parse(oversized);
        CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtValueRelease(parsed);
        free(oversized);
    }
    {
        size_t size = 16u * 1024u * 1024u + 2u;
        char *oversized = (char *)malloc(size);
        CHECK(oversized != NULL);
        memset(oversized, 'x', size - 1u);
        oversized[size - 1u] = '\0';
        CHECK(xfix_build(oversized, fields) == NULL);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        parsed = xfix_field(58, oversized);
        CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtValueRelease(parsed);
        {
            xvalue *large_fields = xrtValueArray();
            xvalue *large_field = xrtValueObject();
            CHECK(large_fields != NULL && large_field != NULL);
            CHECK(xrtValueObjectSetNew(large_field, XRT_STR_LITERAL("tag"),
                xrtValueInt(58)));
            CHECK(xrtValueObjectSetNew(large_field, XRT_STR_LITERAL("value"),
                xrtValueString((xstrview){oversized, size - 1u})));
            CHECK(xrtValueArrayAppendTake(large_fields, &large_field));
            CHECK(xfix_build("FIX.4.4", large_fields) == NULL);
            CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
            xrtValueRelease(large_fields);
        }
        /* 字段文本可恰好达到上限；完整报文还需为帧字段留空间。 */
        oversized[size - 2u] = '\0';
        parsed = xfix_field(58, oversized);
        {
            xvalue *value = parsed != NULL && xrtValueType(parsed) == XVALUE_OBJECT ?
                xrtValueObjectGet(parsed, XRT_STR_LITERAL("value")) : NULL;
            xstrview text;
            CHECK(value != NULL && xrtValueGetString(value, &text) &&
                text.Size == size - 2u && xrtGetError() == NULL);
        }
        xrtValueRelease(parsed);
        oversized[size - 2u] = 'x';
        /* 没有终止符时也只能读取上限内的字节。 */
        oversized[size - 1u] = 'x';
        CHECK(xfix_build(oversized, fields) == NULL);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        parsed = xfix_field(58, oversized);
        CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtValueRelease(parsed);
        free(oversized);
    }
    CHECK(!xfix_validate("8=\x01" "9=0\x01" "10=029\x01"));
    CHECK(xrtGetError() == NULL);
    CHECK(xfix_validate("8=FIX.4.4\x01" "9=11\x01" "35=0\x01" "58=AD\x01" "10=000\x01"));
    xrtSetErrorInfo(XERR_VALUE, "test", 0, "stale error");
    parsed = xfix_parse(message);
    CHECK(parsed && xrtValueType(parsed) == XVALUE_OBJECT && xrtGetError() == NULL);
    xrtValueRelease(parsed);
    xrtSetErrorInfo(XERR_VALUE, "test", 0, "stale error");
    parsed = xfix_field(58, "clean");
    CHECK(parsed && xrtValueType(parsed) == XVALUE_OBJECT && xrtGetError() == NULL);
    xrtValueRelease(parsed);
    xrtSetErrorInfo(XERR_VALUE, "test", 0, "stale error");
    CHECK(xfix_validate(message) && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "test", 0, "stale error");
    {
        char *rebuilt = xfix_build("FIX.4.4", fields);
        CHECK(rebuilt && xrtGetError() == NULL);
        xrtFree(rebuilt);
    }
    CHECK(xrtValueType(xfix_field(10, "000")) == XVALUE_NULL);
    xrtClearError();
    CHECK(xrtValueType(xfix_field(58, "bad\x01value")) == XVALUE_NULL);
    xrtClearError();

    {
        xvalue *unsigned_fields = xrtValueArray();
        xvalue *unsigned_field = xfix_field(38, "0");
        char *unsigned_message;
        CHECK(unsigned_fields && unsigned_field &&
            xrtValueObjectSetNew(unsigned_field, XRT_STR_LITERAL("value"),
                xrtValueUInt(UINT64_MAX)) &&
            xrtValueArrayAppendTake(unsigned_fields, &unsigned_field));
        unsigned_message = xfix_build("FIX.4.4", unsigned_fields);
        CHECK(unsigned_message &&
            strstr(unsigned_message, "38=18446744073709551615\x01") != NULL &&
            xfix_validate(unsigned_message));
        xrtFree(unsigned_message);
        xrtValueRelease(unsigned_fields);
    }

    bulk_fields = xrtValueArray();
    CHECK(bulk_fields != NULL);
    for (i = 0; i < 4096; ++i) {
        xvalue *field = xfix_field(58, "x");
        CHECK(field != NULL && xrtValueType(field) == XVALUE_OBJECT);
        CHECK(xrtValueArrayAppendTake(bulk_fields, &field));
    }
    {
        char *bulk_message = xfix_build("FIX.4.4", bulk_fields);
        CHECK(bulk_message != NULL);
        parsed = xfix_parse(bulk_message);
        CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_OBJECT);
        parsed_fields = xrtValueObjectGet(parsed, XRT_STR_LITERAL("fields"));
        CHECK(parsed_fields != NULL && xrtValueCount(parsed_fields) == 4099u);
        xrtValueRelease(parsed);
        xrtFree(bulk_message);
    }
    {
        xvalue *field = xfix_field(58, "extra");
        CHECK(field != NULL && xrtValueArrayAppendTake(bulk_fields, &field));
        CHECK(xfix_build("FIX.4.4", bulk_fields) == NULL);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    }
    xrtValueRelease(bulk_fields);
    xrtFree(corrupted);
    xrtFree(message);
    xrtValueRelease(fields);
    CHECK(test_numeric_locale());
    return 0;
}
