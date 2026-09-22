#define XRT_MODULE_BUFFER
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 输出与 xlang 示例一致的检查结果，便于发布门禁直接比较。 */
static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}



/* 比较缓冲有效区和零结尾文本。 */
static int buffer_equals(const xbuffer* buffer, const char* text)
{
    size_t length;

    if ( (buffer == NULL) || (text == NULL) ) {
        return 0;
    }
    length = strlen(text);
    return (buffer->Size == length) &&
        ((length == 0) ||
         ((buffer->Data != NULL) &&
          (memcmp(buffer->Data, text, length) == 0)));
}



/* 演示连续字节缓冲的创建、增长、清空和插入契约。 */
int main(void)
{
    xbuffer* buffer = xrtBufferCreate();
    xbuffer* stepped = xrtBufferCreate();
    xbuffer* inserted = xrtBufferCreate();

    require_line(
        "basic",
        (buffer != NULL) &&
        (buffer->Size == 0) &&
        xrtBufferAppend(buffer, XRT_BYTES_LITERAL("abc")) &&
        xrtBufferAppendByte(buffer, 100) &&
        (buffer->Size == 4) &&
        (buffer->Data[0] == 97) &&
        (buffer->Data[1] == 98) &&
        ((buffer->Data[1] = 66), 1) &&
        buffer_equals(buffer, "aBcd")
    );

    require_line(
        "reserve_clear",
        xrtBufferReserve(buffer, 128) &&
        (buffer->Capacity >= 128) &&
        ((xrtBufferClear(buffer)),
         (buffer->Size == 0) && buffer_equals(buffer, ""))
    );

    require_line(
        "append_bytes",
        (stepped != NULL) &&
        xrtBufferReserve(stepped, 64) &&
        xrtBufferAppend(
            stepped,
            (xbytesview){(const unsigned char*)"hello", 2}
        ) &&
        buffer_equals(stepped, "he") &&
        /* 零长度追加是成功的 no-op。 */
        xrtBufferAppend(stepped, (xbytesview){NULL, 0}) &&
        (stepped->Size == 2)
    );

    require_line(
        "insert",
        (inserted != NULL) &&
        xrtBufferAppend(inserted, XRT_BYTES_LITERAL("ad")) &&
        xrtBufferInsert(inserted, 1, XRT_BYTES_LITERAL("bc")) &&
        buffer_equals(inserted, "abcd") &&
        xrtBufferInsert(inserted, 0, xrtBufferView(stepped)) &&
        buffer_equals(inserted, "heabcd")
    );

    xrtBufferDestroy(buffer);
    xrtBufferDestroy(stepped);
    xrtBufferDestroy(inserted);
    return 0;
}
