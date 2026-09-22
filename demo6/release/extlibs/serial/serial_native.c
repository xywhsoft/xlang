#include "include/serial.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif



/* 设备句柄与字节缓冲由 XRT 管理；配置完成后保持不变。 */
typedef struct xserial_port {
    xfile File;
    xmutex* Mutex;
    xbuffer Queue;
    str Name;
    int64_t Baudrate;
    bool Loopback;
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
    port->Baudrate = baudrate;
    port->Loopback = strcmp(name, "LOOPBACK") == 0 || strcmp(name, "SIM") == 0;
    port->Name = xrtStrDup(name);
    port->Mutex = xrtMutexCreate();
    if (port->Name == NULL || port->Mutex == NULL) {
        xserial_close(port);
        return NULL;
    }
    if (!port->Loopback) {
#ifdef _WIN32
        if (!serial_open_device(port, databits, stopbits, parity)) {
            xserial_close(port);
            return NULL;
        }
#else
        serial_error(XERR_UNSUPPORTED, "serial: physical ports require Windows");
        xserial_close(port);
        return NULL;
#endif
    }
    return port;
}



/* 调用方必须先结束使用该句柄的操作；close 消费句柄，不允许再次使用。 */
void xserial_close(void* handle)
{
    xserial_port* port = (xserial_port*)handle;
    if (port == NULL) return;
    if (port->File != NULL) (void)xrtClose(port->File);
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
        ok = xrtWrite(port->File, data, request, &count);
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
        ok = xrtRead(port->File, result->Data, count, &count);
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
