/* RSS/Atom 生成器：直接写 XRT xbuffer，不使用固定字段缓冲。 */
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "rss_native.h"

#define RSS_OUTPUT_LIMIT (16u * 1024u * 1024u)

static void rss_build_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.rss", 0, message);
}

static void rss_memory_error(void) {
    if (xrtGetError() == NULL)
        xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.rss", 0,
            "out of memory while generating feed");
}

static int rss_bytes(xbuffer *out, const char *text, size_t size) {
    if (out->Size > RSS_OUTPUT_LIMIT ||
        size > RSS_OUTPUT_LIMIT - out->Size) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.rss", 0,
            "generated feed exceeds 16 MiB");
        return 0;
    }
    return xrtBufferAppend(out, (xbytesview){(cbytes)text, size});
}

static int rss_literal(xbuffer *out, const char *text) {
    return rss_bytes(out, text, strlen(text));
}

/* XML 1.0 字符校验；UTF-8 本身有效仍可能包含 U+FFFE/U+FFFF。 */
static int rss_xml_valid(xstrview text) {
    size_t pos = 0;
    while (pos < text.Size) {
        uint32 scalar;
        size_t read;
        if (xrtUtf8Decode((xstrview){text.Data + pos, text.Size - pos},
                &scalar, &read) != XUTF_OK || read == 0 ||
            (scalar < 0x20 && scalar != '\t' && scalar != '\n' && scalar != '\r') ||
            scalar == 0xfffe || scalar == 0xffff) return 0;
        pos += read;
    }
    return 1;
}

/* XML 会规范化字面 CR，以及属性里的字面 Tab/LF；字符引用才能保留原值。 */
static int rss_escaped(xbuffer *out, xstrview text, int attribute) {
    size_t i, start = 0;
    if (!rss_xml_valid(text)) return 0;
    for (i = 0; i < text.Size; ++i) {
        const char *replacement = NULL;
        unsigned char c = (unsigned char)text.Data[i];
        switch (c) {
            case '&': replacement = "&amp;"; break;
            case '<': replacement = "&lt;"; break;
            case '>': replacement = "&gt;"; break;
            case '"': replacement = "&quot;"; break;
            case '\'': replacement = "&apos;"; break;
            case '\r': replacement = "&#xD;"; break;
            case '\t': if (attribute) replacement = "&#x9;"; break;
            case '\n': if (attribute) replacement = "&#xA;"; break;
            default: break;
        }
        if (replacement == NULL) continue;
        if (!rss_bytes(out, text.Data + start, i - start) ||
            !rss_literal(out, replacement)) return 0;
        start = i + 1;
    }
    return rss_bytes(out, text.Data + start, text.Size - start);
}

static int rss_field(const xvalue *object, const char *key, xstrview *text) {
    xvalue *value;
    text->Data = "";
    text->Size = 0;
    if (object == NULL || xrtValueType(object) != XVALUE_OBJECT) return 0;
    value = xrtValueObjectGet(object, (xstrview){key, strlen(key)});
    return value == NULL ? 1 : xrtValueGetString(value, text);
}

static int rss_has_text(const xvalue *object, const char *key) {
    xstrview text;
    return rss_field(object, key, &text) && text.Size != 0;
}

static int rss_element(xbuffer *out, const xvalue *object,
    const char *field, const char *tag, const char *indent, int optional) {
    xstrview text;
    if (!rss_field(object, field, &text)) return 0;
    if (optional && text.Size == 0) return 1;
    return rss_literal(out, indent) && rss_literal(out, "<") &&
        rss_literal(out, tag) && rss_literal(out, ">") &&
        rss_escaped(out, text, 0) && rss_literal(out, "</") &&
        rss_literal(out, tag) && rss_literal(out, ">\n");
}

static int rss_link(xbuffer *out, const xvalue *object, const char *indent) {
    xstrview text;
    if (!rss_field(object, "link", &text)) return 0;
    if (text.Size == 0) return 1;
    return rss_literal(out, indent) && rss_literal(out, "<link href=\"") &&
        rss_escaped(out, text, 1) && rss_literal(out, "\"/>\n");
}

static int rss_author(xbuffer *out, const xvalue *object, const char *indent) {
    xstrview text;
    if (!rss_field(object, "author", &text)) return 0;
    if (text.Size == 0) return 1;
    return rss_literal(out, indent) && rss_literal(out, "<author><name>") &&
        rss_escaped(out, text, 0) && rss_literal(out, "</name></author>\n");
}

static void *rss_finish(xbuffer *out, int ok) {
    if (!ok || out->Size > RSS_OUTPUT_LIMIT) {
        if (out->Size > RSS_OUTPUT_LIMIT && xrtGetError() == NULL)
            xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.rss", 0,
                "generated feed exceeds 16 MiB");
        xrtBufferUnit(out);
        if (xrtGetError() == NULL)
            rss_build_error("invalid feed field or generated feed exceeds 16 MiB");
        return NULL;
    }
    if (!xrtBufferAppendByte(out, 0)) {
        xrtBufferUnit(out);
        rss_memory_error();
        return NULL;
    }
    return xrtBufferTake(out, NULL, NULL);
}

