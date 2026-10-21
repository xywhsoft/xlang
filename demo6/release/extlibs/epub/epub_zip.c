/* EPUB 专用 stored ZIP：mimetype 必须首位、无 extra；所有偏移先检查再读取。 */
#include "epub_internal.h"
#include "include/epub_native.h"

#include <string.h>

int xep_append(xbuffer *out, const void *data, size_t size) {
    if (out->Size > XEP_LIMIT || size > XEP_LIMIT - out->Size) return 0;
    return xrtBufferAppend(out, (xbytesview){(cbytes)data, size});
}

int xep_literal(xbuffer *out, const char *text) {
    return xep_append(out, text, strlen(text));
}

static int xep_u16(xbuffer *out, uint32 value) {
    unsigned char bytes[2] = {(unsigned char)value, (unsigned char)(value >> 8)};
    return xep_append(out, bytes, sizeof(bytes));
}

static int xep_u32(xbuffer *out, uint32 value) {
    unsigned char bytes[4] = {(unsigned char)value, (unsigned char)(value >> 8),
        (unsigned char)(value >> 16), (unsigned char)(value >> 24)};
    return xep_append(out, bytes, sizeof(bytes));
}

static uint16 xep_get16(const unsigned char *data) {
    return (uint16)((uint16)data[0] | ((uint16)data[1] << 8));
}

static uint32 xep_get32(const unsigned char *data) {
    return (uint32)data[0] | ((uint32)data[1] << 8) |
        ((uint32)data[2] << 16) | ((uint32)data[3] << 24);
}

/* 每个归档最多 8 MiB；按字节计算避免初始化全局可变 CRC 表。 */
static uint32 xep_crc(const unsigned char *data, size_t size) {
    uint32 value = 0xffffffffu;
    size_t i;
    for (i = 0; i < size; ++i) {
        unsigned j;
        value ^= data[i];
        for (j = 0; j < 8; ++j) {
            value = (value >> 1) ^ ((value & 1u) ? 0xedb88320u : 0);
        }
    }
    return ~value;
}

int xep_archive_add(XepArchive *archive, const char *name,
    const void *data, size_t size) {
    xbuffer *out = &archive->data;
    XepEntry *entry;
    size_t name_size = strlen(name);
    if (archive->count >= XEP_MAX_ENTRIES || name_size == 0 ||
        name_size >= sizeof(archive->entries[0].name) || size > XEP_LIMIT ||
        data == NULL || out->Size > XEP_LIMIT) return 0;
    entry = &archive->entries[archive->count];
    memcpy(entry->name, name, name_size + 1);
    entry->size = (uint32)size;
    entry->offset = (uint32)out->Size;
    entry->crc = xep_crc((const unsigned char *)data, size);
    if (!xep_literal(out, "PK\x03\x04") || !xep_u16(out, 20) ||
        !xep_u16(out, 0) || !xep_u16(out, 0) ||
        !xep_u16(out, 0) || !xep_u16(out, 0x5821) ||
        !xep_u32(out, entry->crc) || !xep_u32(out, entry->size) ||
        !xep_u32(out, entry->size) || !xep_u16(out, (uint32)name_size) ||
        !xep_u16(out, 0) || !xep_append(out, name, name_size) ||
        !xep_append(out, data, size)) return 0;
    ++archive->count;
    return 1;
}

int xep_archive_finish(XepArchive *archive) {
    xbuffer *out = &archive->data;
    size_t start = out->Size, i, size;
    for (i = 0; i < archive->count; ++i) {
        const XepEntry *entry = &archive->entries[i];
        size_t name_size = strlen(entry->name);
        if (!xep_literal(out, "PK\x01\x02") ||
            !xep_u16(out, 20) || !xep_u16(out, 20) ||
            !xep_u16(out, 0) || !xep_u16(out, 0) ||
            !xep_u16(out, 0) || !xep_u16(out, 0x5821) ||
            !xep_u32(out, entry->crc) || !xep_u32(out, entry->size) ||
            !xep_u32(out, entry->size) || !xep_u16(out, (uint32)name_size) ||
            !xep_u16(out, 0) || !xep_u16(out, 0) ||
            !xep_u16(out, 0) || !xep_u16(out, 0) ||
            !xep_u32(out, 0) || !xep_u32(out, entry->offset) ||
            !xep_append(out, entry->name, name_size)) return 0;
    }
    size = out->Size - start;
    return xep_literal(out, "PK\x05\x06") &&
        xep_u16(out, 0) && xep_u16(out, 0) &&
        xep_u16(out, (uint32)archive->count) &&
        xep_u16(out, (uint32)archive->count) &&
        xep_u32(out, (uint32)size) && xep_u32(out, (uint32)start) &&
        xep_u16(out, 0);
}

