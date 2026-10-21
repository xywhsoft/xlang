/* 只对 TEXT 字段转义；RRULE、日期和 URI 原值输出，避免改写语义。 */
#include "ical_internal.h"
#include "include/ical_native.h"

#include <string.h>

static xvalue *xi_get(const xvalue *object, const char *key) {
    return xrtValueObjectGet(object, (xstrview){key, strlen(key)});
}

static int xi_text(const xvalue *object, const char *key, xstrview *out) {
    xvalue *value = xi_get(object, key);
    out->Data = "";
    out->Size = 0;
    return value == NULL || xrtValueGetString(value, out);
}

static int xi_piece(xbuffer *out, const char *text, size_t size, size_t *column) {
    if (*column + size > 75) {
        if (!xi_literal(out, "\r\n ")) return 0;
        *column = 1;
    }
    if (!xi_append(out, text, size)) return 0;
    *column += size;
    return 1;
}

static int xi_value_bytes(xbuffer *out, xstrview value, int is_text, size_t *column) {
    size_t i;
    for (i = 0; i < value.Size;) {
        const char *escape = NULL;
        unsigned char ch = (unsigned char)value.Data[i];
        size_t width = ch < 0x80 ? 1 : ch < 0xe0 ? 2 : ch < 0xf0 ? 3 : 4;
        if (is_text) {
            if (ch == '\\') escape = "\\\\";
            else if (ch == ';') escape = "\\;";
            else if (ch == ',') escape = "\\,";
            else if (ch == '\n') escape = "\\n";
        }
        if (escape != NULL) {
            if (!xi_piece(out, escape, 2, column)) return 0;
        } else if (!xi_piece(out, value.Data + i, width, column)) return 0;
        i += width;
    }
    return 1;
}

static int xi_prodid(xbuffer *out, xstrview value) {
    size_t column = 7;
    if (!xi_literal(out, "PRODID:")) return 0;
    return xi_value_bytes(out, value, 1, &column) && xi_literal(out, "\r\n");
}

static int xi_emit(xbuffer *out, const XiProperty *property,
    const xvalue *item, xstrview value) {
    xstrview tzid = {"", 0}, value_type = {"", 0};
    char key[48];
    size_t base = strlen(property->key), column, i;
    if (!xrtUtf8Valid(value, NULL) ||
        !xi_wire_chars(value, property->is_text) ||
        base + 5 >= sizeof(key)) return 0;
    memcpy(key, property->key, base);
    strcpy(key + base, "Tzid");
    if (!xi_text(item, key, &tzid)) return 0;
    if (xi_get(item, key) != NULL && tzid.Size == 0) return 0;
    strcpy(key + base, "Value");
    if (!xi_text(item, key, &value_type)) return 0;
    if (xi_get(item, key) != NULL && value_type.Size == 0) return 0;
    if ((tzid.Size || value_type.Size) && !xi_is_time_property(property)) return 0;
    if (tzid.Size) {
        if (tzid.Size > 128 || !xrtUtf8Valid(tzid, NULL)) return 0;
        for (i = 0; i < tzid.Size; ++i) {
            unsigned char ch = (unsigned char)tzid.Data[i];
            if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                  (ch >= '0' && ch <= '9') || ch == '/' || ch == '_' ||
                  ch == '-' || ch == '+' || ch == '.')) return 0;
        }
    }
    if (value_type.Size &&
        !xi_ascii_equal(value_type.Data, value_type.Size, "DATE") &&
        !xi_ascii_equal(value_type.Data, value_type.Size, "DATE-TIME")) return 0;
    if (!xi_literal(out, property->wire)) return 0;
    column = strlen(property->wire);
    if (tzid.Size) {
        if (!xi_piece(out, ";TZID=", 6, &column)) return 0;
        for (i = 0; i < tzid.Size; ++i) {
            if (!xi_piece(out, tzid.Data + i, 1, &column)) return 0;
        }
    }
    if (value_type.Size) {
        if (!xi_piece(out, ";VALUE=", 7, &column) ||
            !xi_piece(out, value_type.Data, value_type.Size, &column)) return 0;
    }
    if (!xi_piece(out, ":", 1, &column)) return 0;
    return xi_value_bytes(out, value, property->is_text, &column) &&
        xi_literal(out, "\r\n");
}

static int xi_allowed_key(xstrview key) {
    size_t i;
    if (xi_property_key(key.Data, key.Size) != NULL) return 1;
    for (i = 0; i < xi_property_count; ++i) {
        const XiProperty *property = xi_properties + i;
        size_t base = strlen(property->key);
        if (!xi_is_time_property(property) || key.Size <= base ||
            memcmp(key.Data, property->key, base) != 0) continue;
        if ((key.Size == base + 4 &&
             memcmp(key.Data + base, "Tzid", 4) == 0) ||
            (key.Size == base + 5 &&
             memcmp(key.Data + base, "Value", 5) == 0)) return 1;
    }
    return 0;
}

