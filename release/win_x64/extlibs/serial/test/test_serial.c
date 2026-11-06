#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#include "../include/serial.h"

/* 测试的操作与断言不可在 -DNDEBUG 构建中消失。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>



static void check_bytes(void)
{
    unsigned char bytes[5000];
    void* port = xserial_open("LOOPBACK", 115200, 8, 1, 0);
    xbuffer* first;
    xbuffer* second;
    size_t i;
    assert(port != NULL && xserial_is_open(port));
    assert(xserial_get_baudrate(port) == 115200);
    assert(strcmp(xserial_get_port(port), "LOOPBACK") == 0);
    for (i = 0; i < sizeof(bytes); ++i) bytes[i] = (unsigned char)i;

    /* 满队列必须短写，4096 字节读取不需要也不允许补终止符。 */
    assert(xserial_write(port, bytes, sizeof(bytes)) == 4096);
    assert(xserial_write(port, bytes, 1) == 0);
    assert(xserial_available(port) == 4096);
    first = (xbuffer*)xserial_read(port, 0);
    assert(first != NULL && first->Size == 0 && xserial_available(port) == 4096);
    xrtBufferDestroy(first);
    first = (xbuffer*)xserial_read(port, LLONG_MAX);
    assert(first != NULL && first->Size == 4096);
    assert(memcmp(first->Data, bytes, 4096) == 0);
    assert(xserial_available(port) == 0);

    assert(xserial_write(port, bytes, 5) == 5);
    second = (xbuffer*)xserial_read(port, 2);
    assert(second != NULL && second->Size == 2 && second->Data[1] == 1);
    assert(xserial_available(port) == 3);
    assert(xserial_flush(port) && xserial_available(port) == 0);
    xserial_close(port);
    /* 读取结果独立拥有，另一次读取或源对象析构不能使旧结果失效。 */
    assert(first->Data[4095] == 255 && second->Data[0] == 0);
    xrtBufferDestroy(first);
    xrtBufferDestroy(second);

    port = xserial_open("SIM", 9600, 8, 1, 0);
    assert(port != NULL);
    assert(xserial_write_text(port, "\xE4\xBD\xA0\xE5\xA5\xBD") == 6);
    first = (xbuffer*)xserial_read(port, 6);
    assert(first != NULL && first->Size == 6);
    assert(memcmp(first->Data, "\xE4\xBD\xA0\xE5\xA5\xBD", 6) == 0);
    xrtBufferDestroy(first);
    assert(xserial_write(port, NULL, 0) == 0);
    first = (xbuffer*)xserial_read(port, 1);
    assert(first != NULL && first->Size == 0);
    xrtBufferDestroy(first);
    assert(xserial_write_text_len(port, "A\0B", 3) == 3);
    first = (xbuffer*)xserial_read(port, 3);
    assert(first != NULL && first->Size == 3);
    assert(memcmp(first->Data, "A\0B", 3) == 0);
    xrtBufferDestroy(first);
    xserial_close(port);
}



static void check_errors(void)
{
    void* port = xserial_open("LOOPBACK", 9600, 8, 1, 0);
    xbuffer* first;
    char long_name[8192];
    assert(port != NULL && xserial_write_text(port, "keep") == 4);
    assert(xserial_set_timeouts(port, 0, 60000));
    assert(!xserial_set_timeouts(port, -1, 1000));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!xserial_set_timeouts(port, 50, 60001));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(xserial_set_timeouts(port, 50, 1000) && xrtGetError() == NULL);
    assert(xserial_read(port, -1) == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xserial_read(port, LLONG_MIN) == NULL);
    assert(xserial_available(port) == 4 && xrtGetError() == NULL);
    assert(xserial_write(port, NULL, 1) == -1);
    first = (xbuffer*)xserial_read(port, 0);
    assert(first != NULL && first->Size == 0 && xrtGetError() == NULL);
    xrtBufferDestroy(first);
    assert(xserial_write_text(port, NULL) == -1);
    assert(xserial_flush(port) && xrtGetError() == NULL);
    assert(xserial_available(port) == 0 && xrtGetError() == NULL);
    xserial_close(port);
    assert(xserial_read(NULL, 0) == NULL);
    assert(xserial_write(NULL, NULL, 0) == -1);
    assert(xserial_available(NULL) == -1 && !xserial_flush(NULL));
    assert(!xserial_set_timeouts(NULL, 50, 1000));
    assert(!xserial_is_open(NULL));
    xserial_close(NULL);

    assert(xserial_open(NULL, 9600, 8, 1, 0) == NULL);
    assert(xserial_open("", 9600, 8, 1, 0) == NULL);
    assert(xserial_open("LOOPBACK", 0, 8, 1, 0) == NULL);
    assert(xserial_open("LOOPBACK", LLONG_MAX, 8, 1, 0) == NULL);
    assert(xserial_open("LOOPBACK", 9600, 4, 1, 0) == NULL);
    assert(xserial_open("LOOPBACK", 9600, 9, 1, 0) == NULL);
    assert(xserial_open("LOOPBACK", 9600, 8, 3, 0) == NULL);
    assert(xserial_open("LOOPBACK", 9600, 8, 1, 3) == NULL);
    memset(long_name, 'x', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    assert(xserial_open(long_name, 9600, 8, 1, 0) == NULL);
    assert(xrtGetError() != NULL && xserial_error()[0] != '\0');
    port = xserial_open("LOOPBACK", 9600, 8, 1, 0);
    assert(port != NULL && xrtGetError() == NULL);
    xserial_close(port);
}



