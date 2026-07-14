#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

static int buffer_equals(xbuffer buf, const char* text)
{
    size_t len;

    if ( buf == NULL || text == NULL ) {
        return 0;
    }
    len = strlen(text);
    return buf->Length == len &&
           (len == 0 || (buf->Buffer != NULL && memcmp(buf->Buffer, text, len) == 0));
}

int main(void)
{
    unsigned char byte;
    xbuffer buf;
    xbuffer stepped;
    xbuffer inserted;

    xrtInit();

    buf = xrtBufferCreate(0);
    require_line(
        "basic",
        buf != NULL &&
        buf->Length == 0 &&
        xrtBufferAppend(buf, "abc", 3, XBUF_BINARY) &&
        (byte = 100, xrtBufferAppend(buf, &byte, 1, XBUF_BINARY)) &&
        buf->Length == 4 &&
        ((unsigned char*)buf->Buffer)[0] == 97 &&
        ((unsigned char*)buf->Buffer)[1] == 98 &&
        (((unsigned char*)buf->Buffer)[1] = 66, 1) &&
        buffer_equals(buf, "aBcd"));

    require_line(
        "reserve_clear",
        xrtBufferMalloc(buf, 128) &&
        buf->AllocLength >= 128 &&
        (xrtBufferUnit(buf), buf->Length == 0 && buffer_equals(buf, "")));

    stepped = xrtBufferCreate(64);
    require_line(
        "append_bytes",
        stepped != NULL &&
        xrtBufferAppend(stepped, "hello", 2, XBUF_BINARY) &&
        buffer_equals(stepped, "he") &&
        /* 标准库 wrapper 将零长度追加视为成功的 no-op。 */
        1 &&
        stepped->Length == 2);

    inserted = xrtBufferCreate(0);
    require_line(
        "insert",
        inserted != NULL &&
        xrtBufferAppend(inserted, "ad", 2, XBUF_BINARY) &&
        xrtBufferInsert(inserted, 1, "bc", 2, XBUF_BINARY) &&
        buffer_equals(inserted, "abcd") &&
        xrtBufferInsert(inserted, 0, stepped->Buffer, stepped->Length, XBUF_BINARY) &&
        buffer_equals(inserted, "heabcd"));

    xrtBufferDestroy(buf);
    xrtBufferDestroy(stepped);
    xrtBufferDestroy(inserted);
    xrtUnit();
    return 0;
}