void *xrss_build_rss(xvalue *channel, xvalue *items) {
    xbuffer out;
    int64 i, count;
    int ok;
    xrtClearError();
    if (channel == NULL || items == NULL ||
        xrtValueType(channel) != XVALUE_OBJECT ||
        xrtValueType(items) != XVALUE_ARRAY) {
        rss_build_error("buildRSS expects a channel object and items array");
        return NULL;
    }
    if (!rss_has_text(channel, "title") || !rss_has_text(channel, "link") ||
        !rss_has_text(channel, "description")) {
        rss_build_error("RSS channel requires title, link and description");
        return NULL;
    }
    if (!xrtBufferInit(&out)) {
        rss_memory_error();
        return NULL;
    }
    ok = rss_literal(&out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<rss version=\"2.0\">\n<channel>\n") &&
        rss_element(&out, channel, "title", "title", "  ", 0) &&
        rss_element(&out, channel, "link", "link", "  ", 0) &&
        rss_element(&out, channel, "description", "description", "  ", 0) &&
        rss_element(&out, channel, "updated", "lastBuildDate", "  ", 1);
    count = xrtValueCount(items);
    for (i = 0; ok && i < count; ++i) {
        xvalue *item = xrtValueArrayAt(items, i);
        xvalue *categories;
        int64 j, categoryCount;
        if (item == NULL || xrtValueType(item) != XVALUE_OBJECT) { ok = 0; break; }
        if (!rss_has_text(item, "title") && !rss_has_text(item, "description")) {
            rss_build_error("RSS item requires title or description");
            ok = 0;
            break;
        }
        ok = rss_literal(&out, "  <item>\n") &&
            rss_element(&out, item, "title", "title", "    ", 1) &&
            rss_element(&out, item, "link", "link", "    ", 1) &&
            rss_element(&out, item, "description", "description", "    ", 1) &&
            rss_element(&out, item, "pubDate", "pubDate", "    ", 1) &&
            rss_element(&out, item, "guid", "guid", "    ", 1) &&
            rss_element(&out, item, "author", "author", "    ", 1);
        categories = xrtValueObjectGet(item, (xstrview){"categories", 10});
        if (categories != NULL && xrtValueType(categories) != XVALUE_ARRAY) ok = 0;
        categoryCount = categories != NULL ? xrtValueCount(categories) : 0;
        for (j = 0; ok && j < categoryCount; ++j) {
            xvalue *category = xrtValueArrayAt(categories, j);
            xstrview text;
            ok = category != NULL && xrtValueGetString(category, &text) &&
                rss_literal(&out, "    <category>") && rss_escaped(&out, text, 0) &&
                rss_literal(&out, "</category>\n");
        }
        if (ok) ok = rss_literal(&out, "  </item>\n");
    }
    if (ok) ok = rss_literal(&out, "</channel>\n</rss>\n");
    return rss_finish(&out, ok);
}

void *xrss_build_atom(xvalue *feed, xvalue *entries) {
    xbuffer out;
    int64 i, count;
    int ok;
    xrtClearError();
    if (feed == NULL || entries == NULL ||
        xrtValueType(feed) != XVALUE_OBJECT ||
        xrtValueType(entries) != XVALUE_ARRAY) {
        rss_build_error("buildAtom expects a feed object and entries array");
        return NULL;
    }
    if (!rss_has_text(feed, "id") || !rss_has_text(feed, "title") ||
        !rss_has_text(feed, "updated")) {
        rss_build_error("Atom feed requires id, title and updated");
        return NULL;
    }
    if (!xrtBufferInit(&out)) {
        rss_memory_error();
        return NULL;
    }
    ok = rss_literal(&out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n") &&
        rss_element(&out, feed, "title", "title", "  ", 0) &&
        rss_element(&out, feed, "subtitle", "subtitle", "  ", 1) &&
        rss_link(&out, feed, "  ") &&
        rss_element(&out, feed, "updated", "updated", "  ", 1) &&
        rss_element(&out, feed, "id", "id", "  ", 1) &&
        rss_author(&out, feed, "  ");
    count = xrtValueCount(entries);
    for (i = 0; ok && i < count; ++i) {
        xvalue *entry = xrtValueArrayAt(entries, i);
        if (entry == NULL || xrtValueType(entry) != XVALUE_OBJECT) { ok = 0; break; }
        if (!rss_has_text(entry, "id") || !rss_has_text(entry, "title") ||
            !rss_has_text(entry, "updated") ||
            (!rss_has_text(entry, "author") && !rss_has_text(feed, "author")) ||
            (!rss_has_text(entry, "content") && !rss_has_text(entry, "link"))) {
            rss_build_error("Atom entry requires id, title, updated, author and content or link");
            ok = 0;
            break;
        }
        ok = rss_literal(&out, "  <entry>\n") &&
            rss_element(&out, entry, "title", "title", "    ", 0) &&
            rss_link(&out, entry, "    ") &&
            rss_element(&out, entry, "id", "id", "    ", 1) &&
            rss_element(&out, entry, "updated", "updated", "    ", 1) &&
            rss_element(&out, entry, "summary", "summary", "    ", 1) &&
            rss_element(&out, entry, "content", "content", "    ", 1) &&
            rss_author(&out, entry, "    ");
        if (ok) ok = rss_literal(&out, "  </entry>\n");
    }
    if (ok) ok = rss_literal(&out, "</feed>\n");
    return rss_finish(&out, ok);
}
