#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FILE_WHOLE
#include "ical_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "ical C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static xvalue *field(xvalue *object, const char *key) {
    return xrtValueObjectGet(object, (xstrview){key, strlen(key)});
}

static int equals(xvalue *value, const char *expected) {
    xstrview text;
    return value != NULL && xrtValueGetString(value, &text) &&
        text.Size == strlen(expected) &&
        memcmp(text.Data, expected, text.Size) == 0;
}

static int put(xvalue *object, const char *key, const char *text) {
    return xrtValueObjectSetNew(object, (xstrview){key, strlen(key)},
        xrtValueString((xstrview){text, strlen(text)}));
}

static int test_parse(void) {
    const char source[] = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//test//EN\r\n"
        "BEGIN:VEVENT\r\nUID:one@demo\r\nDTSTAMP:20260801T000000Z\r\n"
        "DTSTART;TZID=Asia/Shanghai:20260810T090000\r\n"
        "SUMMARY:真实日程\r\nDESCRIPTION:第一行\\n第二行\\;逗号\\,\r\n"
        "CATEGORIES:开发\r\nCATEGORIES:会议\r\n"
        "RRULE:FREQ=WEEKLY;BYDAY=MO,TU\r\n"
        "BEGIN:VALARM\r\nSUMMARY:警报标题\r\nEND:VALARM\r\nEND:VEVENT\r\n"
        "BEGIN:VTODO\r\nUID:todo@demo\r\nDTSTAMP:20260801T000000Z\r\n"
        "DUE;VALUE=DATE:20260820\r\nSUMMARY:待办\r\nEND:VTODO\r\n"
        "END:VCALENDAR\r\n";
    xvalue *calendar = xi_parse(source, sizeof(source) - 1);
    xvalue *events, *todos, *event, *todo;
    char *rebuilt;
    CHECK(calendar != NULL && equals(field(calendar, "prodid"), "-//test//EN"));
    events = field(calendar, "events");
    todos = field(calendar, "todos");
    CHECK(xrtValueCount(events) == 1 && xrtValueCount(todos) == 1);
    event = xrtValueArrayAt(events, 0);
    todo = xrtValueArrayAt(todos, 0);
    CHECK(equals(field(event, "summary"), "真实日程"));
    CHECK(equals(field(event, "dtstartTzid"), "Asia/Shanghai"));
    CHECK(equals(field(event, "rrule"), "FREQ=WEEKLY;BYDAY=MO,TU"));
    CHECK(equals(field(event, "description"), "第一行\n第二行;逗号,"));
    CHECK(xrtValueCount(field(event, "categories")) == 2);
    CHECK(equals(field(todo, "dueValue"), "DATE"));
    rebuilt = xi_build(calendar);
    CHECK(rebuilt != NULL);
    CHECK(strstr(rebuilt, "RRULE:FREQ=WEEKLY;BYDAY=MO,TU\r\n") != NULL);
    CHECK(strstr(rebuilt, "DTSTART;TZID=Asia/Shanghai:20260810T090000\r\n") != NULL);
    CHECK(strstr(rebuilt, "DUE;VALUE=DATE:20260820\r\n") != NULL);
    CHECK(strstr(rebuilt, "CATEGORIES:开发\r\nCATEGORIES:会议\r\n") != NULL);
    CHECK(strstr(rebuilt, "SUMMARY:警报标题") == NULL);
    xrtFree(rebuilt);
    xrtValueRelease(calendar);
    return 0;
}

static int test_text_identifiers(void) {
    const char source[] = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n"
        "PRODID:Widget\\, Inc\\; ical\\nEdition\r\n"
        "BEGIN:VEVENT\r\nUID:meet\\,1\\;v2@example\r\n"
        "DTSTAMP:20260801T000000Z\r\nDTSTART:20260810T090000Z\r\n"
        "END:VEVENT\r\nEND:VCALENDAR\r\n";
    xvalue *calendar = xi_parse(source, sizeof(source) - 1);
    xvalue *event;
    char *built;
    CHECK(calendar != NULL);
    event = xrtValueArrayAt(field(calendar, "events"), 0);
    CHECK(equals(field(calendar, "prodid"), "Widget, Inc; ical\nEdition"));
    CHECK(equals(field(event, "uid"), "meet,1;v2@example"));
    built = xi_build(calendar);
    CHECK(built != NULL);
    CHECK(strstr(built, "PRODID:Widget\\, Inc\\; ical\\nEdition\r\n") != NULL);
    CHECK(strstr(built, "UID:meet\\,1\\;v2@example\r\n") != NULL);
    xrtFree(built);
    xrtValueRelease(calendar);
    return 0;
}