void *xep_read_entry(const char *path, const char *name) {
    unsigned char *zip = NULL;
    char *result = NULL;
    size_t size, pos, end, directory_start, i;
    uint16 count;
    xrtClearError();
    if (path == NULL || name == NULL || name[0] == 0 ||
        strlen(name) >= sizeof(((XepEntry *)0)->name)) goto invalid;
    zip = xrtFileReadAllLimit(path, XEP_LIMIT, &size);
    if (zip == NULL || size < 22 || memcmp(zip + size - 22, "PK\x05\x06", 4) != 0 ||
        xep_get16(zip + size - 2) != 0 ||
        xep_get16(zip + size - 22 + 4) != 0 ||
        xep_get16(zip + size - 22 + 6) != 0 ||
        xep_get16(zip + size - 22 + 8) != xep_get16(zip + size - 22 + 10))
        goto invalid;
    count = xep_get16(zip + size - 22 + 10);
    pos = xep_get32(zip + size - 22 + 16);
    end = xep_get32(zip + size - 22 + 12);
    if (count == 0 || count > XEP_MAX_ENTRIES || pos > size - 22 ||
        end != size - 22 - pos) goto invalid;
    end += pos;
    directory_start = pos;
    for (i = 0; i < count; ++i) {
        size_t n, extra, comment, next;
        if (end - pos < 46 || memcmp(zip + pos, "PK\x01\x02", 4) != 0) goto invalid;
        if (xep_get16(zip + pos + 34) != 0) goto invalid;
        n = xep_get16(zip + pos + 28);
        extra = xep_get16(zip + pos + 30);
        comment = xep_get16(zip + pos + 32);
        if (n > end - pos - 46 || extra > end - pos - 46 - n ||
            comment > end - pos - 46 - n - extra) goto invalid;
        next = pos + 46 + n + extra + comment;
        if (n == strlen(name) && memcmp(zip + pos + 46, name, n) == 0) {
            size_t local = xep_get32(zip + pos + 42), len, data_start;
            uint32 crc = xep_get32(zip + pos + 16);
            if (result != NULL) goto invalid;
            len = xep_get32(zip + pos + 24);
            if (xep_get16(zip + pos + 8) != 0 ||
                xep_get16(zip + pos + 10) != 0 ||
                xep_get32(zip + pos + 20) != len ||
                local >= directory_start || directory_start - local < 30 ||
                memcmp(zip + local, "PK\x03\x04", 4) != 0 ||
                xep_get16(zip + local + 6) != 0 ||
                xep_get16(zip + local + 8) != 0 ||
                xep_get32(zip + local + 14) != crc ||
                xep_get32(zip + local + 18) != len ||
                xep_get32(zip + local + 22) != len ||
                xep_get16(zip + local + 26) != n) goto invalid;
            extra = xep_get16(zip + local + 28);
            if (n > directory_start - local - 30 ||
                extra > directory_start - local - 30 - n ||
                memcmp(zip + local + 30, name, n) != 0) goto invalid;
            data_start = local + 30 + n + extra;
            if (len > directory_start - data_start ||
                memchr(zip + data_start, 0, len) != NULL ||
                !xrtUtf8Valid((xstrview){(char *)zip + data_start, len}, NULL) ||
                xep_crc(zip + data_start, len) != crc) goto invalid;
            result = xrtMalloc(len + 1);
            if (result == NULL) goto invalid;
            memcpy(result, zip + data_start, len);
            result[len] = 0;
        }
        pos = next;
    }
    if (pos != end) goto invalid;
    if (result == NULL) {
        result = xrtMalloc(1);
        if (result == NULL) goto invalid;
        result[0] = 0;
    }
    xrtFree(zip);
    return result;
invalid:
    xrtFree(result);
    xrtFree(zip);
    if (xrtGetError() == NULL) {
        xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.epub", 0,
            "invalid or unsupported EPUB ZIP entry");
    }
    return NULL;
}
