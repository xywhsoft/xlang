/* vCard 常用字段文本生成：按 UTF-8 字符边界折行，结果独立持有。 */
#include "vcard_internal.h"
#include "include/vcard_native.h"
#ifdef XL_VALUE_READ_WIRE_ONLY
#include "../_common/xl_value_read_wire.h"
#else
#include <xl_value_read.h>
#endif

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

static int xv_present(XL_ValueBorrowV1 value) {
    return value.ref != NULL || value.kind != XVALUE_NULL;
}

static int xv_text(XL_ValueBorrowV1 value, xstrview *text) {
    if (value.kind != XVALUE_STRING) {
        xrtSetErrorKind(XERR_TYPE);
        return 0;
    }
    *text = value.data.text;
    return 1;
}

static int xv_emit_value(xbuffer *output, const XvProperty *property,
    XL_ValueBorrowV1 value) {
    xstrview text;
    return xv_text(value, &text) && xv_emit(output, property, text);
}

void *xv_build(xvalue *card) {
    xbuffer output = {0};
    XL_ValueReadV1 object = {0}, array = {0};
    XL_ValueBorrowV1 value;
    int initialized = 0;
    void *result = NULL;
    xstrview text;
    const char *version = "3.0";
    size_t i;
    xrtClearError();
    if (card == NULL || (xrtValueType(card) != XVALUE_OBJECT &&
            xrtValueType(card) != XVALUE_HANDLE)) goto invalid;
    if (!xlValueReadBeginV1(xlValueBorrowV1(card), &object) ||
            object.kind != XVALUE_OBJECT) goto invalid;
    value = xlValueReadFindV1(&object, (xstrview){"version", 7});
    if (xrtGetError()) goto invalid;
    if (xv_present(value) && (!xv_text(value, &text) ||
            text.Size != 3 ||
            (memcmp(text.Data, "3.0", 3) != 0 &&
             memcmp(text.Data, "4.0", 3) != 0))) goto invalid;
    if (xv_present(value)) version = text.Data;
    value = xlValueReadFindV1(&object, (xstrview){"fn", 2});
    if (xrtGetError() || !xv_present(value) || !xv_text(value, &text) || text.Size == 0) goto invalid;
    for (i = 0; i < object.count; ++i) {
        xstrview key = {0};
        (void)xlValueReadAtV1(&object, i, &key);
        if (xrtGetError()) goto invalid;
        if ((key.Size == 7 && memcmp(key.Data, "version", 7) == 0) ||
            xv_property_key(key.Data, key.Size) != NULL) continue;
        goto invalid;
    }
    if (!xrtBufferInit(&output)) goto invalid;
    initialized = 1;
    if (!xv_literal(&output, "BEGIN:VCARD\r\nVERSION:") ||
        !xv_append(&output, version, 3) || !xv_literal(&output, "\r\n")) goto invalid;
    for (i = 0; i < xv_property_count; ++i) {
        const XvProperty *property = xv_properties + i;
        value = xlValueReadFindV1(&object,
            (xstrview){property->key, strlen(property->key)});
        if (xrtGetError()) goto invalid;
        if (!xv_present(value)) continue;
        if (value.kind == XVALUE_ARRAY || value.kind == XVALUE_HANDLE) {
            /* A native array is a HANDLE until its resident policy is
             * authenticated. Never inspect its payload or create a snapshot. */
            if (!xlValueReadBeginV1(value, &array) || array.kind != XVALUE_ARRAY ||
                    array.count == 0 || property == xv_properties) goto invalid;
            for (size_t j = 0; j < array.count; ++j) {
                XL_ValueBorrowV1 item = xlValueReadAtV1(&array, j, NULL);
                if (xrtGetError() || !xv_emit_value(&output, property, item)) goto invalid;
            }
            if (!xlValueReadEndV1(&array)) goto invalid;
        } else if (!xv_emit_value(&output, property, value)) goto invalid;
    }
    if (!xv_literal(&output, "END:VCARD\r\n") ||
            !xrtBufferAppendByte(&output, 0)) goto invalid;
    result = xrtBufferTake(&output, NULL, NULL);
    if (result != NULL) goto cleanup;
invalid:
    if (xrtGetError() == NULL) xv_error("invalid vCard field or output exceeds 4 MiB");
cleanup:
    {
        /* End every successful scope, including OOM/error paths. Preserve the
         * original failure while cleanup operates on the borrowed owners. */
        xerror *failure = xrtTakeError();
        if (initialized) xrtBufferUnit(&output);
        int ended = xlValueReadEndV1(&array);
        if (!xlValueReadEndV1(&object)) ended = 0;
        if (!ended) { xrtFree(result); result = NULL; }
        if (failure) { xrtClearError(); xrtSetErrorTake(failure); }
    }
    return result;
}
