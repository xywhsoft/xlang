/* sitemap XML：结构解析交给 XRT XML SAX；本文件只处理 sitemap 字段。 */
#include <stdint.h>
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "../xml.sax/include/xrt_xml.h"

#define XSM_DOMAIN "xlang.extlib.sitemap"
#define XSM_NS "http://www.sitemaps.org/schemas/sitemap/0.9"
#define XSM_MAX_ENTRIES 50000
#define XSM_MAX_BYTES (50u * 1024u * 1024u)

static void xsm_xml_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSM_DOMAIN, 0, message);
}

static xvalue *xsm_field(xvalue *object, const char *name) {
    int64 i, count;
    size_t size = strlen(name);
    if (object == NULL || xrtValueType(object) != XVALUE_OBJECT) return NULL;
    count = xrtValueCount(object);
    for (i = 0; i < count; ++i) {
        xstrview key;
        xvalue *item = xrtValueObjectAt(object, i, &key);
        if (key.Size == size && memcmp(key.Data, name, size) == 0) return item;
    }
    return NULL;
}

static int xsm_append(xbuffer *buffer, const char *data, size_t size) {
    if (buffer->Size > XSM_MAX_BYTES || size > XSM_MAX_BYTES - buffer->Size) {
        xrtSetErrorInfo(XERR_RANGE, XSM_DOMAIN, 0,
            "sitemap XML exceeds 50 MiB");
        return 0;
    }
    return xrtBufferAppend(buffer, (xbytesview){(const uint8 *)data, size});
}

static int xsm_literal(xbuffer *buffer, const char *text) {
    return xsm_append(buffer, text, strlen(text));
}

/* UTF-8 有效不等于 XML 1.0 字符有效，尤其要排除 U+FFFE/U+FFFF。 */
static int xsm_xml_valid(xstrview value) {
    size_t pos = 0;
    while (pos < value.Size) {
        uint32 scalar;
        size_t read;
        if (xrtUtf8Decode((xstrview){value.Data + pos, value.Size - pos},
                &scalar, &read) != XUTF_OK || read == 0 ||
            (scalar < 0x20 && scalar != '\t' && scalar != '\n' && scalar != '\r') ||
            scalar == 0xfffe || scalar == 0xffff) return 0;
        pos += read;
    }
    return 1;
}

/* 协议要求 loc 少于 2048 个字符；字节预筛可避免扫描超大非法字段。 */
static int xsm_loc_valid(xstrview value) {
    size_t count;
    if (value.Size == 0) return 0;
    if (value.Size >= 2048u * 4u) {
        xrtSetErrorInfo(XERR_RANGE, XSM_DOMAIN, 0, "sitemap loc exceeds 2047 characters");
        return 0;
    }
    count = xrtUtf8Count(value);
    if (count == XRT_NPOS) return 0;
    if (count >= 2048u) {
        xrtSetErrorInfo(XERR_RANGE, XSM_DOMAIN, 0, "sitemap loc exceeds 2047 characters");
        return 0;
    }
    return 1;
}

/* XML 解析会把字面 CR 规范化为 LF；字符引用保留字段原值。 */
static int xsm_escape(xbuffer *buffer, xstrview value) {
    size_t i, start = 0;
    if (!xsm_xml_valid(value)) return 0;
    for (i = 0; i < value.Size; ++i) {
        const char *escaped = NULL;
        unsigned char ch = (unsigned char)value.Data[i];
        switch (ch) {
            case '&': escaped = "&amp;"; break;
            case '<': escaped = "&lt;"; break;
            case '>': escaped = "&gt;"; break;
            case '\"': escaped = "&quot;"; break;
            case '\'': escaped = "&apos;"; break;
            case '\r': escaped = "&#xD;"; break;
            default: break;
        }
        if (escaped != NULL) {
            if (!xsm_append(buffer, value.Data + start, i - start) ||
                !xsm_literal(buffer, escaped)) return 0;
            start = i + 1;
        }
    }
    return xsm_append(buffer, value.Data + start, value.Size - start);
}

static int xsm_write_field(xbuffer *buffer, xvalue *entry,
                           const char *name, int required) {
    xvalue *field = xsm_field(entry, name);
    xstrview value;
    if (field == NULL) return !required;
    if (xrtValueType(field) != XVALUE_STRING || !xrtValueGetString(field, &value) ||
        (required && value.Size == 0)) return 0;
    if (strcmp(name, "loc") == 0 && !xsm_loc_valid(value)) return 0;
    if (!value.Size) return 1;
    return xsm_literal(buffer, "    <") && xsm_literal(buffer, name) &&
           xsm_literal(buffer, ">") && xsm_escape(buffer, value) &&
           xsm_literal(buffer, "</") && xsm_literal(buffer, name) &&
           xsm_literal(buffer, ">\n");
}

