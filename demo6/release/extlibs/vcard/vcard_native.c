/* 值映射的公共字段表；解析与生成分别放在独立编译单元。 */
#include "vcard_internal.h"

#include <string.h>

const XvProperty xv_properties[] = {
    {"fn", "FN", 0}, {"n", "N", 1}, {"nickname", "NICKNAME", 0},
    {"email", "EMAIL", 0}, {"tel", "TEL", 0}, {"org", "ORG", 1},
    {"title", "TITLE", 0}, {"role", "ROLE", 0}, {"adr", "ADR", 1},
    {"url", "URL", 1}, {"bday", "BDAY", 0}, {"categories", "CATEGORIES", 0},
    {"note", "NOTE", 0}, {"uid", "UID", 0}, {"photo", "PHOTO", 1},
    {"logo", "LOGO", 1}, {"lang", "LANG", 0}, {"impp", "IMPP", 1},
    {"tz", "TZ", 0}, {"geo", "GEO", 1}, {"rev", "REV", 0}
};
const size_t xv_property_count = sizeof(xv_properties) / sizeof(xv_properties[0]);

void xv_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.vcard", 0, message);
}

int xv_ascii_equal(const char *left, size_t length, const char *right) {
    size_t i;
    if (length != strlen(right)) return 0;
    for (i = 0; i < length; ++i) {
        char ch = left[i];
        if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
        if (ch != right[i]) return 0;
    }
    return 1;
}

/* vCard 3.0/4.0 的内容行只接纳可打印字符和空白；内存 TEXT 换行稍后转义。 */
int xv_wire_chars(xstrview text, int allow_newline) {
    size_t i;
    for (i = 0; i < text.Size; ++i) {
        unsigned char ch = (unsigned char)text.Data[i];
        if ((ch < 0x20 && ch != '\t' && !(allow_newline && ch == '\n')) ||
            ch == 0x7f) return 0;
    }
    return 1;
}

const XvProperty *xv_property_key(const char *name, size_t length) {
    size_t i;
    for (i = 0; i < xv_property_count; ++i) {
        if (length == strlen(xv_properties[i].key) &&
            memcmp(name, xv_properties[i].key, length) == 0) return &xv_properties[i];
    }
    return NULL;
}

const XvProperty *xv_property_wire(const char *name, size_t length) {
    size_t i;
    for (i = 0; i < xv_property_count; ++i) {
        if (xv_ascii_equal(name, length, xv_properties[i].wire)) return &xv_properties[i];
    }
    return NULL;
}

int xv_append(xbuffer *buffer, const char *text, size_t length) {
    if (buffer->Size > XV_LIMIT || length > XV_LIMIT - buffer->Size) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.vcard", 0,
            "generated vCard exceeds 4 MiB");
        return 0;
    }
    return xrtBufferAppend(buffer, (xbytesview){(cbytes)text, length});
}

int xv_literal(xbuffer *buffer, const char *text) {
    return xv_append(buffer, text, strlen(text));
}
