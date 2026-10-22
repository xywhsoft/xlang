#define _GNU_SOURCE
/* PTY 创建与读写在断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <poll.h>
#include <pty.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../include/serial.h"

static void check_pty(void)
{
    int master;
    int slave;
    char path[128];
    void* port;
    struct termios config;
    struct pollfd readable;
    struct pollfd slave_readable;
    struct timespec start;
    struct timespec finish;
    xbuffer* result;
    unsigned char data[4];

    assert(openpty(&master, &slave, path, NULL, NULL) == 0);
    port = xserial_open(path, 115200, 8, 1, 0);
    assert(port != NULL && xserial_is_open(port));
    assert(strcmp(xserial_get_port(port), path) == 0);
    assert(xserial_get_baudrate(port) == 115200);
    assert(tcgetattr(slave, &config) == 0);
    assert(cfgetispeed(&config) == B115200 && cfgetospeed(&config) == B115200);
    assert((config.c_lflag & (ICANON | ECHO)) == 0);
    assert((config.c_cflag & CSIZE) == CS8);
#ifdef TIOCGEXCL
    int exclusive = 0;
    assert(ioctl(slave, TIOCGEXCL, &exclusive) == 0 && exclusive == 1);
#endif

    /* 主端与串口 API 双向传输二进制，包含 NUL 和高位字节。 */
    assert(write(master, "\0\377A\n", 4) == 4);
    slave_readable.fd = slave;
    slave_readable.events = POLLIN;
    slave_readable.revents = 0;
    assert(poll(&slave_readable, 1, 1000) == 1);
    assert(xserial_available(port) == 4);
    result = (xbuffer*)xserial_read(port, 4);
    assert(result != NULL && result->Size == 4);
    assert(memcmp(result->Data, "\0\377A\n", 4) == 0);
    xrtBufferDestroy(result);
    assert(xserial_write(port, "\0\377B\n", 4) == 4);
    readable.fd = master;
    readable.events = POLLIN;
    readable.revents = 0;
    assert(poll(&readable, 1, 1000) == 1);
    assert(read(master, data, sizeof(data)) == 4);
    assert(memcmp(data, "\0\377B\n", 4) == 0);

    result = (xbuffer*)xserial_read(port, 1);
    assert(result != NULL && result->Size == 0);
    xrtBufferDestroy(result);
    assert(xserial_set_timeouts(port, 150, 1000));
    assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    result = (xbuffer*)xserial_read(port, 1);
    assert(clock_gettime(CLOCK_MONOTONIC, &finish) == 0);
    assert(result != NULL && result->Size == 0);
    assert((finish.tv_sec - start.tv_sec) * 1000 +
        (finish.tv_nsec - start.tv_nsec) / 1000000 >= 100);
    xrtBufferDestroy(result);
    assert(xserial_set_timeouts(port, 0, 1000));
    result = (xbuffer*)xserial_read(port, 1);
    assert(result != NULL && result->Size == 0);
    xrtBufferDestroy(result);
    assert(xserial_set_timeouts(port, 50, 1000));
    assert(write(master, "old", 3) == 3);
    slave_readable.revents = 0;
    assert(poll(&slave_readable, 1, 1000) == 1);
    assert(xserial_flush(port));
    assert(xserial_available(port) == 0);
    close(master);
    assert(xserial_read(port, 1) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_IO);
    xrtClearError();
    xserial_close(port);
    close(slave);

    assert(xserial_open(path, 12345, 8, 1, 0) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED);
    xrtClearError();
    assert(xserial_open("/dev/null", 9600, 8, 1, 0) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED);
    xrtClearError();
}

static void check_write_backpressure(void)
{
    int master;
    int slave;
    char path[128];
    void* port;
    unsigned char block[4096] = {0};
    int i;
    int stopped = 0;

    assert(openpty(&master, &slave, path, NULL, NULL) == 0);
    port = xserial_open(path, 115200, 8, 1, 0);
    assert(port != NULL && xserial_set_timeouts(port, 50, 0));
    for (i = 0; i < 1024; ++i) {
        int64_t written = xserial_write(port, block, sizeof(block));
        assert(written >= 0 && written <= (int64_t)sizeof(block));
        if (written < (int64_t)sizeof(block)) {
            stopped = 1;
            break;
        }
    }
    assert(stopped);
    xserial_close(port);
    close(master);
    close(slave);
}

int main(void)
{
    check_pty();
    check_write_backpressure();
    puts("serial POSIX PTY: PASS");
    return 0;
}
