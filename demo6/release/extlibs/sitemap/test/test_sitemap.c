#define XRT_MODULE_ALL
#include "sitemap_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "sitemap C test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static xvalue *url(const char *location) {
    xvalue *entry = xrtValueObject();
    xstrview key = {"loc", 3};
    xvalue *value = xrtValueString((xstrview){location, strlen(location)});
    if (entry == NULL || value == NULL ||
        !xrtValueObjectSetNew(entry, key, value)) {
        xrtValueRelease(entry);
        return NULL;
    }
    return entry;
}

static int unknown_fields(void) {
    xvalue *entries = xrtValueArray();
    xvalue *entry = url("https://example.test/a");
    char *output;
    CHECK(entries != NULL && entry != NULL);
    CHECK(xrtValueObjectSetNew(entry, (xstrview){"image", 5},
                              xrtValueString((xstrview){"hidden", 6})));
    CHECK(xrtValueArrayAppendNew(entries, entry));
    output = (char *)xsm_build(entries);
    CHECK(output == NULL && xrtGetError() != NULL &&
          xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(entries);

    entries = xrtValueArray();
    entry = url("https://example.test/a.xml");
    CHECK(entries != NULL && entry != NULL);
    CHECK(xrtValueObjectSetNew(entry, (xstrview){"priority", 8},
                              xrtValueString((xstrview){"0.5", 3})));
    CHECK(xrtValueArrayAppendNew(entries, entry));
    output = (char *)xsm_build_index(entries);
    CHECK(output == NULL && xrtGetError() != NULL &&
          xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(entries);
    xrtClearError();
    return 0;
}

static int xml_line_endings(void) {
    xvalue *urls = xrtValueArray();
    xvalue *entry = url("https://example.test/a");
    xvalue *parsed, *back, *field;
    xstrview text;
    char *xml;
    CHECK(urls != NULL && entry != NULL);
    CHECK(xrtValueObjectSetNew(entry, (xstrview){"lastmod", 7},
        xrtValueString((xstrview){"2026-10-12\rZ", 12})));
    CHECK(xrtValueArrayAppendNew(urls, entry));
    xml = (char *)xsm_build(urls);
    CHECK(xml != NULL);
    parsed = xsm_parse(xml);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY);
    back = xrtValueArrayAt(parsed, 0);
    field = xrtValueObjectGet(back, (xstrview){"lastmod", 7});
    CHECK(field != NULL && xrtValueGetString(field, &text) &&
        text.Size == 12 && memcmp(text.Data, "2026-10-12\rZ", 12) == 0);
    xrtValueRelease(parsed);
    xrtFree(xml);
    xrtValueRelease(urls);
    return 0;
}

static int xml_escaping(void) {
    static const char original[] = "&<>\"'\r";
    static const char escaped[] = "<lastmod>&amp;&lt;&gt;&quot;&apos;&#xD;</lastmod>";
    xvalue *urls = xrtValueArray();
    xvalue *entry = url("https://example.test/a");
    xvalue *parsed, *field;
    xstrview text;
    char *xml;
    CHECK(urls != NULL && entry != NULL);
    CHECK(xrtValueObjectSetNew(entry, (xstrview){"lastmod", 7},
        xrtValueString((xstrview){original, sizeof(original) - 1u})));
    CHECK(xrtValueArrayAppendNew(urls, entry));
    xml = (char *)xsm_build(urls);
    CHECK(xml != NULL && strstr(xml, escaped) != NULL);
    parsed = xsm_parse(xml);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY);
    field = xrtValueObjectGet(xrtValueArrayAt(parsed, 0),
        (xstrview){"lastmod", 7});
    CHECK(field != NULL && xrtValueGetString(field, &text) &&
        text.Size == sizeof(original) - 1u &&
        memcmp(text.Data, original, text.Size) == 0);
    xrtValueRelease(parsed);
    xrtFree(xml);
    xrtValueRelease(urls);
    return 0;
}