static int test_mutated_text(void) {
    char sample[] = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:test\r\n"
        "BEGIN:VEVENT\r\nUID:one@demo\r\nDTSTAMP:20260801T000000Z\r\n"
        "DTSTART:20260810T090000Z\r\nSUMMARY:hello\\nworld\r\n"
        "END:VEVENT\r\nEND:VCALENDAR\r\n";
    const unsigned char replacements[] = {'A', ';', ':', '\\', '\n', '\r', 0, 0xff};
    char *summary = strstr(sample, "SUMMARY:") + strlen("SUMMARY:");
    size_t i, j;
    for (i = 0; i < strlen("hello\\nworld"); ++i) {
        char original = summary[i];
        for (j = 0; j < sizeof(replacements); ++j) {
            xvalue *calendar, *back;
            char *built, *rebuilt;
            summary[i] = (char)replacements[j];
            calendar = xi_parse(sample, sizeof(sample) - 1);
            if (calendar == NULL) {
                CHECK(xrtGetError() != NULL);
                continue;
            }
            CHECK(xrtGetError() == NULL);
            built = xi_build(calendar);
            CHECK(built != NULL);
            back = xi_parse(built, strlen(built));
            CHECK(back != NULL);
            rebuilt = xi_build(back);
            CHECK(rebuilt != NULL && strcmp(built, rebuilt) == 0);
            xrtFree(rebuilt);
            xrtValueRelease(back);
            xrtFree(built);
            xrtValueRelease(calendar);
        }
        summary[i] = original;
    }
    return 0;
}

static int test_build_long(const char *output_path) {
    xvalue *calendar = xrtValueObject();
    xvalue *events = xrtValueArray();
    xvalue *event = xrtValueObject();
    xvalue *back;
    char note[9001];
    char product[301];
    char *output, *second;
    size_t i, column = 0;
    CHECK(calendar != NULL && events != NULL && event != NULL);
    for (i = 0; i < 3000; ++i) memcpy(note + i * 3, "长", 3);
    note[9000] = 0;
    memset(product, 'P', 300);
    product[300] = 0;
    CHECK(put(calendar, "prodid", product));
    CHECK(put(event, "uid", "long@demo") &&
        put(event, "dtstamp", "20260801T000000Z") &&
        put(event, "dtstart", "20260810T090000Z") &&
        put(event, "summary", "长日程") && put(event, "description", note) &&
        xrtValueArrayAppendNew(events, event) &&
        xrtValueObjectSetNew(calendar, (xstrview){"events", 6}, events));
    output = xi_build(calendar);
    CHECK(output != NULL && strlen(output) > 9000);
    CHECK(xrtUtf8Valid((xstrview){output, strlen(output)}, NULL));
    for (i = 0; output[i] != 0; ++i) {
        if (output[i] == '\r') {
            CHECK(output[i + 1] == '\n' && column <= 75);
            column = 0;
            ++i;
        } else ++column;
    }
    back = xi_parse(output, strlen(output));
    CHECK(back != NULL && equals(field(xrtValueArrayAt(field(back, "events"), 0),
        "description"), note));
    CHECK(equals(field(back, "prodid"), product));
    second = xi_build(calendar);
    CHECK(second != NULL && second != output && strcmp(second, output) == 0);
    if (output_path != NULL) CHECK(xrtFileWriteAll(output_path,
        (xbytesview){(const unsigned char *)output, strlen(output)}));
    xrtFree(second);
    xrtFree(output);
    xrtValueRelease(back);
    xrtValueRelease(calendar);
    return 0;
}

