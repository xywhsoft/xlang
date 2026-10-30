/* 解析日历包络与事件/待办；嵌套 VALARM 等组件不会污染父事件。 */
#include "ical_internal.h"
#include "include/ical_native.h"

#include <string.h>

typedef struct {
    xvalue *calendar;
    xvalue *current;
    xvalue *target;
    char stack[8][32];
    int depth;
    int version;
    int prodid;
} XiParser;

static int xi_set_text(xvalue *object, const char *key, xstrview value) {
    xvalue *item = xrtValueString(value);
    return item != NULL && xrtValueObjectSetNew(object,
        (xstrview){key, strlen(key)}, item);
}

static xvalue *xi_take_decoded(xbuffer *decoded) {
    size_t size = decoded->Size;
    char *text = (char *)xrtBufferTake(decoded, NULL, NULL);
    xvalue *item = (text != NULL || size == 0u) ? xrtValueStringTake(&text, size) : NULL;
    xrtFree(text);
    xrtBufferUnit(decoded);
    return item;
}

static int xi_unescape(xstrview raw, xbuffer *out) {
    size_t i;
    for (i = 0; i < raw.Size; ++i) {
        unsigned char ch = (unsigned char)raw.Data[i];
        if (ch == '\\') {
            if (++i == raw.Size) return 0;
            ch = (unsigned char)raw.Data[i];
            if (ch == 'n' || ch == 'N') ch = '\n';
            else if (ch != '\\' && ch != ';' && ch != ',') return 0;
        }
        if (ch == 0 || ch == '\r' || !xi_append(out, (const char *)&ch, 1)) return 0;
    }
    return 1;
}

static int xi_set_unescaped_text(xvalue *object, const char *key, xstrview raw) {
    xbuffer decoded;
    xvalue *item;
    if (!xrtBufferInit(&decoded)) return 0;
    if (!xi_unescape(raw, &decoded)) {
        xrtBufferUnit(&decoded);
        return 0;
    }
    item = xi_take_decoded(&decoded);
    return item != NULL && xrtValueObjectSetNew(object,
        (xstrview){key, strlen(key)}, item);
}

static int xi_add_property(xvalue *object, const XiProperty *property, xstrview raw) {
    xstrview key = {property->key, strlen(property->key)};
    xvalue *previous = xrtValueObjectGet(object, key);
    xvalue *item;
    xbuffer decoded;
    if (!xrtBufferInit(&decoded)) return 0;
    if (property->is_text) {
        if (!xi_unescape(raw, &decoded)) goto fail;
    } else if (!xi_append(&decoded, raw.Data, raw.Size)) goto fail;
    item = xi_take_decoded(&decoded);
    if (item == NULL) return 0;
    if (previous == NULL) return xrtValueObjectSetNew(object, key, item);
    if (xi_is_single_property(property)) {
        xrtValueRelease(item);
        return 0;
    }
    if (xrtValueType(previous) == XVALUE_ARRAY) {
        return xrtValueArrayAppendNew(previous, item);
    }
    {
        xvalue *array = xrtValueArray();
        int appended = 0;
        if (array == NULL) { xrtValueRelease(item); return 0; }
        if (xrtValueType(previous) == XVALUE_STRING) {
            xvalue *old = xrtValueRetain(previous);
            if (old != NULL && xrtValueArrayAppendNew(array, old)) {
                appended = xrtValueArrayAppendNew(array, item);
                item = NULL;
            }
        }
        xrtValueRelease(item);
        if (!appended) { xrtValueRelease(array); return 0; }
        return xrtValueObjectSetNew(object, key, array);
    }
fail:
    xrtBufferUnit(&decoded);
    return 0;
}

static int xi_param_value(xvalue *object, const XiProperty *property,
    xstrview name, xstrview value) {
    char key[48];
    size_t base = strlen(property->key), i;
    const char *suffix;
    if (name.Size == 4 && xi_ascii_equal(name.Data, name.Size, "TZID")) {
        suffix = "Tzid";
        if (value.Size == 0 || value.Size > 128) return 0;
        for (i = 0; i < value.Size; ++i) {
            unsigned char ch = (unsigned char)value.Data[i];
            if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                  (ch >= '0' && ch <= '9') || ch == '/' || ch == '_' ||
                  ch == '-' || ch == '+' || ch == '.')) return 0;
        }
    } else if (xi_ascii_equal(name.Data, name.Size, "VALUE")) {
        suffix = "Value";
        if (!xi_ascii_equal(value.Data, value.Size, "DATE") &&
            !xi_ascii_equal(value.Data, value.Size, "DATE-TIME")) return 0;
    } else return 1;
    if (base + strlen(suffix) >= sizeof(key)) return 0;
    memcpy(key, property->key, base);
    strcpy(key + base, suffix);
    if (xrtValueObjectGet(object, (xstrview){key, strlen(key)}) != NULL) return 0;
    return xi_set_text(object, key, value);
}