static int xi_required(const xvalue *item, const char *key) {
    xstrview value;
    return xi_get(item, key) != NULL && xi_text(item, key, &value) && value.Size != 0;
}

static int xi_component(xbuffer *out, const char *kind, const xvalue *item) {
    size_t i;
    if (item == NULL || xrtValueType(item) != XVALUE_OBJECT ||
        !xi_required(item, "uid") || !xi_required(item, "dtstamp") ||
        (strcmp(kind, "VEVENT") == 0 && !xi_required(item, "dtstart"))) return 0;
    for (i = 0; i < (size_t)xrtValueCount(item); ++i) {
        xstrview key;
        xrtValueObjectAt(item, i, &key);
        if (!xi_allowed_key(key)) return 0;
    }
    if (!xi_literal(out, "BEGIN:") || !xi_literal(out, kind) ||
        !xi_literal(out, "\r\n")) return 0;
    for (i = 0; i < xi_property_count; ++i) {
        const XiProperty *property = xi_properties + i;
        xvalue *value = xi_get(item, property->key);
        if (value == NULL) {
            char companion[48];
            size_t base = strlen(property->key);
            if (xi_is_time_property(property)) {
                memcpy(companion, property->key, base);
                strcpy(companion + base, "Tzid");
                if (xi_get(item, companion) != NULL) return 0;
                strcpy(companion + base, "Value");
                if (xi_get(item, companion) != NULL) return 0;
            }
            continue;
        }
        if (xrtValueType(value) == XVALUE_ARRAY) {
            int64_t j, count = xrtValueCount(value);
            if (count == 0 || property == xi_properties ||
                property == xi_properties + 1 || xi_is_time_property(property)) return 0;
            for (j = 0; j < count; ++j) {
                xstrview text;
                if (!xrtValueGetString(xrtValueArrayAt(value, j), &text) ||
                    !xi_emit(out, property, item, text)) return 0;
            }
        } else {
            xstrview text;
            if (!xrtValueGetString(value, &text) ||
                !xi_emit(out, property, item, text)) return 0;
        }
    }
    return xi_literal(out, "END:") && xi_literal(out, kind) &&
        xi_literal(out, "\r\n");
}

void *xi_build(xvalue *calendar) {
    xbuffer out;
    xvalue *events, *todos;
    xstrview prodid;
    size_t i;
    char *result;
    xrtClearError();
    if (calendar == NULL || xrtValueType(calendar) != XVALUE_OBJECT) goto invalid;
    for (i = 0; i < (size_t)xrtValueCount(calendar); ++i) {
        xstrview key;
        xrtValueObjectAt(calendar, (int64_t)i, &key);
        if (!((key.Size == 7 && memcmp(key.Data, "version", 7) == 0) ||
              (key.Size == 6 && memcmp(key.Data, "prodid", 6) == 0) ||
              (key.Size == 6 && memcmp(key.Data, "events", 6) == 0) ||
              (key.Size == 5 && memcmp(key.Data, "todos", 5) == 0))) goto invalid;
    }
    if (xi_get(calendar, "version") != NULL) {
        xstrview version;
        if (!xi_text(calendar, "version", &version) || version.Size != 3 ||
            memcmp(version.Data, "2.0", 3) != 0) goto invalid;
    }
    if (!xi_text(calendar, "prodid", &prodid)) goto invalid;
    if (prodid.Size == 0) {
        prodid = (xstrview){"-//xlang//ical extlib//EN",
            sizeof("-//xlang//ical extlib//EN") - 1};
    }
    events = xi_get(calendar, "events");
    todos = xi_get(calendar, "todos");
    if ((events != NULL && xrtValueType(events) != XVALUE_ARRAY) ||
        (todos != NULL && xrtValueType(todos) != XVALUE_ARRAY)) goto invalid;
    if (!xrtBufferInit(&out)) return NULL;
    if (!xi_literal(&out, "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n") ||
        !xrtUtf8Valid(prodid, NULL) || !xi_wire_chars(prodid, 1) ||
        !xi_prodid(&out, prodid)) goto failed;
    for (i = 0; events != NULL && i < xrtValueCount(events); ++i) {
        if (!xi_component(&out, "VEVENT", xrtValueArrayAt(events, (int64_t)i))) goto failed;
    }
    for (i = 0; todos != NULL && i < xrtValueCount(todos); ++i) {
        if (!xi_component(&out, "VTODO", xrtValueArrayAt(todos, (int64_t)i))) goto failed;
    }
    if (!xi_literal(&out, "END:VCALENDAR\r\n")) goto failed;
    result = xrtMalloc(out.Size + 1);
    if (result == NULL) goto failed;
    memcpy(result, out.Data, out.Size);
    result[out.Size] = 0;
    xrtBufferUnit(&out);
    return result;
failed:
    xrtBufferUnit(&out);
invalid:
    if (xrtGetError() == NULL) xi_error("invalid calendar fields or output exceeds 8 MiB");
    return NULL;
}