static int test_invalid_and_failure(void) {
    const char incomplete[] = "BEGIN:VCALENDAR\nVERSION:2.0\nEND:VCALENDAR\n";
    const char mismatched[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:test\n"
        "BEGIN:VEVENT\nEND:VTODO\nEND:VCALENDAR\n";
    const char extra[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:test\n"
        "END:VCALENDAR\nBEGIN:VCALENDAR\nVERSION:2.0\nPRODID:test\nEND:VCALENDAR\n";
    const char nested_calendar[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:test\n"
        "BEGIN:VCALENDAR\nEND:VCALENDAR\nEND:VCALENDAR\n";
    const char parameterized_begin[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:test\n"
        "BEGIN;X=1:VALARM\nEND:VCALENDAR\n";
    const char parameterized_end[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:test\n"
        "END;X=1:VCALENDAR\nEND:VCALENDAR\n";
    const char bad_version_parameter[] = "BEGIN:VCALENDAR\nVERSION;=x:2.0\n"
        "PRODID:test\nEND:VCALENDAR\n";
    const char bad_prodid_parameter[] = "BEGIN:VCALENDAR\nVERSION:2.0\n"
        "PRODID;X:test\nEND:VCALENDAR\n";
    const char empty_version_parameter[] = "BEGIN:VCALENDAR\nVERSION;:2.0\n"
        "PRODID:test\nEND:VCALENDAR\n";
    const char trailing_version_parameter[] = "BEGIN:VCALENDAR\nVERSION;X=1;:2.0\n"
        "PRODID:test\nEND:VCALENDAR\n";
    const char bad_summary_parameter[] = "BEGIN:VCALENDAR\nVERSION:2.0\n"
        "PRODID:test\nBEGIN:VEVENT\nSUMMARY;X:hello\nEND:VEVENT\nEND:VCALENDAR\n";
    const char bad_summary_name[] = "BEGIN:VCALENDAR\nVERSION:2.0\n"
        "PRODID:test\nBEGIN:VEVENT\nSUMMARY;X_BAD=1:hello\nEND:VEVENT\nEND:VCALENDAR\n";
    const char valid_parameters[] = "BEGIN:VCALENDAR\nVERSION;X=\"a;b:c\":2.0\n"
        "PRODID;X=\"b:c;d\":test\nEND:VCALENDAR\n";
    const char bad_parameter_name[] = "BEGIN:VCALENDAR\nVERSION;X_BAD=1:2.0\n"
        "PRODID:test\nEND:VCALENDAR\n";
    const char bad_parameter_tail[] = "BEGIN:VCALENDAR\nVERSION:2.0\n"
        "PRODID;X=\"ok\"tail:test\nEND:VCALENDAR\n";
    const char bad_parameter_quote[] = "BEGIN:VCALENDAR\nVERSION:2.0\n"
        "PRODID;X=a\"b\"c:test\nEND:VCALENDAR\n";
    const char valid_parameter_list[] = "BEGIN:VCALENDAR\nVERSION:2.0\n"
        "PRODID;X=\"a;b:c\",\"d,e\":test\nEND:VCALENDAR\n";
    const char nul[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:test\0junk\nEND:VCALENDAR\n";
    const char invalid_utf8[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:\xff\nEND:VCALENDAR\n";
    const char control[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:bad\x1b\nEND:VCALENDAR\n";
    const char valid[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:test\nEND:VCALENDAR\n";
    const char extension[] = "BEGIN:VCALENDAR\nVERSION:2.0\nPRODID:test\n"
        "X-CUSTOM;X-KEY=1:opaque\nEND:VCALENDAR\n";
    const char later_version[] = "BEGIN:VCALENDAR\nPRODID:test\n"
        "BEGIN:VEVENT\nUID:one@demo\nDTSTAMP:20260801T000000Z\n"
        "DTSTART:20260810T090000Z\nEND:VEVENT\nVERSION:2.0\nEND:VCALENDAR\n";
    const char truncation[] = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:test\r\n"
        "BEGIN:VEVENT\r\nUID:one@demo\r\nDTSTAMP:20260801T000000Z\r\n"
        "SUMMARY:中文日程\r\nBEGIN:VALARM\r\nACTION:DISPLAY\r\n"
        "END:VALARM\r\nEND:VEVENT\r\nEND:VCALENDAR";
    xvalue *calendar, *events, *event;
    size_t prefix;
    CHECK(xi_parse(incomplete, sizeof(incomplete) - 1) == NULL);
    CHECK(xi_parse(mismatched, sizeof(mismatched) - 1) == NULL);
    CHECK(xi_parse(extra, sizeof(extra) - 1) == NULL);
    CHECK(xi_parse(nested_calendar, sizeof(nested_calendar) - 1) == NULL);
    CHECK(xi_parse(parameterized_begin, sizeof(parameterized_begin) - 1) == NULL);
    CHECK(xi_parse(parameterized_end, sizeof(parameterized_end) - 1) == NULL);
    CHECK(xi_parse(bad_version_parameter, sizeof(bad_version_parameter) - 1) == NULL);
    CHECK(xi_parse(bad_prodid_parameter, sizeof(bad_prodid_parameter) - 1) == NULL);
    CHECK(xi_parse(empty_version_parameter, sizeof(empty_version_parameter) - 1) == NULL);
    CHECK(xi_parse(trailing_version_parameter, sizeof(trailing_version_parameter) - 1) == NULL);
    CHECK(xi_parse(bad_summary_parameter, sizeof(bad_summary_parameter) - 1) == NULL);
    CHECK(xi_parse(bad_summary_name, sizeof(bad_summary_name) - 1) == NULL);
    CHECK(xi_parse(bad_parameter_name, sizeof(bad_parameter_name) - 1) == NULL);
    CHECK(xi_parse(bad_parameter_tail, sizeof(bad_parameter_tail) - 1) == NULL);
    CHECK(xi_parse(bad_parameter_quote, sizeof(bad_parameter_quote) - 1) == NULL);
    CHECK(xi_parse(nul, sizeof(nul) - 1) == NULL);
    CHECK(xi_parse(invalid_utf8, sizeof(invalid_utf8) - 1) == NULL);
    CHECK(xi_parse(control, sizeof(control) - 1) == NULL);
    CHECK(xi_parse(valid, 8u * 1024u * 1024u + 1u) == NULL);
    for (prefix = 1; prefix < sizeof(truncation) - 1; ++prefix) {
        CHECK(xi_parse(truncation, prefix) == NULL && xrtGetError() != NULL);
    }
    calendar = xi_parse(truncation, sizeof(truncation) - 1);
    CHECK(calendar != NULL && xrtValueCount(field(calendar, "events")) == 1);
    /* 读取可保留不完整事件，生成必须有 DTSTART。 */
    CHECK(xi_build(calendar) == NULL && xrtGetError() != NULL);
    xrtValueRelease(calendar);
    calendar = xi_parse(later_version, sizeof(later_version) - 1);
    CHECK(calendar != NULL && xrtValueCount(field(calendar, "events")) == 1);
    xrtValueRelease(calendar);
    calendar = xi_parse(extension, sizeof(extension) - 1);
    CHECK(calendar != NULL && xrtValueCount(field(calendar, "events")) == 0);
    xrtValueRelease(calendar);
    calendar = xi_parse(valid_parameters, sizeof(valid_parameters) - 1);
    CHECK(calendar != NULL && equals(field(calendar, "prodid"), "test"));
    xrtValueRelease(calendar);
    calendar = xi_parse(valid_parameter_list, sizeof(valid_parameter_list) - 1);
    CHECK(calendar != NULL && equals(field(calendar, "prodid"), "test"));
    xrtValueRelease(calendar);
    calendar = xrtValueObject();
    events = xrtValueArray();
    event = xrtValueObject();
    CHECK(calendar != NULL && events != NULL && event != NULL);
    CHECK(put(event, "uid", "one@demo") &&
        put(event, "dtstart", "20260810T090000Z") &&
        xrtValueArrayAppendNew(events, event) &&
        xrtValueObjectSetNew(calendar, (xstrview){"events", 6}, events));
    CHECK(xi_build(calendar) == NULL); /* 缺 DTSTAMP */
    xrtClearError();
    xrtValueRelease(calendar);
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(xi_parse(valid, sizeof(valid) - 1) == NULL);
    xrtMemDebugFailClear();
    xrtClearError();
    calendar = xi_parse(valid, sizeof(valid) - 1);
    CHECK(calendar != NULL);
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(xi_build(calendar) == NULL);
    xrtMemDebugFailClear();
    xrtClearError();
    xrtValueRelease(calendar);
    calendar = xrtValueObject();
    CHECK(calendar != NULL && put(calendar, "event", "misspelled events"));
    CHECK(xi_build(calendar) == NULL); /* 顶层拼写错误不能生成空日历。 */
    xrtValueRelease(calendar);
    return 0;
}

static int test_companion_key_case(void) {
    const char *wrong[] = {"dtstartTZID", "dtstartVALUE"};
    const char *correct[] = {"dtstartTzid", "dtstartValue"};
    const char *value[] = {"Asia/Shanghai", "DATE-TIME"};
    const char *wire_value[] = {"DTSTART;TZID=Asia/Shanghai:",
        "DTSTART;VALUE=DATE-TIME:"};
    size_t i;
    for (i = 0; i < 2; ++i) {
        xvalue *calendar = xrtValueObject();
        xvalue *events = xrtValueArray();
        xvalue *event = xrtValueObject();
        char *wire;
        CHECK(calendar != NULL && events != NULL && event != NULL);
        CHECK(put(event, "uid", "one@demo") &&
            put(event, "dtstamp", "20260801T000000Z") &&
            put(event, "dtstart", "20260810T090000Z") &&
            put(event, wrong[i], value[i]) &&
            xrtValueArrayAppendNew(events, event) &&
            xrtValueObjectSetNew(calendar, (xstrview){"events", 6}, events));
        wire = xi_build(calendar);
        CHECK(wire == NULL && xrtGetError() != NULL);
        xrtValueRelease(calendar);

        calendar = xrtValueObject();
        events = xrtValueArray();
        event = xrtValueObject();
        CHECK(calendar != NULL && events != NULL && event != NULL);
        CHECK(put(event, "uid", "one@demo") &&
            put(event, "dtstamp", "20260801T000000Z") &&
            put(event, "dtstart", "20260810T090000Z") &&
            put(event, correct[i], value[i]) &&
            xrtValueArrayAppendNew(events, event) &&
            xrtValueObjectSetNew(calendar, (xstrview){"events", 6}, events));
        wire = xi_build(calendar);
        CHECK(wire != NULL && xrtGetError() == NULL);
        CHECK(strstr(wire, wire_value[i]) != NULL);
        xrtFree(wire);
        xrtValueRelease(calendar);

        calendar = xrtValueObject();
        events = xrtValueArray();
        event = xrtValueObject();
        CHECK(calendar != NULL && events != NULL && event != NULL);
        CHECK(put(event, "uid", "one@demo") &&
            put(event, "dtstamp", "20260801T000000Z") &&
            put(event, "dtstart", "20260810T090000Z") &&
            put(event, correct[i], "") &&
            xrtValueArrayAppendNew(events, event) &&
            xrtValueObjectSetNew(calendar, (xstrview){"events", 6}, events));
        CHECK(xi_build(calendar) == NULL && xrtGetError() != NULL);
        xrtValueRelease(calendar);
    }
    return 0;
}

static int test_build_control(void) {
    xvalue *calendar = xrtValueObject();
    xvalue *events = xrtValueArray();
    xvalue *event = xrtValueObject();
    CHECK(calendar != NULL && events != NULL && event != NULL);
    CHECK(put(event, "uid", "one@demo") &&
        put(event, "dtstamp", "20260801T000000Z") &&
        put(event, "dtstart", "20260810T090000Z") &&
        put(event, "summary", "bad\x1b") &&
        xrtValueArrayAppendNew(events, event) &&
        xrtValueObjectSetNew(calendar, (xstrview){"events", 6}, events));
    CHECK(xi_build(calendar) == NULL);
    CHECK(put(event, "summary", "ok"));
    CHECK(put(calendar, "prodid", "bad\x1b"));
    CHECK(xi_build(calendar) == NULL);
    xrtValueRelease(calendar);
    return 0;
}

static int test_output_limit(void) {
    size_t size = 5u * 1024u * 1024u;
    char *large = xrtMalloc(size + 1u);
    xvalue *calendar = xrtValueObject();
    xvalue *events = xrtValueArray();
    xvalue *event = xrtValueObject();
    char *output;
    CHECK(large != NULL && calendar != NULL && events != NULL && event != NULL);
    memset(large, ',', size);
    large[size] = 0;
    CHECK(put(event, "uid", "limit@demo") &&
        put(event, "dtstamp", "20260801T000000Z") &&
        put(event, "dtstart", "20260810T090000Z") &&
        put(event, "summary", large) &&
        xrtValueArrayAppendNew(events, event) &&
        xrtValueObjectSetNew(calendar, (xstrview){"events", 6}, events));
    xrtFree(large);
    output = xi_build(calendar);
    CHECK(output == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(calendar);
    return 0;
}

static int test_input_limit(void) {
    const char valid[] = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:test\r\nEND:VCALENDAR\r\n";
    size_t size = 8u * 1024u * 1024u + 1u;
    char *large = xrtMalloc(size);
    xvalue *calendar;
    CHECK(large != NULL);
    memset(large, 'x', size);
    CHECK(xi_parse(large, size) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtFree(large);
    calendar = xi_parse(valid, sizeof(valid) - 1u);
    CHECK(calendar != NULL && xrtGetError() == NULL);
    xrtValueRelease(calendar);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(test_parse() == 0);
    CHECK(test_text_identifiers() == 0);
    CHECK(test_mutated_text() == 0);
    CHECK(test_build_long(argc > 1 ? argv[1] : NULL) == 0);
    CHECK(test_invalid_and_failure() == 0);
    CHECK(test_companion_key_case() == 0);
    CHECK(test_build_control() == 0);
    CHECK(test_output_limit() == 0);
    CHECK(test_input_limit() == 0);
    puts("ical C test: PASS");
    return 0;
}