static int xi_params(xvalue *object, const XiProperty *property,
    xstrview line, size_t semi, size_t colon) {
    size_t pos;
    if (semi >= colon) return 1;
    if (semi + 1 == colon) return 0;
    pos = semi + 1;
    while (pos < colon) {
        size_t start = pos, end, equal, i;
        int quoted = 0;
        while (pos < colon) {
            if (line.Data[pos] == '"') quoted = !quoted;
            if (line.Data[pos] == ';' && !quoted) break;
            ++pos;
        }
        end = pos;
        if (quoted) return 0;
        if (pos < colon) {
            ++pos;
            if (pos == colon) return 0;
        }
        for (equal = start; equal < end && line.Data[equal] != '='; ++equal) {}
        if (equal == start || equal == end || equal + 1 == end) return 0;
        for (i = start; i < equal; ++i) {
            unsigned char ch = (unsigned char)line.Data[i];
            if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                  (ch >= '0' && ch <= '9') || ch == '-')) return 0;
        }
        /* 引号只能包住完整的单个参数值；逗号可分隔多个值。 */
        i = equal + 1;
        while (i < end) {
            size_t valueStart = i;
            if (line.Data[i] == '"') {
                ++i;
                while (i < end && line.Data[i] != '"') {
                    unsigned char ch = (unsigned char)line.Data[i++];
                    if ((ch < 0x20 && ch != '\t') || ch == 0x7f) return 0;
                }
                if (i == end) return 0;
                ++i;
            } else {
                while (i < end && line.Data[i] != ',') {
                    unsigned char ch = (unsigned char)line.Data[i++];
                    if (ch == '"' || ch == ';' || ch == ':' ||
                        (ch < 0x20 && ch != '\t') || ch == 0x7f) return 0;
                }
                if (i == valueStart) return 0;
            }
            if (i < end && (line.Data[i++] != ',' || i == end)) return 0;
        }
        {
            xstrview value = {line.Data + equal + 1, end - equal - 1};
            if (value.Size >= 2 && value.Data[0] == '"' &&
                value.Data[value.Size - 1] == '"') {
                ++value.Data;
                value.Size -= 2;
            }
            if (property != NULL && xi_is_time_property(property) &&
                !xi_param_value(object, property,
                (xstrview){line.Data + start, equal - start}, value)) return 0;
        }
    }
    return 1;
}

static int xi_property_line(XiParser *parser, xstrview line) {
    size_t i, colon = line.Size, semi = line.Size;
    int quoted = 0;
    const XiProperty *property;
    for (i = 0; i < line.Size; ++i) {
        char ch = line.Data[i];
        if (ch == '"') quoted = !quoted;
        if (ch == ';' && !quoted && semi == line.Size) semi = i;
        if (ch == ':' && !quoted) { colon = i; break; }
    }
    if (quoted || colon == line.Size || colon == 0) return 0;
    if (semi > colon) semi = colon;
    if (semi == 0) return 0;
    for (i = 0; i < semi; ++i) {
        unsigned char ch = (unsigned char)line.Data[i];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '-')) return 0;
    }
    /* 结构标记只能是无参数的 BEGIN:/END: 行，不能降级为未知属性。 */
    if (xi_ascii_equal(line.Data, semi, "BEGIN") ||
        xi_ascii_equal(line.Data, semi, "END")) return 0;
    if (parser->depth == 1) {
        if (xi_ascii_equal(line.Data, semi, "VERSION")) {
            if (!xi_params(NULL, NULL, line, semi, colon) ||
                parser->version || line.Size - colon - 1 != 3 ||
                memcmp(line.Data + colon + 1, "2.0", 3) != 0) return 0;
            parser->version = 1;
            return 1;
        }
        if (xi_ascii_equal(line.Data, semi, "PRODID")) {
            if (!xi_params(NULL, NULL, line, semi, colon) ||
                parser->prodid || colon + 1 == line.Size) return 0;
            parser->prodid = 1;
            return xi_set_unescaped_text(parser->calendar, "prodid",
                (xstrview){line.Data + colon + 1, line.Size - colon - 1});
        }
        return xi_params(NULL, NULL, line, semi, colon);
    }
    if (parser->depth != 2 || parser->current == NULL)
        return xi_params(NULL, NULL, line, semi, colon);
    property = xi_property_wire(line.Data, semi);
    if (property == NULL) return xi_params(NULL, NULL, line, semi, colon);
    return xi_params(parser->current, property, line, semi, colon) &&
        xi_add_property(parser->current, property,
            (xstrview){line.Data + colon + 1, line.Size - colon - 1});
}

static int xi_component_name_valid(const char *name, size_t length) {
    size_t i;
    if (length == 0) return 0;
    for (i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)name[i];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '-')) return 0;
    }
    return 1;
}