static int invalid_xml(const char *text) {
    xvalue *result = xsm_parse(text);
    int invalid = result != NULL && xrtValueType(result) == XVALUE_NULL;
    xrtValueRelease(result);
    xrtClearError();
    return invalid;
}

static int reference_rule_match(const char *rule, const char *path,
                                int anchored) {
    if (*rule == '\0') return !anchored || *path == '\0';
    if (*rule == '*') {
        for (;;) {
            if (reference_rule_match(rule + 1, path, anchored)) return 1;
            if (*path == '\0') return 0;
            ++path;
        }
    }
    return *path != '\0' && *rule == *path &&
           reference_rule_match(rule + 1, path + 1, anchored);
}

static int robots_match_matrix(void) {
    static const char rule_chars[] = "ab*";
    static const char path_chars[] = "ab";
    unsigned rule_length, path_length, rule_code, path_code, anchored;
    unsigned rule_count = 1;

    // 独立回溯模型检查多段通配符和末尾锚点的组合。
    for (rule_length = 0; rule_length <= 3; ++rule_length) {
        for (rule_code = 0; rule_code < rule_count; ++rule_code) {
            char bare[8] = "/", rule[9], document[64];
            unsigned code = rule_code, i;
            for (i = 0; i < rule_length; ++i) {
                bare[i + 1] = rule_chars[code % 3u];
                code /= 3u;
            }
            bare[rule_length + 1] = '\0';
            for (anchored = 0; anchored <= 1; ++anchored) {
                xvalue *robots;
                snprintf(rule, sizeof(rule), "%s%s", bare, anchored ? "$" : "");
                snprintf(document, sizeof(document),
                         "User-agent: *\nDisallow: %s\n", rule);
                robots = xsm_robots_parse(document);
                CHECK(robots != NULL && xrtValueType(robots) == XVALUE_OBJECT);
                {
                    unsigned path_count = 1;
                    for (path_length = 0; path_length <= 3; ++path_length) {
                        for (path_code = 0; path_code < path_count; ++path_code) {
                            char path[8] = "/";
                            unsigned path_digits = path_code;
                            int actual, expected;
                            for (i = 0; i < path_length; ++i) {
                                path[i + 1] = path_chars[path_digits % 2u];
                                path_digits /= 2u;
                            }
                            path[path_length + 1] = '\0';
                            actual = xsm_robots_allows(robots, "Bot", path);
                            expected = !reference_rule_match(bare, path, anchored);
                            if (actual != expected) {
                                fprintf(stderr, "robots mismatch: rule=%s path=%s got=%d want=%d\n",
                                        rule, path, actual, expected);
                                xrtValueRelease(robots);
                                return 1;
                            }
                        }
                        path_count *= 2u;
                    }
                }
                xrtValueRelease(robots);
            }
        }
        rule_count *= 3u;
    }
    return 0;
}

