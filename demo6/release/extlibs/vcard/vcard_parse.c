/* 单卡 vCard 3.0/4.0 值解析：保留重复字段，不截断逻辑行。 */
#include "vcard_internal.h"
#include "include/vcard_native.h"

#include <string.h>

static int xv_unescape(xstrview raw, xbuffer *out) {
    size_t i;
    for (i = 0; i < raw.Size; ++i) {
        unsigned char ch = (unsigned char)raw.Data[i];
        if (ch == '\\') {
            if (++i == raw.Size) return 0;
            ch = (unsigned char)raw.Data[i];
            if (ch == 'n' || ch == 'N') ch = '\n';
            else if (ch != '\\' && ch != ';' && ch != ',') return 0;
        }
        if (ch == 0 || ch == '\r' ||
            !xv_append(out, (const char *)&ch, 1)) return 0;
    }
    return 1;
}

static int xv_add_field(xvalue *object, const XvProperty *property, xstrview raw) {
    xstrview key = {property->key, strlen(property->key)};
    xvalue *existing = xrtValueObjectGet(object, key);
    xvalue *item;
    xbuffer decoded;
    char *text;
    size_t size;
    if (!xrtBufferInit(&decoded)) return 0;
    if (property->raw_wire) {
        if (!xv_append(&decoded, raw.Data, raw.Size)) goto fail;
    } else if (!xv_unescape(raw, &decoded)) goto fail;
    size = decoded.Size;
    text = (char *)xrtBufferTake(&decoded, NULL, NULL);
    item = (text != NULL || size == 0u) ? xrtValueStringTake(&text, size) : NULL;
    xrtFree(text);
    xrtBufferUnit(&decoded);
    if (item == NULL) return 0;
    if (existing == NULL) return xrtValueObjectSetNew(object, key, item);
    if (xrtValueType(existing) == XVALUE_ARRAY) {
        return xrtValueArrayAppendNew(existing, item);
    }
    {
        xvalue *array = xrtValueArray();
        int appended = 0;
        if (array == NULL) { xrtValueRelease(item); return 0; }
        if (xrtValueType(existing) == XVALUE_STRING) {
            xvalue *old = xrtValueRetain(existing);
            if (old != NULL && xrtValueArrayAppendNew(array, old)) {
                appended = xrtValueArrayAppendNew(array, item);
                item = NULL;
            }
        }
        xrtValueRelease(item);
        if (!appended) {
            xrtValueRelease(array);
            return 0;
        }
        return xrtValueObjectSetNew(object, key, array);
    }
fail:
    xrtBufferUnit(&decoded);
    return 0;
}

static int xv_name_valid(const char *name, size_t length) {
    size_t i;
    if (length == 0) return 0;
    for (i = 0; i < length; ++i) {
        unsigned char ch = (unsigned char)name[i];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '-')) return 0;
    }
    return 1;
}

/* 参数只校验语法外壳，不把未映射的参数带入结果对象。 */
static int xv_params_valid(xstrview line, size_t semi, size_t colon) {
    size_t pos = semi;
    while (pos < colon) {
        size_t start = ++pos;
        while (pos < colon && line.Data[pos] != '=' && line.Data[pos] != ';') ++pos;
        if (pos == colon || line.Data[pos] != '=' ||
            !xv_name_valid(line.Data + start, pos - start)) return 0;
        ++pos;
        for (;;) {
            if (pos < colon && line.Data[pos] == '"') {
                ++pos;
                while (pos < colon && line.Data[pos] != '"') ++pos;
                if (pos == colon) return 0;
                ++pos;
            } else {
                while (pos < colon && line.Data[pos] != ',' && line.Data[pos] != ';') {
                    if (line.Data[pos] == '"') return 0;
                    ++pos;
                }
            }
            if (pos >= colon || line.Data[pos] != ',') break;
            ++pos;
        }
        if (pos < colon && line.Data[pos] != ';') return 0;
    }
    return 1;
}

