#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_UNICODE
#include "include/memcached_native.h"
#include <string.h>

#define XMC_DOMAIN "xlang.extlib.memcached"
#define XMC_MAX_LINE 1024u
#define XMC_MAX_REPLY 67108864u
#define XMC_MAX_ITEMS 100000u

typedef struct {
    xstrview key;
    uint64 flags;
    size_t bytes;
    xstrview cas;
    bool hasCas;
} xmc_value_header;

static void xmc_error(const char *message)
{
    xrtSetErrorInfo(XERR_VALUE, XMC_DOMAIN, 0, message);
}

/* The wire format is length-framed inside VALUE, not NUL-terminated text. */
static int xmc_line(xbytesview data, size_t start, size_t *end)
{
    size_t i;
    if (start > data.Size) return 0;
    for (i = start; i + 1 < data.Size && i - start < XMC_MAX_LINE; ++i) {
        if (data.Data[i] == '\r' && data.Data[i + 1] == '\n') {
            *end = i;
            return 1;
        }
    }
    return data.Size - start > XMC_MAX_LINE ? -1 : 0;
}

static bool xmc_equal(const uint8 *text, size_t size, const char *word)
{
    size_t length = strlen(word);
    return size == length && memcmp(text, word, length) == 0;
}

static bool xmc_prefix(const uint8 *text, size_t size, const char *prefix)
{
    size_t length = strlen(prefix);
    return size >= length && memcmp(text, prefix, length) == 0;
}

static bool xmc_number(const uint8 *text, size_t size, uint64 *value)
{
    size_t i;
    uint64 number = 0;
    if (size == 0) return false;
    for (i = 0; i < size; ++i) {
        uint8 digit = text[i];
        if (digit < '0' || digit > '9' ||
            number > (UINT64_MAX - (digit - '0')) / 10) return false;
        number = number * 10 + digit - '0';
    }
    *value = number;
    return true;
}

static bool xmc_header(const uint8 *text, size_t size, xmc_value_header *out)
{
    size_t start = 6;
    size_t end;
    uint64 number;
    if (!xmc_prefix(text, size, "VALUE ")) return false;
    end = start;
    while (end < size && text[end] != ' ') ++end;
    if (end == start || end - start > 250 || end == size) return false;
    out->key = (xstrview){(const char *)text + start, end - start};
    for (start = 0; start < out->key.Size; ++start) {
        uint8 c = (uint8)out->key.Data[start];
        if (c <= 32 || c == 127) return false;
    }
    start = end + 1;
    end = start;
    while (end < size && text[end] != ' ') ++end;
    if (end == size || !xmc_number(text + start, end - start, &number) ||
        number > UINT32_MAX) return false;
    out->flags = number;
    start = end + 1;
    end = start;
    while (end < size && text[end] != ' ') ++end;
    if (!xmc_number(text + start, end - start, &number) ||
        number > XMC_MAX_REPLY) return false;
    out->bytes = (size_t)number;
    out->hasCas = end < size;
    if (out->hasCas) {
        if (!xmc_number(text + end + 1, size - end - 1, &number))
            return false;
        out->cas = (xstrview){(const char *)text + end + 1, size - end - 1};
    }
    return true;
}

