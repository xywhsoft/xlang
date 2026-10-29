#define XRT_MODULE_ALL
#include "rss_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "rss C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static const char *field(xvalue *object, const char *name, size_t *length) {
    xvalue *value = xrtValueObjectGet(object, (xstrview){name, strlen(name)});
    xstrview text;
    if (value == NULL || !xrtValueGetString(value, &text)) return NULL;
    if (length != NULL) *length = text.Size;
    return text.Data;
}

static int set_text(xvalue *object, const char *name, const char *text) {
    return xrtValueObjectSetNew(object, (xstrview){name, strlen(name)},
        xrtValueString((xstrview){text, strlen(text)}));
}

static int test_document_ownership(void) {
    static const char *documents[] = {
        "<rss version='2.0'><channel><title>A</title><item><title>B</title>"
        "<category>C</category><skip/><category>D</category></item><skip/></channel></rss>",
        "<feed xmlns='http://www.w3.org/2005/Atom'><author><name>Feed</name></author>"
        "<link rel='self' href='self'/><link href='alt'/>"
        "<entry><title>B</title><source><author><name>Source</name></author></source></entry>"
        "<entry><title>C</title></entry></feed>",
        "<feed xmlns='http://www.w3.org/2005/Atom'><entry>"
        "<author><name>Own</name></author><title>D</title></entry></feed>"
    };
    static const char invalid[] = "<unknown><child/></unknown>";
    xmemdebugsnapshot before, after;
    size_t i;
    CHECK(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    /* 预热一次，排除 XRT 首次使用的进程级缓存。 */
    for (i = 0; i < 3; ++i) {
        xvalue *value = xrss_parse(documents[i], (int64)strlen(documents[i]));
        CHECK(value != NULL && xrtValueType(value) == XVALUE_OBJECT);
        xrtValueRelease(value);
    }
    xrtMemDebugSnapshot(&before);
    for (i = 0; i < 12; ++i) {
        const char *text = documents[i % 3];
        xvalue *value = xrss_parse(text, (int64)strlen(text));
        CHECK(value != NULL && xrtValueType(value) == XVALUE_OBJECT);
        xrtValueRelease(value);
        CHECK(xrss_detect(text, (int64)strlen(text))[0] != '\0');
        value = xrss_parse(invalid, (int64)(sizeof(invalid) - 1u));
        CHECK(value != NULL && xrtValueType(value) == XVALUE_NULL);
        xrtValueRelease(value);
        CHECK(xrss_detect(invalid, (int64)(sizeof(invalid) - 1u))[0] == '\0');
    }
    xrtMemDebugSnapshot(&after);
    CHECK(after.LiveCount == before.LiveCount);
    return 0;
}

static int rejected(const char *xml, size_t size) {
    xvalue *value = xrss_parse(xml, (int64)size);
    int result = value != NULL && xrtValueType(value) == XVALUE_NULL;
    xrtValueRelease(value);
    xrtClearError();
    return result;
}

static int test_parse(void) {
    const char rss[] = "<rss version='2.0'><channel><title>A &amp; B</title>"
        "<item><title><![CDATA[<raw>]]></title><category>\xe4\xb8\xad\xe6\x96\x87</category>"
        "<category>&#x1F600;</category></item></channel></rss>";
    const char atom[] = "<feed xmlns='http://www.w3.org/2005/Atom'>"
        "<title>Atom</title><link rel='self' href='first'/><link rel='hub' href='hub'/>"
        "<link rel='alternate' href='site'/><entry><title>Post</title>"
        "<summary>A &lt; B</summary></entry></feed>";
    const char implicitAlternate[] =
        "<feed xmlns='http://www.w3.org/2005/Atom'>"
        "<link rel='hub' href='feed-hub'/><link href='feed-page'/>"
        "<entry><link rel='self' href='entry-self'/>"
        "<link href='entry-page'/></entry></feed>";
    const char hiddenNul[] = "<rss version='2.0'><channel/></rss>\0<evil/>";
    xvalue *feed, *items, *item, *categories;
    size_t size;
    CHECK(strcmp(xrss_detect(rss, sizeof(rss) - 1u), "rss") == 0);
    CHECK(strcmp(xrss_detect(atom, sizeof(atom) - 1u), "atom") == 0);
    CHECK(strcmp(xrss_detect("<rss><channel/></rss>",
        strlen("<rss><channel/></rss>")), "") == 0);
    CHECK(strcmp(xrss_detect("<rss version='0.91'><channel/></rss>",
        strlen("<rss version='0.91'><channel/></rss>")), "") == 0);
    CHECK(rejected("<rss><channel/></rss>", strlen("<rss><channel/></rss>")));
    CHECK(rejected("<rss version='0.91'><channel/></rss>",
        strlen("<rss version='0.91'><channel/></rss>")));
    CHECK(strcmp(xrss_detect("<rss><channel>", 14), "") == 0 &&
        xrtGetError() == NULL);
    feed = xrss_parse_rss(rss, sizeof(rss) - 1u);
    CHECK(feed != NULL && xrtValueType(feed) == XVALUE_OBJECT);
    CHECK(strcmp(field(feed, "title", NULL), "A & B") == 0);
    items = xrtValueObjectGet(feed, (xstrview){"items", 5});
    CHECK(items != NULL && xrtValueCount(items) == 1);
    item = xrtValueArrayAt(items, 0);
    CHECK(strcmp(field(item, "title", NULL), "<raw>") == 0);
    categories = xrtValueObjectGet(item, (xstrview){"categories", 10});
    CHECK(categories != NULL && xrtValueCount(categories) == 2);
    {
        xstrview category;
        CHECK(xrtValueGetString(xrtValueArrayAt(categories, 1), &category));
        CHECK(category.Size == 4u && memcmp(category.Data, "\xf0\x9f\x98\x80", 4u) == 0);
    }
    xrtValueRelease(feed);

    feed = xrss_parse_atom(atom, sizeof(atom) - 1u);
    CHECK(feed != NULL && xrtValueType(feed) == XVALUE_OBJECT);
    CHECK(strcmp(field(feed, "link", NULL), "site") == 0);
    items = xrtValueObjectGet(feed, (xstrview){"items", 5});
    item = xrtValueArrayAt(items, 0);
    CHECK(strcmp(field(item, "summary", &size), "A < B") == 0 && size == 5u);
    xrtValueRelease(feed);

    feed = xrss_parse_atom(implicitAlternate, sizeof(implicitAlternate) - 1u);
    CHECK(feed != NULL && xrtValueType(feed) == XVALUE_OBJECT);
    CHECK(strcmp(field(feed, "link", NULL), "feed-page") == 0);
    items = xrtValueObjectGet(feed, (xstrview){"items", 5});
    CHECK(items != NULL && xrtValueCount(items) == 1);
    item = xrtValueArrayAt(items, 0);
    CHECK(strcmp(field(item, "link", NULL), "entry-page") == 0);
    xrtValueRelease(feed);

    CHECK(rejected("<rss version='2.0'><channel></rss>",
        strlen("<rss version='2.0'><channel></rss>")));
    CHECK(rejected("<rss version='2.0'><channel/></rss>tail",
        strlen("<rss version='2.0'><channel/></rss>tail")));
    CHECK(rejected("<!DOCTYPE rss [<!ENTITY e 'bad'>]><rss version='2.0'><channel/></rss>",
        strlen("<!DOCTYPE rss [<!ENTITY e 'bad'>]><rss version='2.0'><channel/></rss>")));
    CHECK(rejected(hiddenNul, sizeof(hiddenNul) - 1u));
    CHECK(rejected("<html><rss><channel/></rss></html>",
        strlen("<html><rss><channel/></rss></html>")));
    {
        xvalue *bad = xrss_parse("<rss version='2.0'><channel/></rss>", -1);
        CHECK(bad != NULL && xrtValueType(bad) == XVALUE_NULL);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtValueRelease(bad);
        xrtClearError();
    }
    return 0;
}

static int test_mutated_feeds(void) {
    static const char *sources[] = {
        "<rss version='2.0'><channel><title>News</title></channel></rss>",
        "<feed xmlns='http://www.w3.org/2005/Atom'><title>News</title></feed>"
    };
    static const unsigned char replacements[] = {0, '<', '>', '&', 0xff};
    unsigned source, mode, trial;
    uint32_t state = 0x72a5f31bu;
    for (source = 0; source < 2; ++source) {
        size_t original = strlen(sources[source]);
        for (mode = 0; mode < 4; ++mode) {
            for (trial = 0; trial < 128; ++trial) {
                char text[128];
                size_t length = original, position;
                xvalue *result;
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                position = (size_t)state % (length + (mode == 2));
                memcpy(text, sources[source], length);
                if (mode == 0) {
                    text[position] = (char)replacements[trial % sizeof(replacements)];
                } else if (mode == 1) {
                    memmove(text + position, text + position + 1, length - position - 1);
                    --length;
                } else if (mode == 2) {
                    memmove(text + position + 1, text + position, length - position);
                    text[position] = (char)replacements[trial % sizeof(replacements)];
                    ++length;
                } else {
                    length = position;
                }
                result = xrss_parse(text, (int64)length);
                CHECK(result != NULL);
                CHECK(xrtValueType(result) == XVALUE_OBJECT ||
                    xrtValueType(result) == XVALUE_NULL);
                CHECK((xrtValueType(result) == XVALUE_OBJECT) == (xrtGetError() == NULL));
                xrtValueRelease(result);
            }
        }
    }
    return 0;
}

static int test_input_limit(void) {
    static const char prefix[] = "<rss version='2.0'><channel/></rss>";
    const size_t limit = 8u * 1024u * 1024u;
    char *xml = (char *)xrtMalloc(limit + 2u);
    xvalue *feed;
    CHECK(xml != NULL);
    memcpy(xml, prefix, sizeof(prefix) - 1u);
    memset(xml + sizeof(prefix) - 1u, ' ', limit + 1u - (sizeof(prefix) - 1u));
    xml[limit + 1u] = 0;

    feed = xrss_parse(xml, (int64)limit);
    CHECK(feed != NULL && xrtValueType(feed) == XVALUE_OBJECT);
    xrtValueRelease(feed);
    feed = xrss_parse(xml, (int64)limit + 1);
    CHECK(feed != NULL && xrtValueType(feed) == XVALUE_NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtValueRelease(feed);
    CHECK(strcmp(xrss_detect(xml, (int64)limit + 1), "") == 0 &&
        xrtGetError() == NULL);
    xrtFree(xml);
    return 0;
}

static int test_build(void) {
    xvalue *channel = xrtValueObject();
    xvalue *items = xrtValueArray();
    xvalue *item = xrtValueObject();
    xvalue *categories = xrtValueArray();
    xvalue *parsed;
    char longTitle[5001];
    char *xml, *other;
    size_t size;
    memset(longTitle, 'x', sizeof(longTitle) - 1u);
    longTitle[sizeof(longTitle) - 1u] = 0;
    CHECK(channel != NULL && items != NULL && item != NULL && categories != NULL);
    CHECK(xrtValueObjectSetNew(channel, (xstrview){"title", 5},
        xrtValueString((xstrview){longTitle, 5000u})));
    CHECK(set_text(channel, "link", "https://example.test"));
    CHECK(set_text(channel, "description", "News"));
    CHECK(xrtValueObjectSetNew(item, (xstrview){"title", 5},
        xrtValueString((xstrview){"A&B\rC", 5})));
    CHECK(xrtValueArrayAppendNew(categories,
        xrtValueString((xstrview){"release", 7})));
    CHECK(xrtValueObjectSetNew(item, (xstrview){"categories", 10}, categories));
    CHECK(xrtValueArrayAppendNew(items, item));
    xml = xrss_build_rss(channel, items);
    CHECK(xml != NULL && strstr(xml, "&amp;") != NULL);
    other = xrss_build_rss(channel, items);
    CHECK(other != NULL && other != xml && strcmp(other, xml) == 0);
    xrtFree(other);
    parsed = xrss_parse(xml, (int64)strlen(xml));
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_OBJECT);
    CHECK(field(parsed, "title", &size) != NULL && size == 5000u);
    {
        xvalue *backItems = xrtValueObjectGet(parsed, (xstrview){"items", 5});
        xvalue *backItem = xrtValueArrayAt(backItems, 0);
        xvalue *backCategories = xrtValueObjectGet(backItem,
            (xstrview){"categories", 10});
        CHECK(strcmp(field(backItem, "title", NULL), "A&B\rC") == 0);
        CHECK(xrtValueCount(backCategories) == 1);
    }
    xrtValueRelease(parsed);
    xrtFree(xml);

    CHECK(xrtValueObjectSetNew(channel, (xstrview){"title", 5},
        xrtValueString((xstrview){"bad \xef\xbf\xbe", 7})));
    CHECK(xrss_build_rss(channel, items) == NULL && xrtGetError() != NULL);
    xrtClearError();
    CHECK(xrtValueObjectSetNew(channel, (xstrview){"title", 5},
        xrtValueString((xstrview){"bad\0field", 9})));
    CHECK(xrss_build_rss(channel, items) == NULL && xrtGetError() != NULL);
    xrtClearError();
    CHECK(xrtValueObjectSetNew(channel, (xstrview){"title", 5}, xrtValueInt(4)));
    CHECK(xrss_build_rss(channel, items) == NULL && xrtGetError() != NULL);
    xrtClearError();
    xrtValueRelease(items);
    xrtValueRelease(channel);
    return 0;
}

static int test_output_limit(void) {
    const size_t limit = 16u * 1024u * 1024u;
    size_t size = limit / 5u + 1u;
    char *large = xrtMalloc(size);
    xvalue *channel = xrtValueObject();
    xvalue *items = xrtValueArray();
    char *output;
    size_t baseline, exact;
    CHECK(large != NULL && channel != NULL && items != NULL);
    memset(large, '&', size);
    CHECK(xrtValueObjectSetNew(channel, (xstrview){"title", 5},
        xrtValueString((xstrview){large, size})));
    CHECK(set_text(channel, "link", "https://example.test"));
    CHECK(set_text(channel, "description", "News"));
    xrtFree(large);
    output = xrss_build_rss(channel, items);
    CHECK(output == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(set_text(channel, "title", "T"));
    CHECK(set_text(channel, "description", "x"));
    output = xrss_build_rss(channel, items);
    CHECK(output != NULL);
    baseline = strlen(output);
    xrtFree(output);
    exact = limit - baseline + 1u;
    large = xrtMalloc(exact + 1u);
    CHECK(large != NULL);
    memset(large, 'x', exact + 1u);
    CHECK(xrtValueObjectSetNew(channel, (xstrview){"description", 11},
        xrtValueString((xstrview){large, exact})));
    output = xrss_build_rss(channel, items);
    CHECK(output != NULL && strlen(output) == limit && xrtGetError() == NULL);
    xrtFree(output);
    CHECK(xrtValueObjectSetNew(channel, (xstrview){"description", 11},
        xrtValueString((xstrview){large, exact + 1u})));
    CHECK(xrss_build_rss(channel, items) == NULL && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtFree(large);
    xrtValueRelease(items);
    xrtValueRelease(channel);
    return 0;
}

static int test_atom_build(void) {
    xvalue *feed = xrtValueObject();
    xvalue *entries = xrtValueArray();
    xvalue *entry = xrtValueObject();
    xvalue *parsed, *items, *back;
    char *xml;
    CHECK(feed != NULL && entries != NULL && entry != NULL);
    CHECK(xrtValueObjectSetNew(feed, (xstrview){"title", 5},
        xrtValueString((xstrview){"News", 4})));
    CHECK(xrtValueObjectSetNew(feed, (xstrview){"subtitle", 8},
        xrtValueString((xstrview){"Updates", 7})));
    CHECK(set_text(feed, "id", "urn:news"));
    CHECK(set_text(feed, "updated", "2026-10-16T00:00:00Z"));
    CHECK(set_text(feed, "author", "Editor"));
    CHECK(set_text(feed, "link", "https://example.test/a\tb\nc\rd"));
    CHECK(xrtValueObjectSetNew(entry, (xstrview){"title", 5},
        xrtValueString((xstrview){"Post", 4})));
    CHECK(xrtValueObjectSetNew(entry, (xstrview){"author", 6},
        xrtValueString((xstrview){"Ada", 3})));
    CHECK(xrtValueObjectSetNew(entry, (xstrview){"content", 7},
        xrtValueString((xstrview){"A < B", 5})));
    CHECK(set_text(entry, "id", "urn:news:1"));
    CHECK(set_text(entry, "updated", "2026-10-16T00:00:00Z"));
    CHECK(xrtValueArrayAppendNew(entries, entry));
    xml = xrss_build_atom(feed, entries);
    CHECK(xml != NULL && strstr(xml, "<author><name>Ada</name></author>") != NULL);
    parsed = xrss_parse_atom(xml, (int64)strlen(xml));
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_OBJECT);
    CHECK(strcmp(field(parsed, "subtitle", NULL), "Updates") == 0);
    CHECK(strcmp(field(parsed, "author", NULL), "Editor") == 0);
    CHECK(strcmp(field(parsed, "link", NULL), "https://example.test/a\tb\nc\rd") == 0);
    items = xrtValueObjectGet(parsed, (xstrview){"items", 5});
    back = xrtValueArrayAt(items, 0);
    CHECK(strcmp(field(back, "author", NULL), "Ada") == 0);
    CHECK(strcmp(field(back, "content", NULL), "A < B") == 0);
    xrtValueRelease(parsed);
    xrtFree(xml);
    CHECK(xrtValueObjectSetNew(feed, (xstrview){"title", 5},
        xrtValueString((xstrview){"bad \xef\xbf\xbf", 7})));
    CHECK(xrss_build_atom(feed, entries) == NULL && xrtGetError() != NULL);
    xrtClearError();
    xrtValueRelease(entries);
    xrtValueRelease(feed);
    return 0;
}

static int test_required_fields(void) {
    xvalue *feed = xrtValueObject();
    xvalue *entries = xrtValueArray();
    xvalue *entry = xrtValueObject();
    xvalue *parsed, *parsedItems, *parsedEntry;
    const char *author;
    char *xml;
    CHECK(feed != NULL && entries != NULL && entry != NULL);

    CHECK(xrss_build_rss(feed, entries) == NULL && xrtGetError() != NULL);
    CHECK(set_text(feed, "title", "News"));
    CHECK(set_text(feed, "link", "https://example.test"));
    CHECK(set_text(feed, "description", "Updates"));
    CHECK(xrtValueArrayAppendNew(entries, entry));
    CHECK(xrss_build_rss(feed, entries) == NULL && xrtGetError() != NULL);
    CHECK(set_text(entry, "description", "An item without title"));
    xml = xrss_build_rss(feed, entries);
    CHECK(xml != NULL);
    xrtFree(xml);

    CHECK(xrss_build_atom(feed, entries) == NULL && xrtGetError() != NULL);
    CHECK(set_text(feed, "id", "urn:news"));
    CHECK(set_text(feed, "updated", "2026-10-16T00:00:00Z"));
    CHECK(xrss_build_atom(feed, entries) == NULL && xrtGetError() != NULL);
    CHECK(set_text(entry, "title", "Post"));
    CHECK(set_text(entry, "id", "urn:news:post"));
    CHECK(set_text(entry, "updated", "2026-10-16T00:00:00Z"));
    CHECK(set_text(entry, "link", "https://example.test/post"));
    CHECK(xrss_build_atom(feed, entries) == NULL && xrtGetError() != NULL);
    CHECK(set_text(feed, "author", "Editor"));
    xml = xrss_build_atom(feed, entries);
    CHECK(xml != NULL && strstr(xml, "<author><name>Editor</name></author>") != NULL);
    parsed = xrss_parse_atom(xml, (int64)strlen(xml));
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_OBJECT);
    parsedItems = xrtValueObjectGet(parsed, (xstrview){"items", 5});
    parsedEntry = xrtValueArrayAt(parsedItems, 0);
    author = field(parsedEntry, "author", NULL);
    CHECK(author != NULL && strcmp(author, "Editor") == 0);
    xrtValueRelease(parsed);
    xrtFree(xml);
    {
        const char sourceXml[] =
            "<feed xmlns='http://www.w3.org/2005/Atom'>"
            "<author><name>Editor</name></author><entry>"
            "<source><author><name>Source</name></author></source>"
            "<title>Post</title></entry></feed>";
        parsed = xrss_parse_atom(sourceXml, (int64)(sizeof(sourceXml) - 1u));
        CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_OBJECT);
        parsedItems = xrtValueObjectGet(parsed, (xstrview){"items", 5});
        parsedEntry = xrtValueArrayAt(parsedItems, 0);
        author = field(parsedEntry, "author", NULL);
        CHECK(author != NULL && strcmp(author, "Source") == 0);
        xrtValueRelease(parsed);
    }
    CHECK(set_text(entry, "link", ""));
    CHECK(xrss_build_atom(feed, entries) == NULL && xrtGetError() != NULL);
    CHECK(set_text(entry, "content", "Post body"));
    xml = xrss_build_atom(feed, entries);
    CHECK(xml != NULL);
    xrtFree(xml);

    CHECK(set_text(feed, "author", ""));
    CHECK(xrss_build_atom(feed, entries) == NULL && xrtGetError() != NULL);
    CHECK(set_text(entry, "author", "Writer"));
    xml = xrss_build_atom(feed, entries);
    CHECK(xml != NULL && strstr(xml, "<author><name>Writer</name></author>") != NULL);
    xrtFree(xml);

    xrtValueRelease(entries);
    xrtValueRelease(feed);
    return 0;
}

