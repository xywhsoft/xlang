/* PKCS 常用 DER 构件：编码与校验交给 XRT ASN.1，文本结果由调用方持有。 */
#ifndef XRT_MODULE_ASN1_DER
#define XRT_MODULE_ASN1_DER
#endif
#ifndef XRT_MODULE_BUFFER_HEX
#define XRT_MODULE_BUFFER_HEX
#endif
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"
#include "include/pkcs_native.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define XPKCS_DOMAIN "xlang.extlib.pkcs"

static void xpkcs_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, XPKCS_DOMAIN, 0, message);
}

static xstrview xpkcs_view(const char* text)
{
    return (xstrview){text, text != NULL ? strlen(text) : 0u};
}

static xbuffer* xpkcs_hex(const char* hex, bool allow_empty)
{
    xbuffer* bytes;
    if (hex == NULL) {
        xpkcs_error(XERR_ARGUMENT, "DER hex text is null");
        return NULL;
    }
    bytes = xrtBufferFromHex(xpkcs_view(hex), 0u);
    if (bytes == NULL) return NULL;
    if (!allow_empty && bytes->Size == 0u) {
        xrtBufferDestroy(bytes);
        xpkcs_error(XERR_VALUE, "DER value is empty");
        return NULL;
    }
    return bytes;
}

static xbuffer* xpkcs_one(const char* hex, xdervalue* value)
{
    xbuffer* bytes = xpkcs_hex(hex, false);
    xdercursor cursor;
    if (bytes == NULL) return NULL;
    if (!xrtDerValidate(bytes->Data, bytes->Size) ||
        !xrtDerInit(&cursor, bytes->Data, bytes->Size) ||
        xrtDerRead(&cursor, value) != XDER_VALUE || !xrtDerDone(&cursor)) {
        xrtBufferDestroy(bytes);
        xpkcs_error(XERR_VALUE, "input must contain one canonical DER value");
        return NULL;
    }
    return bytes;
}

static void* xpkcs_result(const xbuffer* bytes)
{
    return xrtHexEncodeNew(bytes->Data, bytes->Size, 0u);
}

int64_t xpkcs_get_tag(const char* hex)
{
    xdervalue value;
    xbuffer* bytes = xpkcs_one(hex, &value);
    int64_t tag;
    if (bytes == NULL) return -1;
    tag = value.Raw.Data[0];
    xrtBufferDestroy(bytes);
    xrtClearError();
    return tag;
}

int64_t xpkcs_get_length(const char* hex)
{
    xdervalue value;
    xbuffer* bytes = xpkcs_one(hex, &value);
    int64_t length;
    if (bytes == NULL) return -1;
    if (value.Value.Size > (size_t)INT64_MAX) {
        xrtBufferDestroy(bytes);
        xpkcs_error(XERR_RANGE, "DER content length exceeds int range");
        return -1;
    }
    length = (int64_t)value.Value.Size;
    xrtBufferDestroy(bytes);
    xrtClearError();
    return length;
}

void* xpkcs_get_content(const char* hex)
{
    xdervalue value;
    xbuffer* bytes = xpkcs_one(hex, &value);
    char* result;
    if (bytes == NULL) return NULL;
    result = xrtHexEncodeNew(value.Value.Data, value.Value.Size, 0u);
    xrtBufferDestroy(bytes);
    if (result != NULL) xrtClearError();
    return result;
}

void* xpkcs_encode_int(int64_t number)
{
    xbuffer der = {0};
    char* result;
    if (!xrtBufferInit(&der) || !xrtDerAppendInt64(&der, number)) {
        xrtBufferUnit(&der);
        return NULL;
    }
    result = xpkcs_result(&der);
    xrtBufferUnit(&der);
    if (result != NULL) xrtClearError();
    return result;
}

int xpkcs_decode_int(const char* hex, int64_t* output)
{
    xdervalue value;
    xbuffer* bytes;
    int64_t number;
    if (output == NULL) {
        xpkcs_error(XERR_ARGUMENT, "integer output is null");
        return 0;
    }
    bytes = xpkcs_one(hex, &value);
    if (bytes == NULL) return 0;
    if (!xrtDerInt64(&value, &number)) {
        xrtBufferDestroy(bytes);
        return 0;
    }
    *output = number;
    xrtBufferDestroy(bytes);
    xrtClearError();
    return 1;
}