/* 生成与解析共用同一字段边界，避免未知键被静默丢弃。 */
static int xsm_entry_fields_valid(xvalue *entry, int index) {
    static const char *const fields[] = {"loc", "lastmod", "changefreq", "priority"};
    int64 i, count = xrtValueCount(entry);
    int fieldCount = index ? 2 : 4;
    for (i = 0; i < count; ++i) {
        xstrview key;
        int field;
        if (xrtValueObjectAt(entry, i, &key) == NULL) return 0;
        for (field = 0; field < fieldCount; ++field) {
            size_t size = strlen(fields[field]);
            if (key.Size == size && memcmp(key.Data, fields[field], size) == 0) break;
        }
        if (field == fieldCount) {
            xsm_xml_error("unsupported sitemap entry field");
            return 0;
        }
    }
    return 1;
}

static void *xsm_build_xml(xvalue *entries, int index) {
    xbuffer output;
    int64 count, i;
    const char *root = index ? "sitemapindex" : "urlset";
    const char *entry = index ? "sitemap" : "url";
    xrtClearError();
    if (entries == NULL || xrtValueType(entries) != XVALUE_ARRAY) goto invalid;
    count = xrtValueCount(entries);
    if (count > XSM_MAX_ENTRIES) {
        xrtSetErrorInfo(XERR_RANGE, XSM_DOMAIN, 0,
            "sitemap exceeds 50000 entries");
        return NULL;
    }
    if (!xrtBufferInit(&output)) return NULL;
    if (!xsm_literal(&output, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<") ||
        !xsm_literal(&output, root) ||
        !xsm_literal(&output, " xmlns=\"" XSM_NS "\">\n")) goto fail;
    for (i = 0; i < count; ++i) {
        xvalue *item = xrtValueArrayAt(entries, i);
        if (item == NULL || xrtValueType(item) != XVALUE_OBJECT ||
            !xsm_entry_fields_valid(item, index)) goto invalid_output;
        if (!xsm_literal(&output, "  <") || !xsm_literal(&output, entry) ||
            !xsm_literal(&output, ">\n")) goto fail;
        if (!xsm_write_field(&output, item, "loc", 1) ||
            !xsm_write_field(&output, item, "lastmod", 0) ||
            (!index && (!xsm_write_field(&output, item, "changefreq", 0) ||
                        !xsm_write_field(&output, item, "priority", 0)))) goto invalid_output;
        if (!xsm_literal(&output, "  </") || !xsm_literal(&output, entry) ||
            !xsm_literal(&output, ">\n")) goto fail;
    }
    if (!xsm_literal(&output, "</") || !xsm_literal(&output, root) ||
        !xsm_literal(&output, ">\n") || !xrtBufferAppendByte(&output, 0)) goto fail;
    return xrtBufferTake(&output, NULL, NULL);
invalid_output:
    if (xrtGetError() == NULL) xsm_xml_error("invalid sitemap entry or XML text");
fail:
    xrtBufferUnit(&output);
    return NULL;
invalid:
    xsm_xml_error("sitemap entries must be an array");
    return NULL;
}

void *xsm_build(xvalue *urls) { return xsm_build_xml(urls, 0); }
void *xsm_build_index(xvalue *entries) { return xsm_build_xml(entries, 1); }

typedef struct {
    xvalue *result;
    xvalue *entry;
    xbuffer text;
    const char *rootName;
    const char *entryName;
    const char *fieldName;
    int depth;
    int index;
    int hasLoc;
    int namespaced;
} XsmParser;

static int xsm_name(const xrtxmlevent *event, const char *expected) {
    const char *name = event->sLocalName ? event->sLocalName : event->sName;
    return name != NULL && strcmp(name, expected) == 0;
}

static const char *xsm_field_name(const xrtxmlevent *event, int index) {
    if (xsm_name(event, "loc")) return "loc";
    if (xsm_name(event, "lastmod")) return "lastmod";
    if (!index && xsm_name(event, "changefreq")) return "changefreq";
    if (!index && xsm_name(event, "priority")) return "priority";
    return NULL;
}

static bool xsm_xml_event(ptr context, const xrtxmlevent *event) {
    XsmParser *parser = (XsmParser *)context;
    if (event->iType == XRT_XML_EVENT_START) {
        parser->depth++;
        if (parser->depth == 1) {
            if (!xsm_name(event, parser->rootName) ||
                (event->sNamespaceUri && event->sNamespaceUri[0] &&
                 strcmp(event->sNamespaceUri, XSM_NS) != 0)) return false;
            parser->namespaced = event->sNamespaceUri && event->sNamespaceUri[0];
        } else if (parser->depth == 2) {
            if (parser->namespaced != (event->sNamespaceUri && event->sNamespaceUri[0]) ||
                (parser->namespaced && strcmp(event->sNamespaceUri, XSM_NS) != 0)) return false;
            if (!xsm_name(event, parser->entryName)) return false;
            if (xrtValueCount(parser->result) >= XSM_MAX_ENTRIES) {
                xrtSetErrorInfo(XERR_RANGE, XSM_DOMAIN, 0,
                    "sitemap exceeds 50000 entries");
                return false;
            }
            parser->entry = xrtValueObject();
            parser->hasLoc = 0;
            if (parser->entry == NULL) return false;
        } else if (parser->depth == 3) {
            if (parser->namespaced != (event->sNamespaceUri && event->sNamespaceUri[0]) ||
                (parser->namespaced && strcmp(event->sNamespaceUri, XSM_NS) != 0)) return false;
            parser->fieldName = xsm_field_name(event, parser->index);
            if (parser->fieldName == NULL) return false;
            xrtBufferClear(&parser->text);
        } else return false;
        return true;
    }
    if (event->iType == XRT_XML_EVENT_TEXT) {
        const char *p = event->sText ? event->sText : "";
        if (parser->depth == 3)
            return xsm_append(&parser->text, p, strlen(p)) != 0;
        while (*p) {
            if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') return false;
            ++p;
        }
        return true;
    }
    if (event->iType == XRT_XML_EVENT_END) {
        if (parser->depth == 3) {
            xbytesview bytes = xrtBufferView(&parser->text);
            xvalue *value;
            xstrview key;
            str owned;
            if (xsm_field(parser->entry, parser->fieldName) != NULL) return false;
            if (strcmp(parser->fieldName, "loc") == 0) {
                if (!xsm_loc_valid((xstrview){
                        bytes.Data ? (const char *)bytes.Data : "", bytes.Size})) return false;
                parser->hasLoc = 1;
            }
            owned = (str)xrtBufferTake(&parser->text, NULL, NULL);
            value = xrtValueStringTake(&owned, bytes.Size);
            if (value == NULL) {
                xrtFree(owned);
                return false;
            }
            key = (xstrview){parser->fieldName, strlen(parser->fieldName)};
            if (!xrtValueObjectSetNew(parser->entry, key, value)) return false;
            parser->fieldName = NULL;
        } else if (parser->depth == 2) {
            xvalue *entry = parser->entry;
            if (!parser->hasLoc) return false;
            parser->entry = NULL;
            if (!xrtValueArrayAppendNew(parser->result, entry)) return false;
        }
        parser->depth--;
    }
    return true;
}

static xvalue *xsm_parse_xml(const char *xml, size_t size, int index) {
    XsmParser parser = {0};
    xrtxmloptions options;
    xrtxmlerror error;
    bool success;
    xrtClearError();
    if (xml == NULL) goto invalid;
    if (size > XSM_MAX_BYTES) {
        xrtSetErrorInfo(XERR_RANGE, XSM_DOMAIN, 0,
            "sitemap XML exceeds 50 MiB");
        goto invalid;
    }
    if (memchr(xml, 0, size) != NULL) goto invalid;
    parser.rootName = index ? "sitemapindex" : "urlset";
    parser.entryName = index ? "sitemap" : "url";
    parser.index = index;
    parser.result = xrtValueArray();
    if (parser.result == NULL) return NULL;
    if (!xrtBufferInit(&parser.text)) { xrtValueRelease(parser.result); return NULL; }
    xrtXmlOptionsInit(&options);
    options.bPreserveComments = false;
    success = xrtXmlSaxParseMemory(xml, (uint64)size, &options,
                                  xsm_xml_event, &parser, &error);
    xrtBufferUnit(&parser.text);
    xrtValueRelease(parser.entry);
    if (success && parser.depth == 0) return parser.result;
    xrtValueRelease(parser.result);
    if (xrtGetError() == NULL && error.iCode == XRT_XML_ERROR_MEMORY)
        xrtSetErrorInfo(XERR_MEMORY, XSM_DOMAIN, 0, "sitemap XML allocation failed");
invalid:
    if (xrtGetError() == NULL) xsm_xml_error("invalid sitemap XML document");
    return xrtValueNull();
}

static size_t xsm_cstr_size(const char *xml) {
    size_t size = 0;
    /* 零结尾入口只探测上限外一个字节，超限由显式长度入口拒绝。 */
    if (xml != NULL)
        while (size <= XSM_MAX_BYTES && xml[size] != 0) size++;
    return size;
}

xvalue *xsm_parse(const char *xml) {
    return xsm_parse_xml(xml, xsm_cstr_size(xml), 0);
}
xvalue *xsm_parse_index(const char *xml) {
    return xsm_parse_xml(xml, xsm_cstr_size(xml), 1);
}
xvalue *xsm_parse_n(const char *xml, size_t size) {
    return xsm_parse_xml(xml, size, 0);
}
xvalue *xsm_parse_index_n(const char *xml, size_t size) {
    return xsm_parse_xml(xml, size, 1);
}
