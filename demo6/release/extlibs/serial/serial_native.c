#include "include/serial.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif



/* 设备句柄与字节缓冲由 XRT 管理；配置完成后保持不变。 */
typedef struct xserial_port {
    xfile File;
    xmutex* Mutex;
    xbuffer Queue;
    str Name;
    int64_t Baudrate;
    bool Loopback;
#ifndef _WIN32
    int Fd;
#endif
} xserial_port;



static void serial_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, "xlang.extlib.serial", 0, message);
}



const char* xserial_error(void)
{
    const xerror* error = xrtGetError();
    return error != NULL ? xrtErrorMessage(error) : "";
}



static bool serial_lock(xserial_port* port)
{
    if (port == NULL) {
        serial_error(XERR_CLOSED, "serial: port is closed");
        return false;
    }
    return xrtMutexLock(port->Mutex);
}



#ifdef _WIN32
static bool serial_windows_error(const char* operation)
{
    xerrordesc error = {XERR_IO, 0, (int32)GetLastError(),
        "xlang.extlib.serial", operation, "serial: device operation failed", NULL, NULL};
    xrtSetErrorTake(xrtErrorBuild(&error));
    return false;
}



/* UTF-8 路径转换和句柄关闭统一交给 XRT，只在这里配置串口设备。 */
static bool serial_open_device(xserial_port* port,
    int64_t databits, int64_t stopbits, int64_t parity)
{
    xfileoptions options;
    str device = strncmp(port->Name, "\\\\.\\", 4) == 0
        ? xrtStrDup(port->Name)
        : xrtStrConcat(xrtStrView("\\\\.\\"), xrtStrView(port->Name));
    HANDLE handle;
    DCB config;
    COMMTIMEOUTS timeouts;

    if (device == NULL) return false;
    xrtFileOptionsInit(&options);
    options.Flags = XFILE_READ | XFILE_WRITE;
    options.Share = 0;
    port->File = xrtFileOpen(device, &options);
    xrtFree(device);
    if (port->File == NULL) return false;
    handle = (HANDLE)xrtFileNative(port->File);
    memset(&config, 0, sizeof(config));
    config.DCBlength = sizeof(config);
    if (!GetCommState(handle, &config)) return serial_windows_error("get-state");
    config.BaudRate = (DWORD)port->Baudrate;
    config.ByteSize = (BYTE)databits;
    config.StopBits = stopbits == 2 ? TWOSTOPBITS : ONESTOPBIT;
    config.Parity = parity == 1 ? ODDPARITY : parity == 2 ? EVENPARITY : NOPARITY;
    config.fBinary = TRUE;
    config.fParity = parity != 0;
    config.fOutxCtsFlow = FALSE;
    config.fOutxDsrFlow = FALSE;
    config.fDsrSensitivity = FALSE;
    config.fOutX = FALSE;
    config.fInX = FALSE;
    config.fErrorChar = FALSE;
    config.fNull = FALSE;
    config.fAbortOnError = FALSE;
    config.fDtrControl = DTR_CONTROL_ENABLE;
    config.fRtsControl = RTS_CONTROL_ENABLE;
    if (!SetCommState(handle, &config)) return serial_windows_error("set-state");
    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutConstant = 1000;
    if (!SetCommTimeouts(handle, &timeouts)) return serial_windows_error("set-timeouts");
    return true;
}
#endif



#ifndef _WIN32
static bool serial_posix_error(const char* operation)
{
    int code = errno;
    bool opening = strcmp(operation, "open") == 0;
    xerrordesc error = {XERR_IO, opening ? XFILE_ERROR_OPEN : 0, code,
        opening ? "xrt.file" : "xlang.extlib.serial", operation,
        "serial: device operation failed", NULL, NULL};
    xrtSetErrorTake(xrtErrorBuild(&error));
    return false;
}



