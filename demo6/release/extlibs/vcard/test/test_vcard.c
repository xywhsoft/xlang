#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_VALUE_GRAPH
#include "vcard_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "vcard C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static xvalue *field(xvalue *card, const char *name) {
    return xrtValueObjectGet(card, (xstrview){name, strlen(name)});
}

static int equals(xvalue *value, const char *expected) {
    xstrview text;
    return value != NULL && xrtValueGetString(value, &text) &&
        text.Size == strlen(expected) &&
        memcmp(text.Data, expected, text.Size) == 0;
}

static int test_roundtrip(void) {
    const char source[] = "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:张三\r\n"
        "N:张;三;;;\r\nTEL;TYPE=CELL:123\r\nTEL:456\r\n"
        "NOTE:line1\\nline2\\;comma\\,\r\nEND:VCARD\r\n";
    xvalue *card = xv_parse(source, sizeof(source) - 1);
    xvalue *tel;
    char *output;
    xvalue *back;
    CHECK(card != NULL && equals(field(card, "fn"), "张三"));
    CHECK(equals(field(card, "n"), "张;三;;;"));
    CHECK(equals(field(card, "note"), "line1\nline2;comma,"));
    tel = field(card, "tel");
    CHECK(tel != NULL && xrtValueType(tel) == XVALUE_ARRAY && xrtValueCount(tel) == 2);
    CHECK(equals(xrtValueArrayAt(tel, 0), "123") &&
        equals(xrtValueArrayAt(tel, 1), "456"));
    output = xv_build(card);
    CHECK(output != NULL && strstr(output, "TEL:123\r\nTEL:456\r\n") != NULL);
    back = xv_parse(output, strlen(output));
    CHECK(back != NULL && equals(field(back, "fn"), "张三"));
    CHECK(xrtValueCount(field(back, "tel")) == 2);
    xrtValueRelease(back);
    xrtFree(output);
    xrtValueRelease(card);
    return 0;
}

static int test_long_unicode_and_ownership(const char *output_path) {
    char note[9001];
    xvalue *card = xrtValueObject();
    xvalue *back;
    char *output, *other;
    size_t i, column = 0;
    CHECK(card != NULL);
    for (i = 0; i < 3000; ++i) memcpy(note + i * 3, "长", 3);
    note[9000] = 0;
    CHECK(xrtValueObjectSetNew(card, (xstrview){"fn", 2},
        xrtValueString((xstrview){"长文本", strlen("长文本")})));
    CHECK(xrtValueObjectSetNew(card, (xstrview){"note", 4},
        xrtValueString((xstrview){note, 9000})));
    output = xv_build(card);
    CHECK(output != NULL && strlen(output) > 9000);
    CHECK(xrtUtf8Valid((xstrview){output, strlen(output)}, NULL));
    for (i = 0; output[i] != 0; ++i) {
        if (output[i] == '\r') {
            CHECK(output[i + 1] == '\n' && column <= 75);
            column = 0;
            ++i;
        } else ++column;
    }
    back = xv_parse(output, strlen(output));
    CHECK(back != NULL && equals(field(back, "note"), note));
    other = xv_build(card);
    CHECK(other != NULL && other != output && strcmp(other, output) == 0);
    if (output_path != NULL) CHECK(xrtFileWriteAll(output_path,
        (xbytesview){(const unsigned char *)output, strlen(output)}));
    xrtFree(other);
    xrtFree(output);
    xrtValueRelease(back);
    xrtValueRelease(card);
    return 0;
}