xvalue* xpkcs_decode_int_value(const char* hex)
{
    int64_t number;
    return xpkcs_decode_int(hex, &number) ? xrtValueInt(number) : NULL;
}

void* xpkcs_encode_oid(const char* dotted)
{
    xbuffer der = {0};
    char* result;
    if (dotted == NULL) {
        xpkcs_error(XERR_ARGUMENT, "OID text is null");
        return NULL;
    }
    if (!xrtBufferInit(&der) || !xrtDerAppendOid(&der, xpkcs_view(dotted))) {
        xrtBufferUnit(&der);
        return NULL;
    }
    result = xpkcs_result(&der);
    xrtBufferUnit(&der);
    if (result != NULL) xrtClearError();
    return result;
}

void* xpkcs_decode_oid(const char* hex)
{
    xdervalue value;
    xbytesview oid;
    xbuffer text = {0};
    xbuffer* bytes = xpkcs_one(hex, &value);
    char* result;
    if (bytes == NULL) return NULL;
    if (!xrtDerOid(&value, &oid) || !xrtBufferInit(&text) ||
        !xrtDerOidDecode(oid, &text) || !xrtBufferAppendByte(&text, 0)) {
        xrtBufferDestroy(bytes);
        xrtBufferUnit(&text);
        return NULL;
    }
    result = (char*)xrtBufferTake(&text, NULL, NULL);
    xrtBufferUnit(&text);
    xrtBufferDestroy(bytes);
    if (result != NULL) xrtClearError();
    return result;
}

static void* xpkcs_wrap(const char* hex, uint32 tag)
{
    xbuffer* content = xpkcs_hex(hex, true);
    xbuffer der = {0};
    char* result;
    if (content == NULL) return NULL;
    if (!xrtBufferInit(&der)) {
        xrtBufferDestroy(content);
        return NULL;
    }
    if (!xrtDerAppend(&der, XASN1_UNIVERSAL, tag, true, xrtBufferView(content))) {
        const xerror* error = xrtGetError();
        bool preserve = error != NULL &&
            (xrtErrorKind(error) == XERR_MEMORY || xrtErrorKind(error) == XERR_RANGE);
        xrtBufferDestroy(content);
        xrtBufferUnit(&der);
        if (!preserve) {
            xrtClearError();
            xpkcs_error(XERR_VALUE, "constructed DER content is not canonical");
        }
        return NULL;
    }
    if (!xrtDerValidate(der.Data, der.Size)) {
        xrtBufferDestroy(content);
        xrtBufferUnit(&der);
        xrtClearError();
        xpkcs_error(XERR_VALUE, "constructed DER content is not canonical");
        return NULL;
    }
    result = xpkcs_result(&der);
    xrtBufferUnit(&der);
    xrtBufferDestroy(content);
    if (result != NULL) xrtClearError();
    return result;
}

void* xpkcs_wrap_sequence(const char* hex)
{
    return xpkcs_wrap(hex, XASN1_SEQUENCE);
}

void* xpkcs_wrap_set(const char* hex)
{
    return xpkcs_wrap(hex, XASN1_SET);
}

void* xpkcs_encode_octet(const char* hex)
{
    xbuffer* content = xpkcs_hex(hex, true);
    xbuffer der = {0};
    char* result;
    if (content == NULL) return NULL;
    if (!xrtBufferInit(&der) || !xrtDerAppendOctets(&der, xrtBufferView(content))) {
        xrtBufferDestroy(content);
        xrtBufferUnit(&der);
        return NULL;
    }
    result = xpkcs_result(&der);
    xrtBufferUnit(&der);
    xrtBufferDestroy(content);
    if (result != NULL) xrtClearError();
    return result;
}

void* xpkcs_encode_null(void)
{
    xbuffer der = {0};
    char* result;
    if (!xrtBufferInit(&der) || !xrtDerAppendNull(&der)) {
        xrtBufferUnit(&der);
        return NULL;
    }
    result = xpkcs_result(&der);
    xrtBufferUnit(&der);
    if (result != NULL) xrtClearError();
    return result;
}