typedef struct writer_args { void* Port; unsigned char Byte; } writer_args;

static int32 write_thread(void* data)
{
    writer_args* args = (writer_args*)data;
    unsigned int i;
    for (i = 0; i < 1000; ++i)
        if (xserial_write(args->Port, &args->Byte, 1) != 1) return 1;
    /* 原生错误留在当前线程，不污染调用方。 */
    assert(xserial_read(args->Port, -1) == NULL);
    assert(xrtGetError() != NULL);
    return 0;
}



static void check_threads(void)
{
    void* port = xserial_open("LOOPBACK", 9600, 8, 1, 0);
    writer_args a = {port, 'a'};
    writer_args b = {port, 'b'};
    xthread* first;
    xthread* second;
    xbuffer* received;
    size_t count_a = 0;
    size_t count_b = 0;
    size_t i;
    assert(port != NULL);
    xrtClearError();
    first = xrtThreadCreate(write_thread, &a, 0);
    second = xrtThreadCreate(write_thread, &b, 0);
    assert(first != NULL && second != NULL);
    assert(xrtThreadWait(first) == XWAIT_OK && xrtThreadWait(second) == XWAIT_OK);
    assert(xrtThreadExitCode(first) == 0 && xrtThreadExitCode(second) == 0);
    xrtThreadDestroy(first);
    xrtThreadDestroy(second);
    assert(xrtGetError() == NULL);
    received = (xbuffer*)xserial_read(port, LLONG_MAX);
    assert(received != NULL && received->Size == 2000);
    for (i = 0; i < received->Size; ++i) {
        count_a += received->Data[i] == 'a';
        count_b += received->Data[i] == 'b';
    }
    assert(count_a == 1000 && count_b == 1000);
    xrtBufferDestroy(received);
    xserial_close(port);
}



static int32 stream_writer(void* port)
{
    unsigned char chunk[257];
    size_t offset = 0;
    while (offset < 65536) {
        size_t count = 65536 - offset;
        size_t i;
        int64_t written;
        if (count > sizeof(chunk)) count = sizeof(chunk);
        for (i = 0; i < count; ++i) chunk[i] = (unsigned char)(offset + i);
        written = xserial_write(port, chunk, count);
        if (written < 0) return 1;
        offset += (size_t)written;
        if (written == 0) xrtThreadYield();
    }
    return 0;
}



static int32 stream_reader(void* port)
{
    size_t offset = 0;
    while (offset < 65536) {
        xbuffer* chunk = (xbuffer*)xserial_read(port, 113);
        size_t i;
        bool empty;
        if (chunk == NULL) return 1;
        for (i = 0; i < chunk->Size; ++i) {
            if (chunk->Data[i] != (unsigned char)(offset + i)) {
                xrtBufferDestroy(chunk);
                return 1;
            }
        }
        offset += chunk->Size;
        empty = chunk->Size == 0;
        xrtBufferDestroy(chunk);
        if (empty) xrtThreadYield();
    }
    return offset == 65536 ? 0 : 1;
}



/* 总量超过队列容量，且读写块大小不同，验证短读/短写之间没有丢字节或乱序。 */
static void check_stream(void)
{
    void* port = xserial_open("LOOPBACK", 9600, 8, 1, 0);
    xthread* writer;
    xthread* reader;
    assert(port != NULL);
    writer = xrtThreadCreate(stream_writer, port, 0);
    reader = xrtThreadCreate(stream_reader, port, 0);
    assert(writer != NULL && reader != NULL);
    /* XRT 等待单位为毫秒，给调度与诊断构建保留完整的 10 秒预算。 */
    assert(xrtThreadWaitFor(writer, 10000) == XWAIT_OK);
    assert(xrtThreadWaitFor(reader, 10000) == XWAIT_OK);
    assert(xrtThreadExitCode(writer) == 0 && xrtThreadExitCode(reader) == 0);
    assert(xserial_available(port) == 0);
    xrtThreadDestroy(writer);
    xrtThreadDestroy(reader);
    xserial_close(port);
}



int main(void)
{
    check_bytes();
    check_errors();
    check_threads();
    check_stream();
    puts("serial C boundaries: PASS");
    return 0;
}