static int test_many_items(void) {
    xbuffer xml;
    xvalue *feed, *items;
    int i;
    const char *start = "<rss version='2.0'><channel>";
    const char *item = "<item><title>x</title></item>";
    const char *end = "</channel></rss>";
    CHECK(xrtBufferInit(&xml));
    CHECK(xrtBufferAppend(&xml, (xbytesview){(cbytes)start, strlen(start)}));
    for (i = 0; i < 600; ++i) {
        CHECK(xrtBufferAppend(&xml, (xbytesview){(cbytes)item, strlen(item)}));
    }
    CHECK(xrtBufferAppend(&xml, (xbytesview){(cbytes)end, strlen(end)}));
    feed = xrss_parse_rss((const char *)xml.Data, (int64)xml.Size);
    CHECK(feed != NULL && xrtValueType(feed) == XVALUE_OBJECT);
    items = xrtValueObjectGet(feed, (xstrview){"items", 5});
    CHECK(items != NULL && xrtValueCount(items) == 600);
    xrtValueRelease(feed);
    xrtBufferUnit(&xml);
    return 0;
}

static int test_allocation_errors(void) {
    static const char *xml[2] = {
        "<rss version='2.0'><channel><title>News</title></channel></rss>",
        "<feed xmlns='http://www.w3.org/2005/Atom'><title>News</title></feed>"
    };
    xvalue *channel = xrtValueObject();
    xvalue *items = xrtValueArray();
    xmemdebugsnapshot before, after;
    unsigned mode, failure;
    CHECK(channel != NULL && items != NULL);
    CHECK(xrtValueObjectSetNew(channel, (xstrview){"title", 5},
        xrtValueString((xstrview){"News", 4})));
    CHECK(set_text(channel, "link", "https://example.test"));
    CHECK(set_text(channel, "description", "Updates"));
    CHECK(set_text(channel, "id", "urn:news"));
    CHECK(set_text(channel, "updated", "2026-10-16T00:00:00Z"));
    xrtMemDebugSnapshot(&before);

    for (mode = 0; mode < 2; ++mode) {
        unsigned buildFailures = 0, parseFailures = 0, detectFailures = 0;
        for (failure = 0; failure < 64; ++failure) {
            char *output;
            int triggered;
            xrtClearError();
            CHECK(xrtMemDebugFailAfter(failure));
            output = mode ? xrss_build_atom(channel, items) :
                xrss_build_rss(channel, items);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(output == NULL && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++buildFailures;
            } else CHECK(output != NULL && xrtGetError() == NULL);
            xrtFree(output);
        }
        for (failure = 0; failure < 64; ++failure) {
            xvalue *result;
            int triggered;
            xrtClearError();
            CHECK(xrtMemDebugFailAfter(failure));
            result = xrss_parse(xml[mode], (int64)strlen(xml[mode]));
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK((result == NULL || xrtValueType(result) == XVALUE_NULL) &&
                    xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++parseFailures;
            } else CHECK(result != NULL && xrtValueType(result) == XVALUE_OBJECT &&
                xrtGetError() == NULL);
            xrtValueRelease(result);
        }
        for (failure = 0; failure < 64; ++failure) {
            const char *kind;
            int triggered;
            xrtClearError();
            CHECK(xrtMemDebugFailAfter(failure));
            kind = xrss_detect(xml[mode], (int64)strlen(xml[mode]));
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(kind != NULL && *kind == 0 && xrtGetError() != NULL &&
                    xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++detectFailures;
            } else CHECK(kind != NULL && strcmp(kind, mode ? "atom" : "rss") == 0 &&
                xrtGetError() == NULL);
        }
        CHECK(buildFailures >= 2 && parseFailures >= 2 && detectFailures >= 2);
    }
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    CHECK(after.LiveCount == before.LiveCount);
    xrtValueRelease(items);
    xrtValueRelease(channel);
    return 0;
}

int main(void) {
    CHECK(test_document_ownership() == 0);
    CHECK(test_allocation_errors() == 0);
    CHECK(test_parse() == 0);
    CHECK(test_mutated_feeds() == 0);
    CHECK(test_input_limit() == 0);
    CHECK(test_build() == 0);
    CHECK(test_output_limit() == 0);
    CHECK(test_atom_build() == 0);
    CHECK(test_required_fields() == 0);
    CHECK(test_many_items() == 0);
    return 0;
}