static int allocation_errors(void) {
    static const char xml[] = "<urlset><url><loc>https://example.test/a</loc></url></urlset>";
    xvalue *urls = xrtValueArray();
    xvalue *entry = url("https://example.test/a");
    unsigned failure, buildFailures = 0, parseFailures = 0, robotsFailures = 0;
    CHECK(urls != NULL && entry != NULL && xrtValueArrayAppendNew(urls, entry));

    for (failure = 0; failure < 48; ++failure) {
        void *output;
        int triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        output = xsm_build(urls);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(output == NULL && xrtGetError() != NULL &&
                  xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++buildFailures;
        } else CHECK(output != NULL && xrtGetError() == NULL);
        xrtFree(output);
    }
    for (failure = 0; failure < 48; ++failure) {
        xvalue *parsed;
        int triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        parsed = xsm_parse(xml);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            if ((parsed != NULL && xrtValueType(parsed) != XVALUE_NULL) ||
                xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                fprintf(stderr, "sitemap parse allocation point %u: type %d, error %d\n",
                    failure, parsed == NULL ? -1 : (int)xrtValueType(parsed),
                    xrtGetError() == NULL ? -1 : (int)xrtErrorKind(xrtGetError()));
            CHECK((parsed == NULL || xrtValueType(parsed) == XVALUE_NULL) &&
                  xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++parseFailures;
        } else CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY &&
                     xrtGetError() == NULL);
        xrtValueRelease(parsed);
    }
    for (failure = 0; failure < 48; ++failure) {
        xvalue *robots;
        int triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        robots = xsm_robots_parse("User-agent: Bot\nDisallow: /private\n"
                                  "Allow: /private/public\nSitemap: https://example.test/map.xml\n");
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK((robots == NULL || xrtValueType(robots) == XVALUE_NULL) &&
                  xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++robotsFailures;
        } else {
            CHECK(robots != NULL && xrtValueType(robots) == XVALUE_OBJECT &&
                  xrtGetError() == NULL);
            CHECK(xsm_robots_allows(robots, "Bot", "/private/a") == 0);
        }
        xrtValueRelease(robots);
    }
    xrtClearError();
    xrtValueRelease(urls);
    CHECK(buildFailures >= 2 && parseFailures >= 2 && robotsFailures >= 2);
    return 0;
}

static int loc_limit(void) {
    const char *prefix = "https://example.test/";
    size_t prefixSize = strlen(prefix);
    char *location = xrtMalloc(2049u);
    char *document = xrtMalloc(2300u);
    xvalue *urls = xrtValueArray();
    xvalue *entry, *parsed;
    char *output;
    CHECK(location != NULL && document != NULL && urls != NULL);
    memcpy(location, prefix, prefixSize);
    memset(location + prefixSize, 'a', 2048u - prefixSize);
    location[2047] = 0;
    entry = url(location);
    CHECK(entry != NULL && xrtValueArrayAppendNew(urls, entry));

    output = xsm_build(urls);
    CHECK(output != NULL);
    parsed = xsm_parse(output);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY);
    xrtValueRelease(parsed);
    xrtFree(output);
    output = xsm_build_index(urls);
    CHECK(output != NULL);
    xrtFree(output);

    location[2047] = 'a';
    location[2048] = 0;
    CHECK(xrtValueObjectSetNew(entry, (xstrview){"loc", 3},
        xrtValueString((xstrview){location, 2048u})));
    CHECK(xsm_build(urls) == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xsm_build_index(urls) == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(snprintf(document, 2300u,
        "<urlset><url><loc>%s</loc></url></urlset>", location) > 0);
    parsed = xsm_parse(document);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(parsed);
    CHECK(snprintf(document, 2300u,
        "<sitemapindex><sitemap><loc>%s</loc></sitemap></sitemapindex>",
        location) > 0);
    parsed = xsm_parse_index(document);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(parsed);

    location[2046] = '\xc3';
    location[2047] = '\xa9';
    CHECK(xrtValueObjectSetNew(entry, (xstrview){"loc", 3},
        xrtValueString((xstrview){location, 2048u})));
    output = xsm_build(urls);
    CHECK(output != NULL);
    xrtFree(output);

    xrtValueRelease(urls);
    xrtFree(document);
    xrtFree(location);
    return 0;
}