static int test_invalid(void) {
    const char missing[] = "BEGIN:VCARD\r\nEND:VCARD\r\n";
    const char duplicate[] = "BEGIN:VCARD\nVERSION:3.0\nFN:A\nFN:B\nEND:VCARD\n";
    const char trailing[] = "BEGIN:VCARD\nVERSION:3.0\nFN:A\nEND:VCARD\nFN:B\n";
    const char nested[] = "BEGIN:VCARD\nVERSION:3.0\nFN:A\nBEGIN:VCARD\nEND:VCARD\n";
    const char stray_end[] = "BEGIN:VCARD\nVERSION:3.0\nFN:A\nEND:VFOO\nEND:VCARD\n";
    const char late_version_four[] = "BEGIN:VCARD\nFN:A\nVERSION:4.0\nEND:VCARD\n";
    const char late_after_unknown[] = "BEGIN:VCARD\nX-TEST:foo\nVERSION:4.0\nFN:A\nEND:VCARD\n";
    const char late_after_blank[] = "BEGIN:VCARD\n\nVERSION:4.0\nFN:A\nEND:VCARD\n";
    const char late_version_three[] = "BEGIN:VCARD\nFN:A\nVERSION:3.0\nEND:VCARD\n";
    const char version_parameter_three[] = "BEGIN:VCARD\nVERSION;X=1:3.0\nFN:A\nEND:VCARD\n";
    const char version_parameter_four[] = "BEGIN:VCARD\nVERSION;VALUE=text:4.0\nFN:A\nEND:VCARD\n";
    const char grouped_version_four[] = "BEGIN:VCARD\nitem1.VERSION:4.0\nFN:A\nEND:VCARD\n";
    const char grouped_version_three[] = "BEGIN:VCARD\nitem1.VERSION:3.0\nFN:A\nEND:VCARD\n";
    const char grouped_begin[] = "BEGIN:VCARD\nVERSION:4.0\nFN:A\nitem1.BEGIN:VFOO\nEND:VCARD\n";
    const char grouped_end[] = "BEGIN:VCARD\nVERSION:4.0\nFN:A\nitem1.END:VFOO\nEND:VCARD\n";
    const char grouped_email[] = "BEGIN:VCARD\nVERSION:4.0\nFN:A\nitem1.EMAIL:a@example.com\nEND:VCARD\n";
    const char bad_group_empty[] = "BEGIN:VCARD\nVERSION:4.0\nFN:A\nitem1..EMAIL:a@example.com\nEND:VCARD\n";
    const char bad_group_space[] = "BEGIN:VCARD\nVERSION:4.0\nFN:A\nitem one.EMAIL:a@example.com\nEND:VCARD\n";
    const char bad_unknown_name[] = "BEGIN:VCARD\nVERSION:4.0\nFN:A\nX_BAD:opaque\nEND:VCARD\n";
    const char good_unknown_name[] = "BEGIN:VCARD\nVERSION:4.0\nFN:A\nX-OTHER:opaque\nEND:VCARD\n";
    const char nul[] = "BEGIN:VCARD\nVERSION:3.0\nFN:A\0B\nEND:VCARD\n";
    const char invalid_utf8[] = "BEGIN:VCARD\nVERSION:3.0\nFN:\xff\nEND:VCARD\n";
    xvalue *card;
    CHECK(xv_parse(missing, sizeof(missing) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(duplicate, sizeof(duplicate) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(trailing, sizeof(trailing) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(nested, sizeof(nested) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(stray_end, sizeof(stray_end) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(late_version_four, sizeof(late_version_four) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(late_after_unknown, sizeof(late_after_unknown) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(late_after_blank, sizeof(late_after_blank) - 1) == NULL);
    xrtClearError();
    card = xv_parse(late_version_three, sizeof(late_version_three) - 1);
    CHECK(card != NULL && equals(field(card, "version"), "3.0"));
    xrtValueRelease(card);
    CHECK(xv_parse(version_parameter_three, sizeof(version_parameter_three) - 1) == NULL);
    xrtClearError();
    card = xv_parse(version_parameter_four, sizeof(version_parameter_four) - 1);
    CHECK(card != NULL && equals(field(card, "version"), "4.0"));
    xrtValueRelease(card);
    CHECK(xv_parse(grouped_version_four, sizeof(grouped_version_four) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(grouped_version_three, sizeof(grouped_version_three) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(grouped_begin, sizeof(grouped_begin) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(grouped_end, sizeof(grouped_end) - 1) == NULL);
    xrtClearError();
    card = xv_parse(grouped_email, sizeof(grouped_email) - 1);
    CHECK(card != NULL && equals(field(card, "email"), "a@example.com"));
    xrtValueRelease(card);
    CHECK(xv_parse(bad_group_empty, sizeof(bad_group_empty) - 1) == NULL);
    CHECK(xv_parse(bad_group_space, sizeof(bad_group_space) - 1) == NULL);
    CHECK(xv_parse(bad_unknown_name, sizeof(bad_unknown_name) - 1) == NULL);
    card = xv_parse(good_unknown_name, sizeof(good_unknown_name) - 1);
    CHECK(card != NULL && equals(field(card, "fn"), "A"));
    xrtValueRelease(card);
    CHECK(xv_parse(nul, sizeof(nul) - 1) == NULL);
    xrtClearError();
    CHECK(xv_parse(invalid_utf8, sizeof(invalid_utf8) - 1) == NULL);
    xrtClearError();
    card = xrtValueObject();
    CHECK(card != NULL);
    CHECK(xv_build(card) == NULL);
    xrtClearError();
    CHECK(xrtValueObjectSetNew(card, (xstrview){"fn", 2},
        xrtValueString((xstrview){"A", 1})));
    CHECK(xrtValueObjectSetNew(card, (xstrview){"unknown", 7},
        xrtValueString((xstrview){"B", 1})));
    CHECK(xv_build(card) == NULL);
    xrtClearError();
    xrtValueRelease(card);
    return 0;
}

static int test_control_characters(void) {
    const char bad_known[] = "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:A\x1b\r\nEND:VCARD\r\n";
    const char bad_unknown[] = "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:A\r\nX-RAW:bad\x1b\r\nEND:VCARD\r\n";
    const char bad_del[] = "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:A\x7f\r\nEND:VCARD\r\n";
    const char good_tab[] = "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:A\tB\r\nEND:VCARD\r\n";
    xvalue *card;
    CHECK(xv_parse(bad_known, sizeof(bad_known) - 1) == NULL);
    CHECK(xv_parse(bad_unknown, sizeof(bad_unknown) - 1) == NULL);
    CHECK(xv_parse(bad_del, sizeof(bad_del) - 1) == NULL);
    card = xv_parse(good_tab, sizeof(good_tab) - 1);
    CHECK(card != NULL && equals(field(card, "fn"), "A\tB"));
    xrtValueRelease(card);

    card = xrtValueObject();
    CHECK(card != NULL);
    CHECK(xrtValueObjectSetNew(card, (xstrview){"fn", 2},
        xrtValueString((xstrview){"bad\x1b", 4})));
    CHECK(xv_build(card) == NULL);
    xrtValueRelease(card);

    card = xrtValueObject();
    CHECK(card != NULL);
    CHECK(xrtValueObjectSetNew(card, (xstrview){"fn", 2},
        xrtValueString((xstrview){"A", 1})));
    CHECK(xrtValueObjectSetNew(card, (xstrview){"note", 4},
        xrtValueString((xstrview){"bad\x1b", 4})));
    CHECK(xv_build(card) == NULL);
    xrtValueRelease(card);

    card = xrtValueObject();
    CHECK(card != NULL);
    CHECK(xrtValueObjectSetNew(card, (xstrview){"fn", 2},
        xrtValueString((xstrview){"A", 1})));
    CHECK(xrtValueObjectSetNew(card, (xstrview){"n", 1},
        xrtValueString((xstrview){"bad\x1b", 4})));
    CHECK(xv_build(card) == NULL);
    xrtValueRelease(card);
    return 0;
}

static int test_version_four(const char *output_path) {
    const char source[] = "BEGIN:VCARD\r\nVERSION:4.0\r\n"
        "FN:Alex\r\nTEL;VALUE=uri:tel:+123\r\nEND:VCARD\r\n";
    xvalue *card = xv_parse(source, sizeof(source) - 1);
    char *built;
    CHECK(card != NULL && equals(field(card, "version"), "4.0"));
    CHECK(equals(field(card, "tel"), "tel:+123"));
    built = xv_build(card);
    CHECK(built != NULL && strstr(built, "VERSION:4.0\r\n") != NULL);
    if (output_path != NULL) CHECK(xrtFileWriteAll(output_path,
        (xbytesview){(const unsigned char *)built, strlen(built)}));
    xrtFree(built);
    xrtValueRelease(card);
    return 0;
}

static int test_uri_values(void) {
    const char source[] = "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Alex\r\n"
        "URL:https://e.test/a,b;c=1\r\n"
        "IMPP:xmpp:alex@e.test?x=1,2\r\n"
        "PHOTO:https://e.test/p;a,b.png\r\n"
        "LOGO:https://e.test/l;a,b.png\r\n"
        "GEO:geo:37.386013,-122.082932\r\nEND:VCARD\r\n";
    const char version_three[] = "BEGIN:VCARD\r\nVERSION:3.0\r\n"
        "FN:Alex\r\nURL:https://e.test/a,b;c=1\r\nEND:VCARD\r\n";
    xvalue *card = xv_parse(source, sizeof(source) - 1);
    xvalue *older = xv_parse(version_three, sizeof(version_three) - 1);
    char *built, *built_older;
    CHECK(card != NULL && older != NULL);
    CHECK(equals(field(card, "url"), "https://e.test/a,b;c=1"));
    built = xv_build(card);
    built_older = xv_build(older);
    CHECK(built != NULL && built_older != NULL);
    CHECK(strstr(built, "URL:https://e.test/a,b;c=1\r\n") != NULL);
    CHECK(strstr(built, "IMPP:xmpp:alex@e.test?x=1,2\r\n") != NULL);
    CHECK(strstr(built, "PHOTO:https://e.test/p;a,b.png\r\n") != NULL);
    CHECK(strstr(built, "LOGO:https://e.test/l;a,b.png\r\n") != NULL);
    CHECK(strstr(built, "GEO:geo:37.386013,-122.082932\r\n") != NULL);
    CHECK(strstr(built_older, "URL:https://e.test/a,b;c=1\r\n") != NULL);
    xrtFree(built);
    xrtFree(built_older);
    xrtValueRelease(card);
    xrtValueRelease(older);
    return 0;
}

static int test_limits_and_allocation(void) {
    const char source[] = "BEGIN:VCARD\nVERSION:3.0\nFN:A\nEND:VCARD\n";
    xvalue *card = xrtValueObject();
    char *large;
    char *built;
    CHECK(card != NULL);
    CHECK(xrtValueObjectSetNew(card, (xstrview){"fn", 2},
        xrtValueString((xstrview){"A", 1})));
    large = xrtMalloc(4u * 1024u * 1024u + 1u);
    CHECK(large != NULL);
    memset(large, 'x', 4u * 1024u * 1024u);
    large[4u * 1024u * 1024u] = 0;
    CHECK(xv_parse(large, 4u * 1024u * 1024u + 1u) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xrtValueObjectSetNew(card, (xstrview){"note", 4},
        xrtValueString((xstrview){large, 4u * 1024u * 1024u})));
    CHECK(xv_build(card) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtFree(large);
    xrtClearError();
    xrtValueRelease(card);

    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(xv_parse(source, sizeof(source) - 1) == NULL);
    xrtMemDebugFailClear();
    xrtClearError();
    card = xv_parse(source, sizeof(source) - 1);
    CHECK(card != NULL);
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(xv_build(card) == NULL);
    xrtMemDebugFailClear();
    xrtClearError();
    built = xv_build(card);
    CHECK(built != NULL);
    xrtFree(built);
    xrtValueRelease(card);
    return 0;
}

static int test_allocation_points(void) {
    const char source[] = "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:张三\r\n"
        "TEL:123\r\nTEL:456\r\nNOTE:line1\\nline2\r\nEND:VCARD\r\n";
    xvalue *card = xv_parse(source, sizeof(source) - 1);
    size_t failure;
    size_t parse_failures = 0, build_failures = 0;
    size_t parse_successes = 0, build_successes = 0;
    CHECK(card != NULL);
    for (failure = 0; failure < 64; ++failure) {
        xvalue *parsed;
        char *built;
        bool triggered;

        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        parsed = xv_parse(source, sizeof(source) - 1);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ++parse_failures;
            CHECK(parsed == NULL && xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else {
            ++parse_successes;
            CHECK(parsed != NULL && xrtGetError() == NULL);
        }
        xrtValueRelease(parsed);

        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        built = xv_build(card);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ++build_failures;
            CHECK(built == NULL && xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else {
            ++build_successes;
            CHECK(built != NULL && xrtGetError() == NULL);
        }
        xrtFree(built);
    }
    CHECK(parse_failures > 1 && build_failures > 1 &&
        parse_successes > 0 && build_successes > 0);
    xrtClearError();
    CHECK(test_roundtrip() == 0);
    xrtValueRelease(card);
    return 0;
}

static int test_mutated_roundtrip(void) {
    char sample[] = "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Alex\r\n"
        "N:Smith;Alex;;;\r\nURL:https://e.test/a,b;c=1\r\n"
        "NOTE:hello\\nworld\r\nEND:VCARD\r\n";
    const char replacements[] = {'A', ';', ':', '\\', '\n', '\r', '\0'};
    size_t i, j;
    for (i = 0; i < sizeof(sample) - 1; ++i) {
        char original = sample[i];
        for (j = 0; j < sizeof(replacements); ++j) {
            xvalue *card, *back;
            char *built;
            sample[i] = replacements[j];
            card = xv_parse(sample, sizeof(sample) - 1);
            if (card == NULL) {
                CHECK(xrtGetError() != NULL);
                continue;
            }
            CHECK(xrtGetError() == NULL);
            built = xv_build(card);
            CHECK(built != NULL);
            back = xv_parse(built, strlen(built));
            CHECK(back != NULL);
            CHECK(xrtValueEqual(card, back));
            xrtValueRelease(back);
            xrtFree(built);
            xrtValueRelease(card);
        }
        sample[i] = original;
    }
    return 0;
}

int main(int argc, char **argv) {
    CHECK(test_roundtrip() == 0);
    CHECK(test_long_unicode_and_ownership(argc > 1 ? argv[1] : NULL) == 0);
    CHECK(test_invalid() == 0);
    CHECK(test_control_characters() == 0);
    CHECK(test_version_four(argc > 2 ? argv[2] : NULL) == 0);
    CHECK(test_uri_values() == 0);
    CHECK(test_limits_and_allocation() == 0);
    CHECK(test_allocation_points() == 0);
    CHECK(test_mutated_roundtrip() == 0);
    puts("vcard C test: PASS");
    return 0;
}