static int xi_line(XiParser *parser, xstrview line) {
    size_t length;
    char *name;
    /* 物理折行可能切开多字节字符；只校验展开后的完整逻辑行，包括未知字段。 */
    if (!xrtUtf8Valid(line, NULL) || !xi_wire_chars(line, 0)) return 0;
    if (line.Size == 0) return 1;
    if (line.Size >= 6 && xi_ascii_equal(line.Data, 5, "BEGIN") && line.Data[5] == ':') {
        length = line.Size - 6;
        if (length >= sizeof(parser->stack[0]) || parser->depth == 8 ||
            !xi_component_name_valid(line.Data + 6, length)) return 0;
        /* VCALENDAR 只能占据唯一根层，不能伪装成可忽略的子组件。 */
        if (xi_ascii_equal(line.Data + 6, length, "VCALENDAR") !=
            (parser->depth == 0)) return 0;
        name = parser->stack[parser->depth];
        memcpy(name, line.Data + 6, length);
        name[length] = 0;
        {
            size_t i;
            for (i = 0; i < length; ++i) {
                if (name[i] >= 'a' && name[i] <= 'z') name[i] -= 'a' - 'A';
            }
        }
        ++parser->depth;
        if (parser->depth == 2) {
            if (xi_ascii_equal(name, length, "VEVENT")) {
                parser->current = xrtValueObject();
                parser->target = xrtValueObjectGet(parser->calendar,
                    (xstrview){"events", 6});
            } else if (xi_ascii_equal(name, length, "VTODO")) {
                parser->current = xrtValueObject();
                parser->target = xrtValueObjectGet(parser->calendar,
                    (xstrview){"todos", 5});
            }
            if (parser->target != NULL && parser->current == NULL) return 0;
        }
        return 1;
    }
    if (line.Size >= 4 && xi_ascii_equal(line.Data, 3, "END") && line.Data[3] == ':') {
        if (parser->depth == 0 ||
            !xi_ascii_equal(line.Data + 4, line.Size - 4,
                parser->stack[parser->depth - 1])) return 0;
        if (parser->depth == 2 && parser->current != NULL) {
            if (!xrtValueArrayAppendNew(parser->target, parser->current)) {
                parser->current = NULL;
                return 0;
            }
            parser->current = NULL;
            parser->target = NULL;
        }
        --parser->depth;
        return parser->depth != 0 || (parser->version && parser->prodid);
    }
    return parser->depth != 0 && xi_property_line(parser, line);
}

xvalue *xi_parse(const char *text, size_t size) {
    XiParser parser = {0};
    xbuffer line;
    size_t pos = 0;
    int closed = 0;
    xrtClearError();
    if (size > XI_LIMIT) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.ical", 0,
            "iCalendar input exceeds 8 MiB");
        return NULL;
    }
    if (text == NULL || size == 0 || memchr(text, 0, size) != NULL) {
        xi_error("invalid iCalendar input");
        return NULL;
    }
    if (!xrtBufferInit(&line)) return NULL;
    parser.calendar = xrtValueObject();
    if (parser.calendar == NULL) goto invalid;
    if (!xrtValueObjectSetNew(parser.calendar, (xstrview){"events", 6},
            xrtValueArray()) ||
        !xrtValueObjectSetNew(parser.calendar, (xstrview){"todos", 5},
            xrtValueArray())) goto invalid;
    while (pos < size) {
        size_t start = pos, end;
        int folded;
        while (pos < size && text[pos] != '\n') ++pos;
        end = pos;
        if (pos < size) ++pos;
        if (end > start && text[end - 1] == '\r') --end;
        if (memchr(text + start, '\r', end - start) != NULL) goto invalid;
        folded = end > start && (text[start] == ' ' || text[start] == '\t');
        if (folded) {
            if (line.Size == 0 ||
                !xi_append(&line, text + start + 1, end - start - 1)) goto invalid;
        } else {
            if (closed && line.Size != 0) goto invalid;
            if (line.Size != 0 && !xi_line(&parser,
                    (xstrview){(char *)line.Data, line.Size})) goto invalid;
            if (parser.depth == 0 && line.Size != 0) closed = 1;
            xrtBufferClear(&line);
            if (!xi_append(&line, text + start, end - start)) goto invalid;
        }
    }
    if (closed && line.Size != 0) goto invalid;
    if (line.Size != 0 && !xi_line(&parser,
            (xstrview){(char *)line.Data, line.Size})) goto invalid;
    if (parser.depth == 0 && line.Size != 0) closed = 1;
    xrtBufferUnit(&line);
    if (closed && parser.version && parser.prodid) return parser.calendar;
    xrtValueRelease(parser.current);
    xrtValueRelease(parser.calendar);
    xi_error("iCalendar requires VCALENDAR, VERSION:2.0 and PRODID");
    return NULL;
invalid:
    xrtBufferUnit(&line);
    xrtValueRelease(parser.current);
    xrtValueRelease(parser.calendar);
    if (xrtGetError() == NULL) xi_error("invalid iCalendar structure or UTF-8");
    return NULL;
}