static int xv_parse_line(xvalue *object, xstrview line,
    int *phase, int *version, int *name, int *seenProperty) {
    size_t i, colon = line.Size, semi = line.Size, start = 0;
    int quoted = 0;
    const XvProperty *property;
    if (line.Size == 0) return 1;
    if (!xv_wire_chars(line, 0)) return 0;
    if (*phase == 0) {
        if (!xv_ascii_equal(line.Data, line.Size, "BEGIN:VCARD")) return 0;
        *phase = 1;
        return 1;
    }
    if (*phase != 1) return 0;
    if (xv_ascii_equal(line.Data, line.Size, "END:VCARD")) {
        *phase = 2;
        return 1;
    }
    for (i = 0; i < line.Size; ++i) {
        char ch = line.Data[i];
        if (ch == '"') quoted = !quoted;
        if (ch == ';' && !quoted && semi == line.Size) semi = i;
        if (ch == ':' && !quoted) { colon = i; break; }
    }
    if (quoted || colon == line.Size || colon == 0) return 0;
    if (semi > colon) semi = colon;
    for (i = 0; i < semi; ++i) {
        if (line.Data[i] != '.') continue;
        if (start != 0 || !xv_name_valid(line.Data, i)) return 0;
        start = i + 1;
    }
    if (!xv_name_valid(line.Data + start, semi - start)) return 0;
    if (!xv_params_valid(line, semi, colon)) return 0;
    /* 卡片内部的结构标记不能作为未知属性忽略。 */
    if (xv_ascii_equal(line.Data + start, semi - start, "BEGIN") ||
        xv_ascii_equal(line.Data + start, semi - start, "END")) return 0;
    if (xv_ascii_equal(line.Data + start, semi - start, "VERSION")) {
        xstrview number = {line.Data + colon + 1, line.Size - colon - 1};
        if (start != 0 || *version || (!xv_ascii_equal(number.Data, number.Size, "3.0") &&
                !xv_ascii_equal(number.Data, number.Size, "4.0"))) return 0;
        /* vCard 3.0 的 VERSION 不带参数；4.0 可以保留参数写法。 */
        if (semi != colon && xv_ascii_equal(number.Data, number.Size, "3.0")) return 0;
        /* 4.0 的 VERSION 必须紧随 BEGIN；3.0 保留原有顺序。 */
        if (*seenProperty && xv_ascii_equal(number.Data, number.Size, "4.0")) return 0;
        *version = 1;
        return xrtValueObjectSetNew(object, (xstrview){"version", 7},
            xrtValueString(number));
    }
    *seenProperty = 1;
    property = xv_property_wire(line.Data + start, semi - start);
    if (property == NULL) return 1; /* 值提取器：未知字段不进入映射。 */
    if (property == xv_properties && *name) return 0;
    if (property == xv_properties) {
        if (colon + 1 == line.Size) return 0;
        *name = 1;
    }
    return xv_add_field(object, property,
        (xstrview){line.Data + colon + 1, line.Size - colon - 1});
}

xvalue *xv_parse(const char *text, size_t size) {
    xvalue *result = NULL;
    xbuffer line;
    size_t pos = 0;
    int phase = 0, version = 0, name = 0, seenProperty = 0;
    xrtClearError();
    if (size > XV_LIMIT) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.vcard", 0,
            "vCard input exceeds 4 MiB");
        return NULL;
    }
    if (text == NULL || size == 0 ||
        memchr(text, 0, size) != NULL ||
        !xrtUtf8Valid((xstrview){text, size}, NULL)) {
        xv_error("invalid vCard input or UTF-8");
        return NULL;
    }
    if (!xrtBufferInit(&line)) return NULL;
    result = xrtValueObject();
    if (result == NULL) goto invalid;
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
                !xv_append(&line, text + start + 1, end - start - 1)) goto invalid;
        } else {
            if (line.Size != 0 && !xv_parse_line(result,
                    (xstrview){(char *)line.Data, line.Size},
                    &phase, &version, &name, &seenProperty)) goto invalid;
            if (end == start && phase == 1 && !version) seenProperty = 1;
            xrtBufferClear(&line);
            if (!xv_append(&line, text + start, end - start)) goto invalid;
        }
    }
    if (line.Size != 0 && !xv_parse_line(result,
            (xstrview){(char *)line.Data, line.Size},
            &phase, &version, &name, &seenProperty)) goto invalid;
    xrtBufferUnit(&line);
    if (phase == 2 && version && name) return result;
    xrtValueRelease(result);
    xv_error("vCard requires BEGIN, VERSION:3.0 or 4.0, FN and END");
    return NULL;
invalid:
    xrtBufferUnit(&line);
    xrtValueRelease(result);
    if (xrtGetError() == NULL) xv_error("invalid vCard field or input too large");
    return NULL;
}