static int protocol_limits(void) {
    static const char *close_tags[] = {"</urlset>\n", "</sitemapindex>\n"};
    static const char *extra_entries[] = {
        "  <url><loc>https://example.test/extra</loc></url>\n",
        "  <sitemap><loc>https://example.test/extra</loc></sitemap>\n"
    };
    const size_t max_bytes = 50u * 1024u * 1024u;
    xvalue *urls = xrtValueArray();
    xvalue *entry = url("https://example.test/a");
    xvalue *parsed;
    char *large, *output;
    int index, i;
    CHECK(urls != NULL && entry != NULL);
    for (i = 0; i < 50000; ++i) CHECK(xrtValueArrayAppend(urls, entry));
    xrtValueRelease(entry);

    for (index = 0; index < 2; ++index) {
        size_t size, tail_size, extra_size, prefix_size;
        char *over;
        output = (char *)(index ? xsm_build_index(urls) : xsm_build(urls));
        CHECK(output != NULL);
        parsed = index ? xsm_parse_index(output) : xsm_parse(output);
        CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY &&
              xrtValueCount(parsed) == 50000);
        xrtValueRelease(parsed);

        size = strlen(output);
        tail_size = strlen(close_tags[index]);
        extra_size = strlen(extra_entries[index]);
        CHECK(size >= tail_size &&
              memcmp(output + size - tail_size, close_tags[index], tail_size) == 0);
        prefix_size = size - tail_size;
        over = xrtMalloc(size + extra_size + 1);
        CHECK(over != NULL);
        memcpy(over, output, prefix_size);
        memcpy(over + prefix_size, extra_entries[index], extra_size);
        memcpy(over + prefix_size + extra_size, close_tags[index], tail_size + 1);
        parsed = index ? xsm_parse_index_n(over, size + extra_size) :
                         xsm_parse_n(over, size + extra_size);
        CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL &&
              xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtValueRelease(parsed);
        xrtFree(over);
        xrtFree(output);
    }

    entry = url("https://example.test/a");
    CHECK(entry != NULL && xrtValueArrayAppendNew(urls, entry));
    CHECK(xsm_build(urls) == NULL && xrtGetError() != NULL &&
          xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xsm_build_index(urls) == NULL && xrtGetError() != NULL &&
          xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(urls);

    large = xrtMalloc(max_bytes + 1);
    CHECK(large != NULL);
    memset(large, 'a', max_bytes);
    large[max_bytes] = 0;
    parsed = xsm_parse_n(large, max_bytes + 1);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL &&
          xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(parsed);
    /* 没有 NUL 的上限外缓冲区也不能让便利入口继续无界扫描。 */
    large[max_bytes] = 'a';
    parsed = xsm_parse(large);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL &&
          xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(parsed);
    parsed = xsm_parse_index(large);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL &&
          xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(parsed);
    large[max_bytes] = 0;
    urls = xrtValueArray();
    entry = url("https://example.test/large");
    CHECK(entry != NULL && xrtValueObjectSetNew(entry, (xstrview){"lastmod", 7},
        xrtValueString((xstrview){large, max_bytes})));
    CHECK(urls != NULL && entry != NULL && xrtValueArrayAppendNew(urls, entry));
    CHECK(xsm_build(urls) == NULL && xrtGetError() != NULL &&
          xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(urls);
    xrtFree(large);
    xrtClearError();
    return 0;
}

static int robots_limit(void) {
    const size_t limit = 1024u * 1024u;
    char *text = (char *)xrtMalloc(limit + 2u);
    xvalue *result;
    CHECK(text != NULL);
    text[0] = '#';
    memset(text + 1, 'a', limit);
    text[limit + 1u] = 0;

    result = xsm_robots_parse_n(text, limit);
    CHECK(result != NULL && xrtValueType(result) == XVALUE_OBJECT &&
          xrtGetError() == NULL);
    xrtValueRelease(result);

    result = xsm_robots_parse_n(text, limit + 1u);
    CHECK(result != NULL && xrtValueType(result) == XVALUE_NULL &&
          xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(result);

    result = xsm_robots_parse(text);
    CHECK(result != NULL && xrtValueType(result) == XVALUE_NULL &&
          xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(result);
    xrtFree(text);
    xrtClearError();
    return 0;
}

int main(void) {
    CHECK(unknown_fields() == 0);
    CHECK(xml_line_endings() == 0);
    CHECK(xml_escaping() == 0);
    CHECK(allocation_errors() == 0);
    CHECK(loc_limit() == 0);
    CHECK(protocol_limits() == 0);
    CHECK(robots_limit() == 0);
    xvalue *urls = xrtValueArray();
    xvalue *entry = url("https://example.test/?a=1&b=2");
    xvalue *parsed, *robots;
    char *first, *second;
    int i;
    CHECK(urls != NULL && entry != NULL);
    for (i = 0; i < 1800; ++i) CHECK(xrtValueArrayAppend(urls, entry));
    xrtValueRelease(entry);

    first = (char *)xsm_build(urls);
    CHECK(first != NULL && strlen(first) > 65536 &&
          strstr(first, "?a=1&amp;b=2") != NULL);
    second = (char *)xsm_build_index(urls);
    CHECK(second != NULL && second != first &&
          strstr(first, "<urlset") != NULL && strstr(second, "<sitemapindex") != NULL);
    parsed = xsm_parse(first);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY &&
          xrtValueCount(parsed) == 1800);
    xrtValueRelease(parsed);
    parsed = xsm_parse_index(second);
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY &&
          xrtValueCount(parsed) == 1800);
    xrtValueRelease(parsed);
    xrtFree(first);
    xrtFree(second);
    xrtValueRelease(urls);

    urls = xrtValueArray();
    entry = url("https://example.test/\xef\xbf\xbe");
    CHECK(urls != NULL && entry != NULL && xrtValueArrayAppendNew(urls, entry));
    CHECK(xsm_build(urls) == NULL && xrtGetError() != NULL);
    xrtClearError();
    CHECK(xsm_build_index(urls) == NULL && xrtGetError() != NULL);
    xrtClearError();
    xrtValueRelease(urls);

    CHECK(invalid_xml("<urlset><url><loc>bad</url></urlset>"));
    CHECK(invalid_xml("<!DOCTYPE urlset [<!ENTITY x 'bad'>]><urlset/>"));
    CHECK(invalid_xml("<other><url><loc>bad</loc></url></other>"));
    CHECK(invalid_xml("<urlset><url><loc>first</loc><loc>second</loc></url></urlset>"));
    CHECK(invalid_xml("<urlset><url><loc>ok</loc><lastmod>one</lastmod><lastmod>two</lastmod></url></urlset>"));
    {
        const char embedded[] = "<urlset/>\0<other/>";
        parsed = xsm_parse_n(embedded, sizeof(embedded) - 1);
        CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_NULL);
        xrtValueRelease(parsed);
        xrtClearError();
    }

    robots = xsm_robots_parse("User-agent: Bot\nDisallow: /private\nAllow: /private/public\n"
                              "Disallow: /download/*/secret$\n\n"
                              "User-agent: *\nDisallow: /\n");
    CHECK(robots != NULL && xrtValueType(robots) == XVALUE_OBJECT);
    CHECK(xsm_robots_allows(robots, "Bot", "/private/x") == 0);
    CHECK(xsm_robots_allows(robots, "Bot", "/private/public/x") == 1);
    CHECK(xsm_robots_allows(robots, "Bot", "/download/a/secret") == 0);
    CHECK(xsm_robots_allows(robots, "Bot", "/download/a/secret/next") == 1);
    CHECK(xsm_robots_allows(robots, "Other", "/open") == 0);
    CHECK(xsm_robots_allows(robots, "NotBot", "/private/x") == 0);
    CHECK(xsm_robots_allows_n(robots, "Bot", 3, "/private\0/open", 13) == -1);
    xrtClearError();
    xrtValueRelease(robots);

    robots = xsm_robots_parse("User-agent: Bot\nDisallow: /private\n"
                              "User-agent: *\nAllow: /private\n");
    CHECK(robots != NULL && xrtValueType(robots) == XVALUE_OBJECT);
    CHECK(xsm_robots_allows(robots, "Bot", "/private/x") == 0);
    CHECK(xsm_robots_allows(robots, "bOt", "/private/x") == 0);
    CHECK(xsm_robots_allows(robots, "NotBot", "/private/x") == 1);
    CHECK(xsm_robots_allows(robots, "BotPlus", "/private/x") == 1);
    xrtValueRelease(robots);

    robots = xsm_robots_parse("User-agent: *\nDisallow: /a*b*c$\n"
                              "Allow: /a*b/c$\nDisallow: /equal\nAllow: /equal\n"
                              "Disallow: /a$b\nAllow: /a*b\n");
    CHECK(robots != NULL && xrtValueType(robots) == XVALUE_OBJECT);
    CHECK(xsm_robots_allows(robots, "Bot", "/abc") == 0);
    CHECK(xsm_robots_allows(robots, "Bot", "/axbyc") == 0);
    CHECK(xsm_robots_allows(robots, "Bot", "/a/b/c") == 1);
    CHECK(xsm_robots_allows(robots, "Bot", "/axbyc/next") == 1);
    CHECK(xsm_robots_allows(robots, "Bot", "/equal/next") == 1);
    CHECK(xsm_robots_allows(robots, "Bot", "/a$b") == 0);
    xrtValueRelease(robots);
    CHECK(robots_match_matrix() == 0);

    robots = xsm_robots_parse("User-agent: Bot\n# explanatory comment\n\n"
                              "User-agent: Also\nDisallow: /private\n\n# another comment\n"
                              "Disallow: /hidden\n\nUser-agent: Other\n"
                              "Disallow: /other\n");
    CHECK(robots != NULL && xrtValueType(robots) == XVALUE_OBJECT);
    CHECK(xsm_robots_allows(robots, "Bot", "/private/x") == 0);
    CHECK(xsm_robots_allows(robots, "Bot", "/hidden/x") == 0);
    CHECK(xsm_robots_allows(robots, "Also", "/private/x") == 0);
    CHECK(xsm_robots_allows(robots, "Other", "/private/x") == 1);
    CHECK(xsm_robots_allows(robots, "Other", "/other/x") == 0);
    xrtValueRelease(robots);

    robots = xsm_robots_parse("\xef\xbb\xbfUser-agent: *\nDisallow: /private\n");
    CHECK(robots != NULL && xrtValueType(robots) == XVALUE_OBJECT);
    CHECK(xsm_robots_allows(robots, "Bot", "/private/x") == 0);
    xrtValueRelease(robots);

    xrtSetErrorInfo(XERR_MEMORY, "sitemap.test", 0, "previous error");
    robots = xsm_robots_parse("User-agent: *\nDisallow: /blocked\n");
    CHECK(robots != NULL && xrtValueType(robots) == XVALUE_OBJECT &&
          xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_MEMORY, "sitemap.test", 0, "previous error");
    CHECK(xsm_robots_allows(robots, "AnyBot", "/blocked") == 0 &&
          xrtGetError() == NULL);
    CHECK(xsm_robots_allows_n(robots, "AnyBot", 6, "/\xff", 2) == -1 &&
          xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xsm_robots_allows_n(robots, "\xff", 1, "/safe", 5) == -1 &&
          xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xsm_robots_allows(robots, "AnyBot", "/safe") == 1 &&
          xrtGetError() == NULL);
    xrtValueRelease(robots);
    robots = xsm_robots_parse("User-agent: *\nDisallow: /\xff\n");
    CHECK(robots != NULL && xrtValueType(robots) == XVALUE_NULL &&
          xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtValueRelease(robots);
    robots = xsm_robots_parse("User-agent: *\nAllow: /safe\n");
    CHECK(robots != NULL && xrtValueType(robots) == XVALUE_OBJECT &&
          xrtGetError() == NULL);
    xrtValueRelease(robots);

    puts("sitemap C boundaries: PASS");
    return 0;
}