void* xpkcs_encode_bitstring(const char* hex)
{
    xbuffer* content = xpkcs_hex(hex, true);
    xbuffer der = {0};
    char* result;
    if (content == NULL) return NULL;
    if (!xrtBufferInit(&der) ||
        !xrtDerAppendBitString(&der, xrtBufferView(content), 0u)) {
        xrtBufferDestroy(content);
        xrtBufferUnit(&der);
        return NULL;
    }
    result = xpkcs_result(&der);
    xrtBufferUnit(&der);
    xrtBufferDestroy(content);
    if (result != NULL) xrtClearError();
    return result;
}

void* xpkcs_concat(const char* left, const char* right)
{
    xbuffer* a = xpkcs_hex(left, true);
    xbuffer* b;
    xbuffer joined = {0};
    char* result = NULL;
    if (a == NULL) return NULL;
    b = xpkcs_hex(right, true);
    if (b != NULL && xrtBufferInit(&joined) &&
        xrtBufferAppend(&joined, xrtBufferView(a)) &&
        xrtBufferAppend(&joined, xrtBufferView(b)))
        result = xpkcs_result(&joined);
    xrtBufferUnit(&joined);
    xrtBufferDestroy(b);
    xrtBufferDestroy(a);
    if (result != NULL) xrtClearError();
    return result;
}

int64_t xpkcs_der_len(const char* hex)
{
    xdervalue value;
    xbuffer* bytes = xpkcs_one(hex, &value);
    int64_t length;
    if (bytes == NULL) return -1;
    if (bytes->Size > (size_t)INT64_MAX) {
        xrtBufferDestroy(bytes);
        xpkcs_error(XERR_RANGE, "DER byte length exceeds int range");
        return -1;
    }
    length = (int64_t)bytes->Size;
    xrtBufferDestroy(bytes);
    xrtClearError();
    return length;
}

int64_t xpkcs_next_offset(const char* hex, int64_t offset)
{
    xbuffer* bytes = xpkcs_hex(hex, false);
    size_t position = 0;
    int64_t result = -1;
    if (bytes == NULL) return -1;
    if (offset < 0 || (uint64_t)offset >= bytes->Size) goto fail;
    while (position < bytes->Size) {
        xdercursor cursor;
        xdervalue value;
        if (!xrtDerInit(&cursor, bytes->Data + position, bytes->Size - position) ||
            xrtDerRead(&cursor, &value) != XDER_VALUE ||
            !xrtDerValidate(value.Raw.Data, value.Raw.Size)) goto fail;
        if (position == (size_t)offset) result = (int64_t)(position + value.Raw.Size);
        position += value.Raw.Size;
    }
    if (result < 0) goto fail;
    xrtBufferDestroy(bytes);
    xrtClearError();
    return result;
fail:
    xrtBufferDestroy(bytes);
    xpkcs_error(XERR_VALUE, "offset is not a DER value boundary");
    return -1;
}

void* xpkcs_tag_name(const char* hex)
{
    xdervalue value;
    xbuffer* bytes = xpkcs_one(hex, &value);
    const char* label = NULL;
    char fallback[48];
    char* result;
    if (bytes == NULL) return NULL;
    if (value.Tag.Class == XASN1_UNIVERSAL) {
        if (value.Tag.Constructed) {
            if (value.Tag.Number == XASN1_SEQUENCE) label = "SEQUENCE";
            if (value.Tag.Number == XASN1_SET) label = "SET";
        } else {
            switch (value.Tag.Number) {
            case XASN1_INTEGER: label = "INTEGER"; break;
            case XASN1_BIT_STRING: label = "BIT STRING"; break;
            case XASN1_OCTET_STRING: label = "OCTET STRING"; break;
            case XASN1_NULL: label = "NULL"; break;
            case XASN1_OBJECT_IDENTIFIER: label = "OID"; break;
            default: break;
            }
        }
    }
    if (label == NULL) {
        snprintf(fallback, sizeof(fallback), "TAG(%u)", (unsigned)value.Tag.Number);
        label = fallback;
    }
    result = xrtStrDup(label);
    xrtBufferDestroy(bytes);
    if (result != NULL) xrtClearError();
    return result;
}
