/* iCalendar 常用字段表：TEXT 与日期/规则/URI 原值严格分开。 */
#include "ical_internal.h"

#include <string.h>

const XiProperty xi_properties[] = {
    {"uid", "UID", 1}, {"dtstamp", "DTSTAMP", 0},
    {"dtstart", "DTSTART", 0}, {"dtend", "DTEND", 0},
    {"summary", "SUMMARY", 1}, {"description", "DESCRIPTION", 1},
    {"location", "LOCATION", 1}, {"organizer", "ORGANIZER", 0},
    {"status", "STATUS", 0}, {"rrule", "RRULE", 0},
    {"due", "DUE", 0}, {"priority", "PRIORITY", 0},
    {"categories", "CATEGORIES", 0}, {"url", "URL", 0},
    {"created", "CREATED", 0}, {"lastModified", "LAST-MODIFIED", 0},
    {"sequence", "SEQUENCE", 0}, {"transp", "TRANSP", 0}
};
const size_t xi_property_count = sizeof(xi_properties) / sizeof(xi_properties[0]);

void xi_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.ical", 0, message);
}

int xi_ascii_equal(const char *left, size_t length, const char *right) {
    size_t i;
    if (length != strlen(right)) return 0;
    for (i = 0; i < length; ++i) {
        char ch = left[i];
        if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
        if (ch != right[i]) return 0;
    }
    return 1;
}

const XiProperty *xi_property_key(const char *key, size_t length) {
    size_t i;
    for (i = 0; i < xi_property_count; ++i) {
        if (length == strlen(xi_properties[i].key) &&
            memcmp(key, xi_properties[i].key, length) == 0) return xi_properties + i;
    }
    return NULL;
}

const XiProperty *xi_property_wire(const char *name, size_t length) {
    size_t i;
    for (i = 0; i < xi_property_count; ++i) {
        if (xi_ascii_equal(name, length, xi_properties[i].wire)) return xi_properties + i;
    }
    return NULL;
}

int xi_is_time_property(const XiProperty *property) {
    return property == xi_properties + 2 || property == xi_properties + 3 ||
        property == xi_properties + 10;
}

/* RFC 5545 VALUE-CHAR：HTAB 可用；TEXT 的内存换行由写出器转义。 */
int xi_wire_chars(xstrview value, int allow_newline) {
    size_t i;
    for (i = 0; i < value.Size; ++i) {
        unsigned char ch = (unsigned char)value.Data[i];
        if ((ch < 0x20 && ch != '\t' && !(allow_newline && ch == '\n')) ||
            ch == 0x7f) return 0;
    }
    return 1;
}

int xi_append(xbuffer *buffer, const char *text, size_t length) {
    if (buffer->Size > XI_LIMIT || length > XI_LIMIT - buffer->Size) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.ical", 0,
            "generated calendar exceeds 8 MiB");
        return 0;
    }
    return xrtBufferAppend(buffer, (xbytesview){(cbytes)text, length});
}

int xi_literal(xbuffer *buffer, const char *text) {
    return xi_append(buffer, text, strlen(text));
}
