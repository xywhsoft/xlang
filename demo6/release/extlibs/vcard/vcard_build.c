/* vCard 常用字段文本生成：按 UTF-8 字符边界折行，结果独立持有。 */
#include "vcard_internal.h"
#include "include/vcard_native.h"

#include <string.h>

static int xv_piece(xbuffer *output, const char *bytes, size_t size, size_t *column) {
    if (*column + size > 75) {
        if (!xv_literal(output, "\r\n ")) return 0;
        *column = 1;
    }
    if (!xv_append(output, bytes, size)) return 0;
    *column += size;
    return 1;
}

static int xv_emit(xbuffer *output, const XvProperty *property, xstrview text) {
    size_t i, column = strlen(property->wire) + 1;
    if (!xrtUtf8Valid(text, NULL) ||
        !xv_wire_chars(text, !property->raw_wire) ||
        !xv_literal(output, property->wire) || !xv_literal(output, ":")) return 0;
    for (i = 0; i < text.Size;) {
        const char *escape = NULL;
        unsigned char ch = (unsigned char)text.Data[i];
        size_t width = ch < 0x80 ? 1 : ch < 0xe0 ? 2 : ch < 0xf0 ? 3 : 4;
        if (!property->raw_wire) {
            if (ch == '\\') escape = "\\\\";
            else if (ch == ';') escape = "\\;";
            else if (ch == ',') escape = "\\,";
            else if (ch == '\n') escape = "\\n";
        }
        if (escape != NULL) {
            if (!xv_piece(output, escape, 2, &column)) return 0;
        } else if (!xv_piece(output, text.Data + i, width, &column)) return 0;
        i += width;
    }
    return xv_literal(output, "\r\n");
}

static int xv_emit_value(xbuffer *output, const XvProperty *property,
    const xvalue *value) {
    xstrview text;
    if (value == NULL || !xrtValueGetString(value, &text)) return 0;
    return xv_emit(output, property, text);
}

void *xv_build(xvalue *card) {
    xbuffer output;
    xvalue *value;
    xstrview text;
    const char *version = "3.0";
    size_t i;
    xrtClearError();
    if (card == NULL || xrtValueType(card) != XVALUE_OBJECT) goto invalid;
    value = xrtValueObjectGet(card, (xstrview){"version", 7});
    if (value != NULL && (!xrtValueGetString(value, &text) ||
            text.Size != 3 ||
            (memcmp(text.Data, "3.0", 3) != 0 &&
             memcmp(text.Data, "4.0", 3) != 0))) goto invalid;
    if (value != NULL) version = text.Data;
    value = xrtValueObjectGet(card, (xstrview){"fn", 2});
    if (value == NULL || !xrtValueGetString(value, &text) || text.Size == 0) goto invalid;
    for (i = 0; i < (size_t)xrtValueCount(card); ++i) {
        xstrview key;
        xrtValueObjectAt(card, i, &key);
        if ((key.Size == 7 && memcmp(key.Data, "version", 7) == 0) ||
            xv_property_key(key.Data, key.Size) != NULL) continue;
        goto invalid;
    }
    if (!xrtBufferInit(&output)) return NULL;
    if (!xv_literal(&output, "BEGIN:VCARD\r\nVERSION:") ||
        !xv_append(&output, version, 3) || !xv_literal(&output, "\r\n")) goto failed;
    for (i = 0; i < xv_property_count; ++i) {
        const XvProperty *property = xv_properties + i;
        value = xrtValueObjectGet(card,
            (xstrview){property->key, strlen(property->key)});
        if (value == NULL) continue;
        if (xrtValueType(value) == XVALUE_ARRAY) {
            int64_t j, count = xrtValueCount(value);
            if (count == 0 || property == xv_properties) goto failed;
            for (j = 0; j < count; ++j) {
                if (!xv_emit_value(&output, property, xrtValueArrayAt(value, j))) goto failed;
            }
        } else if (!xv_emit_value(&output, property, value)) goto failed;
    }
    if (!xv_literal(&output, "END:VCARD\r\n")) goto failed;
    if (!xrtBufferAppendByte(&output, 0)) goto failed;
    return xrtBufferTake(&output, NULL, NULL);
failed:
    xrtBufferUnit(&output);
invalid:
    if (xrtGetError() == NULL) xv_error("invalid vCard field or output exceeds 4 MiB");
    return NULL;
}