static int64 xmc_scan(xbytesview data)
{
    size_t position = 0;
    size_t lineEnd;
    int line;
    bool values = false;
    bool stats = false;
    size_t items = 0;
    if (data.Size == 0) return 0;
    if (data.Data == NULL || data.Size > XMC_MAX_REPLY) return -1;
    while (true) {
        line = xmc_line(data, position, &lineEnd);
        if (line <= 0) return line;
        if (xmc_equal(data.Data + position, lineEnd - position, "END"))
            return (int64)(lineEnd + 2);
        if (xmc_prefix(data.Data + position, lineEnd - position, "VALUE ")) {
            xmc_value_header header;
            size_t body = lineEnd + 2;
            if (stats || ++items > XMC_MAX_ITEMS ||
                !xmc_header(data.Data + position,
                lineEnd - position, &header)) return -1;
            values = true;
            if (header.bytes > data.Size - body ||
                data.Size - body - header.bytes < 2) return 0;
            if (data.Data[body + header.bytes] != '\r' ||
                data.Data[body + header.bytes + 1] != '\n') return -1;
            position = body + header.bytes + 2;
            continue;
        }
        if (xmc_prefix(data.Data + position, lineEnd - position, "STAT ")) {
            if (values || ++items > XMC_MAX_ITEMS ||
                lineEnd - position <= 6) return -1;
            stats = true;
            position = lineEnd + 2;
            continue;
        }
        if (position != 0 || values || stats) return -1;
        if (xmc_equal(data.Data, lineEnd, "STORED") ||
            xmc_equal(data.Data, lineEnd, "NOT_STORED") ||
            xmc_equal(data.Data, lineEnd, "DELETED") ||
            xmc_equal(data.Data, lineEnd, "NOT_FOUND") ||
            xmc_equal(data.Data, lineEnd, "TOUCHED") ||
            xmc_equal(data.Data, lineEnd, "OK") ||
            xmc_equal(data.Data, lineEnd, "ERROR") ||
            xmc_prefix(data.Data, lineEnd, "VERSION ") ||
            xmc_prefix(data.Data, lineEnd, "CLIENT_ERROR ") ||
            xmc_prefix(data.Data, lineEnd, "SERVER_ERROR "))
            return (int64)(lineEnd + 2);
        return -1;
    }
}

int64 xmc_frame_size(xbytesview data)
{
    return xmc_scan(data);
}

static bool xmc_put(xvalue *object, const char *key, xvalue *value)
{
    return xrtValueObjectSetNew(object,
        (xstrview){key, strlen(key)}, value);
}

static bool xmc_put_text(xvalue *object, const char *key,
    const uint8 *data, size_t size)
{
    return xmc_put(object, key,
        xrtValueString((xstrview){(const char *)data, size}));
}

static bool xmc_put_word(xvalue *object, const char *key, const char *word)
{
    return xmc_put_text(object, key, (const uint8 *)word, strlen(word));
}

static bool xmc_put_number(xvalue *object, const char *key, uint64 number)
{
    return xmc_put(object, key, xrtValueInt((int64)number));
}

static bool xmc_put_data(xvalue *object, const uint8 *data, size_t size)
{
    xstrview text = {(const char *)data, size};
    xvalue *value = xrtUtf8Valid(text, NULL) ?
        xrtValueString(text) : xrtValueBytes((xbytesview){data, size});
    return xmc_put(object, "data", value);
}

static xvalue *xmc_parse_values(xbytesview data, xvalue *result)
{
    xvalue *items = xrtValueArray();
    size_t position = 0;
    if (items == NULL) return NULL;
    while (position < data.Size) {
        size_t lineEnd;
        xmc_value_header header;
        xvalue *item;
        if (xmc_line(data, position, &lineEnd) != 1) goto fail;
        if (xmc_equal(data.Data + position, lineEnd - position, "END")) break;
        if (!xmc_header(data.Data + position, lineEnd - position,
            &header)) goto fail;
        position = lineEnd + 2;
        item = xrtValueObject();
        if (item == NULL) goto fail;
        if (!xmc_put_text(item, "key", (const uint8 *)header.key.Data,
                header.key.Size) ||
            !xmc_put_number(item, "flags", header.flags) ||
            !xmc_put_number(item, "bytes", header.bytes) ||
            (header.hasCas && !xmc_put_text(item, "cas",
                (const uint8 *)header.cas.Data, header.cas.Size)) ||
            !xmc_put_data(item, data.Data + position, header.bytes)) {
            xrtValueRelease(item);
            goto fail;
        }
        if (!xrtValueArrayAppendNew(items, item)) goto fail;
        position += header.bytes + 2;
    }
    if (!xmc_put_word(result, "status", "value")) goto fail;
    if (!xmc_put(result, "items", items)) {
        items = NULL;
        goto fail;
    }
    return result;
fail:
    xrtValueRelease(items);
    return NULL;
}