static speed_t serial_posix_speed(int64_t baudrate)
{
    switch (baudrate) {
    case 50: return B50;
    case 75: return B75;
    case 110: return B110;
    case 134: return B134;
    case 150: return B150;
    case 200: return B200;
    case 300: return B300;
    case 600: return B600;
    case 1200: return B1200;
    case 1800: return B1800;
    case 2400: return B2400;
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
#ifdef B57600
    case 57600: return B57600;
#endif
#ifdef B115200
    case 115200: return B115200;
#endif
#ifdef B230400
    case 230400: return B230400;
#endif
#ifdef B460800
    case 460800: return B460800;
#endif
#ifdef B500000
    case 500000: return B500000;
#endif
#ifdef B576000
    case 576000: return B576000;
#endif
#ifdef B921600
    case 921600: return B921600;
#endif
#ifdef B1000000
    case 1000000: return B1000000;
#endif
#ifdef B1500000
    case 1500000: return B1500000;
#endif
#ifdef B2000000
    case 2000000: return B2000000;
#endif
#ifdef B2500000
    case 2500000: return B2500000;
#endif
#ifdef B3000000
    case 3000000: return B3000000;
#endif
#ifdef B3500000
    case 3500000: return B3500000;
#endif
#ifdef B4000000
    case 4000000: return B4000000;
#endif
    default: return (speed_t)0;
    }
}



/* 串口打开必须使用 O_NOCTTY/O_NONBLOCK；通用 XRT 文件选项不表达这两个设备语义。 */
static bool serial_open_device(xserial_port* port,
    int64_t databits, int64_t stopbits, int64_t parity)
{
    speed_t speed = serial_posix_speed(port->Baudrate);
    struct termios config;
    int fd;
    if (speed == (speed_t)0) {
        serial_error(XERR_UNSUPPORTED, "serial: unsupported POSIX baudrate");
        return false;
    }
    fd = open(port->Name, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return serial_posix_error("open");
    if (!isatty(fd)) {
        close(fd);
        serial_error(XERR_UNSUPPORTED, "serial: path is not a terminal device");
        return false;
    }
    if (tcgetattr(fd, &config) != 0) {
        (void)serial_posix_error("get-state");
        close(fd);
        return false;
    }
    config.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF);
    config.c_oflag &= ~OPOST;
    config.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    config.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
#ifdef CRTSCTS
    config.c_cflag &= ~CRTSCTS;
#endif
    config.c_cflag |= CREAD | CLOCAL;
    config.c_cflag |= databits == 5 ? CS5 : databits == 6 ? CS6 : databits == 7 ? CS7 : CS8;
    if (stopbits == 2) config.c_cflag |= CSTOPB;
    if (parity != 0) {
        config.c_cflag |= PARENB;
        config.c_iflag |= INPCK;
        if (parity == 1) config.c_cflag |= PARODD;
        else config.c_cflag &= ~PARODD;
    } else {
        config.c_iflag &= ~INPCK;
    }
    config.c_cc[VMIN] = 0;
    config.c_cc[VTIME] = 0;
    if (cfsetispeed(&config, speed) != 0 || cfsetospeed(&config, speed) != 0 ||
        tcsetattr(fd, TCSANOW, &config) != 0) {
        (void)serial_posix_error("set-state");
        close(fd);
        return false;
    }
#ifdef TIOCEXCL
    if (ioctl(fd, TIOCEXCL) != 0) {
        (void)serial_posix_error("exclusive");
        close(fd);
        return false;
    }
#endif
    port->Fd = fd;
    return true;
}



/* 非阻塞描述符配合 poll 给读/写明确的等待上限。 */
static int serial_posix_wait(int fd, short events, int timeout)
{
    struct pollfd request = {fd, events, 0};
    struct timespec now;
    int64_t deadline;
    int result;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        (void)serial_posix_error("clock");
        return -1;
    }
    deadline = (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000 + timeout;
    for (;;) {
        result = poll(&request, 1, timeout);
        if (result >= 0 || errno != EINTR) break;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
            (void)serial_posix_error("clock");
            return -1;
        }
        int64_t remaining = deadline - ((int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000);
        timeout = remaining > 0 ? (int)remaining : 0;
    }
    if (result < 0) { (void)serial_posix_error("poll"); return -1; }
    if (result == 0) return 0;
    if ((request.revents & (POLLERR | POLLHUP | POLLNVAL)) == 0 &&
        (request.revents & events) != 0) return 1;
    errno = EIO;
    (void)serial_posix_error("disconnected");
    return -1;
}
#endif



