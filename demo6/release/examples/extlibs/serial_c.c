#include "../../extlibs/serial/include/serial.h"

#include <stdio.h>



int main(void)
{
    const unsigned char payload[] = {0, 255, 65};
    unsigned char large[5000];
    void* port = xserial_open("LOOPBACK", 115200, 8, 1, 0);
    xbuffer* received = NULL;
    int64_t written;
    int64_t available;
    size_t i;
    if (port == NULL) goto fail;
    printf("port: %s\n", xserial_get_port(port));
    printf("baudrate: %lld\n", (long long)xserial_get_baudrate(port));
    printf("open: %s\n", xserial_is_open(port) ? "true" : "false");

    /* C 与 xlang 使用相同原生接口，UTF-8 按字节传输。 */
    written = xserial_write_text(port, "\xE4\xBD\xA0\xE5\xA5\xBD");
    if (written != 6) goto fail;
    printf("text bytes: %lld\n", (long long)written);
    received = (xbuffer*)xserial_read(port, 6);
    if (received == NULL || received->Size != 6) goto fail;
    fputs("text: ", stdout);
    if (fwrite(received->Data, 1, received->Size, stdout) != received->Size) goto fail;
    putchar('\n');
    xrtBufferDestroy(received);
    received = NULL;

    /* 读取结果由调用方拥有，不借用 port 内部内存。 */
    if (xserial_write(port, payload, sizeof(payload)) != 3) goto fail;
    received = (xbuffer*)xserial_read(port, 3);
    if (received == NULL || received->Size != 3) goto fail;
    printf("binary: %zu,%u,%u,%u\n", received->Size,
        (unsigned)received->Data[0], (unsigned)received->Data[1], (unsigned)received->Data[2]);
    xrtBufferDestroy(received);
    received = NULL;

    if (xserial_write_text_len(port, "A\0B", 3) != 3) goto fail;
    received = (xbuffer*)xserial_read(port, 3);
    if (received == NULL || received->Size != 3 ||
        received->Data[0] != 65 || received->Data[1] != 0 || received->Data[2] != 66) goto fail;
    printf("nul text bytes: %zu\n", received->Size);
    xrtBufferDestroy(received);
    received = NULL;

    if (xserial_write_text(port, "abcd") != 4) goto fail;
    received = (xbuffer*)xserial_read(port, 2);
    if (received == NULL || received->Size != 2 || received->Data[0] != 'a') goto fail;
    xrtBufferDestroy(received);
    received = NULL;
    available = xserial_available(port);
    if (available != 2) goto fail;
    printf("pending: %lld\n", (long long)available);
    if (!xserial_flush(port)) goto fail;

    /* 满队列返回短写或零；调用方处理余下数据，而不是假定一次写完。 */
    for (i = 0; i < sizeof(large); ++i) large[i] = (unsigned char)i;
    written = xserial_write(port, large, sizeof(large));
    if (written != XSERIAL_LOOPBACK_CAPACITY) goto fail;
    printf("short write: %lld\n", (long long)written);
    written = xserial_write_text(port, "more");
    if (written != 0) goto fail;
    printf("full write: %lld\n", (long long)written);
    received = (xbuffer*)xserial_read(port, XSERIAL_IO_CHUNK);
    if (received == NULL || received->Size != XSERIAL_LOOPBACK_CAPACITY) goto fail;
    printf("drained: %zu\n", received->Size);
    xrtBufferDestroy(received);
    received = NULL;
    available = xserial_available(port);
    if (available != 0) goto fail;
    printf("after flush: %lld\n", (long long)available);
    xserial_close(port);
    puts("serial example: PASS");
    return 0;

fail:
    fputs("serial example: operation failed\n", stderr);
    xrtBufferDestroy(received);
    xserial_close(port);
    return 1;
}

/* 支持直接 xl run serial_c.c；不重复编译宿主已经提供的 XRT。 */
#include "../../extlibs/serial/serial_native.c"