static xvalue *xmc_parse_stats(xbytesview data, xvalue *result)
{
    xvalue *stats = xrtValueObject();
    size_t position = 0;
    if (stats == NULL) return NULL;
    while (position < data.Size) {
        size_t lineEnd;
        size_t nameEnd;
        char key[256];
        if (xmc_line(data, position, &lineEnd) != 1) goto fail;
        if (xmc_equal(data.Data + position, lineEnd - position, "END")) break;
        nameEnd = position + 5;
        while (nameEnd < lineEnd && data.Data[nameEnd] != ' ') ++nameEnd;
        if (nameEnd == position + 5 || nameEnd - position - 5 >= sizeof(key) ||
            nameEnd == lineEnd) goto fail;
        {
            size_t i;
            for (i = position + 5; i < nameEnd; ++i) {
                if (data.Data[i] <= 32 || data.Data[i] == 127) goto fail;
            }
        }
        memcpy(key, data.Data + position + 5, nameEnd - position - 5);
        key[nameEnd - position - 5] = '\0';
        if (!xmc_put_text(stats, key, data.Data + nameEnd + 1,
            lineEnd - nameEnd - 1)) goto fail;
        position = lineEnd + 2;
    }
    if (!xmc_put_word(result, "status", "stats")) goto fail;
    if (!xmc_put(result, "stats", stats)) {
        stats = NULL;
        goto fail;
    }
    return result;
fail:
    xrtValueRelease(stats);
    return NULL;
}

xvalue *xmc_parse_response_span(xbytesview data)
{
    int64 length = xmc_scan(data);
    xvalue *result;
    size_t end;
    const char *status = NULL;
    if (length < 1 || (size_t)length != data.Size ||
        xmc_line(data, 0, &end) != 1) {
        xmc_error("response is incomplete, malformed, or has trailing data");
        return xrtValueNull();
    }
    result = xrtValueObject();
    if (result == NULL) return NULL;
    if (xmc_prefix(data.Data, end, "VALUE ")) {
        if (xmc_parse_values(data, result) != NULL) return result;
    } else if (xmc_prefix(data.Data, end, "STAT ")) {
        if (xmc_parse_stats(data, result) != NULL) return result;
    } else {
        if (xmc_equal(data.Data, end, "STORED")) status = "stored";
        else if (xmc_equal(data.Data, end, "NOT_STORED")) status = "not_stored";
        else if (xmc_equal(data.Data, end, "DELETED")) status = "deleted";
        else if (xmc_equal(data.Data, end, "NOT_FOUND")) status = "not_found";
        else if (xmc_equal(data.Data, end, "TOUCHED")) status = "touched";
        else if (xmc_equal(data.Data, end, "OK")) status = "ok";
        else if (xmc_equal(data.Data, end, "END")) status = "end";
        else if (xmc_equal(data.Data, end, "ERROR")) status = "error";
        else if (xmc_prefix(data.Data, end, "VERSION ")) status = "version";
        else if (xmc_prefix(data.Data, end, "CLIENT_ERROR ")) status = "client_error";
        else if (xmc_prefix(data.Data, end, "SERVER_ERROR ")) status = "server_error";
        if (status != NULL && xmc_put_word(result, "status", status)) {
            if (strcmp(status, "version") == 0 &&
                xmc_put_text(result, "version", data.Data + 8, end - 8))
                return result;
            if (strcmp(status, "client_error") == 0 &&
                xmc_put_text(result, "message", data.Data + 13, end - 13))
                return result;
            if (strcmp(status, "server_error") == 0 &&
                xmc_put_text(result, "message", data.Data + 13, end - 13))
                return result;
            if (strcmp(status, "version") != 0 &&
                strcmp(status, "client_error") != 0 &&
                strcmp(status, "server_error") != 0) return result;
        }
    }
    xrtValueRelease(result);
    if (xrtGetError() == NULL) xmc_error("malformed response or allocation failed");
    return NULL;
}

int64 xmc_frame_size_abi(void *data, size_t size)
{
    return xmc_frame_size((xbytesview){data, size});
}

xvalue *xmc_parse_response_abi(void *data, size_t size)
{
    return xmc_parse_response_span((xbytesview){data, size});
}
