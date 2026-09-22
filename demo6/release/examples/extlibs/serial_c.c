#include "../../extlibs/serial/include/serial.h"

#include <assert.h>
#include <stdio.h>



int main(void)
{
    const unsigned char payload[] = {0, 255, 65};
    unsigned char large[5000];
    void* port = xserial_open("LOOPBACK", 115200, 8, 1, 0);
    xbuffer* received;
    size_t i;
    assert(port != NULL);
    printf("port: %s\n", xserial_get_port(port));
    printf("baudrate: %lld\n", (long long)xserial_get_baudrate(port));
    printf("open: %s\n", xserial_is_open(port) ? "true" : "false");

    /* C 与 xlang 使用相同原生接口，UTF-8 按字节传输。 */
    printf("text bytes: %lld\n", (long long)xserial_write_text(port, "\xE4\xBD\xA0\xE5\xA5\xBD"));
    received = (xbuffer*)xserial_read(port, 6);
    assert(received != NULL && received->Size == 6);
    fputs("text: ", stdout);
    assert(fwrite(received->Data, 1, received->Size, stdout) == received->Size);
    putchar('\n');
    xrtBufferDestroy(received);

    /* 读取结果由调用方拥有，不借用 port 内部内存。 */
    assert(xserial_write(port, payload, sizeof(payload)) == 3);
    received = (xbuffer*)xserial_read(port, 3);
    assert(received != NULL && received->Size == 3);
    printf("binary: %zu,%u,%u,%u\n", received->Size,
        (unsigned)received->Data[0], (unsigned)received->Data[1], (unsigned)received->Data[2]);
    xrtBufferDestroy(received);

    assert(xserial_write_text(port, "abcd") == 4);
    received = (xbuffer*)xserial_read(port, 2);
    assert(received != NULL && received->Size == 2 && received->Data[0] == 'a');
    xrtBufferDestroy(received);
    printf("pending: %lld\n", (long long)xserial_available(port));
    assert(xserial_flush(port));

    /* 满队列返回短写或零；调用方处理余下数据，而不是假定一次写完。 */
    for (i = 0; i < sizeof(large); ++i) large[i] = (unsigned char)i;
    printf("short write: %lld\n", (long long)xserial_write(port, large, sizeof(large)));
    printf("full write: %lld\n", (long long)xserial_write_text(port, "more"));
    received = (xbuffer*)xserial_read(port, XSERIAL_IO_CHUNK);
    assert(received != NULL);
    printf("drained: %zu\n", received->Size);
    xrtBufferDestroy(received);
    printf("after flush: %lld\n", (long long)xserial_available(port));
    xserial_close(port);
    puts("serial example: PASS");
    return 0;
}

/* 支持直接 xl run serial_c.c；不重复编译宿主已经提供的 XRT。 */
#include "../../extlibs/serial/serial_native.c"