void* xserial_open(const char* name, int64_t baudrate,
    int64_t databits, int64_t stopbits, int64_t parity)
{
    xserial_port* port;
    if (name == NULL || name[0] == '\0' || baudrate <= 0 ||
        (uint64_t)baudrate > UINT32_MAX || databits < 5 || databits > 8 ||
        (stopbits != 1 && stopbits != 2) || parity < 0 || parity > 2) {
        serial_error(XERR_ARGUMENT, "serial: invalid port or serial configuration");
        return NULL;
    }
    port = (xserial_port*)xrtCalloc(1, sizeof(*port));
    if (port == NULL) return NULL;
#ifndef _WIN32
    port->Fd = -1;
#endif
    port->Baudrate = baudrate;
    port->Loopback = strcmp(name, "LOOPBACK") == 0 || strcmp(name, "SIM") == 0;
    port->Name = xrtStrDup(name);
    port->Mutex = xrtMutexCreate();
    if (port->Name == NULL || port->Mutex == NULL) {
        xserial_close(port);
        return NULL;
    }
    if (!port->Loopback) {
        if (!serial_open_device(port, databits, stopbits, parity)) {
            xserial_close(port);
            return NULL;
        }
    }
    return port;
}



/* 调用方必须先结束使用该句柄的操作；close 消费句柄，不允许再次使用。 */
void xserial_close(void* handle)
{
    xserial_port* port = (xserial_port*)handle;
    if (port == NULL) return;
    if (port->File != NULL) (void)xrtClose(port->File);
#ifndef _WIN32
    if (port->Fd >= 0) (void)close(port->Fd);
#endif
    xrtBufferUnit(&port->Queue);
    if (port->Mutex != NULL) (void)xrtMutexDestroy(port->Mutex);
    xrtFree(port->Name);
    xrtFree(port);
}



bool xserial_is_open(void* handle)
{
    return handle != NULL;
}



int64_t xserial_write(void* handle, const void* data, size_t size)
{
    xserial_port* port = (xserial_port*)handle;
    size_t count = 0;
    bool ok;
    if (size != 0 && data == NULL) {
        serial_error(XERR_ARGUMENT, "serial: nonempty write requires data");
        return -1;
    }
    if (!serial_lock(port)) return -1;
    if (port->Loopback) {
        size_t room = XSERIAL_LOOPBACK_CAPACITY - port->Queue.Size;
        count = size < room ? size : room;
        ok = count == 0 || xrtBufferAppend(&port->Queue,
            (xbytesview){(cbytes)data, count});
    } else {
        size_t request = size < XSERIAL_IO_CHUNK ? size : XSERIAL_IO_CHUNK;
#ifdef _WIN32
        ok = xrtWrite(port->File, data, request, &count);
#else
        int ready = request == 0 ? 0 : serial_posix_wait(port->Fd, POLLOUT, 1000);
        if (ready > 0) {
            ssize_t written;
            do { written = write(port->Fd, data, request); } while (written < 0 && errno == EINTR);
            if (written >= 0) count = (size_t)written;
            else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                (void)serial_posix_error("write");
                ready = -1;
            }
        }
        ok = ready >= 0;
#endif
    }
    (void)xrtMutexUnlock(port->Mutex);
    return ok ? (int64_t)count : -1;
}



/* C 字符串长度就是字节数，不把 UTF-8 字符数量传给设备。 */
int64_t xserial_write_text(void* handle, const char* text)
{
    if (text == NULL) {
        serial_error(XERR_ARGUMENT, "serial: text is null");
        return -1;
    }
    return xserial_write(handle, text, strlen(text));
}



/* XLang 字符串可包含 NUL，按显式字节长度传输。 */
int64_t xserial_write_text_len(void* handle, const char* text, size_t size)
{
    return xserial_write(handle, text, size);
}



int64_t xserial_write_bytes(void* handle, void* data, size_t size)
{
    return xserial_write(handle, data, size);
}



/* 返回独立拥有的 xbuffer；先完成分配，失败时不消费接收队列。 */
void* xserial_read(void* handle, int64_t maxBytes)
{
    xserial_port* port = (xserial_port*)handle;
    xbuffer* result;
    size_t count;
    bool ok = true;
    if (maxBytes < 0) {
        serial_error(XERR_ARGUMENT, "serial: maxBytes must be nonnegative");
        return NULL;
    }
    if (!serial_lock(port)) return NULL;
    count = (uint64_t)maxBytes < XSERIAL_IO_CHUNK
        ? (size_t)maxBytes : XSERIAL_IO_CHUNK;
    if (port->Loopback && count > port->Queue.Size) count = port->Queue.Size;
    result = xrtBufferCreate();
    if (result == NULL || !xrtBufferReserve(result, count)) {
        xrtBufferDestroy(result);
        (void)xrtMutexUnlock(port->Mutex);
        return NULL;
    }
    if (port->Loopback) {
        if (count != 0) {
            memcpy(result->Data, port->Queue.Data, count);
            ok = xrtBufferRemove(&port->Queue, 0, count);
        }
    } else if (count != 0) {
#ifdef _WIN32
        ok = xrtRead(port->File, result->Data, count, &count);
#else
        int ready = serial_posix_wait(port->Fd, POLLIN, 50);
        if (ready > 0) {
            ssize_t received;
            do { received = read(port->Fd, result->Data, count); } while (received < 0 && errno == EINTR);
            if (received >= 0) count = (size_t)received;
            else if (errno == EAGAIN || errno == EWOULDBLOCK) count = 0;
            else ok = serial_posix_error("read");
        } else {
            count = 0;
            ok = ready == 0;
        }
#endif
    }
    result->Size = ok ? count : 0;
    (void)xrtMutexUnlock(port->Mutex);
    if (!ok) {
        xrtBufferDestroy(result);
        return NULL;
    }
    return result;
}



int64_t xserial_available(void* handle)
{
    xserial_port* port = (xserial_port*)handle;
    int64_t result = -1;
    if (!serial_lock(port)) return -1;
    if (port->Loopback) {
        result = (int64_t)port->Queue.Size;
    }
#ifdef _WIN32
    else {
        COMSTAT status;
        DWORD errors;
        if (ClearCommError((HANDLE)xrtFileNative(port->File), &errors, &status))
            result = (int64_t)status.cbInQue;
        else (void)serial_windows_error("available");
    }
#endif
#ifndef _WIN32
    else {
        int pending = 0;
        if (ioctl(port->Fd, FIONREAD, &pending) == 0) result = pending;
        else (void)serial_posix_error("available");
    }
#endif
    (void)xrtMutexUnlock(port->Mutex);
    return result;
}



/* flush 丢弃排队的数据，不等待设备把数据发送完毕。 */
bool xserial_flush(void* handle)
{
    xserial_port* port = (xserial_port*)handle;
    bool ok = true;
    if (!serial_lock(port)) return false;
    if (port->Loopback) xrtBufferClear(&port->Queue);
#ifdef _WIN32
    else if (!PurgeComm((HANDLE)xrtFileNative(port->File), PURGE_RXCLEAR | PURGE_TXCLEAR))
        ok = serial_windows_error("flush");
#endif
#ifndef _WIN32
    else if (tcflush(port->Fd, TCIOFLUSH) != 0)
        ok = serial_posix_error("flush");
#endif
    (void)xrtMutexUnlock(port->Mutex);
    return ok;
}



int64_t xserial_get_baudrate(void* handle)
{
    const xserial_port* port = (const xserial_port*)handle;
    return port != NULL ? port->Baudrate : 0;
}



const char* xserial_get_port(void* handle)
{
    const xserial_port* port = (const xserial_port*)handle;
    return port != NULL ? port->Name : "";
}
